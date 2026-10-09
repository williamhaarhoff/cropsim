#pragma once

#include "cropsim/generators/generator.hpp"

#include <memory>

namespace cropsim::generators {

class PlacementGeneratorFactory;
class FieldGeneratorFactory;
class RowGeneratorFactory;
class CropGeneratorFactory;

class GridGenerator final : public PlacementGenerator {
public:
  void generate(const YAML::Node &, GenerationContext &, GenerationKey,
                const GeneratorRegistry &, std::vector<Crop> &) const override;
};

class FieldSetGenerator : public PlacementGenerator {
public:
  void generate(const YAML::Node &, GenerationContext &, GenerationKey,
                const GeneratorRegistry &, std::vector<Crop> &) const final;

protected:
  [[nodiscard]] virtual std::vector<FieldRegion>
  generate_fields(const YAML::Node &, GenerationKey) const = 0;
};

class VoronoiFieldSetGenerator final : public FieldSetGenerator {
protected:
  [[nodiscard]] std::vector<FieldRegion>
  generate_fields(const YAML::Node &, GenerationKey) const override;
};

class ParallelRowFieldGenerator final : public FieldGenerator {
public:
  [[nodiscard]] std::vector<RowSegment> generate(const YAML::Node &,
                                                 const FieldRegion &,
                                                 GenerationKey) const override;
};

class LinearRowGenerator final : public RowGenerator {
public:
  [[nodiscard]] std::vector<Point2> generate(const YAML::Node &,
                                             const RowSegment &,
                                             GenerationKey) const override;
};

class FixedCropGenerator final : public CropGenerator {
public:
  void configure(const YAML::Node &, const ModifierFieldSet *) override;
  [[nodiscard]] std::vector<EllipseLeaf>
  generate(const YAML::Node &, MorphologyContext &) const override;
  [[nodiscard]] std::vector<EllipseLeaf> generate(Context &) const override;

private:
  std::vector<EllipseLeaf> configured_leaves_;
};

class GenericCropGenerator final : public CropGenerator {
public:
  void configure(const YAML::Node &, const ModifierFieldSet *) override;
  [[nodiscard]] std::vector<EllipseLeaf>
  generate(const YAML::Node &, MorphologyContext &) const override;
  [[nodiscard]] std::vector<EllipseLeaf> generate(Context &) const override;

private:
  struct Configuration;
  std::shared_ptr<const Configuration> configuration_;
};

void register_builtin_placement_generators(PlacementGeneratorFactory &);
void register_builtin_field_set_generators(PlacementGeneratorFactory &);
void register_builtin_field_generators(FieldGeneratorFactory &);
void register_builtin_row_generators(RowGeneratorFactory &);
void register_builtin_crop_generators(CropGeneratorFactory &);

} // namespace cropsim::generators
