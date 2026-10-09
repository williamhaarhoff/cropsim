#include "cropsim/world.hpp"

#include "cropsim/generators/generation_context.hpp"
#include "cropsim/generators/generator_factory.hpp"

#include <yaml-cpp/yaml.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace cropsim {
namespace {

double coordinate(const YAML::Node& node, const std::size_t index, const char* field) {
    if (!node || !node.IsSequence() || node.size() != 2) {
        throw std::invalid_argument(std::string(field) + " must contain exactly two numbers");
    }
    return node[index].as<double>();
}

}  // namespace

World world_from_yaml(const std::string_view yaml) {
    return world_from_yaml(yaml, generators::make_builtin_generator_factory());
}

World world_from_yaml(const std::string_view yaml,
                      const generators::GeneratorFactory& factory) {
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
        for (const auto& node : explicit_crops) {
            crops.push_back(context.make_crop(coordinate(node["position"], 0, "position"),
                                              coordinate(node["position"], 1, "position"),
                                              node["radius"].as<double>()));
        }
    }

    const auto generators = root["generators"];
    if (generators) {
        if (!generators.IsSequence()) {
            throw std::invalid_argument("generators must be a sequence");
        }
        for (const auto& generator : generators) {
            const auto gentype = generator["gentype"];
            if (!gentype) {
                throw std::invalid_argument("generator is missing gentype");
            }
            factory.create(gentype.as<std::string>())->generate(generator, context, crops);
        }
    }

    return World(seed, std::move(crops));
}

}  // namespace cropsim
