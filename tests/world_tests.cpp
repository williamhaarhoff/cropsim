#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "cropsim/generators/generator_factory.hpp"
#include "cropsim/generators/grid_generator.hpp"
#include "cropsim/renderer.hpp"
#include "cropsim/snapshot.hpp"
#include "cropsim/world.hpp"

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

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

std::uint64_t snapshot_hash(const std::vector<std::byte>& bytes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto byte : bytes) {
        hash ^= std::to_integer<std::uint8_t>(byte);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::vector<std::size_t> brute_force_query(const cropsim::World& world,
                                           const cropsim::Aabb& bounds) {
    std::vector<std::size_t> result;
    for (std::size_t index = 0; index < world.size(); ++index) {
        const auto& crop = world.crops()[index];
        if (crop.x - crop.radius <= bounds.max_x && crop.x + crop.radius >= bounds.min_x &&
            crop.y - crop.radius <= bounds.max_y && crop.y + crop.radius >= bounds.min_y) {
            result.push_back(index);
        }
    }
    return result;
}

class TestGenerator final : public cropsim::generators::Generator {
public:
    void generate(const YAML::Node&, cropsim::generators::GenerationContext& context,
                  std::vector<cropsim::Crop>& destination) const override {
        destination.push_back(context.make_crop(context.symmetric_unit(), 4.0, 0.2));
    }
};

}  // namespace

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

TEST_CASE("version 2 snapshots have deterministic bytes") {
    const auto first = cropsim::serialise_snapshot(cropsim::world_from_yaml(description));
    const auto second = cropsim::serialise_snapshot(cropsim::world_from_yaml(description));
    CHECK(first == second);
    CHECK(first.size() == 508U);
    CHECK(snapshot_hash(first) == 8900297656637660529ULL);
}

TEST_CASE("generator factory dispatch and validation") {
    auto factory = cropsim::generators::make_builtin_generator_factory();
    CHECK(dynamic_cast<cropsim::generators::GridGenerator*>(factory.create("grid").get()) !=
          nullptr);
    CHECK_THROWS_AS(static_cast<void>(factory.create("unknown")), std::invalid_argument);
    CHECK_THROWS_AS(factory.register_generator(
                        "grid", [] { return std::make_unique<TestGenerator>(); }),
                    std::invalid_argument);

    CHECK_THROWS_AS(static_cast<void>(cropsim::world_from_yaml("generators: [{rows: 1}]")),
                    std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(
                        cropsim::world_from_yaml("generators: [{gentype: unknown}]")),
                    std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(cropsim::world_from_yaml("generators: [{type: grid}]")),
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
        const auto yaml = std::string("generators: [{gentype: grid, origin: [0, 0], spacing: "
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
    cropsim::generators::GeneratorFactory factory;
    factory.register_generator("test", [] { return std::make_unique<TestGenerator>(); });
    const auto first = cropsim::world_from_yaml(
        "seed: 99\ncrops: [{position: [1, 2], radius: 0.1}]\n"
        "generators: [{gentype: test}]\n",
        factory);
    const auto second = cropsim::world_from_yaml(
        "seed: 99\ncrops: [{position: [1, 2], radius: 0.1}]\n"
        "generators: [{gentype: test}]\n",
        factory);
    CHECK(first == second);
    REQUIRE(first.size() == 2U);
    CHECK(first.crops()[0].id == 0U);
    CHECK(first.crops()[1].id == 1U);
    CHECK(first.crops()[1].y == 4.0);
}

TEST_CASE("invalid snapshots are rejected") {
    auto snapshot = cropsim::serialise_snapshot(cropsim::world_from_yaml(description));
    snapshot.resize(snapshot.size() - 1U);
    CHECK_THROWS_AS(static_cast<void>(cropsim::deserialise_snapshot(snapshot)),
                    std::runtime_error);
}

TEST_CASE("version 1 snapshots load and upgrade deterministically") {
    const auto world = cropsim::world_from_yaml(description);
    auto version_one = cropsim::serialise_snapshot(world);
    version_one.resize(8U + 4U + 8U + 8U + world.size() * 32U);
    version_one[8U] = std::byte{1};

    const auto restored = cropsim::deserialise_snapshot(version_one);
    CHECK(restored == world);
    CHECK(cropsim::serialise_snapshot(restored) == cropsim::serialise_snapshot(world));
}

TEST_CASE("spatial queries match brute force and preserve canonical order") {
    const cropsim::World world(7, {{40, -2.0, -1.0, 0.5},
                                   {10, 0.0, 0.0, 2.0},
                                   {30, 1.5, 0.0, 0.5},
                                   {20, 0.0, 0.0, 0.25}});
    const std::vector<cropsim::Aabb> queries{{-10, -10, 10, 10},
                                             {-1, -1, 0, 0},
                                             {2, 0, 2, 0},
                                             {1.5, -0.1, 1.5, 0.1},
                                             {-3, -2, -2.5, -1.5},
                                             {20, 20, 21, 21}};
    for (const auto& query : queries) {
        CHECK(world.query(query) == brute_force_query(world, query));
    }
    CHECK(world.query({-10, -10, 10, 10}) == std::vector<std::size_t>{0, 1, 2, 3});
    CHECK(world.spatial_index() == cropsim::World(7, world.crops()).spatial_index());

    const cropsim::World empty(0, {});
    CHECK(empty.query({0, 0, 1, 1}).empty());
    CHECK(empty.spatial_index().cell_count() == 0U);
}

TEST_CASE("spatial inputs reject invalid geometry and query bounds") {
    CHECK_THROWS_AS(cropsim::World(0, {{0, std::numeric_limits<double>::infinity(), 0, 1}}),
                    std::invalid_argument);
    CHECK_THROWS(cropsim::World(
        0, {{0, std::numeric_limits<double>::max(), 0,
             std::numeric_limits<double>::max()}}));
    const cropsim::World world(0, {{0, 0, 0, 1}});
    CHECK_THROWS_AS(static_cast<void>(world.query({1, 0, 0, 1})), std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(world.query(
                        {0, 0, std::numeric_limits<double>::quiet_NaN(), 1})),
                    std::invalid_argument);
}

TEST_CASE("version 2 rejects altered spatial index data") {
    const cropsim::World world(0, {{0, 0, 0, 0.5}, {1, 2, 2, 0.5}});
    const auto valid = cropsim::serialise_snapshot(world);
    const auto crop_bytes = 8U + 4U + 8U + 8U + world.size() * 32U;

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
    const cropsim::World world(0, {{0, -5, -5, 1}, {1, 0.5, 0.5, 0.3},
                                   {2, 1.0, 0.1, 0.4}, {3, 8, 8, 1}});
    const auto indexed = cropsim::render(world, {0, 0, 1, 1}, 31, 29);
    cropsim::GrayscaleImage brute{31, 29, std::vector<std::uint8_t>(31U * 29U, 0U)};
    for (const auto& crop : world.crops()) {
        for (std::size_t row = 0; row < brute.height; ++row) {
            const auto y = 1.0 - (static_cast<double>(row) + 0.5) / 29.0;
            for (std::size_t column = 0; column < brute.width; ++column) {
                const auto x = (static_cast<double>(column) + 0.5) / 31.0;
                const auto dx = x - crop.x;
                const auto dy = y - crop.y;
                if (dx * dx + dy * dy <= crop.radius * crop.radius) {
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
