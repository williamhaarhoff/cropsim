#include "cropsim/world.hpp"

#include <yaml-cpp/yaml.h>

#include <cmath>
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

std::uint64_t splitmix64(std::uint64_t& state) noexcept {
    state += 0x9e3779b97f4a7c15ULL;
    auto value = state;
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

double symmetric_unit(std::uint64_t& state) noexcept {
    constexpr double denominator = 9007199254740992.0;
    const auto value = splitmix64(state) >> 11U;
    return (static_cast<double>(value) / denominator) * 2.0 - 1.0;
}

}  // namespace

World world_from_yaml(const std::string_view yaml) {
    const auto root = YAML::Load(std::string(yaml));
    if (!root || !root.IsMap()) {
        throw std::invalid_argument("world description must be a YAML map");
    }

    const auto seed = root["seed"] ? root["seed"].as<std::uint64_t>() : 0U;
    std::vector<Crop> crops;
    std::uint64_t next_id = 0;

    const auto explicit_crops = root["crops"];
    if (explicit_crops) {
        if (!explicit_crops.IsSequence()) {
            throw std::invalid_argument("crops must be a sequence");
        }
        crops.reserve(explicit_crops.size());
        for (const auto& node : explicit_crops) {
            crops.push_back(Crop{next_id++, coordinate(node["position"], 0, "position"),
                                 coordinate(node["position"], 1, "position"),
                                 node["radius"].as<double>()});
        }
    }

    const auto generators = root["generators"];
    if (generators) {
        if (!generators.IsSequence()) {
            throw std::invalid_argument("generators must be a sequence");
        }
        std::uint64_t random_state = seed;
        for (const auto& generator : generators) {
            if (generator["type"].as<std::string>() != "grid") {
                throw std::invalid_argument("unsupported generator type");
            }
            const auto rows = generator["rows"].as<std::size_t>();
            const auto columns = generator["columns"].as<std::size_t>();
            const auto origin_x = coordinate(generator["origin"], 0, "origin");
            const auto origin_y = coordinate(generator["origin"], 1, "origin");
            const auto spacing_x = coordinate(generator["spacing"], 0, "spacing");
            const auto spacing_y = coordinate(generator["spacing"], 1, "spacing");
            const auto radius = generator["radius"].as<double>();
            const auto jitter = generator["jitter"] ? generator["jitter"].as<double>() : 0.0;
            if (rows == 0 || columns == 0 || spacing_x <= 0.0 || spacing_y <= 0.0 ||
                radius <= 0.0 || jitter < 0.0) {
                throw std::invalid_argument("invalid grid generator dimensions");
            }
            crops.reserve(crops.size() + rows * columns);
            for (std::size_t row = 0; row < rows; ++row) {
                for (std::size_t column = 0; column < columns; ++column) {
                    const auto x = origin_x + static_cast<double>(column) * spacing_x +
                                   symmetric_unit(random_state) * jitter;
                    const auto y = origin_y + static_cast<double>(row) * spacing_y +
                                   symmetric_unit(random_state) * jitter;
                    crops.push_back(Crop{next_id++, x, y, radius});
                }
            }
        }
    }

    return World(seed, std::move(crops));
}

}  // namespace cropsim
