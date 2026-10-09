#pragma once

#include "cropsim/world.hpp"

#include <filesystem>

namespace cropsim::viewer {

enum class InputKind { automatic, yaml, snapshot };

struct Options {
    InputKind input_kind{InputKind::automatic};
    std::filesystem::path input_path;
    bool check_only{};
    bool smoke_test{};
};

[[nodiscard]] Options parse_options(int argc, const char* const* argv);
[[nodiscard]] World load_world(const Options& options);

}  // namespace cropsim::viewer
