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
  [[nodiscard]] std::uint64_t world_seed() const noexcept { return world_seed_; }
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
  void add_rail(std::uint64_t field_index, std::uint64_t row_index,
                std::vector<RailPoint> points);
  [[nodiscard]] std::vector<Rail> take_rails() noexcept;
  [[nodiscard]] std::uint64_t add_crop_line(std::optional<std::uint64_t> bed_id,
                     std::uint64_t field_index, std::uint64_t row_index,
                     std::vector<RailPoint> points,
                     std::optional<std::uint64_t> first_crop_id = {},
                     std::optional<std::uint64_t> crop_count = {});
  [[nodiscard]] std::vector<CropLine> take_crop_lines() noexcept;
  [[nodiscard]] std::uint64_t add_traversal(std::uint64_t, std::uint64_t);
  [[nodiscard]] std::uint64_t add_bed(std::uint64_t);
  void add_attachment_rail(RailKind, std::uint64_t, std::optional<std::uint64_t>, std::vector<RailPoint>);
  void traversal_add_bed(std::uint64_t, std::uint64_t);
  void bed_add_crop_line(std::uint64_t, std::uint64_t);
  [[nodiscard]] std::vector<Traversal> take_traversals() noexcept;
  [[nodiscard]] std::vector<Bed> take_beds() noexcept;

private:
  std::uint64_t random_state_{};
  std::uint64_t world_seed_{};
  std::uint64_t next_id_{};
  std::vector<Rail> rails_;
  std::vector<CropLine> crop_lines_;
  std::vector<Traversal> traversals_;
  std::vector<Bed> beds_;
};

} // namespace cropsim::generators
