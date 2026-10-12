#include <doctest/doctest.h>
#include <yaml-cpp/yaml.h>

#include "cropsim/generators/landscape.hpp"
#include "cropsim/world.hpp"

TEST_CASE("farm landscape layout is deterministic and transient") {
  const auto node = YAML::Load(R"(
gentype: farm_landscape
bounds: [[0, 0], [80, 0], [80, 60], [0, 60]]
count: 6
terrain: {relief: 8, scale: 250}
infrastructure: {lane_width: 4, track_width: 2.5, curvature: 0.35}
parcels: {area_variation: 0.6, boundary_irregularity: 0.15}
field: {gentype: parallel_rows, headland: 0.5,
  row: {gentype: linear, crop_spacing: 2,
    crop: {gentype: fixed, radius: 0.05}}}
)");
  const cropsim::generators::GenerationKey key{12345U};
  const auto first = cropsim::generators::generate_landscape_layout(node, key);
  const auto second = cropsim::generators::generate_landscape_layout(node, key);
  REQUIRE(first.fields.size() == 6U);
  REQUIRE(first.corridors.front().type == cropsim::generators::CorridorType::lane);
  CHECK(first.corridors.front().width == doctest::Approx(4.0));
  CHECK(first.access.size() == first.fields.size());
  CHECK(cropsim::generators::landscape_layout_svg(first, 320U, 240U) ==
        cropsim::generators::landscape_layout_svg(second, 320U, 240U));
  CHECK(cropsim::generators::render_landscape_layout(first, 64U, 48U) ==
        cropsim::generators::render_landscape_layout(second, 64U, 48U));
}

TEST_CASE("one-field landscape has no through lane and validates parameters") {
  auto node = YAML::Load(R"(
bounds: [[0, 0], [20, 0], [20, 10], [0, 10]]
count: 1
)");
  const auto layout = cropsim::generators::generate_landscape_layout(
      node, cropsim::generators::GenerationKey{7U});
  CHECK(layout.fields.size() == 1U);
  CHECK(layout.corridors.empty());
  node["infrastructure"] = YAML::Load("{curvature: 2.0}");
  const auto generate_invalid = [&] {
    (void)cropsim::generators::generate_landscape_layout(
        node, cropsim::generators::GenerationKey{7U});
  };
  CHECK_THROWS_AS(generate_invalid(), std::invalid_argument);
}

TEST_CASE("farm landscape dispatches through world generation") {
  const auto world = cropsim::world_from_yaml(R"(
seed: 42
generators:
  - gentype: farm_landscape
    bounds: [[0, 0], [20, 0], [20, 12], [0, 12]]
    count: 1
    field:
      gentype: parallel_rows
      headland: 0.2
      row:
        gentype: linear
        crop_spacing: 2
        crop: {gentype: fixed, radius: 0.05}
)");
  CHECK(world.size() > 0U);
}
