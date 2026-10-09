#pragma once

#include "cropsim/generators/generator.hpp"
#include "cropsim/renderer.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace YAML { class Node; }

namespace cropsim::generators {

struct ModifierDomain final {
  Polygon2 polygon;
  double min_x{};
  double min_y{};
  double max_x{};
  double max_y{};
};

class ModifierField {
public:
  virtual ~ModifierField() = default;
  [[nodiscard]] virtual double evaluate(double x, double y, double u, double v,
                                        const std::vector<double> &values) const = 0;
  [[nodiscard]] virtual std::vector<std::string> dependencies() const { return {}; }
  virtual void bind(const std::unordered_map<std::string, std::size_t> &) {}
};

class ModifierFieldFactory final {
public:
  using Creator = std::function<std::unique_ptr<ModifierField>(
      const YAML::Node &, const ModifierDomain &, std::uint64_t)>;
  void register_generator(std::string name, Creator creator);
  [[nodiscard]] std::unique_ptr<ModifierField>
  create(std::string_view name, const YAML::Node &node,
         const ModifierDomain &domain, std::uint64_t seed) const;
private:
  std::unordered_map<std::string, Creator> creators_;
};

class ModifierFieldSet final {
public:
  ModifierFieldSet() = default;
  [[nodiscard]] static ModifierFieldSet compile(
      const YAML::Node &node, const Polygon2 &domain, std::uint64_t world_seed,
      GenerationKey generator_key, const ModifierFieldFactory &factory);
  [[nodiscard]] std::size_t resolve(std::string_view name) const;
  void evaluate(Point2 position, std::vector<double> &buffer) const;
  [[nodiscard]] std::size_t size() const noexcept { return fields_.size(); }
  [[nodiscard]] const ModifierDomain &domain() const noexcept { return domain_; }
private:
  ModifierDomain domain_;
  std::vector<std::string> names_;
  std::vector<std::unique_ptr<ModifierField>> fields_;
  std::vector<std::size_t> evaluation_order_;
};

[[nodiscard]] ModifierFieldFactory make_builtin_modifier_field_factory();
[[nodiscard]] GrayscaleImage render_modifier_field(
    const ModifierFieldSet &fields, std::size_t field_index,
    std::size_t width, std::size_t height,
    std::optional<std::pair<double, double>> range = std::nullopt);

} // namespace cropsim::generators
