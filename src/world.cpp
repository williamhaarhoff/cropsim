#include "cropsim/world.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace cropsim {

bool EllipseLeaf::operator==(const EllipseLeaf& other) const noexcept {
    return x == other.x && y == other.y && radius_x == other.radius_x &&
           radius_y == other.radius_y && rotation == other.rotation;
}

Crop::Crop(const std::uint64_t crop_id, const double crop_x, const double crop_y,
           const double radius)
    : id(crop_id), x(crop_x), y(crop_y), leaves{{0.0, 0.0, radius, radius, 0.0}} {}

Crop::Crop(const std::uint64_t crop_id, const double crop_x, const double crop_y,
           std::vector<EllipseLeaf> crop_leaves)
    : id(crop_id), x(crop_x), y(crop_y), leaves(std::move(crop_leaves)) {}

bool Crop::operator==(const Crop& other) const noexcept {
    return id == other.id && x == other.x && y == other.y && leaves == other.leaves;
}

bool Aabb::operator==(const Aabb& other) const noexcept {
    return min_x == other.min_x && min_y == other.min_y && max_x == other.max_x &&
           max_y == other.max_y;
}

World::World(const std::uint64_t seed, std::vector<Crop> crops)
    : seed_(seed), crops_(std::move(crops)), spatial_index_(crops_) {
    for (const auto& crop : crops_) {
        static_cast<void>(crop_bounds(crop));
    }
}

std::vector<std::size_t> World::query(const Aabb& bounds) const {
    return spatial_index_.query(crops_, bounds);
}

bool World::operator==(const World& other) const noexcept {
    return seed_ == other.seed_ && crops_ == other.crops_ && spatial_index_ == other.spatial_index_;
}

} // namespace cropsim
