#include "cropsim/testing/image_assertion.hpp"

#include "cropsim/terminal_image.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

namespace cropsim::testing {

bool terminal_supports_ansi(std::FILE* stream) noexcept {
    if (stream == nullptr || std::getenv("NO_COLOR") != nullptr) {
        return false;
    }
#if defined(__unix__) || defined(__APPLE__)
    return ::isatty(::fileno(stream)) != 0;
#else
    return false;
#endif
}

bool compare_images(const GrayscaleImage& expected, const GrayscaleImage& actual,
                    const std::string_view name,
                    const std::filesystem::path& artifact_directory,
                    std::ostream& diagnostics, const bool use_ansi) {
    if (expected == actual) {
        return true;
    }
    if (expected.width == 0U || expected.height == 0U ||
        expected.pixels.size() != expected.width * expected.height || actual.width == 0U ||
        actual.height == 0U || actual.pixels.size() != actual.width * actual.height) {
        throw std::invalid_argument("cannot compare invalid grayscale images");
    }

    const auto difference_width = std::max(expected.width, actual.width);
    const auto difference_height = std::max(expected.height, actual.height);
    GrayscaleImage difference{difference_width, difference_height,
                              std::vector<std::uint8_t>(difference_width * difference_height,
                                                        255U)};
    for (std::size_t y = 0; y < std::min(expected.height, actual.height); ++y) {
        for (std::size_t x = 0; x < std::min(expected.width, actual.width); ++x) {
            const auto expected_value = expected.pixels[y * expected.width + x];
            const auto actual_value = actual.pixels[y * actual.width + x];
            difference.pixels[y * difference_width + x] =
                static_cast<std::uint8_t>(std::max(expected_value, actual_value) -
                                          std::min(expected_value, actual_value));
        }
    }

    const TerminalImageOptions options{use_ansi ? TerminalImageMode::ansi_truecolor
                                                : TerminalImageMode::ascii,
                                       80U, 30U};
    diagnostics << "image comparison failed: " << name << "\nexpected:\n"
                << format_terminal_image(expected, options) << "actual:\n"
                << format_terminal_image(actual, options) << "difference:\n"
                << format_terminal_image(difference, options);

    std::filesystem::create_directories(artifact_directory);
    const auto prefix = std::string(name);
    write_pgm(expected, artifact_directory / (prefix + "_expected.pgm"));
    write_pgm(actual, artifact_directory / (prefix + "_actual.pgm"));
    write_pgm(difference, artifact_directory / (prefix + "_difference.pgm"));
    return false;
}

}  // namespace cropsim::testing
