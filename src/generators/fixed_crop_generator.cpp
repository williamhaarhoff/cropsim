#include "cropsim/generators/fixed_crop_generator.hpp"
#include <stdexcept>
#include <string>
#include <yaml-cpp/yaml.h>

namespace cropsim::generators {
namespace {
double coordinate(const YAML::Node &node, std::size_t index,
                  const char *field) {
  if (!node || !node.IsSequence() || node.size() != 2U) {
    throw std::invalid_argument(std::string(field) +
                                " must contain exactly two numbers");
  }
  return node[index].as<double>();
}
} // namespace

std::vector<EllipseLeaf>
FixedCropGenerator::generate(const YAML::Node &node,
                             MorphologyContext &) const {
  const auto nodes = node["leaves"];
  if (!nodes) {
    const auto radius = node["radius"].as<double>();
    return {{0.0, 0.0, radius, radius, 0.0}};
  }
  if (!nodes.IsSequence() || nodes.size() == 0U) {
    throw std::invalid_argument("leaves must be a non-empty sequence");
  }
  std::vector<EllipseLeaf> result;
  result.reserve(nodes.size());
  for (const auto &leaf : nodes) {
    if (leaf["type"] && leaf["type"].as<std::string>() != "ellipse") {
      throw std::invalid_argument("leaf type must be ellipse");
    }
    const auto position = leaf["position"] ? leaf["position"] : leaf["pos"];
    const auto radii = leaf["radii"];
    result.push_back({coordinate(position, 0U, "leaf position"),
                      coordinate(position, 1U, "leaf position"),
                      radii              ? coordinate(radii, 0U, "radii")
                      : leaf["radius_x"] ? leaf["radius_x"].as<double>()
                                         : leaf["rx"].as<double>(),
                      radii              ? coordinate(radii, 1U, "radii")
                      : leaf["radius_y"] ? leaf["radius_y"].as<double>()
                                         : leaf["ry"].as<double>(),
                      leaf["rotation"] ? leaf["rotation"].as<double>()
                      : leaf["theta"]  ? leaf["theta"].as<double>()
                                       : 0.0});
  }
  return result;
}

void FixedCropGenerator::configure(const YAML::Node &node,
                                   const ModifierFieldSet *) {
  MorphologyContext unused(0U);
  configured_leaves_ = generate(node, unused);
}

std::vector<EllipseLeaf> FixedCropGenerator::generate(Context &) const {
  return configured_leaves_;
}
} // namespace cropsim::generators
