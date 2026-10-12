#include <doctest/doctest.h>

#include "cropsim/snapshot.hpp"
#include "cropsim/world.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
constexpr std::string_view straight_rows = R"(
seed: 77
generators:
  - gentype: field_set
    bounds: [[0, 0], [6, 0], [6, 4], [0, 4]]
    count: 1
    road_width: 0
    seed_jitter: 0
    field:
      gentype: parallel_rows
      orientation: 0
      headland: 0
      row_spacing: 1
      row:
        gentype: linear
        crop_spacing: 1
        crop: {gentype: fixed, radius: 0.1}
)";
}

TEST_CASE("legacy generated rows become unassociated crop lines") {
  const auto world = cropsim::world_from_yaml(straight_rows);
  CHECK(world.rails().empty());
  REQUIRE(world.crop_lines().size() == 4U);
  for (std::size_t index = 0; index < world.crop_lines().size(); ++index) {
    const auto &rail = world.crop_lines()[index];
    CHECK(rail.id() == index);
    CHECK_FALSE(rail.bed_id());
    CHECK(rail.field_index() == 0U);
    CHECK(rail.row_index() == index);
    REQUIRE(rail.points().size() == 2U);
    CHECK(rail.points().front().y == doctest::Approx(0.5 + static_cast<double>(index)));
    if (index % 2U == 0U) {
      CHECK(rail.points().front().x < rail.points().back().x);
    } else {
      CHECK(rail.points().front().x > rail.points().back().x);
    }
  }
}

TEST_CASE("curved planting rails preserve generated GPS paths") {
  const auto world = cropsim::world_from_yaml(R"(
seed: 91
generators:
  - gentype: field_set
    bounds: [[0, 0], [30, 0], [30, 8], [0, 8]]
    count: 1
    road_width: 0
    field:
      gentype: parallel_rows
      orientation: 0
      headland: 0
      planting:
        top_width: 1.8
        center_spacing: 2
        lines_per_bed: [2]
        gps_lateral_offset: 0.1
        wave: {amplitude: 0.2, wavelength: 10, phase: 0}
      row:
        gentype: linear
        crop_spacing: 1
        crop: {gentype: fixed, radius: 0.1}
)");
  REQUIRE_FALSE(world.rails().empty());
  REQUIRE_FALSE(world.traversals().empty());
  REQUIRE_FALSE(world.beds().empty());
  REQUIRE_FALSE(world.crop_lines().empty());
  CHECK(world.rails().front().points().size() > 2U);
  CHECK(world.rails().front().length() > 29.0);
  for (const auto &rail : world.rails()) {
    CHECK(rail.points().size() > 2U);
  }
  for (const auto &traversal : world.traversals()) {
    CHECK_FALSE(traversal.nominal_rail_ids.empty());
    CHECK_FALSE(traversal.actual_rail_ids.empty());
    CHECK_FALSE(traversal.bed_ids.empty());
    for (const auto id : traversal.nominal_rail_ids)
      CHECK(world.rails()[id].kind() == cropsim::RailKind::nominal_traversal);
    for (const auto id : traversal.actual_rail_ids)
      CHECK(world.rails()[id].kind() == cropsim::RailKind::actual_traversal);
  }
  for (const auto &bed : world.beds()) {
    CHECK_FALSE(bed.nominal_rail_ids.empty());
    CHECK_FALSE(bed.real_rail_ids.empty());
    CHECK_FALSE(bed.crop_line_ids.empty());
    for (const auto id : bed.crop_line_ids) {
      const auto &line = world.crop_lines()[id];
      REQUIRE(line.bed_id());
      CHECK(*line.bed_id() == bed.id);
      REQUIRE(line.first_crop_id());
      REQUIRE(line.crop_count());
      CHECK(*line.crop_count() > 0U);
    }
  }
}

TEST_CASE("crop-line sampling follows polyline arc length") {
  const cropsim::CropLine line(0U, std::nullopt, 3U, 4U,
                              {{0.0, 0.0}, {3.0, 0.0}, {3.0, 4.0}});
  CHECK(line.length() == doctest::Approx(7.0));
  CHECK(line.position_at(5.0).y == doctest::Approx(2.0));
  CHECK(line.tangent_at(3.0).y == doctest::Approx(1.0));
}

TEST_CASE("rail sampling follows polyline arc length") {
  const cropsim::Rail rail(0U, 3U, 4U, {{0.0, 0.0}, {3.0, 0.0}, {3.0, 4.0}});
  CHECK(rail.length() == doctest::Approx(7.0));
  CHECK(rail.position_at(1.5).x == doctest::Approx(1.5));
  CHECK(rail.position_at(1.5).y == doctest::Approx(0.0));
  CHECK(rail.position_at(5.0).x == doctest::Approx(3.0));
  CHECK(rail.position_at(5.0).y == doctest::Approx(2.0));
  CHECK(rail.tangent_at(3.0).x == doctest::Approx(0.0));
  CHECK(rail.tangent_at(3.0).y == doctest::Approx(1.0));
  CHECK(rail.tangent_at(7.0).y == doctest::Approx(1.0));
  CHECK_THROWS_AS(static_cast<void>(rail.position_at(-0.1)), std::out_of_range);
  CHECK_THROWS_AS(static_cast<void>(rail.tangent_at(7.1)), std::out_of_range);
  CHECK_THROWS_AS(static_cast<void>(rail.position_at(
                      std::numeric_limits<double>::quiet_NaN())),
                  std::out_of_range);
}

TEST_CASE("rail geometry and IDs are validated") {
  CHECK_THROWS_AS(cropsim::Rail(0U, 0U, 0U, {{0.0, 0.0}}),
                  std::invalid_argument);
  CHECK_THROWS_AS(cropsim::Rail(0U, 0U, 0U,
                               {{0.0, 0.0}, {0.0, 0.0}}),
                  std::invalid_argument);
  CHECK_THROWS_AS(cropsim::Rail(0U, 0U, 0U,
                               {{0.0, 0.0},
                                {std::numeric_limits<double>::infinity(), 0.0}}),
                  std::invalid_argument);
  CHECK_THROWS_AS(cropsim::World(0U, {}, {cropsim::Rail(
                                             1U, 0U, 0U,
                                             {{0.0, 0.0}, {1.0, 0.0}})}),
                  std::invalid_argument);
}

TEST_CASE("snapshot version 5 round-trips hierarchy") {
  const auto world = cropsim::world_from_yaml(straight_rows);
  const auto bytes = cropsim::serialise_snapshot(world);
  REQUIRE(bytes.size() > 12U);
  CHECK(std::to_integer<unsigned int>(bytes[8U]) == 5U);
  const auto restored = cropsim::deserialise_snapshot(bytes);
  CHECK(restored == world);
  CHECK(cropsim::serialise_snapshot(restored) == bytes);

  auto truncated = bytes;
  truncated.resize(truncated.size() - sizeof(double));
  CHECK_THROWS_AS(static_cast<void>(cropsim::deserialise_snapshot(truncated)),
                  std::runtime_error);
}

TEST_CASE("non-row generators leave the rail collection empty") {
  const auto world = cropsim::world_from_yaml(R"(
seed: 1
crops: [{position: [0, 0], radius: 0.1}]
generators:
  - {gentype: grid, origin: [1, 1], rows: 1, columns: 1,
     spacing: [1, 1], radius: 0.1}
)");
  CHECK(world.rails().empty());
}

TEST_CASE("snapshot version 3 remains readable with empty rails") {
  const cropsim::World world(5U, {{0U, 1.0, 2.0, 0.1}});
  auto bytes = cropsim::serialise_snapshot(world);
  REQUIRE(bytes.size() >= 20U);
  bytes[8U] = std::byte{3U};
  bytes.resize(bytes.size() - 4U * sizeof(std::uint64_t));
  const auto restored = cropsim::deserialise_snapshot(bytes);
  CHECK(restored.seed() == world.seed());
  CHECK(restored.crops() == world.crops());
  CHECK(restored.rails().empty());
}
