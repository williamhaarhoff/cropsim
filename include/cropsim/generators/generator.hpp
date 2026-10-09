#pragma once

#include "cropsim/generators/generation_context.hpp"

#include <vector>

namespace YAML {
class Node;
}

namespace cropsim::generators {

class Generator {
public:
    virtual ~Generator() = default;
    virtual void generate(const YAML::Node& node, GenerationContext& context,
                          std::vector<Crop>& destination) const = 0;
};

}  // namespace cropsim::generators
