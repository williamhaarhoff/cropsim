#pragma once

#include "cropsim/generators/generator.hpp"

namespace cropsim::generators {

class GridGenerator final : public PlacementGenerator {
public:
  void generate(const YAML::Node &node, GenerationContext &context,
                GenerationKey key,
                const GeneratorRegistry &registry,
                std::vector<Crop> &destination) const override;
};

} // namespace cropsim::generators
