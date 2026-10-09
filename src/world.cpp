#include "cropsim/world.hpp"

#include <stdexcept>
#include <utility>

namespace cropsim {

bool Crop::operator==(const Crop& other) const noexcept {
    return id == other.id && x == other.x && y == other.y && radius == other.radius;
}

World::World(const std::uint64_t seed, std::vector<Crop> crops)
    : seed_(seed), crops_(std::move(crops)) {
    for (const auto& crop : crops_) {
        if (crop.radius <= 0.0) {
            throw std::invalid_argument("crop radius must be positive");
        }
    }
}

bool World::operator==(const World& other) const noexcept {
    return seed_ == other.seed_ && crops_ == other.crops_;
}

}  // namespace cropsim
