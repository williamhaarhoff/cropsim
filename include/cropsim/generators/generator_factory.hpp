#pragma once

#include "cropsim/generators/generator.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace cropsim::generators {

class PlacementGeneratorFactory final {
public:
  using Creator = std::function<std::unique_ptr<PlacementGenerator>()>;

  void register_generator(std::string name, Creator creator);
  [[nodiscard]] std::unique_ptr<PlacementGenerator>
  create(std::string_view name) const;

private:
  std::unordered_map<std::string, Creator> creators_;
};

class CropGeneratorFactory final {
public:
  using Creator = std::function<std::unique_ptr<CropGenerator>()>;
  void register_generator(std::string name, Creator creator);
  [[nodiscard]] std::unique_ptr<CropGenerator>
  create(std::string_view name) const;

private:
  std::unordered_map<std::string, Creator> creators_;
};

class GeneratorRegistry final {
public:
  [[nodiscard]] PlacementGeneratorFactory &placement() noexcept {
    return placement_;
  }
  [[nodiscard]] const PlacementGeneratorFactory &placement() const noexcept {
    return placement_;
  }
  [[nodiscard]] CropGeneratorFactory &crops() noexcept { return crops_; }
  [[nodiscard]] const CropGeneratorFactory &crops() const noexcept {
    return crops_;
  }

private:
  PlacementGeneratorFactory placement_;
  CropGeneratorFactory crops_;
};

[[nodiscard]] GeneratorRegistry make_builtin_generator_registry();

} // namespace cropsim::generators
