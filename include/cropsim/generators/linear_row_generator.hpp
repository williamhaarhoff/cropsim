#pragma once

#include "cropsim/generators/generator.hpp"

namespace cropsim::generators {

class LinearRowGenerator final : public RowGenerator {
public:
  [[nodiscard]] std::vector<Point2>
  generate(const YAML::Node &node, const RowSegment &row,
           GenerationKey key) const override;
};

} // namespace cropsim::generators
