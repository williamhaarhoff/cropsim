#include "cropsim/generators/parallel_row_field_generator.hpp"

#include "geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <yaml-cpp/yaml.h>

namespace cropsim::generators {
namespace {
constexpr double pi = 3.141592653589793238462643383279502884;
constexpr std::uint64_t row_jitter_domain = 0x524f574a49545452ULL;

double normalize(double angle) {
  angle = std::fmod(angle, pi);
  if (angle < 0.0) {
    angle += pi;
  }
  return angle;
}

double orientation(const YAML::Node &node, const Polygon2 &boundary) {
  const auto value = node["orientation"];
  if (!value || (value.IsScalar() && value.as<std::string>() == "auto")) {
    return geometry::principal_orientation(boundary);
  }
  return value.as<double>();
}
} // namespace

std::vector<RowSegment>
ParallelRowFieldGenerator::generate(const YAML::Node &node,
                                    const FieldRegion &field,
                                    const GenerationKey key) const {
  const auto spacing =
      node["row_spacing"] ? node["row_spacing"].as<double>() : 0.75;
  const auto jitter =
      node["row_jitter"] ? node["row_jitter"].as<double>() : 0.0;
  const auto headland =
      node["headland"] ? node["headland"].as<double>() : 0.5;
  const auto offset = node["orientation_offset"]
                          ? node["orientation_offset"].as<double>()
                          : 0.0;
  auto angle = orientation(node, field.boundary);
  if (!std::isfinite(spacing) || spacing <= 0.0 || !std::isfinite(jitter) ||
      jitter < 0.0 || jitter >= spacing / 2.0 ||
      !std::isfinite(headland) || headland < 0.0 ||
      !std::isfinite(angle) || !std::isfinite(offset)) {
    throw std::invalid_argument("invalid parallel-row field parameters");
  }
  angle = normalize(angle + offset);
  const Point2 direction{std::cos(angle), std::sin(angle)};
  const Point2 normal{-direction.y, direction.x};
  const auto plantable = geometry::inset(field.boundary, headland);
  if (plantable.empty()) {
    throw std::runtime_error("field headland leaves no plantable area");
  }
  std::vector<RowSegment> result;
  std::uint64_t row_index = 0U;
  for (const auto &component : plantable) {
    auto min_along = std::numeric_limits<double>::infinity();
    auto max_along = -min_along;
    auto min_across = min_along;
    auto max_across = -min_along;
    for (const auto &point : component.vertices) {
      const auto along = point.x * direction.x + point.y * direction.y;
      const auto across = point.x * normal.x + point.y * normal.y;
      min_along = std::min(min_along, along);
      max_along = std::max(max_along, along);
      min_across = std::min(min_across, across);
      max_across = std::max(max_across, across);
    }
    const auto width = max_across - min_across;
    const auto raw_count = std::floor(width / spacing);
    if (!std::isfinite(raw_count) ||
        raw_count > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
      throw std::overflow_error("field row count overflow");
    }
    const auto count = std::max<std::size_t>(
        1U, static_cast<std::size_t>(raw_count));
    const auto occupied = static_cast<double>(count - 1U) * spacing;
    const auto start = (min_across + max_across - occupied) / 2.0;
    const auto extension = std::max(spacing, max_along - min_along);
    for (std::size_t index = 0; index < count; ++index) {
      auto stream = RandomStream(key.child(row_jitter_domain, row_index).value);
      const auto across = start + static_cast<double>(index) * spacing +
                          stream.symmetric_unit() * jitter;
      const Point2 begin{direction.x * (min_along - extension) +
                             normal.x * across,
                         direction.y * (min_along - extension) +
                             normal.y * across};
      const Point2 end{direction.x * (max_along + extension) + normal.x * across,
                       direction.y * (max_along + extension) + normal.y * across};
      auto segments = geometry::clip_line(
          component, begin, end, field.seed_index, row_index, spacing);
      result.insert(result.end(), segments.begin(), segments.end());
      ++row_index;
    }
  }
  return result;
}

} // namespace cropsim::generators
