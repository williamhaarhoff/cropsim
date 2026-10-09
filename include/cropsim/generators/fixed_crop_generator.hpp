#pragma once

#include "cropsim/generators/generator.hpp"

namespace cropsim::generators {

class FixedCropGenerator final : public CropGenerator {
public:
  void configure(const YAML::Node &node, const ModifierFieldSet *) override;
  [[nodiscard]] std::vector<EllipseLeaf>
  generate(const YAML::Node &node, MorphologyContext &context) const override;
  [[nodiscard]] std::vector<EllipseLeaf> generate(Context &context) const override;
private:
  std::vector<EllipseLeaf> configured_leaves_;
};

} // namespace cropsim::generators
