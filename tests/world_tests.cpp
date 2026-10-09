#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "cropsim/renderer.hpp"
#include "cropsim/snapshot.hpp"
#include "cropsim/world.hpp"

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace {

constexpr auto description = R"(
seed: 568616
crops:
  - position: [0.25, 0.75]
    radius: 0.1
generators:
  - type: grid
    origin: [1.0, 2.0]
    rows: 2
    columns: 3
    spacing: [0.5, 0.25]
    radius: 0.08
    jitter: 0.01
)";

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
