#include "cropsim/generators/field_set_generator.hpp"

#include "cropsim/generators/generator_factory.hpp"
#include "geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <yaml-cpp/yaml.h>

namespace cropsim::generators {
namespace {
constexpr std::uint64_t seed_domain = 0x534545444c41594fULL;
constexpr std::uint64_t priority_domain = 0x534545445052494fULL;
constexpr std::uint64_t field_domain = 0x4649454c4447454eULL;
constexpr std::uint64_t row_domain = 0x524f5747454e4552ULL;
constexpr std::uint64_t crop_domain = 0x43524f5047454e45ULL;
constexpr std::size_t maximum_layout_attempts = 128U;
constexpr std::size_t maximum_lattice_refinements = 16U;

struct Bounds final {
  double min_x;
  double min_y;
  double max_x;
  double max_y;
};

struct SeedCandidate final {
  Point2 point;
  std::size_t rank;
  double priority;
};

Point2 yaml_point(const YAML::Node &node, const char *field) {
  if (!node || !node.IsSequence() || node.size() != 2U) {
    throw std::invalid_argument(std::string(field) +
                                " point must contain two numbers");
  }
  return {node[0U].as<double>(), node[1U].as<double>()};
}

Polygon2 yaml_polygon(const YAML::Node &node) {
  if (!node || !node.IsSequence()) {
    throw std::invalid_argument("field set bounds must be a point sequence");
  }
  Polygon2 polygon;
  polygon.vertices.reserve(node.size());
  for (const auto &point : node) {
    polygon.vertices.push_back(yaml_point(point, "bounds"));
  }
  return geometry::validate_polygon(polygon, "field set bounds");
}

Bounds bounds(const Polygon2 &polygon) {
  Bounds result{std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity(),
                -std::numeric_limits<double>::infinity()};
  for (const auto &point : polygon.vertices) {
    result.min_x = std::min(result.min_x, point.x);
    result.min_y = std::min(result.min_y, point.y);
    result.max_x = std::max(result.max_x, point.x);
    result.max_y = std::max(result.max_y, point.y);
  }
  return result;
}

std::vector<Point2> clip_halfplane(const std::vector<Point2> &polygon,
                                   const Point2 midpoint, const Point2 normal,
                                   const double limit) {
  std::vector<Point2> output;
  if (polygon.empty()) {
    return output;
  }
  const auto signed_distance = [&](const Point2 point) {
    return (point.x - midpoint.x) * normal.x +
           (point.y - midpoint.y) * normal.y - limit;
  };
  auto previous = polygon.back();
  auto previous_distance = signed_distance(previous);
  for (const auto current : polygon) {
    const auto current_distance = signed_distance(current);
    const auto previous_inside = previous_distance <= 0.0;
    const auto current_inside = current_distance <= 0.0;
    if (previous_inside != current_inside) {
      const auto ratio = previous_distance / (previous_distance - current_distance);
      output.push_back({previous.x + ratio * (current.x - previous.x),
                        previous.y + ratio * (current.y - previous.y)});
    }
    if (current_inside) {
      output.push_back(current);
    }
    previous = current;
    previous_distance = current_distance;
  }
  return output;
}

std::vector<Point2> rectangle(const Bounds &value) {
  return {{value.min_x, value.min_y}, {value.max_x, value.min_y},
          {value.max_x, value.max_y}, {value.min_x, value.max_y}};
}

std::vector<std::size_t>
active_neighbours(const std::vector<Point2> &cell, const Point2 seed,
                  const std::vector<Point2> &seeds, const double tolerance) {
  std::vector<std::size_t> result;
  for (std::size_t other = 0; other < seeds.size(); ++other) {
    if (seeds[other].x == seed.x && seeds[other].y == seed.y) {
      continue;
    }
    const auto dx = seeds[other].x - seed.x;
    const auto dy = seeds[other].y - seed.y;
    const auto length = std::hypot(dx, dy);
    const Point2 normal{dx / length, dy / length};
    const Point2 midpoint{(seed.x + seeds[other].x) / 2.0,
                          (seed.y + seeds[other].y) / 2.0};
    std::size_t on_boundary = 0U;
    for (const auto point : cell) {
      const auto distance = std::abs((point.x - midpoint.x) * normal.x +
                                     (point.y - midpoint.y) * normal.y);
      if (distance <= tolerance) {
        ++on_boundary;
      }
    }
    if (on_boundary >= 2U) {
      result.push_back(other);
    }
  }
  return result;
}

std::vector<Point2> seeds_for_attempt(const Polygon2 &domain,
                                      const std::size_t count,
                                      const double jitter,
                                      const GenerationKey attempt_key) {
  const auto box = bounds(domain);
  const auto width = box.max_x - box.min_x;
  const auto height = box.max_y - box.min_y;
  const auto fill = geometry::area(domain) / (width * height);
  const auto estimated = std::ceil(static_cast<double>(count) / fill * 1.25);
  if (!std::isfinite(estimated) ||
      estimated > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
    throw std::overflow_error("field-set seed count overflow");
  }
  auto target = static_cast<std::size_t>(estimated);
  target = std::max(target, count);
  for (std::size_t refinement = 0; refinement < maximum_lattice_refinements;
       ++refinement) {
    const auto columns = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::ceil(
                std::sqrt(static_cast<double>(target) * width / height))));
    const auto rows = (target + columns - 1U) / columns;
    const auto cell_width = width / static_cast<double>(columns);
    const auto cell_height = height / static_cast<double>(rows);
    std::vector<SeedCandidate> candidates;
    for (std::size_t row = 0; row < rows; ++row) {
      for (std::size_t column = 0; column < columns; ++column) {
        const auto rank = row * columns + column;
        auto stream = RandomStream(
            attempt_key.child(seed_domain, rank).value);
        const Point2 point{
            box.min_x + (static_cast<double>(column) + 0.5) * cell_width +
                stream.symmetric_unit() * jitter * cell_width / 2.0,
            box.min_y + (static_cast<double>(row) + 0.5) * cell_height +
                stream.symmetric_unit() * jitter * cell_height / 2.0};
        if (geometry::covered_by(point, domain)) {
          auto priority = RandomStream(
              attempt_key.child(priority_domain, rank).value);
          candidates.push_back({point, rank, priority.uniform_open()});
        }
      }
    }
    if (candidates.size() >= count) {
      std::sort(candidates.begin(), candidates.end(), [](const auto &left,
                                                         const auto &right) {
        return left.priority < right.priority;
      });
      candidates.resize(count);
      std::sort(candidates.begin(), candidates.end(), [](const auto &left,
                                                         const auto &right) {
        return left.rank < right.rank;
      });
      std::vector<Point2> result;
      result.reserve(count);
      for (const auto &candidate : candidates) {
        result.push_back(candidate.point);
      }
      return result;
    }
    if (target > std::numeric_limits<std::size_t>::max() / 2U) {
      throw std::overflow_error("field-set seed lattice overflow");
    }
    target *= 2U;
  }
  throw std::runtime_error("field-set seed lattice refinement exhausted");
}

const YAML::Node require_child(const YAML::Node &parent, const char *name) {
  const auto child = parent[name];
  if (!child || !child.IsMap() || !child["gentype"]) {
    throw std::invalid_argument(std::string(name) +
                                " must be a map with gentype");
  }
  return child;
}
} // namespace

void FieldSetGenerator::generate(const YAML::Node &node,
                                 GenerationContext &context,
                                 const GenerationKey key,
                                 const GeneratorRegistry &registry,
                                 std::vector<Crop> &destination) const {
  const auto field_node = require_child(node, "field");
  const auto row_node = require_child(field_node, "row");
  const auto crop_node = require_child(row_node, "crop");
  const auto field_generator =
      registry.fields().create(field_node["gentype"].as<std::string>());
  const auto row_generator =
      registry.rows().create(row_node["gentype"].as<std::string>());
  const auto crop_generator =
      registry.crops().create(crop_node["gentype"].as<std::string>());
  const auto fields = generate_fields(node, key);
  for (std::size_t field_index = 0; field_index < fields.size(); ++field_index) {
    const auto field_key = key.child(field_domain, field_index);
    const auto rows = field_generator->generate(field_node, fields[field_index],
                                                field_key);
    if (rows.empty()) {
      throw std::runtime_error("field generator produced no rows");
    }
    for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
      const auto row_key = field_key.child(row_domain, row_index);
      const auto positions =
          row_generator->generate(row_node, rows[row_index], row_key);
      if (positions.empty()) {
        throw std::runtime_error("row generator produced no crops");
      }
      if (positions.size() > destination.max_size() - destination.size()) {
        throw std::overflow_error("hierarchical crop destination size overflow");
      }
      for (std::size_t crop_index = 0; crop_index < positions.size();
           ++crop_index) {
        const auto crop_key = row_key.child(crop_domain, crop_index);
        auto morphology = context.morphology(crop_key);
        auto leaves = crop_generator->generate(crop_node, morphology);
        destination.push_back(context.make_crop(positions[crop_index].x,
                                                positions[crop_index].y,
                                                std::move(leaves)));
      }
    }
  }
}

std::vector<FieldRegion>
VoronoiFieldSetGenerator::generate_fields(const YAML::Node &node,
                                          const GenerationKey key) const {
  const auto domain = yaml_polygon(node["bounds"]);
  const auto count = node["count"].as<std::size_t>();
  const auto road_width =
      node["road_width"] ? node["road_width"].as<double>() : 3.0;
  const auto jitter =
      node["seed_jitter"] ? node["seed_jitter"].as<double>() : 0.8;
  const auto minimum_area = node["minimum_field_area"]
                                ? node["minimum_field_area"].as<double>()
                                : 0.0;
  const auto minimum_width = node["minimum_field_width"]
                                 ? node["minimum_field_width"].as<double>()
                                 : 0.0;
  if (count == 0U || !std::isfinite(road_width) || road_width < 0.0 ||
      !std::isfinite(jitter) || jitter < 0.0 || jitter > 1.0 ||
      !std::isfinite(minimum_area) || minimum_area < 0.0 ||
      !std::isfinite(minimum_width) || minimum_width < 0.0) {
    throw std::invalid_argument("invalid field-set parameters");
  }
  if (count > std::vector<FieldRegion>{}.max_size()) {
    throw std::overflow_error("field-set count overflow");
  }
  const auto box = bounds(domain);
  const auto diagonal = std::hypot(box.max_x - box.min_x,
                                   box.max_y - box.min_y);
  const auto tolerance = diagonal * 1e-9;
  for (std::size_t attempt = 0; attempt < maximum_layout_attempts; ++attempt) {
    const auto attempt_key = key.child(seed_domain, attempt);
    const auto seeds = seeds_for_attempt(domain, count, jitter, attempt_key);
    std::vector<FieldRegion> fields;
    fields.reserve(count);
    bool valid = true;
    for (std::size_t index = 0; index < seeds.size(); ++index) {
      auto raw = rectangle(box);
      for (std::size_t other = 0; other < seeds.size(); ++other) {
        if (other == index) {
          continue;
        }
        const auto dx = seeds[other].x - seeds[index].x;
        const auto dy = seeds[other].y - seeds[index].y;
        const auto length = std::hypot(dx, dy);
        const Point2 normal{dx / length, dy / length};
        const Point2 midpoint{(seeds[index].x + seeds[other].x) / 2.0,
                              (seeds[index].y + seeds[other].y) / 2.0};
        raw = clip_halfplane(raw, midpoint, normal, 0.0);
      }
      const auto neighbours =
          active_neighbours(raw, seeds[index], seeds, tolerance);
      auto shifted = rectangle(box);
      for (const auto other : neighbours) {
        const auto dx = seeds[other].x - seeds[index].x;
        const auto dy = seeds[other].y - seeds[index].y;
        const auto length = std::hypot(dx, dy);
        const Point2 normal{dx / length, dy / length};
        const Point2 midpoint{(seeds[index].x + seeds[other].x) / 2.0,
                              (seeds[index].y + seeds[other].y) / 2.0};
        shifted = clip_halfplane(shifted, midpoint, normal, -road_width / 2.0);
      }
      if (shifted.size() < 3U) {
        valid = false;
        break;
      }
      const auto pieces = geometry::intersect({shifted}, domain);
      if (pieces.empty() || geometry::area(pieces.front()) < minimum_area ||
          geometry::minimum_oriented_width(pieces.front()) < minimum_width) {
        valid = false;
        break;
      }
      fields.push_back({static_cast<std::uint64_t>(index), seeds[index],
                        pieces.front()});
    }
    if (valid && fields.size() == count) {
      return fields;
    }
  }
  throw std::runtime_error(
      "field-set layout exhausted after 128 deterministic attempts");
}

} // namespace cropsim::generators
