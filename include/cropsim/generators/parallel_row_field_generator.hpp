#pragma once

#include "cropsim/generators/generator.hpp"

namespace cropsim::generators {

class ParallelRowFieldGenerator final : public FieldGenerator {
public:
  [[nodiscard]] std::vector<RowSegment>
  generate(const YAML::Node &node, const FieldRegion &field,
           GenerationKey key) const override;
};

} // namespace cropsim::generators
