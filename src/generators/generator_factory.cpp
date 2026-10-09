#include "cropsim/generators/generator_factory.hpp"

#include "cropsim/generators/fixed_crop_generator.hpp"
#include "cropsim/generators/field_set_generator.hpp"
#include "cropsim/generators/generic_crop_generator.hpp"
#include "cropsim/generators/grid_generator.hpp"
#include "cropsim/generators/linear_row_generator.hpp"
#include "cropsim/generators/parallel_row_field_generator.hpp"

#include <stdexcept>
#include <utility>

namespace cropsim::generators {

std::vector<EllipseLeaf> CropGenerator::generate(Context &context) const {
  return generate(YAML::Node{}, context.random);
}

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
  if (!creator) {
    throw std::invalid_argument("crop generator registration requires a name and creator");
  }
  register_generator(std::move(name),
                     [creator = std::move(creator)](const YAML::Node &node,
                                                    const ModifierFieldSet *fields) {
                       auto result = creator();
                       if (result && node.IsDefined() && !node.IsNull()) {
                         result->configure(node, fields);
                       }
                       return result;
                     });
}

void CropGeneratorFactory::register_generator(std::string name,
                                              ConfiguredCreator creator) {
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
CropGeneratorFactory::create(const std::string_view name, const YAML::Node &node,
                             const ModifierFieldSet *modifiers) const {
  const auto found = creators_.find(std::string(name));
  if (found == creators_.end()) {
    throw std::invalid_argument("unknown crop generator type: " +
                                std::string(name));
  }
  auto generator = found->second(node, modifiers);
  if (!generator) {
    throw std::runtime_error("crop generator creator returned null");
  }
  return generator;
}

void FieldGeneratorFactory::register_generator(std::string name,
                                               Creator creator) {
  if (name.empty() || !creator) {
    throw std::invalid_argument(
        "field generator registration requires a name and creator");
  }
  const auto [unused, inserted] =
      creators_.emplace(std::move(name), std::move(creator));
  static_cast<void>(unused);
  if (!inserted) {
    throw std::invalid_argument("field generator name is already registered");
  }
}

std::unique_ptr<FieldGenerator>
FieldGeneratorFactory::create(const std::string_view name) const {
  const auto found = creators_.find(std::string(name));
  if (found == creators_.end()) {
    throw std::invalid_argument("unknown field generator type: " +
                                std::string(name));
  }
  auto generator = found->second();
  if (!generator) {
    throw std::runtime_error("field generator creator returned null");
  }
  return generator;
}

void RowGeneratorFactory::register_generator(std::string name,
                                             Creator creator) {
  if (name.empty() || !creator) {
    throw std::invalid_argument(
        "row generator registration requires a name and creator");
  }
  const auto [unused, inserted] =
      creators_.emplace(std::move(name), std::move(creator));
  static_cast<void>(unused);
  if (!inserted) {
    throw std::invalid_argument("row generator name is already registered");
  }
}

std::unique_ptr<RowGenerator>
RowGeneratorFactory::create(const std::string_view name) const {
  const auto found = creators_.find(std::string(name));
  if (found == creators_.end()) {
    throw std::invalid_argument("unknown row generator type: " +
                                std::string(name));
  }
  auto generator = found->second();
  if (!generator) {
    throw std::runtime_error("row generator creator returned null");
  }
  return generator;
}

GeneratorRegistry make_builtin_generator_registry() {
  GeneratorRegistry registry;
  registry.modifier_fields() = make_builtin_modifier_field_factory();
  registry.placement().register_generator(
      "grid", [] { return std::make_unique<GridGenerator>(); });
  registry.placement().register_generator(
      "field_set", [] { return std::make_unique<VoronoiFieldSetGenerator>(); });
  registry.fields().register_generator(
      "parallel_rows",
      [] { return std::make_unique<ParallelRowFieldGenerator>(); });
  registry.rows().register_generator(
      "linear", [] { return std::make_unique<LinearRowGenerator>(); });
  registry.crops().register_generator(
      "fixed", [] { return std::make_unique<FixedCropGenerator>(); });
  registry.crops().register_generator(
      "generic", [] { return std::make_unique<GenericCropGenerator>(); });
  return registry;
}

} // namespace cropsim::generators
