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

class FieldGeneratorFactory final {
public:
  using Creator = std::function<std::unique_ptr<FieldGenerator>()>;
  void register_generator(std::string name, Creator creator);
  [[nodiscard]] std::unique_ptr<FieldGenerator>
  create(std::string_view name) const;

private:
  std::unordered_map<std::string, Creator> creators_;
};

class RowGeneratorFactory final {
public:
  using Creator = std::function<std::unique_ptr<RowGenerator>()>;
  void register_generator(std::string name, Creator creator);
  [[nodiscard]] std::unique_ptr<RowGenerator>
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
  [[nodiscard]] FieldGeneratorFactory &fields() noexcept { return fields_; }
  [[nodiscard]] const FieldGeneratorFactory &fields() const noexcept {
    return fields_;
  }
  [[nodiscard]] RowGeneratorFactory &rows() noexcept { return rows_; }
  [[nodiscard]] const RowGeneratorFactory &rows() const noexcept {
    return rows_;
  }

private:
  PlacementGeneratorFactory placement_;
  FieldGeneratorFactory fields_;
  RowGeneratorFactory rows_;
  CropGeneratorFactory crops_;
};

[[nodiscard]] GeneratorRegistry make_builtin_generator_registry();

} // namespace cropsim::generators
