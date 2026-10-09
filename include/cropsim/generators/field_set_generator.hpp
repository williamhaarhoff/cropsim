#pragma once

#include "cropsim/generators/generator.hpp"

namespace cropsim::generators {

class FieldSetGenerator : public PlacementGenerator {
public:
  void generate(const YAML::Node &node, GenerationContext &context,
                GenerationKey key, const GeneratorRegistry &registry,
                std::vector<Crop> &destination) const final;

protected:
  [[nodiscard]] virtual std::vector<FieldRegion>
  generate_fields(const YAML::Node &node, GenerationKey key) const = 0;
};

class VoronoiFieldSetGenerator : public FieldSetGenerator {
protected:
  [[nodiscard]] std::vector<FieldRegion>
  generate_fields(const YAML::Node &node, GenerationKey key) const override;
};

} // namespace cropsim::generators
