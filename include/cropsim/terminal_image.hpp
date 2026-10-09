#pragma once

#include "cropsim/renderer.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace cropsim {

enum class TerminalImageMode { ascii, ansi_truecolor };

struct TerminalImageOptions {
    TerminalImageMode mode{TerminalImageMode::ascii};
    std::size_t max_columns{80U};
    std::size_t max_rows{40U};
};

[[nodiscard]] std::string format_terminal_image(
    const GrayscaleImage& image, const TerminalImageOptions& options = {});

}  // namespace cropsim
