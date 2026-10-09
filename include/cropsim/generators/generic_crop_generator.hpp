#pragma once

#include "cropsim/generators/generator.hpp"
#include <memory>

namespace cropsim::generators {

class GenericCropGenerator final : public CropGenerator {
public:
  void configure(const YAML::Node &node, const ModifierFieldSet *fields) override;
  [[nodiscard]] std::vector<EllipseLeaf>
  generate(const YAML::Node &node, MorphologyContext &context) const override;
  [[nodiscard]] std::vector<EllipseLeaf> generate(Context &context) const override;
private:
  struct Configuration;
  std::shared_ptr<const Configuration> configuration_;
};

} // namespace cropsim::generators
