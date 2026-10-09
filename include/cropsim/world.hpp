#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace cropsim {

namespace generators {
class GeneratorRegistry;
}

struct EllipseLeaf {
  double x{};
  double y{};
  double radius_x{};
  double radius_y{};
  double rotation{};

  [[nodiscard]] bool operator==(const EllipseLeaf &other) const noexcept;
};

struct Crop {
  std::uint64_t id{};
  double x{};
  double y{};
  std::vector<EllipseLeaf> leaves;

  Crop() = default;
  Crop(std::uint64_t crop_id, double crop_x, double crop_y, double radius);
  Crop(std::uint64_t crop_id, double crop_x, double crop_y,
       std::vector<EllipseLeaf> crop_leaves);

  [[nodiscard]] bool operator==(const Crop &other) const noexcept;
};

struct Aabb {
  double min_x{};
  double min_y{};
  double max_x{};
  double max_y{};

  [[nodiscard]] bool operator==(const Aabb &other) const noexcept;
};

[[nodiscard]] Aabb crop_bounds(const Crop &crop);
[[nodiscard]] bool leaf_contains(const Crop &crop, const EllipseLeaf &leaf,
                                 double world_x, double world_y) noexcept;

class SpatialIndex final {
public:
  explicit SpatialIndex(const std::vector<Crop> &crops);

  [[nodiscard]] const Aabb &bounds() const noexcept { return bounds_; }
  [[nodiscard]] double cell_size() const noexcept { return cell_size_; }
  [[nodiscard]] std::size_t columns() const noexcept { return columns_; }
  [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
  [[nodiscard]] std::size_t cell_count() const noexcept {
    return columns_ * rows_;
  }
  [[nodiscard]] std::size_t reference_count() const noexcept {
    return references_.size();
  }
  [[nodiscard]] std::size_t allocated_bytes() const noexcept;
  [[nodiscard]] const std::vector<std::size_t> &cell_offsets() const noexcept {
    return cell_offsets_;
  }
  [[nodiscard]] const std::vector<std::size_t> &references() const noexcept {
    return references_;
  }
  [[nodiscard]] std::vector<std::size_t> query(const std::vector<Crop> &crops,
                                               const Aabb &bounds) const;
  [[nodiscard]] bool operator==(const SpatialIndex &other) const noexcept;

private:
  Aabb bounds_{};
  double cell_size_{1.0};
  std::size_t columns_{};
  std::size_t rows_{};
  std::vector<std::size_t> cell_offsets_{0U};
  std::vector<std::size_t> references_;
};

class World final {
public:
  World(std::uint64_t seed, std::vector<Crop> crops);

  [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
  [[nodiscard]] const std::vector<Crop> &crops() const noexcept {
    return crops_;
  }
  [[nodiscard]] const SpatialIndex &spatial_index() const noexcept {
    return spatial_index_;
  }
  [[nodiscard]] std::size_t size() const noexcept { return crops_.size(); }
  [[nodiscard]] std::vector<std::size_t> query(const Aabb &bounds) const;
  [[nodiscard]] bool operator==(const World &other) const noexcept;

private:
  std::uint64_t seed_{};
  std::vector<Crop> crops_;
  SpatialIndex spatial_index_;
};

[[nodiscard]] World world_from_yaml(std::string_view yaml);
[[nodiscard]] World
world_from_yaml(std::string_view yaml,
                const generators::GeneratorRegistry &registry);

} // namespace cropsim
