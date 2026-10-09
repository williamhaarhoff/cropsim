#include <doctest/doctest.h>

#include <fstream>
#include <stdexcept>

#include "cli.hpp"
#include "cropsim/snapshot.hpp"

TEST_CASE("viewer requires exactly one input") {
    const char* none[] = {"cropsim_viewer"};
    CHECK_THROWS_AS(static_cast<void>(cropsim::viewer::parse_options(1, none)),
                    std::invalid_argument);
    const char* both[] = {"cropsim_viewer", "--yaml", "a", "--snapshot", "b"};
    CHECK_THROWS_AS(static_cast<void>(cropsim::viewer::parse_options(5, both)),
                    std::invalid_argument);
}

TEST_CASE("viewer selects occupancy and diagnostic leaf modes") {
    const char* occupancy[] = {"cropsim_viewer", "world.yaml"};
    CHECK(cropsim::viewer::parse_options(2, occupancy).view_mode ==
          cropsim::viewer::ViewMode::occupancy);
    const char* diagnostic[] = {"cropsim_viewer", "world.yaml", "--diagnostic-colors"};
    CHECK(cropsim::viewer::parse_options(3, diagnostic).view_mode ==
          cropsim::viewer::ViewMode::diagnostic_leaves);
}

TEST_CASE("viewer loads snapshots without a display") {
    const auto path = std::filesystem::path(CROPSIM_VIEWER_TEST_DIR) / "viewer-world.snapshot";
    std::filesystem::create_directories(path.parent_path());
    cropsim::save_snapshot(cropsim::World(9U, {{0U, 2.0, 3.0, 0.25}}), path);
    const auto path_string = path.string();
    const char* arguments[] = {"cropsim_viewer", "--snapshot", path_string.c_str(), "--check"};
    const auto world = cropsim::viewer::load_world(cropsim::viewer::parse_options(4, arguments));
    CHECK(world.seed() == 9U);
    CHECK(world.size() == 1U);
}

TEST_CASE("viewer automatically detects snapshot input") {
    const auto path = std::filesystem::path(CROPSIM_VIEWER_TEST_DIR) / "automatic-world.data";
    std::filesystem::create_directories(path.parent_path());
    cropsim::save_snapshot(cropsim::World(11U, {{0U, 2.0, 3.0, 0.25}}), path);
    const auto path_string = path.string();
    const char* arguments[] = {"cropsim_viewer", path_string.c_str(), "--check"};
    const auto options = cropsim::viewer::parse_options(3, arguments);
    CHECK(options.input_kind == cropsim::viewer::InputKind::automatic);
    CHECK(cropsim::viewer::load_world(options).seed() == 11U);
}

TEST_CASE("viewer parses and loads YAML without a display") {
    const auto path = std::filesystem::path(CROPSIM_VIEWER_TEST_DIR) / "viewer-world.yaml";
    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream stream(path);
        stream << "seed: 3\ncrops: [{position: [1, 2], radius: 0.5}]\n";
    }
    const auto path_string = path.string();
    const char* arguments[] = {"cropsim_viewer", "--yaml", path_string.c_str(), "--check"};
    const auto options = cropsim::viewer::parse_options(4, arguments);
    CHECK(options.check_only);
    const auto world = cropsim::viewer::load_world(options);
    CHECK(world.size() == 1U);
    CHECK(world.seed() == 3U);
}

TEST_CASE("viewer automatically detects YAML and rejects invalid input") {
    const auto directory = std::filesystem::path(CROPSIM_VIEWER_TEST_DIR);
    std::filesystem::create_directories(directory);
    const auto yaml_path = directory / "automatic-world.data";
    {
        std::ofstream stream(yaml_path);
        stream << "seed: 13\ncrops: []\n";
    }
    const auto yaml_path_string = yaml_path.string();
    const char* yaml_arguments[] = {"cropsim_viewer", yaml_path_string.c_str(), "--check"};
    CHECK(cropsim::viewer::load_world(cropsim::viewer::parse_options(3, yaml_arguments)).seed() ==
          13U);

    const auto invalid_path = directory / "invalid-world.data";
    {
        std::ofstream stream(invalid_path, std::ios::binary);
        stream << "not a world";
    }
    const auto invalid_path_string = invalid_path.string();
    const char* invalid_arguments[] = {"cropsim_viewer", invalid_path_string.c_str(), "--check"};
    CHECK_THROWS_AS(static_cast<void>(cropsim::viewer::load_world(
                        cropsim::viewer::parse_options(3, invalid_arguments))),
                    std::runtime_error);
}
