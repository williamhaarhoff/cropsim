#include "builtin_generators.hpp"
#include "cropsim/generators/generator_registry.hpp"

#include "geometry.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <yaml-cpp/yaml.h>

namespace cropsim::generators {
namespace {
constexpr double pi = 3.141592653589793238462643383279502884;
constexpr std::uint64_t row_jitter_domain = 0x524f574a49545452ULL;
constexpr std::uint64_t bed_wave_domain = 0x4245445741564553ULL;
constexpr std::uint64_t crop_phase_domain = 0x43524f5050484153ULL;
constexpr std::size_t curve_segments_per_wavelength = 64U;
constexpr std::size_t maximum_curve_segments = 1'000'000U;

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

struct ProjectedBounds final {
  double min_along{std::numeric_limits<double>::infinity()};
  double max_along{-std::numeric_limits<double>::infinity()};
  double min_across{std::numeric_limits<double>::infinity()};
  double max_across{-std::numeric_limits<double>::infinity()};
};

ProjectedBounds projected_bounds(const Polygon2 &polygon,
                                 const Point2 direction, const Point2 normal) {
  ProjectedBounds bounds;
  for (const auto &point : polygon.vertices) {
    const auto along = point.x * direction.x + point.y * direction.y;
    const auto across = point.x * normal.x + point.y * normal.y;
    bounds.min_along = std::min(bounds.min_along, along);
    bounds.max_along = std::max(bounds.max_along, along);
    bounds.min_across = std::min(bounds.min_across, across);
    bounds.max_across = std::max(bounds.max_across, across);
  }
  return bounds;
}

std::vector<Point2> sine_line(const Point2 direction, const Point2 normal,
                              const double first, const double last,
                              const double bed_across, const double line_offset,
                              const double amplitude, const double wavelength,
                              const double phase) {
  const auto raw_segments = std::ceil(
      (last - first) * static_cast<double>(curve_segments_per_wavelength) /
      wavelength);
  if (!std::isfinite(raw_segments) || raw_segments < 1.0 ||
      raw_segments > static_cast<double>(maximum_curve_segments)) {
    throw std::overflow_error("bed curve sampling limit exceeded");
  }
  const auto segments = static_cast<std::size_t>(raw_segments);
  std::vector<Point2> result;
  result.reserve(segments + 1U);
  const auto angular_frequency = 2.0 * pi / wavelength;
  for (std::size_t index = 0; index <= segments; ++index) {
    const auto fraction =
        static_cast<double>(index) / static_cast<double>(segments);
    const auto along = first + (last - first) * fraction;
    const auto argument = angular_frequency * along + phase;
    const auto wave = amplitude * std::sin(argument);
    const auto slope = amplitude * angular_frequency * std::cos(argument);
    const auto inverse_tangent_length = 1.0 / std::hypot(1.0, slope);
    const Point2 local_normal{
        (normal.x - direction.x * slope) * inverse_tangent_length,
        (normal.y - direction.y * slope) * inverse_tangent_length};
    result.push_back({direction.x * along + normal.x * (bed_across + wave) +
                          local_normal.x * line_offset,
                      direction.y * along + normal.y * (bed_across + wave) +
                          local_normal.y * line_offset});
  }
  return result;
}

std::vector<RowSegment> generate_bedded_rows(const YAML::Node &node,
                                             const FieldRegion &field,
                                             const GenerationKey key,
                                             const double headland,
                                             double angle) {
  if (node["row_spacing"] || node["row_jitter"]) {
    throw std::invalid_argument(
        "beds cannot be combined with row_spacing or row_jitter");
  }
  auto beds = node["beds"];
  if (!beds.IsMap()) {
    throw std::invalid_argument("beds must be a map");
  }
  const auto top_width =
      beds["top_width"] ? beds["top_width"].as<double>() : 1.8;
  const auto center_spacing =
      beds["center_spacing"] ? beds["center_spacing"].as<double>() : 2.0;
  const auto lines_per_bed =
      beds["lines_per_bed"] ? beds["lines_per_bed"].as<std::size_t>() : 6U;
  auto wave = beds["wave"];
  if (wave && !wave.IsMap()) {
    throw std::invalid_argument("bed wave must be a map");
  }
  const auto amplitude =
      wave && wave["amplitude"] ? wave["amplitude"].as<double>() : 0.2;
  const auto wavelength =
      wave && wave["wavelength"] ? wave["wavelength"].as<double>() : 100.0;
  auto random_phase = true;
  auto explicit_phase = 0.0;
  if (wave) {
    auto configured_phase = wave["phase"];
    if (configured_phase) {
      if (configured_phase.IsScalar() &&
          configured_phase.as<std::string>() == "random") {
        random_phase = true;
      } else {
        random_phase = false;
        explicit_phase = configured_phase.as<double>();
      }
    }
  }
  if (!std::isfinite(top_width) || top_width <= 0.0 ||
      !std::isfinite(center_spacing) || center_spacing < top_width ||
      lines_per_bed == 0U || lines_per_bed > 10U || !std::isfinite(amplitude) ||
      amplitude < 0.0 || !std::isfinite(wavelength) || wavelength <= 0.0 ||
      !std::isfinite(explicit_phase)) {
    throw std::invalid_argument("invalid bed parameters");
  }

  angle = normalize(angle);
  const Point2 direction{std::cos(angle), std::sin(angle)};
  const Point2 normal{-direction.y, direction.x};
  const auto plantable = geometry::inset(field.boundary, headland);
  if (plantable.empty()) {
    throw std::runtime_error("field headland leaves no plantable area");
  }
  const auto line_spacing = top_width / static_cast<double>(lines_per_bed);
  std::vector<RowSegment> result;
  std::uint64_t row_index = 0U;
  std::uint64_t bed_index = 0U;
  for (const auto &component : plantable) {
    const auto bounds = projected_bounds(component, direction, normal);
    const auto width = bounds.max_across - bounds.min_across;
    if (width < top_width) {
      continue;
    }
    const auto raw_count =
        std::floor((width - top_width) / center_spacing) + 1.0;
    if (!std::isfinite(raw_count) ||
        raw_count >
            static_cast<double>(std::numeric_limits<std::size_t>::max())) {
      throw std::overflow_error("field bed count overflow");
    }
    const auto bed_count = static_cast<std::size_t>(raw_count);
    if (bed_count > result.max_size() / lines_per_bed) {
      throw std::overflow_error("field bed line count overflow");
    }
    const auto occupied = static_cast<double>(bed_count - 1U) * center_spacing;
    const auto first_bed =
        (bounds.min_across + bounds.max_across - occupied) / 2.0;
    const auto extension =
        std::max(center_spacing, bounds.max_along - bounds.min_along);
    const auto first = bounds.min_along - extension;
    const auto last = bounds.max_along + extension;
    for (std::size_t bed = 0; bed < bed_count; ++bed, ++bed_index) {
      const auto bed_across =
          first_bed + static_cast<double>(bed) * center_spacing;
      auto phase_stream =
          RandomStream(key.child(bed_wave_domain, bed_index).value);
      const auto phase = random_phase ? 2.0 * pi * phase_stream.uniform_open()
                                      : explicit_phase;
      for (std::size_t line = 0; line < lines_per_bed; ++line) {
        if (row_index == std::numeric_limits<std::uint64_t>::max()) {
          throw std::overflow_error("field row index overflow");
        }
        const auto line_offset =
            (static_cast<double>(line) + 0.5) * line_spacing - top_width / 2.0;
        const auto path = sine_line(direction, normal, first, last, bed_across,
                                    line_offset, amplitude, wavelength, phase);
        auto crop_phase_stream =
            RandomStream(key.child(crop_phase_domain, row_index).value);
        const auto crop_phase = crop_phase_stream.uniform_open();
        auto segments = geometry::clip_path(component, path, field.seed_index,
                                            row_index, line_spacing,
                                            crop_phase);
        if (segments.size() > result.max_size() - result.size()) {
          throw std::overflow_error("field row segment count overflow");
        }
        result.insert(result.end(), std::make_move_iterator(segments.begin()),
                      std::make_move_iterator(segments.end()));
        ++row_index;
      }
    }
  }
  if (result.empty()) {
    throw std::runtime_error("field is too narrow for a complete bed");
  }
  return result;
}
} // namespace

std::vector<RowSegment>
ParallelRowFieldGenerator::generate(const YAML::Node &node,
                                    const FieldRegion &field,
                                    const GenerationKey key) const {
  const auto headland = node["headland"] ? node["headland"].as<double>() : 0.5;
  const auto offset = node["orientation_offset"]
                          ? node["orientation_offset"].as<double>()
                          : 0.0;
  auto angle = orientation(node, field.boundary);
  if (node["beds"]) {
    if (!std::isfinite(headland) || headland < 0.0 || !std::isfinite(angle) ||
        !std::isfinite(offset)) {
      throw std::invalid_argument("invalid parallel-row field parameters");
    }
    return generate_bedded_rows(node, field, key, headland, angle + offset);
  }
  const auto spacing =
      node["row_spacing"] ? node["row_spacing"].as<double>() : 0.75;
  const auto jitter =
      node["row_jitter"] ? node["row_jitter"].as<double>() : 0.0;
  if (!std::isfinite(spacing) || spacing <= 0.0 || !std::isfinite(jitter) ||
      jitter < 0.0 || jitter >= spacing / 2.0 || !std::isfinite(headland) ||
      headland < 0.0 || !std::isfinite(angle) || !std::isfinite(offset)) {
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
        raw_count >
            static_cast<double>(std::numeric_limits<std::size_t>::max())) {
      throw std::overflow_error("field row count overflow");
    }
    const auto count =
        std::max<std::size_t>(1U, static_cast<std::size_t>(raw_count));
    const auto occupied = static_cast<double>(count - 1U) * spacing;
    const auto start = (min_across + max_across - occupied) / 2.0;
    const auto extension = std::max(spacing, max_along - min_along);
    for (std::size_t index = 0; index < count; ++index) {
      auto stream = RandomStream(key.child(row_jitter_domain, row_index).value);
      const auto across = start + static_cast<double>(index) * spacing +
                          stream.symmetric_unit() * jitter;
      const Point2 begin{
          direction.x * (min_along - extension) + normal.x * across,
          direction.y * (min_along - extension) + normal.y * across};
      const Point2 end{
          direction.x * (max_along + extension) + normal.x * across,
          direction.y * (max_along + extension) + normal.y * across};
      auto segments = geometry::clip_line(component, begin, end,
                                          field.seed_index, row_index, spacing);
      result.insert(result.end(), segments.begin(), segments.end());
      ++row_index;
    }
  }
  return result;
}

void register_builtin_field_generators(FieldGeneratorFactory &factory) {
  factory.register_generator("parallel_rows", [] {
    return std::make_unique<ParallelRowFieldGenerator>();
  });
}

} // namespace cropsim::generators
