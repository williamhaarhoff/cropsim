#pragma once

#include <filesystem>

#include "cropsim/world.hpp"

namespace cropsim::viewer {

enum class InputKind { automatic, yaml, snapshot };
enum class ViewMode { occupancy, diagnostic_leaves };

struct Options {
    InputKind input_kind{InputKind::automatic};
    std::filesystem::path input_path;
    bool check_only{};
    bool smoke_test{};
    bool show_rails{};
    bool show_crop_lines{};
    ViewMode view_mode{ViewMode::occupancy};
};

[[nodiscard]] Options parse_options(int argc, const char* const* argv);
[[nodiscard]] World load_world(const Options& options);

} // namespace cropsim::viewer
