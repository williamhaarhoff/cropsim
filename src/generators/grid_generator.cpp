#include "cropsim/generators/grid_generator.hpp"

#include <yaml-cpp/yaml.h>

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace cropsim::generators {
namespace {

double coordinate(const YAML::Node& node, const std::size_t index, const char* field) {
    if (!node || !node.IsSequence() || node.size() != 2) {
        throw std::invalid_argument(std::string(field) + " must contain exactly two numbers");
    }
    return node[index].as<double>();
}

std::vector<EllipseLeaf> leaves(const YAML::Node& node) {
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
    for (const auto& leaf : nodes) {
        if (leaf["type"] && leaf["type"].as<std::string>() != "ellipse") {
            throw std::invalid_argument("leaf type must be ellipse");
        }
        const auto position = leaf["position"] ? leaf["position"] : leaf["pos"];
        const auto radii = leaf["radii"];
        result.push_back({coordinate(position, 0, "leaf position"),
                          coordinate(position, 1, "leaf position"),
                          radii              ? coordinate(radii, 0, "radii")
                          : leaf["radius_x"] ? leaf["radius_x"].as<double>()
                                             : leaf["rx"].as<double>(),
                          radii              ? coordinate(radii, 1, "radii")
                          : leaf["radius_y"] ? leaf["radius_y"].as<double>()
                                             : leaf["ry"].as<double>(),
                          leaf["rotation"] ? leaf["rotation"].as<double>()
                          : leaf["theta"]  ? leaf["theta"].as<double>()
                                           : 0.0});
    }
    return result;
}

} // namespace

void GridGenerator::generate(const YAML::Node& node, GenerationContext& context,
                             std::vector<Crop>& destination) const {
    const auto rows = node["rows"].as<std::size_t>();
    const auto columns = node["columns"].as<std::size_t>();
    const auto origin_x = coordinate(node["origin"], 0, "origin");
    const auto origin_y = coordinate(node["origin"], 1, "origin");
    const auto spacing_x = coordinate(node["spacing"], 0, "spacing");
    const auto spacing_y = coordinate(node["spacing"], 1, "spacing");
    const auto crop_leaves = leaves(node);
    const auto jitter = node["jitter"] ? node["jitter"].as<double>() : 0.0;
    if (rows == 0 || columns == 0 || !std::isfinite(origin_x) || !std::isfinite(origin_y) ||
        !std::isfinite(spacing_x) || !std::isfinite(spacing_y) || !std::isfinite(jitter) ||
        spacing_x <= 0.0 || spacing_y <= 0.0 || jitter < 0.0) {
        throw std::invalid_argument("invalid grid generator dimensions");
    }
    if (columns > std::numeric_limits<std::size_t>::max() / rows) {
        throw std::overflow_error("grid crop count overflow");
    }
    const auto count = rows * columns;
    if (count > destination.max_size() - destination.size()) {
        throw std::overflow_error("generated crop destination size overflow");
    }
    destination.reserve(destination.size() + count);
    for (std::size_t row = 0; row < rows; ++row) {
        for (std::size_t column = 0; column < columns; ++column) {
            const auto x = origin_x + static_cast<double>(column) * spacing_x +
                           context.symmetric_unit() * jitter;
            const auto y =
                origin_y + static_cast<double>(row) * spacing_y + context.symmetric_unit() * jitter;
            destination.push_back(context.make_crop(x, y, crop_leaves));
        }
    }
}

} // namespace cropsim::generators
