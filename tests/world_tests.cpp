#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "cropsim/generators/generator_factory.hpp"
#include "cropsim/generators/grid_generator.hpp"
#include "cropsim/renderer.hpp"
#include "cropsim/snapshot.hpp"
#include "cropsim/world.hpp"

#include <cstddef>
#include <cstdint>
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

TEST_CASE("refactored generation preserves the original snapshot bytes") {
    const auto snapshot = cropsim::serialise_snapshot(cropsim::world_from_yaml(description));
    CHECK(snapshot.size() == 252U);
    CHECK(snapshot_hash(snapshot) == 5869903418197071496ULL);
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

TEST_CASE("headless rendering is deterministic and uses world-up orientation") {
    const cropsim::World world(0, {{0, 0.25, 0.75, 0.2}});
    const auto first = cropsim::render(world, {0.0, 0.0, 1.0, 1.0}, 4, 4);
    const auto second = cropsim::render(world, {0.0, 0.0, 1.0, 1.0}, 4, 4);

    CHECK(first == second);
    CHECK(first.pixels[1] == 255U);
    CHECK(first.pixels[13] == 0U);
}
