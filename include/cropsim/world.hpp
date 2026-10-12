#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <optional>
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

struct RailPoint final {
  double x{};
  double y{};

  [[nodiscard]] bool operator==(const RailPoint &other) const noexcept;
};

enum class RailKind : std::uint8_t {
  nominal_traversal,
  actual_traversal,
  nominal_bed_center,
  real_bed_center
};

class Rail final {
public:
  Rail(std::uint64_t id, RailKind kind, std::optional<std::uint64_t> traversal_id,
       std::optional<std::uint64_t> bed_id, std::vector<RailPoint> points);
  // Compatibility constructor for source code which created v4 row rails.
  Rail(std::uint64_t id, std::uint64_t field_index,
       std::uint64_t row_index, std::vector<RailPoint> points);

  [[nodiscard]] std::uint64_t id() const noexcept { return id_; }
  [[nodiscard]] std::uint64_t field_index() const noexcept {
    return field_index_;
  }
  [[nodiscard]] std::uint64_t row_index() const noexcept { return row_index_; }
  [[nodiscard]] RailKind kind() const noexcept { return kind_; }
  [[nodiscard]] const std::optional<std::uint64_t> &traversal_id() const noexcept { return traversal_id_; }
  [[nodiscard]] const std::optional<std::uint64_t> &bed_id() const noexcept { return bed_id_; }
  [[nodiscard]] const std::vector<RailPoint> &points() const noexcept {
    return points_;
  }
  [[nodiscard]] double length() const noexcept { return cumulative_lengths_.back(); }
  [[nodiscard]] RailPoint position_at(double distance) const;
  [[nodiscard]] RailPoint tangent_at(double distance) const;
  [[nodiscard]] bool operator==(const Rail &other) const noexcept;

private:
  [[nodiscard]] std::size_t segment_at(double distance) const;

  std::uint64_t id_{};
  std::uint64_t field_index_{};
  std::uint64_t row_index_{};
  RailKind kind_{RailKind::actual_traversal};
  std::optional<std::uint64_t> traversal_id_;
  std::optional<std::uint64_t> bed_id_;
  std::vector<RailPoint> points_;
  std::vector<double> cumulative_lengths_;
};

class CropLine final {
public:
  CropLine(std::uint64_t id, std::optional<std::uint64_t> bed_id,
           std::uint64_t field_index, std::uint64_t row_index,
           std::vector<RailPoint> points,
           std::optional<std::uint64_t> first_crop_id = {},
           std::optional<std::uint64_t> crop_count = {});
  [[nodiscard]] std::uint64_t id() const noexcept { return id_; }
  [[nodiscard]] const std::optional<std::uint64_t> &bed_id() const noexcept { return bed_id_; }
  [[nodiscard]] std::uint64_t field_index() const noexcept { return field_index_; }
  [[nodiscard]] std::uint64_t row_index() const noexcept { return row_index_; }
  [[nodiscard]] const std::vector<RailPoint> &points() const noexcept { return points_; }
  [[nodiscard]] const std::optional<std::uint64_t> &first_crop_id() const noexcept { return first_crop_id_; }
  [[nodiscard]] const std::optional<std::uint64_t> &crop_count() const noexcept { return crop_count_; }
  [[nodiscard]] double length() const noexcept { return cumulative_lengths_.back(); }
  [[nodiscard]] RailPoint position_at(double distance) const;
  [[nodiscard]] RailPoint tangent_at(double distance) const;
  [[nodiscard]] bool operator==(const CropLine &) const noexcept;
private:
  [[nodiscard]] std::size_t segment_at(double distance) const;
  std::uint64_t id_{}; std::optional<std::uint64_t> bed_id_;
  std::uint64_t field_index_{}; std::uint64_t row_index_{};
  std::vector<RailPoint> points_; std::vector<double> cumulative_lengths_;
  std::optional<std::uint64_t> first_crop_id_, crop_count_;
};

struct Traversal final {
  std::uint64_t id{}, field_index{}, pass_index{};
  std::vector<std::uint64_t> nominal_rail_ids, actual_rail_ids, bed_ids;
  [[nodiscard]] bool operator==(const Traversal &) const noexcept;
};

struct Bed final {
  std::uint64_t id{}, traversal_id{};
  std::vector<std::uint64_t> nominal_rail_ids, real_rail_ids, crop_line_ids;
  [[nodiscard]] bool operator==(const Bed &) const noexcept;
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
  World(std::uint64_t seed, std::vector<Crop> crops,
        std::vector<Rail> rails = {}, std::vector<CropLine> crop_lines = {},
        std::vector<Traversal> traversals = {}, std::vector<Bed> beds = {});

  [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
  [[nodiscard]] const std::vector<Crop> &crops() const noexcept {
    return crops_;
  }
  [[nodiscard]] const SpatialIndex &spatial_index() const noexcept {
    return spatial_index_;
  }
  [[nodiscard]] const std::vector<Rail> &rails() const noexcept {
    return rails_;
  }
  [[nodiscard]] const std::vector<CropLine> &crop_lines() const noexcept { return crop_lines_; }
  [[nodiscard]] const std::vector<Traversal> &traversals() const noexcept { return traversals_; }
  [[nodiscard]] const std::vector<Bed> &beds() const noexcept { return beds_; }
  [[nodiscard]] std::size_t size() const noexcept { return crops_.size(); }
  [[nodiscard]] std::vector<std::size_t> query(const Aabb &bounds) const;
  [[nodiscard]] bool operator==(const World &other) const noexcept;

private:
  std::uint64_t seed_{};
  std::vector<Crop> crops_;
  std::vector<Rail> rails_;
  std::vector<CropLine> crop_lines_;
  std::vector<Traversal> traversals_;
  std::vector<Bed> beds_;
  SpatialIndex spatial_index_;
};

[[nodiscard]] World world_from_yaml(std::string_view yaml);
[[nodiscard]] World
world_from_yaml(std::string_view yaml,
                const generators::GeneratorRegistry &registry);

} // namespace cropsim
