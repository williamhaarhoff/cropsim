#include "cropsim/generators/generic_crop_generator.hpp"
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <yaml-cpp/yaml.h>

namespace cropsim::generators {
namespace {
constexpr double pi = 3.141592653589793238462643383279502884;
constexpr std::size_t maximum_rejections = 10000U;

struct Distribution {
  double mean;
  double minimum;
  double maximum;
  double standard_deviation;
  bool fixed;
};

Distribution distribution(const YAML::Node &parent, const char *name,
                          double default_mean, double default_minimum,
                          double default_maximum,
                          double default_standard_deviation) {
  const auto node = parent[name];
  if (!node) {
    return {default_mean, default_minimum, default_maximum,
            default_standard_deviation, false};
  }
  if (node.IsScalar()) {
    const auto value = node.as<double>();
    if (!std::isfinite(value)) {
      throw std::invalid_argument(std::string(name) + " must be finite");
    }
    return {value, value, value, 0.0, true};
  }
  if (!node.IsMap()) {
    throw std::invalid_argument(std::string(name) + " must be a scalar or map");
  }
  const auto mean = node["mean"] ? node["mean"].as<double>() : default_mean;
  const auto minimum = node["min"] ? node["min"].as<double>() : default_minimum;
  const auto maximum = node["max"] ? node["max"].as<double>() : default_maximum;
  const auto standard_deviation =
      node["stddev"] ? node["stddev"].as<double>() : (maximum - minimum) / 6.0;
  if (!std::isfinite(mean) || !std::isfinite(minimum) ||
      !std::isfinite(maximum) || !std::isfinite(standard_deviation) ||
      minimum > maximum || mean < minimum || mean > maximum ||
      standard_deviation < 0.0 ||
      (standard_deviation == 0.0 && minimum != maximum)) {
    throw std::invalid_argument(std::string("invalid distribution: ") + name);
  }
  return {mean, minimum, maximum, standard_deviation, minimum == maximum};
}

double sample(const Distribution &value, MorphologyContext &context) {
  if (value.fixed) {
    return value.mean;
  }
  for (std::size_t rejected = 0; rejected < maximum_rejections; ++rejected) {
    const auto magnitude = std::sqrt(-2.0 * std::log(context.uniform_open()));
    const auto normal = magnitude * std::cos(2.0 * pi * context.uniform_open());
    const auto candidate = value.mean + value.standard_deviation * normal;
    if (candidate >= value.minimum && candidate <= value.maximum) {
      return candidate;
    }
  }
  throw std::runtime_error(
      "truncated Gaussian sampling exhausted after 10000 rejections");
}
} // namespace

std::vector<EllipseLeaf>
GenericCropGenerator::generate(const YAML::Node &node,
                               MorphologyContext &context) const {
  const auto scale = node["scale"] ? node["scale"].as<double>() : 0.05;
  const auto count_dist = distribution(node, "leaf_num", 4.0, 1.0, 10.0, 1.5);
  const auto length_dist =
      distribution(node, "leaf_length", 1.0, 0.8, 1.2, 0.0666667);
  const auto width_dist =
      distribution(node, "leaf_width", 0.4, 0.2, 0.6, 0.0666667);
  const auto offset_dist =
      distribution(node, "leaf_offset", 0.0, -0.05, 0.05, 0.0166667);
  const auto orientation_dist =
      distribution(node, "leaf_orientation", 0.0, -0.5, 0.5, 0.1666667);
  if (!std::isfinite(scale) || scale <= 0.0 || count_dist.minimum < 1.0 ||
      length_dist.minimum <= 0.0 || width_dist.minimum <= 0.0 ||
      length_dist.minimum + offset_dist.minimum < 0.0) {
    throw std::invalid_argument("invalid generic crop parameters");
  }
  const auto sampled_count = sample(count_dist, context);
  if (sampled_count >
      static_cast<double>(std::numeric_limits<std::size_t>::max())) {
    throw std::overflow_error("generic crop leaf count overflow");
  }
  const auto leaf_count = static_cast<std::size_t>(std::llround(sampled_count));
  if (leaf_count == 0U) {
    throw std::invalid_argument("generic crop leaf count must be positive");
  }
  std::vector<EllipseLeaf> leaves;
  if (leaf_count > leaves.max_size()) {
    throw std::overflow_error("generic crop leaf count overflow");
  }
  leaves.reserve(leaf_count);
  for (std::size_t index = 0; index < leaf_count; ++index) {
    const auto length = sample(length_dist, context);
    const auto width = sample(width_dist, context);
    const auto offset = sample(offset_dist, context);
    const auto orientation = sample(orientation_dist, context);
    const auto theta = 2.0 * pi * static_cast<double>(index) /
                           static_cast<double>(leaf_count) +
                       orientation;
    const auto distance = scale * (length + offset);
    leaves.push_back({distance * std::cos(theta), distance * std::sin(theta),
                      scale * length, scale * width, theta});
  }
  return leaves;
}
} // namespace cropsim::generators
