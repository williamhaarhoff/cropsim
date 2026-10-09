#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "cropsim/generators/generation_context.hpp"
#include "cropsim/generators/generator_factory.hpp"
#include "cropsim/world.hpp"

namespace cropsim {
namespace {

double coordinate(const YAML::Node &node, const std::size_t index,
                  const char *field) {
  if (!node || !node.IsSequence() || node.size() != 2) {
    throw std::invalid_argument(std::string(field) +
                                " must contain exactly two numbers");
  }
  return node[index].as<double>();
}

std::vector<EllipseLeaf> leaves(const YAML::Node &crop) {
  const auto nodes = crop["leaves"];
  if (!nodes) {
    return {{0.0, 0.0, crop["radius"].as<double>(), crop["radius"].as<double>(),
             0.0}};
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
    const auto radius_x = radii              ? coordinate(radii, 0, "radii")
                          : leaf["radius_x"] ? leaf["radius_x"].as<double>()
                                             : leaf["rx"].as<double>();
    const auto radius_y = radii              ? coordinate(radii, 1, "radii")
                          : leaf["radius_y"] ? leaf["radius_y"].as<double>()
                                             : leaf["ry"].as<double>();
    const auto rotation = leaf["rotation"] ? leaf["rotation"].as<double>()
                          : leaf["theta"]  ? leaf["theta"].as<double>()
                                           : 0.0;
    result.push_back({coordinate(position, 0, "leaf position"),
                      coordinate(position, 1, "leaf position"), radius_x,
                      radius_y, rotation});
  }
  return result;
}

} // namespace

World world_from_yaml(const std::string_view yaml) {
  return world_from_yaml(yaml, generators::make_builtin_generator_registry());
}

World world_from_yaml(const std::string_view yaml,
                      const generators::GeneratorRegistry &registry) {
  const auto root = YAML::Load(std::string(yaml));
  if (!root || !root.IsMap()) {
    throw std::invalid_argument("world description must be a YAML map");
  }

  const auto seed = root["seed"] ? root["seed"].as<std::uint64_t>() : 0U;
  std::vector<Crop> crops;
  generators::GenerationContext context(seed);

  const auto explicit_crops = root["crops"];
  if (explicit_crops) {
    if (!explicit_crops.IsSequence()) {
      throw std::invalid_argument("crops must be a sequence");
    }
    crops.reserve(explicit_crops.size());
    for (const auto &node : explicit_crops) {
      crops.push_back(context.make_crop(
          coordinate(node["position"], 0, "position"),
          coordinate(node["position"], 1, "position"), leaves(node)));
    }
  }

  const auto generators = root["generators"];
  if (generators) {
    if (!generators.IsSequence()) {
      throw std::invalid_argument("generators must be a sequence");
    }
    for (const auto &generator : generators) {
      const auto gentype = generator["gentype"];
      if (!gentype) {
        throw std::invalid_argument("generator is missing gentype");
      }
      registry.placement()
          .create(gentype.as<std::string>())
          ->generate(generator, context, registry.crops(), crops);
    }
  }

  return World(seed, std::move(crops));
}

} // namespace cropsim
