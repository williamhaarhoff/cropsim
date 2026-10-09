#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cropsim/generators/generator_registry.hpp"
#include "cropsim/renderer.hpp"
#include "cropsim/snapshot.hpp"
#include "cropsim/world.hpp"

namespace {

constexpr auto description = R"(
seed: 568616
crops:
  - position: [0.25, 0.75]
    radius: 0.1
generators:
  - gentype: grid
    origin: [1.0, 2.0]
    rows: 2
    columns: 3
    spacing: [0.5, 0.25]
    radius: 0.08
    jitter: 0.01
)";

std::uint64_t snapshot_hash(const std::vector<std::byte> &bytes) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const auto byte : bytes) {
    hash ^= std::to_integer<std::uint8_t>(byte);
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::vector<std::size_t> brute_force_query(const cropsim::World &world,
                                           const cropsim::Aabb &bounds) {
  std::vector<std::size_t> result;
  for (std::size_t index = 0; index < world.size(); ++index) {
    const auto crop = cropsim::crop_bounds(world.crops()[index]);
    if (crop.min_x <= bounds.max_x && crop.max_x >= bounds.min_x &&
        crop.min_y <= bounds.max_y && crop.max_y >= bounds.min_y) {
      result.push_back(index);
    }
  }
  return result;
}

template <typename Integer>
void append_integer(std::vector<std::byte> &bytes, Integer value) {
  for (std::size_t index = 0; index < sizeof(value); ++index) {
    bytes.push_back(static_cast<std::byte>(value & 0xffU));
    value >>= 8U;
  }
}

void append_double(std::vector<std::byte> &bytes, const double value) {
  std::uint64_t bits{};
  std::memcpy(&bits, &value, sizeof(bits));
  append_integer(bytes, bits);
}

std::vector<std::byte> legacy_snapshot(const cropsim::World &world,
                                       const std::uint32_t version) {
  std::vector<std::byte> bytes;
  for (const char character : std::string_view("CROPSIM\0", 8U)) {
    bytes.push_back(static_cast<std::byte>(character));
  }
  append_integer(bytes, version);
  append_integer(bytes, world.seed());
  append_integer(bytes, static_cast<std::uint64_t>(world.size()));
  for (const auto &crop : world.crops()) {
    REQUIRE(crop.leaves.size() == 1U);
    append_integer(bytes, crop.id);
    append_double(bytes, crop.x);
    append_double(bytes, crop.y);
    append_double(bytes, crop.leaves.front().radius_x);
  }
  if (version == 2U) {
    const auto &index = world.spatial_index();
    append_double(bytes, index.bounds().min_x);
    append_double(bytes, index.bounds().min_y);
    append_double(bytes, index.bounds().max_x);
    append_double(bytes, index.bounds().max_y);
    append_double(bytes, index.cell_size());
    append_integer(bytes, static_cast<std::uint64_t>(index.columns()));
    append_integer(bytes, static_cast<std::uint64_t>(index.rows()));
    append_integer(bytes,
                   static_cast<std::uint64_t>(index.cell_offsets().size()));
    for (const auto offset : index.cell_offsets()) {
      append_integer(bytes, static_cast<std::uint64_t>(offset));
    }
    append_integer(bytes,
                   static_cast<std::uint64_t>(index.references().size()));
    for (const auto reference : index.references()) {
      append_integer(bytes, static_cast<std::uint64_t>(reference));
    }
  }
  return bytes;
}

class TestGenerator final : public cropsim::generators::PlacementGenerator {
public:
  void generate(const YAML::Node &,
                cropsim::generators::GenerationContext &context,
                cropsim::generators::GenerationKey,
                const cropsim::generators::GeneratorRegistry &,
                std::vector<cropsim::Crop> &destination) const override {
    destination.push_back(
        context.make_crop(context.symmetric_unit(), 4.0, 0.2));
  }
};

class TestCropGenerator final : public cropsim::generators::CropGenerator {
public:
  std::vector<cropsim::EllipseLeaf>
  generate(const YAML::Node &,
           cropsim::generators::MorphologyContext &) const override {
    return {{0.25, -0.5, 0.125, 0.75, 0.3}};
  }
};

class TestFieldGenerator final : public cropsim::generators::FieldGenerator {
public:
  std::vector<cropsim::generators::RowSegment>
  generate(const YAML::Node &, const cropsim::generators::FieldRegion &field,
           cropsim::generators::GenerationKey) const override {
    return {{field.seed_index,
             0U,
             {1.0, 2.0},
             {5.0, 2.0},
             {{{0.0, 0.0}, {6.0, 0.0}, {6.0, 4.0}, {0.0, 4.0}}},
             1.0}};
  }
};

class TestRowGenerator final : public cropsim::generators::RowGenerator {
public:
  std::vector<cropsim::generators::Point2>
  generate(const YAML::Node &, const cropsim::generators::RowSegment &,
           cropsim::generators::GenerationKey) const override {
    return {{2.0, 2.0}, {4.0, 2.0}};
  }
};

class TwoRowFieldGenerator final : public cropsim::generators::FieldGenerator {
public:
  std::vector<cropsim::generators::RowSegment>
  generate(const YAML::Node &, const cropsim::generators::FieldRegion &field,
           cropsim::generators::GenerationKey) const override {
    const cropsim::generators::Polygon2 boundary{
        {{0.0, 0.0}, {10.0, 0.0}, {10.0, 5.0}, {0.0, 5.0}}};
    return {{field.seed_index, 0U, {1.0, 1.0}, {9.0, 1.0}, boundary, 1.0},
            {field.seed_index, 1U, {1.0, 3.0}, {9.0, 3.0}, boundary, 1.0}};
  }
};

class VariableFirstRowGenerator final : public cropsim::generators::RowGenerator {
public:
  std::vector<cropsim::generators::Point2>
  generate(const YAML::Node &node,
           const cropsim::generators::RowSegment &row,
           cropsim::generators::GenerationKey) const override {
    if (row.row_index != 0U) {
      return {{5.0, 3.0}};
    }
    std::vector<cropsim::generators::Point2> result;
    const auto count = node["first_count"].as<std::size_t>();
    for (std::size_t index = 0; index < count; ++index) {
      result.push_back({1.0 + static_cast<double>(index), 1.0});
    }
    return result;
  }
};

} // namespace

TEST_CASE("YAML world generation is deterministic") {
  const auto first = cropsim::world_from_yaml(description);
  const auto second = cropsim::world_from_yaml(description);

  CHECK(first == second);
  CHECK(first.seed() == 568616U);
  REQUIRE(first.size() == 7U);
  for (std::size_t index = 0; index < first.size(); ++index) {
    CHECK(first.crops()[index].id == index);
  }
}

TEST_CASE("world snapshot round-trip preserves exact state") {
  const auto original = cropsim::world_from_yaml(description);
  const auto first_snapshot = cropsim::serialise_snapshot(original);
  const auto restored = cropsim::deserialise_snapshot(first_snapshot);

  CHECK(restored == original);
  CHECK(cropsim::serialise_snapshot(restored) == first_snapshot);
}

TEST_CASE("version 3 snapshots have deterministic bytes") {
  const auto first =
      cropsim::serialise_snapshot(cropsim::world_from_yaml(description));
  const auto second =
      cropsim::serialise_snapshot(cropsim::world_from_yaml(description));
  CHECK(first == second);
  CHECK(first.size() > 508U);
  CHECK(snapshot_hash(first) == snapshot_hash(second));
}

TEST_CASE("generator factory dispatch and validation") {
  auto registry = cropsim::generators::make_builtin_generator_registry();
  CHECK(registry.placement().create("grid") != nullptr);
  CHECK_THROWS_AS(static_cast<void>(registry.placement().create("unknown")),
                  std::invalid_argument);
  CHECK_THROWS_AS(registry.placement().register_generator(
                      "grid", [] { return std::make_unique<TestGenerator>(); }),
                  std::invalid_argument);

  CHECK_THROWS_AS(
      static_cast<void>(cropsim::world_from_yaml("generators: [{rows: 1}]")),
      std::invalid_argument);
  CHECK_THROWS_AS(static_cast<void>(cropsim::world_from_yaml(
                      "generators: [{gentype: unknown}]")),
                  std::invalid_argument);
  CHECK_THROWS_AS(
      static_cast<void>(cropsim::world_from_yaml("generators: [{type: grid}]")),
      std::invalid_argument);
}

TEST_CASE("multiple generators share IDs and deterministic random state") {
  constexpr std::string_view split = R"(
seed: 42
generators:
  - {gentype: grid, origin: [0, 0], rows: 1, columns: 2, spacing: [1, 1], radius: 0.1, jitter: 0.2}
  - {gentype: grid, origin: [2, 0], rows: 1, columns: 2, spacing: [1, 1], radius: 0.1, jitter: 0.2}
)";
  constexpr std::string_view combined = R"(
seed: 42
generators:
  - {gentype: grid, origin: [0, 0], rows: 1, columns: 4, spacing: [1, 1], radius: 0.1, jitter: 0.2}
)";
  const auto first = cropsim::world_from_yaml(split);
  const auto second = cropsim::world_from_yaml(split);
  CHECK(first == second);
  CHECK(first == cropsim::world_from_yaml(combined));
  REQUIRE(first.size() == 4U);
  for (std::size_t index = 0; index < first.size(); ++index) {
    CHECK(first.crops()[index].id == index);
  }
}

TEST_CASE("explicit and generated crops have stable ordering") {
  const auto world = cropsim::world_from_yaml(R"(
seed: 1
crops:
  - {position: [8, 9], radius: 0.5}
  - {position: [6, 7], radius: 0.4}
generators:
  - {gentype: grid, origin: [1, 2], rows: 1, columns: 1, spacing: [1, 1], radius: 0.3}
)");
  REQUIRE(world.size() == 3U);
  CHECK(world.crops()[0] == cropsim::Crop{0, 8, 9, 0.5});
  CHECK(world.crops()[1] == cropsim::Crop{1, 6, 7, 0.4});
  CHECK(world.crops()[2] == cropsim::Crop{2, 1, 2, 0.3});
}

TEST_CASE("grid generator rejects invalid values and size overflow") {
  const auto check_invalid = [](const std::string_view fields) {
    const auto yaml =
        std::string("generators: [{gentype: grid, origin: [0, 0], spacing: "
                    "[1, 1], radius: 1, ") +
        std::string(fields) + "}]";
    CHECK_THROWS(static_cast<void>(cropsim::world_from_yaml(yaml)));
  };
  check_invalid("rows: 0, columns: 1");
  check_invalid("rows: 1, columns: 0");
  check_invalid("rows: 1, columns: 1, jitter: -1");
  check_invalid("rows: 18446744073709551615, columns: 2");
  check_invalid("rows: 18446744073709551615, columns: 1");
}

TEST_CASE("an injected generator uses the unchanged world-building flow") {
  auto registry = cropsim::generators::make_builtin_generator_registry();
  registry.placement().register_generator(
      "test", [] { return std::make_unique<TestGenerator>(); });
  const auto first = cropsim::world_from_yaml(
      "seed: 99\ncrops: [{position: [1, 2], radius: 0.1}]\n"
      "generators: [{gentype: test}]\n",
      registry);
  const auto second = cropsim::world_from_yaml(
      "seed: 99\ncrops: [{position: [1, 2], radius: 0.1}]\n"
      "generators: [{gentype: test}]\n",
      registry);
  CHECK(first == second);
  REQUIRE(first.size() == 2U);
  CHECK(first.crops()[0].id == 0U);
  CHECK(first.crops()[1].id == 1U);
  CHECK(first.crops()[1].y == 4.0);
}

TEST_CASE(
    "generic scalar parameters follow the rosette formula in leaf order") {
  const auto world = cropsim::world_from_yaml(R"(
seed: 7
generators:
  - gentype: grid
    origin: [2, 3]
    rows: 1
    columns: 1
    spacing: [1, 1]
    crop:
      gentype: generic
      scale: 2
      leaf_num: 4
      leaf_length: 3
      leaf_width: 0.5
      leaf_offset: 1
      leaf_orientation: 0
)");
  REQUIRE(world.size() == 1U);
  const auto &leaves = world.crops().front().leaves;
  REQUIRE(leaves.size() == 4U);
  CHECK(leaves[0].x == doctest::Approx(8.0));
  CHECK(leaves[0].y == doctest::Approx(0.0));
  CHECK(leaves[0].radius_x == doctest::Approx(6.0));
  CHECK(leaves[0].radius_y == doctest::Approx(1.0));
  CHECK(leaves[0].rotation == doctest::Approx(0.0));
  CHECK(leaves[1].x == doctest::Approx(0.0).epsilon(1e-12));
  CHECK(leaves[1].y == doctest::Approx(8.0));
  CHECK(leaves[1].rotation == doctest::Approx(1.5707963267948966));
  CHECK(leaves[2].x == doctest::Approx(-8.0));
  CHECK(leaves[3].y == doctest::Approx(-8.0));
}

TEST_CASE("generic scale distribution varies crops rather than leaves") {
  const auto world = cropsim::world_from_yaml(R"(
seed: 701
generators:
  - gentype: grid
    origin: [0, 0]
    rows: 1
    columns: 6
    spacing: [3, 1]
    crop:
      gentype: generic
      scale: {mean: 0.5, min: 0.1, max: 1.0}
      leaf_num: 4
      leaf_length: 1
      leaf_width: 0.25
      leaf_offset: 0
      leaf_orientation: 0
)");
  REQUIRE(world.size() == 6U);
  for (const auto &crop : world.crops()) {
    REQUIRE(crop.leaves.size() == 4U);
    const auto crop_scale = crop.leaves.front().radius_x;
    CHECK(crop_scale >= 0.1);
    CHECK(crop_scale <= 1.0);
    for (const auto &leaf : crop.leaves) {
      CHECK(leaf.radius_x == crop_scale);
      CHECK(leaf.radius_y == doctest::Approx(crop_scale * 0.25));
    }
  }
  CHECK(world.crops()[0].leaves.front().radius_x !=
        world.crops()[1].leaves.front().radius_x);
}

TEST_CASE("crop generator choice cannot alter placement jitter or IDs") {
  constexpr std::string_view fixed = R"(
seed: 123
generators:
  - {gentype: grid, origin: [1, 2], rows: 2, columns: 3, spacing: [0.5, 0.7], jitter: 0.2,
     crop: {gentype: fixed, radius: 0.1}}
)";
  constexpr std::string_view generic = R"(
seed: 123
generators:
  - {gentype: grid, origin: [1, 2], rows: 2, columns: 3, spacing: [0.5, 0.7], jitter: 0.2,
     crop: {gentype: generic, leaf_num: 3, leaf_length: 1, leaf_width: 0.2,
            leaf_offset: 0, leaf_orientation: 0}}
)";
  const auto first = cropsim::world_from_yaml(fixed);
  const auto second = cropsim::world_from_yaml(generic);
  REQUIRE(first.size() == second.size());
  for (std::size_t index = 0; index < first.size(); ++index) {
    CHECK(first.crops()[index].id == second.crops()[index].id);
    CHECK(first.crops()[index].x == second.crops()[index].x);
    CHECK(first.crops()[index].y == second.crops()[index].y);
  }
}

TEST_CASE("an injected crop generator composes with the unchanged grid") {
  auto registry = cropsim::generators::make_builtin_generator_registry();
  registry.crops().register_generator(
      "test", [] { return std::make_unique<TestCropGenerator>(); });
  const auto world = cropsim::world_from_yaml(R"(
seed: 5
generators:
  - {gentype: grid, origin: [0, 0], rows: 1, columns: 2, spacing: [1, 1],
     crop: {gentype: test}}
)",
                                              registry);
  REQUIRE(world.size() == 2U);
  CHECK(world.crops()[0].leaves == world.crops()[1].leaves);
  CHECK(world.crops()[0].leaves.front() ==
        cropsim::EllipseLeaf{0.25, -0.5, 0.125, 0.75, 0.3});
}

TEST_CASE("generic crop configuration validation rejects ambiguous and invalid "
          "input") {
  const auto invalid = [](const std::string_view crop) {
    const auto yaml =
        std::string("generators: [{gentype: grid, origin: [0, 0], rows: 1, "
                    "columns: 1, spacing: [1, 1], crop: ") +
        std::string(crop) + "}]";
    CHECK_THROWS(static_cast<void>(cropsim::world_from_yaml(yaml)));
  };
  invalid("{scale: 1}");
  invalid("{gentype: unknown}");
  invalid("{gentype: generic, scale: 0}");
  invalid("{gentype: generic, leaf_num: 0}");
  invalid("{gentype: generic, leaf_length: {mean: 2, min: 3, max: 1}}");
  invalid("{gentype: generic, leaf_width: {mean: 1, min: 0, max: 2}}");
  invalid("{gentype: generic, leaf_length: 0.01, leaf_offset: -0.02}");
  CHECK_THROWS(static_cast<void>(cropsim::world_from_yaml(R"(
generators:
  - {gentype: grid, origin: [0, 0], rows: 1, columns: 1, spacing: [1, 1], radius: 1,
     crop: {gentype: fixed, radius: 1}}
)")));
}

TEST_CASE("legacy and nested fixed grid geometry serialize identically") {
  constexpr std::string_view legacy = R"(
seed: 88
generators:
  - {gentype: grid, origin: [0, 0], rows: 2, columns: 2, spacing: [1, 1], radius: 0.3, jitter: 0.1}
)";
  constexpr std::string_view nested = R"(
seed: 88
generators:
  - {gentype: grid, origin: [0, 0], rows: 2, columns: 2, spacing: [1, 1], jitter: 0.1,
     crop: {gentype: fixed, radius: 0.3}}
)";
  CHECK(cropsim::serialise_snapshot(cropsim::world_from_yaml(legacy)) ==
        cropsim::serialise_snapshot(cropsim::world_from_yaml(nested)));
}

TEST_CASE("field hierarchy fills a rectangular field in canonical order") {
  constexpr std::string_view yaml = R"(
seed: 19
generators:
  - gentype: field_set
    bounds: [[0, 0], [10, 0], [10, 10], [0, 10]]
    count: 1
    road_width: 0
    seed_jitter: 0
    field:
      gentype: parallel_rows
      orientation: 0
      row_spacing: 2
      row_jitter: 0
      headland: 0
      row:
        gentype: linear
        crop_spacing: 2
        along_jitter: 0
        cross_jitter: 0
        crop: {gentype: fixed, radius: 0.1}
)";
  const auto first = cropsim::world_from_yaml(yaml);
  const auto second = cropsim::world_from_yaml(yaml);
  CHECK(first == second);
  REQUIRE(first.size() == 25U);
  for (std::size_t index = 0; index < first.size(); ++index) {
    CHECK(first.crops()[index].id == index);
  }
  CHECK(first.crops()[0].x == doctest::Approx(1.0));
  CHECK(first.crops()[0].y == doctest::Approx(1.0));
  CHECK(first.crops()[4].x == doctest::Approx(9.0));
  CHECK(first.crops()[4].y == doctest::Approx(1.0));
  CHECK(first.crops()[20].x == doctest::Approx(1.0));
  CHECK(first.crops()[20].y == doctest::Approx(9.0));
}

TEST_CASE("field-set roads have exact width and preserve domain edges") {
  const auto world = cropsim::world_from_yaml(R"(
seed: 123
generators:
  - gentype: field_set
    bounds: [[0, 0], [20, 0], [20, 10], [0, 10]]
    count: 2
    road_width: 2
    seed_jitter: 0
    field:
      gentype: parallel_rows
      row_spacing: 1
      row_orientation: 0
      headland: 0
      row:
        gentype: linear
        crop_spacing: 1
        crop: {gentype: fixed, radius: 0.1}
)");
  REQUIRE(world.size() > 100U);
  auto left_max = -std::numeric_limits<double>::infinity();
  auto right_min = std::numeric_limits<double>::infinity();
  for (const auto &crop : world.crops()) {
    if (crop.x < 10.0)
      left_max = std::max(left_max, crop.x);
    if (crop.x > 10.0)
      right_min = std::min(right_min, crop.x);
  }
  CHECK(left_max < 9.0);
  CHECK(right_min > 11.0);
  CHECK(left_max > 8.0);
  CHECK(right_min < 12.0);
}

TEST_CASE("concave multi-field hierarchy is deterministic and bounded") {
  constexpr std::string_view yaml = R"(
seed: 991
generators:
  - gentype: field_set
    bounds: [[0, 0], [30, 0], [30, 20], [18, 20], [18, 12], [12, 12], [12, 20], [0, 20]]
    count: 4
    road_width: 1
    seed_jitter: 0.6
    field:
      gentype: parallel_rows
      row_spacing: 2
      headland: 0.25
      row:
        gentype: linear
        crop_spacing: 1
        crop: {gentype: generic, scale: 0.05, leaf_num: 3}
)";
  const auto first = cropsim::world_from_yaml(yaml);
  const auto second = cropsim::world_from_yaml(yaml);
  CHECK(first == second);
  CHECK(cropsim::serialise_snapshot(first) == cropsim::serialise_snapshot(second));
  CHECK(first.size() > 100U);
  for (const auto &crop : first.crops()) {
    CHECK(crop.x >= 0.0);
    CHECK(crop.x <= 30.0);
    CHECK(crop.y >= 0.0);
    CHECK(crop.y <= 20.0);
  }
}

TEST_CASE("injected field and row generators compose through a field set") {
  auto registry = cropsim::generators::make_builtin_generator_registry();
  registry.fields().register_generator(
      "test_field", [] { return std::make_unique<TestFieldGenerator>(); });
  registry.rows().register_generator(
      "test_row", [] { return std::make_unique<TestRowGenerator>(); });
  const auto world = cropsim::world_from_yaml(R"(
generators:
  - gentype: field_set
    bounds: [[0, 0], [6, 0], [6, 4], [0, 4]]
    count: 1
    road_width: 0
    field:
      gentype: test_field
      row:
        gentype: test_row
        crop: {gentype: fixed, radius: 0.2}
)", registry);
  REQUIRE(world.size() == 2U);
  CHECK(world.crops()[0] == cropsim::Crop{0U, 2.0, 2.0, 0.2});
  CHECK(world.crops()[1] == cropsim::Crop{1U, 4.0, 2.0, 0.2});
}

TEST_CASE("hierarchical morphology is stable when an earlier row grows") {
  auto registry = cropsim::generators::make_builtin_generator_registry();
  registry.fields().register_generator(
      "two_rows", [] { return std::make_unique<TwoRowFieldGenerator>(); });
  registry.rows().register_generator(
      "variable_first",
      [] { return std::make_unique<VariableFirstRowGenerator>(); });
  const auto description_with = [](const std::size_t first_count) {
    return std::string(R"(
seed: 812
generators:
  - gentype: field_set
    bounds: [[0, 0], [10, 0], [10, 5], [0, 5]]
    count: 1
    road_width: 0
    field:
      gentype: two_rows
      row:
        gentype: variable_first
        first_count: )") + std::to_string(first_count) + R"(
        crop:
          gentype: generic
          scale: {mean: 0.1, min: 0.05, max: 0.15}
)";
  };
  const auto short_first =
      cropsim::world_from_yaml(description_with(1U), registry);
  const auto long_first = cropsim::world_from_yaml(description_with(3U), registry);
  REQUIRE(short_first.size() == 2U);
  REQUIRE(long_first.size() == 4U);
  CHECK(short_first.crops().back().x == long_first.crops().back().x);
  CHECK(short_first.crops().back().y == long_first.crops().back().y);
  CHECK(short_first.crops().back().leaves == long_first.crops().back().leaves);
  CHECK(short_first.crops().back().id != long_first.crops().back().id);
}

TEST_CASE("field hierarchy rejects invalid geometry and nested generators") {
  const auto invalid = [](const std::string_view body) {
    CHECK_THROWS(static_cast<void>(cropsim::world_from_yaml(body)));
  };
  invalid(R"(generators: [{gentype: field_set, bounds: [[0,0],[1,1],[0,1],[1,0]], count: 1}])");
  invalid(R"(generators: [{gentype: field_set, bounds: [[0,0],[1,0],[1,1],[0,1]], count: 0}])");
  invalid(R"(generators: [{gentype: field_set, bounds: [[0,0],[10,0],[10,10],[0,10]], count: 1, road_width: -1}])");
  invalid(R"(
generators:
  - gentype: field_set
    bounds: [[0,0],[10,0],[10,10],[0,10]]
    count: 1
    field: {gentype: unknown, row: {gentype: linear, crop: {gentype: fixed, radius: 1}}}
)");
}

TEST_CASE("invalid snapshots are rejected") {
  auto snapshot =
      cropsim::serialise_snapshot(cropsim::world_from_yaml(description));
  snapshot.resize(snapshot.size() - 1U);
  CHECK_THROWS_AS(static_cast<void>(cropsim::deserialise_snapshot(snapshot)),
                  std::runtime_error);
}

TEST_CASE("version 1 snapshots load and upgrade deterministically") {
  const auto world = cropsim::world_from_yaml(description);
  const auto version_one = legacy_snapshot(world, 1U);

  const auto restored = cropsim::deserialise_snapshot(version_one);
  CHECK(restored == world);
  CHECK(cropsim::serialise_snapshot(restored) ==
        cropsim::serialise_snapshot(world));
}

TEST_CASE("version 2 circle snapshots load and upgrade deterministically") {
  const auto world = cropsim::world_from_yaml(description);
  const auto restored =
      cropsim::deserialise_snapshot(legacy_snapshot(world, 2U));
  CHECK(restored == world);
  CHECK(cropsim::deserialise_snapshot(cropsim::serialise_snapshot(restored)) ==
        world);
}

TEST_CASE("spatial queries match brute force and preserve canonical order") {
  const cropsim::World world(7, {{40, -2.0, -1.0, 0.5},
                                 {10, 0.0, 0.0, 2.0},
                                 {30, 1.5, 0.0, 0.5},
                                 {20, 0.0, 0.0, 0.25}});
  const std::vector<cropsim::Aabb> queries{
      {-10, -10, 10, 10},    {-1, -1, 0, 0},       {2, 0, 2, 0},
      {1.5, -0.1, 1.5, 0.1}, {-3, -2, -2.5, -1.5}, {20, 20, 21, 21}};
  for (const auto &query : queries) {
    CHECK(world.query(query) == brute_force_query(world, query));
  }
  CHECK(world.query({-10, -10, 10, 10}) ==
        std::vector<std::size_t>{0, 1, 2, 3});
  CHECK(world.spatial_index() ==
        cropsim::World(7, world.crops()).spatial_index());

  const cropsim::World empty(0, {});
  CHECK(empty.query({0, 0, 1, 1}).empty());
  CHECK(empty.spatial_index().cell_count() == 0U);
}

TEST_CASE("spatial inputs reject invalid geometry and query bounds") {
  CHECK_THROWS_AS(
      cropsim::World(0, {{0, std::numeric_limits<double>::infinity(), 0, 1}}),
      std::invalid_argument);
  CHECK_THROWS(cropsim::World(0, {{0, std::numeric_limits<double>::max(), 0,
                                   std::numeric_limits<double>::max()}}));
  const cropsim::World world(0, {{0, 0, 0, 1}});
  CHECK_THROWS_AS(static_cast<void>(world.query({1, 0, 0, 1})),
                  std::invalid_argument);
  CHECK_THROWS_AS(static_cast<void>(world.query(
                      {0, 0, std::numeric_limits<double>::quiet_NaN(), 1})),
                  std::invalid_argument);
}

TEST_CASE("version 3 rejects altered spatial index data") {
  const cropsim::World world(0, {{0, 0, 0, 0.5}, {1, 2, 2, 0.5}});
  const auto valid = cropsim::serialise_snapshot(world);
  auto crop_bytes = 8U + 4U + 8U + 8U;
  for (const auto &crop : world.crops()) {
    crop_bytes += 32U + crop.leaves.size() * 40U;
  }

  auto metadata = valid;
  metadata[crop_bytes] ^= std::byte{1};
  CHECK_THROWS_AS(static_cast<void>(cropsim::deserialise_snapshot(metadata)),
                  std::runtime_error);

  auto offsets = valid;
  const auto offsets_start = crop_bytes + 40U + 8U + 8U + 8U;
  offsets[offsets_start + 8U] ^= std::byte{1};
  CHECK_THROWS_AS(static_cast<void>(cropsim::deserialise_snapshot(offsets)),
                  std::runtime_error);

  auto trailing = valid;
  trailing.push_back(std::byte{0});
  CHECK_THROWS_AS(static_cast<void>(cropsim::deserialise_snapshot(trailing)),
                  std::runtime_error);
}

TEST_CASE("indexed rendering matches canonical brute-force rendering") {
  const cropsim::World world(
      0,
      {{0, -5, -5, 1}, {1, 0.5, 0.5, 0.3}, {2, 1.0, 0.1, 0.4}, {3, 8, 8, 1}});
  const auto indexed = cropsim::render(world, {0, 0, 1, 1}, 31, 29);
  cropsim::GrayscaleImage brute{31, 29,
                                std::vector<std::uint8_t>(31U * 29U, 0U)};
  for (const auto &crop : world.crops()) {
    for (std::size_t row = 0; row < brute.height; ++row) {
      const auto y = 1.0 - (static_cast<double>(row) + 0.5) / 29.0;
      for (std::size_t column = 0; column < brute.width; ++column) {
        const auto x = (static_cast<double>(column) + 0.5) / 31.0;
        if (std::any_of(crop.leaves.begin(), crop.leaves.end(),
                        [&](const cropsim::EllipseLeaf &leaf) {
                          return cropsim::leaf_contains(crop, leaf, x, y);
                        })) {
          brute.pixels[row * brute.width + column] = 255U;
        }
      }
    }
  }
  CHECK(indexed == brute);
}

TEST_CASE("headless rendering is deterministic and uses world-up orientation") {
  const cropsim::World world(0, {{0, 0.25, 0.75, 0.2}});
  const auto first = cropsim::render(world, {0.0, 0.0, 1.0, 1.0}, 4, 4);
  const auto second = cropsim::render(world, {0.0, 0.0, 1.0, 1.0}, 4, 4);

  CHECK(first == second);
  CHECK(first.pixels[1] == 255U);
  CHECK(first.pixels[13] == 0U);
}

TEST_CASE("ellipse transforms use crop-relative positions and rotation") {
  const cropsim::Crop crop(0U, 10.0, 20.0,
                           {{1.0, -2.0, 2.0, 0.5, std::acos(-1.0) * 0.5}});
  const auto &leaf = crop.leaves.front();
  CHECK(cropsim::leaf_contains(crop, leaf, 11.0, 19.9));
  CHECK_FALSE(cropsim::leaf_contains(crop, leaf, 12.0, 18.0));
  CHECK(cropsim::leaf_contains(crop, leaf, 11.49, 18.0));
}

TEST_CASE("explicit and generated multi-leaf crops are deterministic") {
  constexpr std::string_view multi_leaf = R"(
seed: 77
crops:
  - position: [1, 2]
    leaves:
      - {type: ellipse, position: [0.2, 0], radii: [0.4, 0.1], rotation: 0.3}
      - {type: ellipse, pos: [-0.2, 0], rx: 0.3, ry: 0.08, theta: -0.4}
generators:
  - gentype: grid
    origin: [3, 4]
    rows: 1
    columns: 2
    spacing: [1, 1]
    leaves:
      - {position: [0, 0.15], radii: [0.2, 0.05], rotation: 1.2}
      - {position: [0, -0.15], radii: [0.2, 0.05], rotation: -1.2}
)";
  const auto first = cropsim::world_from_yaml(multi_leaf);
  const auto second = cropsim::world_from_yaml(multi_leaf);
  CHECK(first == second);
  REQUIRE(first.size() == 3U);
  CHECK(first.crops()[0].leaves.size() == 2U);
  CHECK(first.crops()[1].leaves == first.crops()[2].leaves);
  CHECK(first.crops()[0].leaves[0].x == 0.2);
  CHECK(first.crops()[0].leaves[1].rotation == -0.4);
}

TEST_CASE("multi-leaf snapshots preserve exact geometry and ordering") {
  const cropsim::World world(5U,
                             {cropsim::Crop(9U, 2.0, 3.0,
                                            {{0.1, 0.2, 0.3, 0.4, 0.5},
                                             {-0.6, -0.7, 0.8, 0.9, -1.0}})});
  const auto snapshot = cropsim::serialise_snapshot(world);
  const auto restored = cropsim::deserialise_snapshot(snapshot);
  CHECK(restored == world);
  CHECK(cropsim::serialise_snapshot(restored) == snapshot);
}

TEST_CASE("canonical rendering is the binary union of overlapping leaves") {
  const cropsim::World one(
      0U, {cropsim::Crop(0U, 0.5, 0.5, {{0, 0, 0.35, 0.12, 0.4}})});
  const cropsim::World overlap(
      0U, {cropsim::Crop(0U, 0.5, 0.5,
                         {{0, 0, 0.35, 0.12, 0.4}, {0, 0, 0.35, 0.12, 0.4}})});
  CHECK(cropsim::render(one, {0, 0, 1, 1}, 51, 51) ==
        cropsim::render(overlap, {0, 0, 1, 1}, 51, 51));
}

TEST_CASE("spatial extents contain every rotated and offset leaf") {
  const auto angle = std::acos(-1.0) * 0.25;
  const cropsim::World world(0U,
                             {cropsim::Crop(0U, 10.0, 20.0,
                                            {{-3.0, 1.0, 2.0, 0.5, angle},
                                             {4.0, -2.0, 0.25, 1.5, 0.0}})});
  const auto bounds = cropsim::crop_bounds(world.crops().front());
  CHECK(bounds.min_x < 6.0);
  CHECK(bounds.max_x == doctest::Approx(14.25));
  CHECK(bounds.min_y == doctest::Approx(16.5));
  CHECK(world.query({14.2, 17.9, 14.3, 18.1}) == std::vector<std::size_t>{0U});
  CHECK(world.query({5.5, 20.5, 6.5, 21.5}) == std::vector<std::size_t>{0U});
}

TEST_CASE("empty or invalid leaf collections are rejected") {
  CHECK_THROWS_AS(
      cropsim::World(
          0U, {cropsim::Crop(0U, 0, 0, std::vector<cropsim::EllipseLeaf>{})}),
      std::invalid_argument);
  CHECK_THROWS_AS(
      cropsim::World(0U, {cropsim::Crop(0U, 0, 0, {{0, 0, -1, 1, 0}})}),
      std::invalid_argument);
}
