#pragma once

#include "cropsim/generators/generator.hpp"

namespace cropsim::generators {

class FixedCropGenerator final : public CropGenerator {
public:
  [[nodiscard]] std::vector<EllipseLeaf>
  generate(const YAML::Node &node, MorphologyContext &context) const override;
};

} // namespace cropsim::generators
