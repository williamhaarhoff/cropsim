#pragma once

#include "cropsim/renderer.hpp"

#include <cstdio>
#include <filesystem>
#include <iosfwd>
#include <string_view>

namespace cropsim::testing {

[[nodiscard]] bool terminal_supports_ansi(std::FILE* stream) noexcept;

// Returns true without output for equal images. On mismatch, emits previews and
// writes <name>_{expected,actual,difference}.pgm into artifact_directory.
[[nodiscard]] bool compare_images(const GrayscaleImage& expected,
                                  const GrayscaleImage& actual,
                                  std::string_view name,
                                  const std::filesystem::path& artifact_directory,
                                  std::ostream& diagnostics, bool use_ansi);

}  // namespace cropsim::testing
