#pragma once

#include "cropsim/generators/generation_context.hpp"

#include <vector>

namespace YAML {
class Node;
}

namespace cropsim::generators {

class CropGeneratorFactory;

class PlacementGenerator {
public:
  virtual ~PlacementGenerator() = default;
  virtual void generate(const YAML::Node &node, GenerationContext &context,
                        const CropGeneratorFactory &crop_generators,
                        std::vector<Crop> &destination) const = 0;
};

class CropGenerator {
public:
  virtual ~CropGenerator() = default;
  [[nodiscard]] virtual std::vector<EllipseLeaf>
  generate(const YAML::Node &node, MorphologyContext &context) const = 0;
};

} // namespace cropsim::generators
