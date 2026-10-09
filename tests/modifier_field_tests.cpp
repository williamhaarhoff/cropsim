#include <doctest/doctest.h>
#include <yaml-cpp/yaml.h>
#include "cropsim/modifiers/modifier_fields.hpp"
#include "cropsim/snapshot.hpp"
#include "cropsim/world.hpp"

using namespace cropsim::generators;

namespace {
const Polygon2 square{{{0,0},{10,0},{10,10},{0,10}}};
ModifierFieldSet compile(std::string_view yaml) {
  return ModifierFieldSet::compile(YAML::Load(std::string(yaml)), square, 17,
                                   {91}, make_builtin_modifier_field_factory());
}
}

TEST_CASE("modifier expressions resolve forward references and precedence") {
  auto fields=compile(R"yaml(
z: {gentype: expression, expression: "clamp(a + 2 * x, 0, 30)"}
a: {gentype: expression, expression: "sin(pi / 2) + -v"}
)yaml");
  std::vector<double> values;
  fields.evaluate({3,5},values);
  CHECK(values[fields.resolve("a")] == doctest::Approx(0.5));
  CHECK(values[fields.resolve("z")] == doctest::Approx(6.5));
}

TEST_CASE("modifier dependency and expression errors are rejected") {
  CHECK_THROWS(compile("a: {gentype: expression, expression: b}\nb: {gentype: expression, expression: a}"));
  CHECK_THROWS(compile("a: {gentype: expression, expression: missing}"));
  CHECK_THROWS(compile("x: {gentype: expression, expression: 1}"));
  CHECK_THROWS(compile("a: {gentype: expression, expression: 'unknown(1)'}"));
}

TEST_CASE("modifier preview masks the domain and maps constant fields to mid-gray") {
  const Polygon2 triangle{{{0,0},{10,0},{0,10}}};
  auto fields=ModifierFieldSet::compile(YAML::Load("constant: {gentype: expression, expression: 4}"),triangle,0,{0},make_builtin_modifier_field_factory());
  const auto image=render_modifier_field(fields,fields.resolve("constant"),2,2);
  CHECK(image.pixels == std::vector<std::uint8_t>{0,0,128,0});
}

TEST_CASE("spatial bindings correlate scale and leaf count without persistence") {
  const auto yaml=R"(
seed: 5
generators:
  - gentype: field_set
    bounds: [[0,0],[10,0],[10,4],[0,4]]
    count: 1
    road_width: 0
    modifier_fields:
      vigor: {gentype: expression, expression: "u"}
    field:
      gentype: parallel_rows
      orientation: 0
      row_spacing: 4
      headland: 0
      row:
        gentype: linear
        crop_spacing: 2
        crop:
          gentype: generic
          scale: {base: 1, modifiers: [{field: vigor, operation: relative, strength: 1}]}
          leaf_num: {base: 2, modifiers: [{field: vigor, operation: add, strength: 4}], clamp: [1, 8]}
          leaf_length: 1
          leaf_width: 0.2
          leaf_offset: 0
          leaf_orientation: 0
)";
  const auto world=cropsim::world_from_yaml(yaml);
  REQUIRE(world.size()==5);
  CHECK(world.crops().front().leaves.size()==2);
  CHECK(world.crops().back().leaves.size()==6);
  CHECK(world.crops().front().leaves.front().radius_x < world.crops().back().leaves.front().radius_x);
  CHECK(cropsim::serialise_snapshot(world)==cropsim::serialise_snapshot(cropsim::world_from_yaml(yaml)));
}

TEST_CASE("distribution bounds clamp the final spatially modified value") {
  const auto world=cropsim::world_from_yaml(R"(
seed: 77
generators:
  - gentype: field_set
    bounds: [[0,0],[6,0],[6,2],[0,2]]
    count: 1
    road_width: 0
    modifier_fields:
      high: {gentype: expression, expression: "10"}
    field:
      gentype: parallel_rows
      orientation: 0
      row_spacing: 2
      headland: 0
      row:
        gentype: linear
        crop_spacing: 2
        crop:
          gentype: generic
          scale:
            mean: 0.06
            min: 0.04
            max: 0.10
            modifiers: [{field: high, operation: add, strength: 1}]
          leaf_num: 1
          leaf_length: 1
          leaf_width: 1
          leaf_offset: 0
          leaf_orientation: 0
)");
  REQUIRE(world.size()==3);
  for(const auto &crop:world.crops()) {
    CHECK(crop.leaves.front().radius_x==doctest::Approx(0.10));
  }
}
