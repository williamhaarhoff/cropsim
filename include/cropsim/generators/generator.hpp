#pragma once

#include "cropsim/generators/generation_context.hpp"

#include <cstdint>
#include <vector>

namespace YAML {
class Node;
}

namespace cropsim::generators {

class CropGeneratorFactory;
class GeneratorRegistry;
class ModifierFieldSet;

struct Point2 final {
  double x{};
  double y{};
};

struct Polygon2 final {
  std::vector<Point2> vertices;
};

struct FieldRegion final {
  std::uint64_t seed_index{};
  Point2 seed;
  Polygon2 boundary;
};

struct RowSegment final {
  std::uint64_t field_index{};
  std::uint64_t row_index{};
  Point2 begin;
  Point2 end;
  Polygon2 plantable_boundary;
  double row_spacing{};
};

class PlacementGenerator {
public:
  virtual ~PlacementGenerator() = default;
  virtual void generate(const YAML::Node &node, GenerationContext &context,
                        GenerationKey key,
                        const GeneratorRegistry &registry,
                        std::vector<Crop> &destination) const = 0;
};

class FieldGenerator {
public:
  virtual ~FieldGenerator() = default;
  [[nodiscard]] virtual std::vector<RowSegment>
  generate(const YAML::Node &node, const FieldRegion &field,
           GenerationKey key) const = 0;
};

class RowGenerator {
public:
  virtual ~RowGenerator() = default;
  [[nodiscard]] virtual std::vector<Point2>
  generate(const YAML::Node &node, const RowSegment &row,
           GenerationKey key) const = 0;
};

class CropGenerator {
public:
  virtual ~CropGenerator() = default;
  virtual void configure(const YAML::Node &, const ModifierFieldSet *) {}
  [[nodiscard]] virtual std::vector<EllipseLeaf>
  generate(const YAML::Node &node, MorphologyContext &context) const = 0;
  struct Context final {
    Point2 position;
    MorphologyContext &random;
    const std::vector<double> &modifiers;
  };
  [[nodiscard]] virtual std::vector<EllipseLeaf>
  generate(Context &context) const;
};

} // namespace cropsim::generators
