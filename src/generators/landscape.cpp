#include "cropsim/generators/landscape.hpp"

#include "geometry.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <yaml-cpp/yaml.h>
#include <boost/geometry/algorithms/buffer.hpp>
#include <boost/geometry/algorithms/difference.hpp>
#include <boost/geometry/strategies/buffer.hpp>

namespace cropsim::generators {
namespace {
constexpr std::uint64_t terrain_domain = 0x4c5445525241494eULL;
constexpr std::uint64_t route_domain = 0x4c524f5554452020ULL;
constexpr std::uint64_t split_domain = 0x4c53504c49542020ULL;
constexpr std::uint64_t boundary_domain = 0x4c424f554e444152ULL;
constexpr std::size_t maximum_fields = 10000U;

struct Box { double min_x, min_y, max_x, max_y; };

Point2 point(const YAML::Node &value) {
  if (!value.IsSequence() || value.size() != 2U) {
    throw std::invalid_argument("landscape bounds points require two numbers");
  }
  return {value[0].as<double>(), value[1].as<double>()};
}

Polygon2 polygon(const YAML::Node &value) {
  if (!value || !value.IsSequence()) {
    throw std::invalid_argument("farm landscape bounds must be a point sequence");
  }
  Polygon2 result;
  result.vertices.reserve(value.size());
  for (const auto &item : value) result.vertices.push_back(point(item));
  return geometry::validate_polygon(result, "farm landscape bounds");
}

Box box(const Polygon2 &value) {
  Box result{std::numeric_limits<double>::infinity(),
             std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity()};
  for (const auto p : value.vertices) {
    result.min_x = std::min(result.min_x, p.x); result.max_x = std::max(result.max_x, p.x);
    result.min_y = std::min(result.min_y, p.y); result.max_y = std::max(result.max_y, p.y);
  }
  return result;
}

double number(const YAML::Node &parent, const char *name, const double fallback) {
  const auto child = parent ? parent[name] : YAML::Node{};
  const auto value = child ? child.as<double>() : fallback;
  if (!std::isfinite(value)) throw std::invalid_argument(std::string(name) + " must be finite");
  return value;
}

Point2 centroid(const Polygon2 &polygon_value) {
  const auto p = geometry::to_boost(polygon_value);
  geometry::BoostPoint result{0.0, 0.0};
  geometry::bg::centroid(p, result);
  return {geometry::bg::get<0>(result), geometry::bg::get<1>(result)};
}

Polygon2 rectangle(double min_x, double min_y, double max_x, double max_y) {
  return geometry::canonical({{{min_x, min_y}, {max_x, min_y},
                               {max_x, max_y}, {min_x, max_y}}});
}

Polygon2 buffered_path(const std::vector<Point2> &path, const double width) {
  geometry::BoostLine line;
  for (const auto p : path) line.emplace_back(p.x, p.y);
  geometry::BoostMultiPolygon output;
  const geometry::bg::strategy::buffer::distance_symmetric<double> distance(width / 2.0);
  const geometry::bg::strategy::buffer::side_straight side;
  const geometry::bg::strategy::buffer::join_round join(16U);
  const geometry::bg::strategy::buffer::end_flat end;
  const geometry::bg::strategy::buffer::point_circle circle(16U);
  geometry::bg::buffer(line, output, distance, side, join, end, circle);
  if (output.empty()) throw std::runtime_error("farm landscape lane buffering failed");
  return geometry::from_boost(output.front());
}

std::vector<Polygon2> subtract(const Polygon2 &domain, const Polygon2 &cut) {
  geometry::BoostMultiPolygon output;
  geometry::bg::difference(geometry::to_boost(domain), geometry::to_boost(cut), output);
  std::vector<Polygon2> result;
  for (const auto &p : output) if (std::abs(geometry::bg::area(p)) > 0.0)
    result.push_back(geometry::from_boost(p));
  std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
    const auto ca = centroid(a); const auto cb = centroid(b);
    return ca.x < cb.x || (ca.x == cb.x && ca.y < cb.y);
  });
  return result;
}

bool valid_field(const Polygon2 &p, double minimum_area, double minimum_width) {
  return geometry::area(p) >= minimum_area &&
         geometry::minimum_oriented_width(p) >= minimum_width;
}

std::vector<Polygon2> split_block(const Polygon2 &block, GenerationKey key,
                                  double variation, double irregularity,
                                  double minimum_area, double minimum_width) {
  const auto b = box(block);
  const bool vertical = (b.max_x - b.min_x) >= (b.max_y - b.min_y);
  auto random = RandomStream(key.value);
  const auto skew = (random.symmetric_unit() * 0.22 * variation) +
                    (random.symmetric_unit() * 0.08 * irregularity);
  const auto fraction = std::clamp(0.5 + skew, 0.25, 0.75);
  const auto margin = std::max(b.max_x - b.min_x, b.max_y - b.min_y) + 1.0;
  Polygon2 first_clip;
  Polygon2 second_clip;
  if (vertical) {
    const auto cut = b.min_x + fraction * (b.max_x - b.min_x);
    first_clip = rectangle(b.min_x - margin, b.min_y - margin, cut, b.max_y + margin);
    second_clip = rectangle(cut, b.min_y - margin, b.max_x + margin, b.max_y + margin);
  } else {
    const auto cut = b.min_y + fraction * (b.max_y - b.min_y);
    first_clip = rectangle(b.min_x - margin, b.min_y - margin, b.max_x + margin, cut);
    second_clip = rectangle(b.min_x - margin, cut, b.max_x + margin, b.max_y + margin);
  }
  auto first = geometry::intersect(block, first_clip);
  auto second = geometry::intersect(block, second_clip);
  if (first.size() != 1U || second.size() != 1U ||
      !valid_field(first.front(), minimum_area, minimum_width) ||
      !valid_field(second.front(), minimum_area, minimum_width)) return {};
  return {first.front(), second.front()};
}

std::string points(const Polygon2 &p, const Box &b, std::size_t width,
                   std::size_t height) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(3);
  for (std::size_t i = 0; i < p.vertices.size(); ++i) {
    if (i != 0U) out << ' ';
    out << (p.vertices[i].x - b.min_x) / (b.max_x - b.min_x) * static_cast<double>(width)
        << ',' << (b.max_y - p.vertices[i].y) / (b.max_y - b.min_y) * static_cast<double>(height);
  }
  return out.str();
}
} // namespace

LandscapeLayout generate_landscape_layout(const YAML::Node &node,
                                           const GenerationKey key) {
  auto domain = polygon(node["bounds"]);
  if (!node["count"]) throw std::invalid_argument("farm_landscape requires count");
  const auto count = node["count"].as<std::size_t>();
  if (count == 0U || count > maximum_fields) throw std::invalid_argument("farm landscape count must be between 1 and 10000");
  YAML::Node terrain(YAML::NodeType::Map);
  YAML::Node infrastructure(YAML::NodeType::Map);
  YAML::Node parcels(YAML::NodeType::Map);
  if (node["terrain"]) terrain = node["terrain"];
  if (node["infrastructure"]) infrastructure = node["infrastructure"];
  if (node["parcels"]) parcels = node["parcels"];
  const auto relief = number(terrain, "relief", 8.0);
  const auto terrain_scale = number(terrain, "scale", 250.0);
  const auto lane_width = number(infrastructure, "lane_width", 4.0);
  const auto track_width = number(infrastructure, "track_width", 2.5);
  const auto curvature = number(infrastructure, "curvature", 0.35);
  const auto variation = number(parcels, "area_variation", 0.6);
  const auto irregularity = number(parcels, "boundary_irregularity", 0.15);
  const auto minimum_area = number(parcels, "minimum_area", 0.0);
  const auto minimum_width = number(parcels, "minimum_width", 0.0);
  if (relief < 0.0 || terrain_scale <= 0.0 || lane_width <= 0.0 ||
      track_width <= 0.0 || curvature < 0.0 || curvature > 1.0 ||
      variation < 0.0 || variation > 1.0 || irregularity < 0.0 ||
      irregularity > 1.0 || minimum_area < 0.0 || minimum_width < 0.0) {
    throw std::invalid_argument("invalid farm landscape parameters");
  }
  LandscapeLayout layout;
  layout.domain = domain;
  layout.terrain = {relief, terrain_scale, key.child(terrain_domain, 0U).value};
  std::vector<Polygon2> blocks;
  const auto bounds = box(domain);
  if (count == 1U) {
    blocks.push_back(domain);
  } else {
    const bool vertical = (bounds.max_y - bounds.min_y) >= (bounds.max_x - bounds.min_x);
    auto route_random = RandomStream(key.child(route_domain, 0U).value);
    const auto transverse = vertical ? bounds.max_x - bounds.min_x : bounds.max_y - bounds.min_y;
    const auto offset = route_random.symmetric_unit() * curvature * transverse * 0.12;
    const auto center = vertical ? (bounds.min_x + bounds.max_x) / 2.0 + offset
                                 : (bounds.min_y + bounds.max_y) / 2.0 + offset;
    std::vector<Point2> line;
    constexpr std::size_t route_segments = 16U;
    line.reserve(route_segments + 1U);
    const auto phase = route_random.uniform_open() * 6.2831853071795864769;
    const auto amplitude = curvature * transverse * 0.08;
    for (std::size_t i = 0; i <= route_segments; ++i) {
      const auto t = static_cast<double>(i) / static_cast<double>(route_segments);
      const auto bend = amplitude * std::sin(3.14159265358979323846 * t) *
                        std::sin(phase + 3.14159265358979323846 * t);
      line.push_back(vertical
        ? Point2{center + bend, bounds.min_y + t * (bounds.max_y - bounds.min_y)}
        : Point2{bounds.min_x + t * (bounds.max_x - bounds.min_x), center + bend});
    }
    auto footprint = buffered_path(line, lane_width);
    layout.corridors.push_back({CorridorType::lane, lane_width, line, footprint});
    blocks = subtract(domain, footprint);
    if (blocks.size() > count) blocks.resize(count);
  }
  std::size_t split_index = 0U;
  while (blocks.size() < count) {
    std::vector<std::size_t> order(blocks.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
      return geometry::area(blocks[a]) > geometry::area(blocks[b]);
    });
    bool split = false;
    for (const auto index : order) {
      auto pieces = split_block(blocks[index], key.child(split_domain, split_index++),
                                variation, irregularity, minimum_area, minimum_width);
      if (pieces.empty()) continue;
      blocks[index] = std::move(pieces[0]);
      blocks.push_back(std::move(pieces[1]));
      split = true;
      break;
    }
    if (!split || split_index > maximum_fields * maximum_fields)
      throw std::runtime_error("farm landscape subdivision exhausted");
  }
  std::sort(blocks.begin(), blocks.end(), [](const auto &a, const auto &b) {
    const auto ca = centroid(a); const auto cb = centroid(b);
    return ca.x < cb.x || (ca.x == cb.x && ca.y < cb.y);
  });
  for (std::size_t i = 0; i < blocks.size(); ++i) {
    if (!valid_field(blocks[i], minimum_area, minimum_width))
      throw std::runtime_error("farm landscape cannot satisfy parcel limits");
    const auto c = centroid(blocks[i]);
    layout.fields.push_back({static_cast<std::uint64_t>(i), c, blocks[i]});
    if (!layout.corridors.empty()) {
      const auto &lane = layout.corridors.front().centerline;
      const auto nearest = std::min_element(lane.begin(), lane.end(), [&](const auto a, const auto b) {
        return std::hypot(a.x-c.x,a.y-c.y) < std::hypot(b.x-c.x,b.y-c.y);
      });
      const Point2 access = *nearest;
      layout.access.push_back({i, 0U, access});
      layout.corridors.push_back({CorridorType::track, track_width, {access, c}, {}});
    }
    for (std::size_t edge = 0; edge < blocks[i].vertices.size(); ++edge) {
      auto stream = RandomStream(key.child(boundary_domain, i * 65537U + edge).value);
      const auto draw = stream.uniform_open();
      const auto type = draw < 0.38 ? BoundaryType::hedge
                        : draw < 0.62 ? BoundaryType::ditch
                                      : BoundaryType::cadastral;
      layout.boundaries.push_back({type, i, blocks[i].vertices[edge],
        blocks[i].vertices[(edge + 1U) % blocks[i].vertices.size()]});
    }
  }
  return layout;
}

GrayscaleImage render_landscape_layout(const LandscapeLayout &layout,
                                        const std::size_t width,
                                        const std::size_t height) {
  if (width == 0U || height == 0U || width > std::numeric_limits<std::size_t>::max() / height)
    throw std::invalid_argument("invalid landscape preview dimensions");
  const auto b = box(layout.domain);
  GrayscaleImage image{width, height, std::vector<std::uint8_t>(width * height, 0U)};
  for (std::size_t y = 0; y < height; ++y) for (std::size_t x = 0; x < width; ++x) {
    const Point2 p{b.min_x + (static_cast<double>(x) + 0.5) / static_cast<double>(width) * (b.max_x - b.min_x),
                   b.max_y - (static_cast<double>(y) + 0.5) / static_cast<double>(height) * (b.max_y - b.min_y)};
    if (!geometry::covered_by(p, layout.domain)) continue;
    std::uint8_t value = 32U;
    for (std::size_t i = 0; i < layout.fields.size(); ++i)
      if (geometry::covered_by(p, layout.fields[i].boundary)) value = static_cast<std::uint8_t>(96U + (i % 6U) * 18U);
    for (const auto &corridor : layout.corridors)
      if (!corridor.footprint.vertices.empty() && geometry::covered_by(p, corridor.footprint)) value = 230U;
    image.pixels[y * width + x] = value;
  }
  return image;
}

std::string landscape_layout_svg(const LandscapeLayout &layout,
                                 const std::size_t width,
                                 const std::size_t height) {
  if (width == 0U || height == 0U) throw std::invalid_argument("invalid SVG dimensions");
  const auto b = box(layout.domain);
  std::ostringstream out;
  out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
      << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << ' ' << height << "\">\n";
  out << "<polygon class=\"domain\" fill=\"#e5e0cf\" points=\"" << points(layout.domain,b,width,height) << "\"/>\n";
  for (std::size_t i=0;i<layout.fields.size();++i)
    out << "<polygon class=\"field\" data-field=\"" << i << "\" fill=\"#a9bd75\" stroke=\"#52643a\" points=\"" << points(layout.fields[i].boundary,b,width,height) << "\"/>\n";
  for (const auto &c : layout.corridors) if (!c.footprint.vertices.empty())
    out << "<polygon class=\"lane\" fill=\"#8b8175\" points=\"" << points(c.footprint,b,width,height) << "\"/>\n";
  out << std::fixed << std::setprecision(3);
  for (const auto &c : layout.corridors) if (c.type == CorridorType::track && c.centerline.size() >= 2U) {
    out << "<polyline class=\"track\" fill=\"none\" stroke=\"#b29c78\" stroke-width=\"2\" points=\"";
    for (std::size_t i=0;i<c.centerline.size();++i) {
      if (i != 0U) out << ' ';
      out << (c.centerline[i].x-b.min_x)/(b.max_x-b.min_x)*static_cast<double>(width)
          << ',' << (b.max_y-c.centerline[i].y)/(b.max_y-b.min_y)*static_cast<double>(height);
    }
    out << "\"/>\n";
  }
  out << "</svg>\n";
  return out.str();
}

void write_landscape_svg(const LandscapeLayout &layout,
                         const std::filesystem::path &path,
                         const std::size_t width, const std::size_t height) {
  std::ofstream stream(path);
  stream << landscape_layout_svg(layout, width, height);
  if (!stream) throw std::runtime_error("failed to write landscape SVG");
}

std::string format_landscape_layout_terminal(
    const LandscapeLayout &layout, const std::size_t width,
    const std::size_t height, const TerminalImageOptions &options) {
  return format_terminal_image(render_landscape_layout(layout, width, height), options);
}
} // namespace cropsim::generators
