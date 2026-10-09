#pragma once

#include <cstdint>
#include <initializer_list>
#include <vector>

#include "cropsim/world.hpp"

namespace cropsim::generators {

struct GenerationKey final {
  std::uint64_t value{};

  [[nodiscard]] GenerationKey child(std::uint64_t domain,
                                    std::uint64_t index) const noexcept;
};

class RandomStream final {
public:
  explicit RandomStream(std::uint64_t seed) noexcept;
  [[nodiscard]] double uniform_open() noexcept;
  [[nodiscard]] double symmetric_unit() noexcept;

private:
  std::uint64_t random_state_{};
};

class MorphologyContext final {
public:
  explicit MorphologyContext(std::uint64_t seed) noexcept;
  [[nodiscard]] double uniform_open() noexcept;

private:
  std::uint64_t random_state_{};
};

class GenerationContext final {
public:
  explicit GenerationContext(std::uint64_t seed) noexcept;

  [[nodiscard]] double symmetric_unit() noexcept;
  [[nodiscard]] std::uint64_t next_id() const noexcept { return next_id_; }
  [[nodiscard]] MorphologyContext
  morphology(std::uint64_t crop_id) const noexcept;
  [[nodiscard]] MorphologyContext
  morphology(GenerationKey key) const noexcept;
  [[nodiscard]] GenerationKey
  key(std::uint64_t domain,
      std::initializer_list<std::uint64_t> indices = {}) const noexcept;
  [[nodiscard]] RandomStream random(GenerationKey key) const noexcept;
  [[nodiscard]] Crop make_crop(double x, double y, double radius);
  [[nodiscard]] Crop make_crop(double x, double y,
                               std::vector<EllipseLeaf> leaves);

private:
  std::uint64_t random_state_{};
  std::uint64_t world_seed_{};
  std::uint64_t next_id_{};
};

} // namespace cropsim::generators
