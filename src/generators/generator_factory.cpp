#include "cropsim/generators/generator_factory.hpp"

#include "cropsim/generators/fixed_crop_generator.hpp"
#include "cropsim/generators/generic_crop_generator.hpp"
#include "cropsim/generators/grid_generator.hpp"

#include <stdexcept>
#include <utility>

namespace cropsim::generators {

void PlacementGeneratorFactory::register_generator(std::string name,
                                                   Creator creator) {
  if (name.empty() || !creator) {
    throw std::invalid_argument(
        "generator registration requires a name and creator");
  }
  const auto [unused, inserted] =
      creators_.emplace(std::move(name), std::move(creator));
  static_cast<void>(unused);
  if (!inserted) {
    throw std::invalid_argument("generator name is already registered");
  }
}

std::unique_ptr<PlacementGenerator>
PlacementGeneratorFactory::create(const std::string_view name) const {
  const auto found = creators_.find(std::string(name));
  if (found == creators_.end()) {
    throw std::invalid_argument("unknown generator type: " + std::string(name));
  }
  auto generator = found->second();
  if (!generator) {
    throw std::runtime_error("generator creator returned null");
  }
  return generator;
}

void CropGeneratorFactory::register_generator(std::string name,
                                              Creator creator) {
  if (name.empty() || !creator) {
    throw std::invalid_argument(
        "crop generator registration requires a name and creator");
  }
  const auto [unused, inserted] =
      creators_.emplace(std::move(name), std::move(creator));
  static_cast<void>(unused);
  if (!inserted) {
    throw std::invalid_argument("crop generator name is already registered");
  }
}

std::unique_ptr<CropGenerator>
CropGeneratorFactory::create(const std::string_view name) const {
  const auto found = creators_.find(std::string(name));
  if (found == creators_.end()) {
    throw std::invalid_argument("unknown crop generator type: " +
                                std::string(name));
  }
  auto generator = found->second();
  if (!generator) {
    throw std::runtime_error("crop generator creator returned null");
  }
  return generator;
}

GeneratorRegistry make_builtin_generator_registry() {
  GeneratorRegistry registry;
  registry.placement().register_generator(
      "grid", [] { return std::make_unique<GridGenerator>(); });
  registry.crops().register_generator(
      "fixed", [] { return std::make_unique<FixedCropGenerator>(); });
  registry.crops().register_generator(
      "generic", [] { return std::make_unique<GenericCropGenerator>(); });
  return registry;
}

} // namespace cropsim::generators
