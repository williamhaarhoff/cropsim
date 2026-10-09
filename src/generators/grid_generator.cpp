#include "cropsim/generators/grid_generator.hpp"
#include "cropsim/generators/generator_factory.hpp"
#include <cmath>
#include <limits>
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

void GridGenerator::generate(const YAML::Node &node, GenerationContext &context,
                             const CropGeneratorFactory &crop_generators,
                             std::vector<Crop> &destination) const {
  const auto rows = node["rows"].as<std::size_t>();
  const auto columns = node["columns"].as<std::size_t>();
  const auto origin_x = coordinate(node["origin"], 0U, "origin");
  const auto origin_y = coordinate(node["origin"], 1U, "origin");
  const auto spacing_x = coordinate(node["spacing"], 0U, "spacing");
  const auto spacing_y = coordinate(node["spacing"], 1U, "spacing");
  const auto jitter = node["jitter"] ? node["jitter"].as<double>() : 0.0;
  if (rows == 0U || columns == 0U || !std::isfinite(origin_x) ||
      !std::isfinite(origin_y) || !std::isfinite(spacing_x) ||
      !std::isfinite(spacing_y) || !std::isfinite(jitter) || spacing_x <= 0.0 ||
      spacing_y <= 0.0 || jitter < 0.0) {
    throw std::invalid_argument("invalid grid generator dimensions");
  }
  if (columns > std::numeric_limits<std::size_t>::max() / rows) {
    throw std::overflow_error("grid crop count overflow");
  }
  const auto count = rows * columns;
  if (count > destination.max_size() - destination.size()) {
    throw std::overflow_error("generated crop destination size overflow");
  }
  const auto crop_node = node["crop"];
  if (crop_node && (node["radius"] || node["leaves"])) {
    throw std::invalid_argument("grid crop geometry is ambiguous");
  }
  if (crop_node && !crop_node.IsMap()) {
    throw std::invalid_argument("crop must be a map");
  }
  const auto type_node = crop_node ? crop_node["gentype"] : YAML::Node{};
  if (crop_node && !type_node) {
    throw std::invalid_argument("crop generator is missing gentype");
  }
  if (!crop_node && !node["radius"] && !node["leaves"]) {
    throw std::invalid_argument("grid is missing crop geometry");
  }
  const auto type =
      crop_node ? type_node.as<std::string>() : std::string("fixed");
  const auto &geometry = crop_node ? crop_node : node;
  const auto crop_generator = crop_generators.create(type);

  destination.reserve(destination.size() + count);
  for (std::size_t row = 0; row < rows; ++row) {
    for (std::size_t column = 0; column < columns; ++column) {
      const auto x = origin_x + static_cast<double>(column) * spacing_x +
                     context.symmetric_unit() * jitter;
      const auto y = origin_y + static_cast<double>(row) * spacing_y +
                     context.symmetric_unit() * jitter;
      auto morphology = context.morphology(context.next_id());
      destination.push_back(context.make_crop(
          x, y, crop_generator->generate(geometry, morphology)));
    }
  }
}
} // namespace cropsim::generators
