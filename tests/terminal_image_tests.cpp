#include <doctest/doctest.h>

#include "cropsim/terminal_image.hpp"
#include "cropsim/testing/image_assertion.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

TEST_CASE("ASCII terminal image formatting is stable") {
    const cropsim::GrayscaleImage image{2U, 2U, {0U, 85U, 170U, 255U}};
    CHECK(cropsim::format_terminal_image(image) == " -\n*@\n");
}

TEST_CASE("ANSI formatter uses truecolor half blocks and handles odd heights") {
    const cropsim::GrayscaleImage image{1U, 3U, {10U, 20U, 30U}};
    const cropsim::TerminalImageOptions options{cropsim::TerminalImageMode::ansi_truecolor,
                                                10U, 10U};
    CHECK(cropsim::format_terminal_image(image, options) ==
          "\x1b[38;2;10;10;10m\x1b[48;2;20;20;20m▀\x1b[0m\n"
          "\x1b[38;2;30;30;30m\x1b[48;2;0;0;0m▀\x1b[0m\n");
}

TEST_CASE("terminal formatter downsamples within row and column limits") {
    const cropsim::GrayscaleImage image{4U, 2U,
                                        {0U, 20U, 100U, 120U, 40U, 60U, 140U, 160U}};
    const cropsim::TerminalImageOptions options{cropsim::TerminalImageMode::ascii, 2U, 1U};
    CHECK(cropsim::format_terminal_image(image, options) == ".=\n");

    const auto tall = cropsim::format_terminal_image(
        cropsim::GrayscaleImage{1U, 20U, std::vector<std::uint8_t>(20U, 255U)},
        {cropsim::TerminalImageMode::ansi_truecolor, 5U, 2U});
    CHECK(std::count(tall.begin(), tall.end(), '\n') == 2);
}

TEST_CASE("terminal formatter rejects empty, malformed, and zero-limit images") {
    CHECK_THROWS_AS(static_cast<void>(cropsim::format_terminal_image({})),
                    std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(cropsim::format_terminal_image({2U, 2U, {0U}})),
                    std::invalid_argument);
    CHECK_THROWS_AS(static_cast<void>(cropsim::format_terminal_image(
                        {1U, 1U, {0U}}, {cropsim::TerminalImageMode::ascii, 0U, 1U})),
                    std::invalid_argument);
}

TEST_CASE("image comparison is silent on success") {
    const cropsim::GrayscaleImage image{2U, 1U, {0U, 255U}};
    std::ostringstream diagnostics;
    CHECK(cropsim::testing::compare_images(image, image, "equal", CROPSIM_TEST_ARTIFACT_DIR,
                                           diagnostics, false));
    CHECK(diagnostics.str().empty());
}

TEST_CASE("failed image comparison prints previews and writes PGM artifacts") {
    const cropsim::GrayscaleImage expected{2U, 1U, {0U, 255U}};
    const cropsim::GrayscaleImage actual{2U, 1U, {255U, 255U}};
    std::ostringstream diagnostics;
    CHECK_FALSE(cropsim::testing::compare_images(expected, actual, "mismatch",
                                                 CROPSIM_TEST_ARTIFACT_DIR, diagnostics, false));
    CHECK(diagnostics.str().find("expected:") != std::string::npos);
    CHECK(diagnostics.str().find("actual:") != std::string::npos);
    CHECK(diagnostics.str().find("difference:") != std::string::npos);
    const auto directory = std::filesystem::path(CROPSIM_TEST_ARTIFACT_DIR);
    CHECK(std::filesystem::file_size(directory / "mismatch_expected.pgm") > 0U);
    CHECK(std::filesystem::file_size(directory / "mismatch_actual.pgm") > 0U);
    CHECK(std::filesystem::file_size(directory / "mismatch_difference.pgm") > 0U);
}
