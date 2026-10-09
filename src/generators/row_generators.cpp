#include "builtin_generators.hpp"
#include "cropsim/generators/generator_registry.hpp"

#include "geometry.hpp"

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace cropsim::generators {
namespace {
constexpr std::uint64_t crop_jitter_domain = 0x43524f504a495454ULL;
constexpr std::size_t maximum_rejections = 10000U;
} // namespace

std::vector<Point2>
LinearRowGenerator::generate(const YAML::Node &node, const RowSegment &row,
                             const GenerationKey key) const {
  const auto spacing =
      node["crop_spacing"] ? node["crop_spacing"].as<double>() : 0.25;
  const auto along_jitter =
      node["along_jitter"] ? node["along_jitter"].as<double>() : 0.0;
  const auto cross_jitter =
      node["cross_jitter"] ? node["cross_jitter"].as<double>() : 0.0;
  if (!std::isfinite(spacing) || spacing <= 0.0 ||
      !std::isfinite(along_jitter) || along_jitter < 0.0 ||
      along_jitter >= spacing / 2.0 || !std::isfinite(cross_jitter) ||
      cross_jitter < 0.0 || cross_jitter >= row.row_spacing / 2.0) {
    throw std::invalid_argument("invalid linear-row parameters");
  }
  const auto dx = row.end.x - row.begin.x;
  const auto dy = row.end.y - row.begin.y;
  const auto length = std::hypot(dx, dy);
  if (!std::isfinite(length) || length <= 0.0) {
    throw std::invalid_argument("row segment must have positive finite length");
  }
  const Point2 direction{dx / length, dy / length};
  const Point2 normal{-direction.y, direction.x};
  const auto raw_count = std::floor(length / spacing);
  if (!std::isfinite(raw_count) ||
      raw_count >
          static_cast<double>(std::numeric_limits<std::size_t>::max())) {
    throw std::overflow_error("row crop count overflow");
  }
  const auto count =
      std::max<std::size_t>(1U, static_cast<std::size_t>(raw_count));
  const auto occupied = static_cast<double>(count - 1U) * spacing;
  const auto start = (length - occupied) / 2.0;
  std::vector<Point2> result;
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    auto stream = RandomStream(key.child(crop_jitter_domain, index).value);
    bool accepted = false;
    for (std::size_t rejected = 0; rejected < maximum_rejections; ++rejected) {
      const auto along = start + static_cast<double>(index) * spacing +
                         stream.symmetric_unit() * along_jitter;
      const auto across = stream.symmetric_unit() * cross_jitter;
      const Point2 candidate{
          row.begin.x + direction.x * along + normal.x * across,
          row.begin.y + direction.y * along + normal.y * across};
      if (geometry::covered_by(candidate, row.plantable_boundary)) {
        result.push_back(candidate);
        accepted = true;
        break;
      }
    }
    if (!accepted) {
      throw std::runtime_error(
          "crop-center jitter exhausted after 10000 rejections");
    }
  }
  return result;
}

void register_builtin_row_generators(RowGeneratorFactory &factory) {
  factory.register_generator(
      "linear", [] { return std::make_unique<LinearRowGenerator>(); });
}

} // namespace cropsim::generators
