#pragma once

#include "cropsim/generators/generator.hpp"

namespace cropsim::generators {

class GridGenerator final : public Generator {
public:
    void generate(const YAML::Node& node, GenerationContext& context,
                  std::vector<Crop>& destination) const override;
};

}  // namespace cropsim::generators
