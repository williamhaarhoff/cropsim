#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "cropsim/world.hpp"

namespace cropsim {
namespace {

constexpr std::size_t max_cells_per_crop = 4U;

[[nodiscard]] bool intersects(const Aabb& lhs, const Aabb& rhs) noexcept {
    return lhs.min_x <= rhs.max_x && lhs.max_x >= rhs.min_x && lhs.min_y <= rhs.max_y &&
           lhs.max_y >= rhs.min_y;
}

[[nodiscard]] std::size_t dimension(const double extent, const double cell_size) {
    const auto value = std::ceil(extent / cell_size);
    if (!std::isfinite(value) || value < 1.0 ||
        value > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        throw std::overflow_error("spatial index dimensions overflow");
    }
    return static_cast<std::size_t>(value);
}

struct CellRange {
    std::size_t first_column;
    std::size_t last_column;
    std::size_t first_row;
    std::size_t last_row;
};

[[nodiscard]] CellRange cells_for(const Aabb& item, const Aabb& world, const double cell_size,
                                  const std::size_t columns, const std::size_t rows) {
    const auto coordinate = [cell_size](const double value, const double origin,
                                        const std::size_t limit) {
        const auto raw = std::floor((value - origin) / cell_size);
        if (raw <= 0.0) {
            return std::size_t{0};
        }
        if (raw >= static_cast<double>(limit)) {
            return limit - 1U;
        }
        return static_cast<std::size_t>(raw);
    };
    return {coordinate(item.min_x, world.min_x, columns),
            coordinate(item.max_x, world.min_x, columns), coordinate(item.min_y, world.min_y, rows),
            coordinate(item.max_y, world.min_y, rows)};
}

} // namespace

Aabb crop_bounds(const Crop& crop) {
    if (!std::isfinite(crop.x) || !std::isfinite(crop.y) || crop.leaves.empty()) {
        throw std::invalid_argument("crop position must be finite and leaves non-empty");
    }
    Aabb result{};
    bool first = true;
    for (const auto& leaf : crop.leaves) {
        if (!std::isfinite(leaf.x) || !std::isfinite(leaf.y) || !std::isfinite(leaf.radius_x) ||
            !std::isfinite(leaf.radius_y) || !std::isfinite(leaf.rotation) ||
            leaf.radius_x <= 0.0 || leaf.radius_y <= 0.0) {
            throw std::invalid_argument("leaf geometry must be finite and radii positive");
        }
        const auto cosine = std::cos(leaf.rotation);
        const auto sine = std::sin(leaf.rotation);
        const auto half_width = std::hypot(leaf.radius_x * cosine, leaf.radius_y * sine);
        const auto half_height = std::hypot(leaf.radius_x * sine, leaf.radius_y * cosine);
        const auto center_x = crop.x + leaf.x;
        const auto center_y = crop.y + leaf.y;
        const Aabb leaf_bounds{center_x - half_width, center_y - half_height, center_x + half_width,
                               center_y + half_height};
        if (!std::isfinite(leaf_bounds.min_x) || !std::isfinite(leaf_bounds.min_y) ||
            !std::isfinite(leaf_bounds.max_x) || !std::isfinite(leaf_bounds.max_y)) {
            throw std::overflow_error("crop leaf bounds overflow");
        }
        if (first) {
            result = leaf_bounds;
            first = false;
        } else {
            result.min_x = std::min(result.min_x, leaf_bounds.min_x);
            result.min_y = std::min(result.min_y, leaf_bounds.min_y);
            result.max_x = std::max(result.max_x, leaf_bounds.max_x);
            result.max_y = std::max(result.max_y, leaf_bounds.max_y);
        }
    }
    return result;
}

bool leaf_contains(const Crop& crop, const EllipseLeaf& leaf, const double world_x,
                   const double world_y) noexcept {
    const auto dx = world_x - (crop.x + leaf.x);
    const auto dy = world_y - (crop.y + leaf.y);
    const auto cosine = std::cos(leaf.rotation);
    const auto sine = std::sin(leaf.rotation);
    const auto local_x = cosine * dx + sine * dy;
    const auto local_y = -sine * dx + cosine * dy;
    const auto normal_x = local_x / leaf.radius_x;
    const auto normal_y = local_y / leaf.radius_y;
    return normal_x * normal_x + normal_y * normal_y <= 1.0;
}

SpatialIndex::SpatialIndex(const std::vector<Crop>& crops) {
    if (crops.empty()) {
        return;
    }

    bounds_ = crop_bounds(crops.front());
    const auto first_bounds = crop_bounds(crops.front());
    long double diameter_sum =
        std::max(first_bounds.max_x - first_bounds.min_x, first_bounds.max_y - first_bounds.min_y);
    for (std::size_t index = 1; index < crops.size(); ++index) {
        const auto item = crop_bounds(crops[index]);
        bounds_.min_x = std::min(bounds_.min_x, item.min_x);
        bounds_.min_y = std::min(bounds_.min_y, item.min_y);
        bounds_.max_x = std::max(bounds_.max_x, item.max_x);
        bounds_.max_y = std::max(bounds_.max_y, item.max_y);
        diameter_sum += std::max(item.max_x - item.min_x, item.max_y - item.min_y);
    }
    const auto width = bounds_.max_x - bounds_.min_x;
    const auto height = bounds_.max_y - bounds_.min_y;
    if (!std::isfinite(width) || !std::isfinite(height)) {
        throw std::overflow_error("spatial index bounds overflow");
    }
    const auto mean_diameter = static_cast<double>(diameter_sum / crops.size());
    const auto area_cell = std::sqrt((width * height) / static_cast<double>(crops.size()));
    cell_size_ = std::max(mean_diameter, area_cell);
    if (!std::isfinite(cell_size_) || cell_size_ <= 0.0) {
        throw std::overflow_error("spatial index cell size is invalid");
    }

    const auto maximum_cells =
        crops.size() > std::numeric_limits<std::size_t>::max() / max_cells_per_crop
            ? std::numeric_limits<std::size_t>::max()
            : crops.size() * max_cells_per_crop;
    for (;;) {
        columns_ = dimension(width, cell_size_);
        rows_ = dimension(height, cell_size_);
        if (columns_ <= maximum_cells / rows_) {
            break;
        }
        cell_size_ *= 2.0;
        if (!std::isfinite(cell_size_)) {
            throw std::overflow_error("spatial index cell size overflow");
        }
    }

    const auto cells = columns_ * rows_;
    cell_offsets_.assign(cells + 1U, 0U);
    for (const auto& crop : crops) {
        const auto range = cells_for(crop_bounds(crop), bounds_, cell_size_, columns_, rows_);
        for (auto row = range.first_row; row <= range.last_row; ++row) {
            for (auto column = range.first_column; column <= range.last_column; ++column) {
                auto& count = cell_offsets_[row * columns_ + column + 1U];
                if (count == std::numeric_limits<std::size_t>::max()) {
                    throw std::overflow_error("spatial index reference count overflow");
                }
                ++count;
            }
        }
    }
    for (std::size_t index = 1; index < cell_offsets_.size(); ++index) {
        if (cell_offsets_[index] >
            std::numeric_limits<std::size_t>::max() - cell_offsets_[index - 1U]) {
            throw std::overflow_error("spatial index reference count overflow");
        }
        cell_offsets_[index] += cell_offsets_[index - 1U];
    }
    references_.resize(cell_offsets_.back());
    auto positions = cell_offsets_;
    for (std::size_t crop_index = 0; crop_index < crops.size(); ++crop_index) {
        const auto range =
            cells_for(crop_bounds(crops[crop_index]), bounds_, cell_size_, columns_, rows_);
        for (auto row = range.first_row; row <= range.last_row; ++row) {
            for (auto column = range.first_column; column <= range.last_column; ++column) {
                const auto cell = row * columns_ + column;
                references_[positions[cell]++] = crop_index;
            }
        }
    }
}

std::size_t SpatialIndex::allocated_bytes() const noexcept {
    return cell_offsets_.capacity() * sizeof(std::size_t) +
           references_.capacity() * sizeof(std::size_t);
}

std::vector<std::size_t> SpatialIndex::query(const std::vector<Crop>& crops,
                                             const Aabb& query_bounds) const {
    if (!std::isfinite(query_bounds.min_x) || !std::isfinite(query_bounds.min_y) ||
        !std::isfinite(query_bounds.max_x) || !std::isfinite(query_bounds.max_y) ||
        query_bounds.max_x < query_bounds.min_x || query_bounds.max_y < query_bounds.min_y) {
        throw std::invalid_argument("query bounds must be finite and ordered");
    }
    if (crops.empty() || !intersects(bounds_, query_bounds)) {
        return {};
    }
    const auto clipped = Aabb{
        std::max(bounds_.min_x, query_bounds.min_x), std::max(bounds_.min_y, query_bounds.min_y),
        std::min(bounds_.max_x, query_bounds.max_x), std::min(bounds_.max_y, query_bounds.max_y)};
    const auto range = cells_for(clipped, bounds_, cell_size_, columns_, rows_);
    std::vector<std::size_t> result;
    for (auto row = range.first_row; row <= range.last_row; ++row) {
        for (auto column = range.first_column; column <= range.last_column; ++column) {
            const auto cell = row * columns_ + column;
            result.insert(result.end(),
                          references_.begin() + static_cast<std::ptrdiff_t>(cell_offsets_[cell]),
                          references_.begin() +
                              static_cast<std::ptrdiff_t>(cell_offsets_[cell + 1U]));
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    result.erase(std::remove_if(result.begin(), result.end(),
                                [&](const std::size_t index) {
                                    return !intersects(crop_bounds(crops[index]), query_bounds);
                                }),
                 result.end());
    return result;
}

bool SpatialIndex::operator==(const SpatialIndex& other) const noexcept {
    return bounds_ == other.bounds_ && cell_size_ == other.cell_size_ &&
           columns_ == other.columns_ && rows_ == other.rows_ &&
           cell_offsets_ == other.cell_offsets_ && references_ == other.references_;
}

} // namespace cropsim
