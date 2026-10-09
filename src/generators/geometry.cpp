#include "geometry.hpp"

#include <boost/geometry/algorithms/buffer.hpp>
#include <boost/geometry/algorithms/convex_hull.hpp>
#include <boost/geometry/algorithms/intersection.hpp>
#include <boost/geometry/algorithms/is_valid.hpp>
#include <boost/geometry/strategies/buffer.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace cropsim::generators::geometry {
namespace {
constexpr double pi = 3.141592653589793238462643383279502884;

bool point_less(const Point2 &left, const Point2 &right) {
  return left.x < right.x || (left.x == right.x && left.y < right.y);
}

double normalize(double angle) {
  angle = std::fmod(angle, pi);
  if (angle < 0.0) {
    angle += pi;
  }
  return angle;
}

struct OrientedBox final {
  double angle{};
  double width{};
  double height{};
  double area{};
};

OrientedBox oriented_box(const Polygon2 &polygon) {
  BoostPolygon hull;
  bg::convex_hull(to_boost(polygon), hull);
  const auto points = from_boost(hull).vertices;
  if (points.size() < 3U) {
    throw std::invalid_argument("field polygon has no oriented bounding box");
  }
  OrientedBox best{0.0, 0.0, 0.0, std::numeric_limits<double>::infinity()};
  for (std::size_t index = 0; index < points.size(); ++index) {
    const auto &first = points[index];
    const auto &second = points[(index + 1U) % points.size()];
    const auto angle = normalize(std::atan2(second.y - first.y,
                                            second.x - first.x));
    const auto cosine = std::cos(angle);
    const auto sine = std::sin(angle);
    auto min_x = std::numeric_limits<double>::infinity();
    auto max_x = -min_x;
    auto min_y = min_x;
    auto max_y = -min_x;
    for (const auto &point : points) {
      const auto x = cosine * point.x + sine * point.y;
      const auto y = -sine * point.x + cosine * point.y;
      min_x = std::min(min_x, x);
      max_x = std::max(max_x, x);
      min_y = std::min(min_y, y);
      max_y = std::max(max_y, y);
    }
    const auto width = max_x - min_x;
    const auto height = max_y - min_y;
    const auto box_area = width * height;
    const auto candidate_angle =
        normalize(width >= height ? angle : angle + pi / 2.0);
    if (box_area < best.area ||
        (box_area == best.area && candidate_angle < best.angle)) {
      best = {candidate_angle, std::max(width, height),
              std::min(width, height), box_area};
    }
  }
  return best;
}
} // namespace

BoostPolygon to_boost(const Polygon2 &polygon) {
  BoostPolygon result;
  auto &ring = result.outer();
  ring.reserve(polygon.vertices.size() + 1U);
  for (const auto &point : polygon.vertices) {
    ring.emplace_back(point.x, point.y);
  }
  if (!polygon.vertices.empty()) {
    ring.emplace_back(polygon.vertices.front().x, polygon.vertices.front().y);
  }
  bg::correct(result);
  return result;
}

Polygon2 from_boost(const BoostPolygon &polygon) {
  Polygon2 result;
  const auto &ring = polygon.outer();
  const auto count = ring.size() > 1U ? ring.size() - 1U : 0U;
  result.vertices.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    result.vertices.push_back({bg::get<0>(ring[index]), bg::get<1>(ring[index])});
  }
  return canonical(std::move(result));
}

Polygon2 canonical(Polygon2 polygon) {
  if (polygon.vertices.size() > 1U &&
      polygon.vertices.front().x == polygon.vertices.back().x &&
      polygon.vertices.front().y == polygon.vertices.back().y) {
    polygon.vertices.pop_back();
  }
  if (polygon.vertices.empty()) {
    return polygon;
  }
  const auto first = std::min_element(polygon.vertices.begin(),
                                      polygon.vertices.end(), point_less);
  std::rotate(polygon.vertices.begin(), first, polygon.vertices.end());
  return polygon;
}

Polygon2 validate_polygon(const Polygon2 &polygon, const char *field) {
  if (polygon.vertices.size() < 3U) {
    throw std::invalid_argument(std::string(field) +
                                " must contain at least three vertices");
  }
  for (const auto &point : polygon.vertices) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
      throw std::invalid_argument(std::string(field) + " must be finite");
    }
  }
  auto normalized = canonical(polygon);
  const auto boost_polygon = to_boost(normalized);
  std::string reason;
  if (!bg::is_valid(boost_polygon, reason) || bg::area(boost_polygon) <= 0.0) {
    throw std::invalid_argument(std::string("invalid ") + field + ": " + reason);
  }
  return from_boost(boost_polygon);
}

double area(const Polygon2 &polygon) { return std::abs(bg::area(to_boost(polygon))); }

bool covered_by(const Point2 &point, const Polygon2 &polygon) {
  return bg::covered_by(BoostPoint(point.x, point.y), to_boost(polygon));
}

std::vector<Polygon2> intersect(const Polygon2 &first, const Polygon2 &second) {
  BoostMultiPolygon output;
  bg::intersection(to_boost(first), to_boost(second), output);
  std::vector<Polygon2> result;
  for (const auto &polygon : output) {
    if (std::abs(bg::area(polygon)) > 0.0) {
      result.push_back(from_boost(polygon));
    }
  }
  std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
    const auto left_area = area(left);
    const auto right_area = area(right);
    if (left_area != right_area) {
      return left_area > right_area;
    }
    return point_less(left.vertices.front(), right.vertices.front());
  });
  return result;
}

std::vector<Polygon2> inset(const Polygon2 &polygon, const double distance) {
  if (distance == 0.0) {
    return {polygon};
  }
  BoostMultiPolygon output;
  const bg::strategy::buffer::distance_symmetric<double> distance_strategy(-distance);
  const bg::strategy::buffer::side_straight side_strategy;
  const bg::strategy::buffer::join_miter join_strategy;
  const bg::strategy::buffer::end_flat end_strategy;
  const bg::strategy::buffer::point_square point_strategy;
  bg::buffer(to_boost(polygon), output, distance_strategy, side_strategy,
             join_strategy, end_strategy, point_strategy);
  std::vector<Polygon2> result;
  for (const auto &item : output) {
    if (std::abs(bg::area(item)) > 0.0) {
      result.push_back(from_boost(item));
    }
  }
  std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
    return point_less(left.vertices.front(), right.vertices.front());
  });
  return result;
}

std::vector<RowSegment>
clip_line(const Polygon2 &polygon, const Point2 begin, const Point2 end,
          const std::uint64_t field_index, const std::uint64_t row_index,
          const double row_spacing) {
  BoostLine line{{begin.x, begin.y}, {end.x, end.y}};
  std::vector<BoostLine> output;
  bg::intersection(line, to_boost(polygon), output);
  const auto dx = end.x - begin.x;
  const auto dy = end.y - begin.y;
  std::vector<RowSegment> result;
  for (const auto &item : output) {
    if (item.size() < 2U || bg::length(item) <= 0.0) {
      continue;
    }
    Point2 first{bg::get<0>(item.front()), bg::get<1>(item.front())};
    Point2 last{bg::get<0>(item.back()), bg::get<1>(item.back())};
    if (first.x * dx + first.y * dy > last.x * dx + last.y * dy) {
      std::swap(first, last);
    }
    result.push_back(
        {field_index, row_index, first, last, polygon, row_spacing});
  }
  std::sort(result.begin(), result.end(), [dx, dy](const auto &left, const auto &right) {
    return left.begin.x * dx + left.begin.y * dy <
           right.begin.x * dx + right.begin.y * dy;
  });
  return result;
}

double principal_orientation(const Polygon2 &polygon) {
  return oriented_box(polygon).angle;
}

double minimum_oriented_width(const Polygon2 &polygon) {
  return oriented_box(polygon).height;
}

} // namespace cropsim::generators::geometry
