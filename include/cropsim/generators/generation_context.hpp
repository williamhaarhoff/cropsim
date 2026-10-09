#pragma once

#include <cstdint>
#include <vector>

#include "cropsim/world.hpp"

namespace cropsim::generators {

class GenerationContext final {
  public:
    explicit GenerationContext(std::uint64_t seed) noexcept;

    [[nodiscard]] double symmetric_unit() noexcept;
    [[nodiscard]] Crop make_crop(double x, double y, double radius);
    [[nodiscard]] Crop make_crop(double x, double y, std::vector<EllipseLeaf> leaves);

  private:
    std::uint64_t random_state_{};
    std::uint64_t next_id_{};
};

} // namespace cropsim::generators
