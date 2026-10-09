#pragma once

#include <cstdint>
#include <vector>

#include "cropsim/world.hpp"

namespace cropsim::generators {

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
  [[nodiscard]] Crop make_crop(double x, double y, double radius);
  [[nodiscard]] Crop make_crop(double x, double y,
                               std::vector<EllipseLeaf> leaves);

private:
  std::uint64_t random_state_{};
  std::uint64_t world_seed_{};
  std::uint64_t next_id_{};
};

} // namespace cropsim::generators
