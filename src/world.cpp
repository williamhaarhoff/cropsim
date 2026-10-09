#include "cropsim/world.hpp"

#include <stdexcept>
#include <cmath>
#include <utility>

namespace cropsim {

bool Crop::operator==(const Crop& other) const noexcept {
    return id == other.id && x == other.x && y == other.y && radius == other.radius;
}

bool Aabb::operator==(const Aabb& other) const noexcept {
    return min_x == other.min_x && min_y == other.min_y && max_x == other.max_x &&
           max_y == other.max_y;
}

World::World(const std::uint64_t seed, std::vector<Crop> crops)
    : seed_(seed), crops_(std::move(crops)), spatial_index_(crops_) {
    for (const auto& crop : crops_) {
        if (!std::isfinite(crop.x) || !std::isfinite(crop.y) ||
            !std::isfinite(crop.radius) || crop.radius <= 0.0) {
            throw std::invalid_argument("crop geometry must be finite and radius positive");
        }
    }
}

std::vector<std::size_t> World::query(const Aabb& bounds) const {
    return spatial_index_.query(crops_, bounds);
}

bool World::operator==(const World& other) const noexcept {
    return seed_ == other.seed_ && crops_ == other.crops_ &&
           spatial_index_ == other.spatial_index_;
}

}  // namespace cropsim
