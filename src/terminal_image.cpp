#include "cropsim/terminal_image.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace cropsim {
namespace {

void validate(const GrayscaleImage& image, const TerminalImageOptions& options) {
    if (image.width == 0U || image.height == 0U || options.max_columns == 0U ||
        options.max_rows == 0U ||
        image.width > std::numeric_limits<std::size_t>::max() / image.height ||
        image.pixels.size() != image.width * image.height) {
        throw std::invalid_argument("invalid terminal image or output limits");
    }
}

std::vector<std::uint8_t> resize_area(const GrayscaleImage& image, const std::size_t width,
                                      const std::size_t height) {
    std::vector<std::uint8_t> output(width * height);
    for (std::size_t y = 0; y < height; ++y) {
        const auto source_y_begin = y * image.height / height;
        const auto source_y_end = std::max(source_y_begin + 1U, (y + 1U) * image.height / height);
        for (std::size_t x = 0; x < width; ++x) {
            const auto source_x_begin = x * image.width / width;
            const auto source_x_end =
                std::max(source_x_begin + 1U, (x + 1U) * image.width / width);
            std::uint64_t sum{};
            std::size_t count{};
            for (auto source_y = source_y_begin; source_y < source_y_end; ++source_y) {
                for (auto source_x = source_x_begin; source_x < source_x_end; ++source_x) {
                    sum += image.pixels[source_y * image.width + source_x];
                    ++count;
                }
            }
            output[y * width + x] = static_cast<std::uint8_t>(sum / count);
        }
    }
    return output;
}

}  // namespace

std::string format_terminal_image(const GrayscaleImage& image,
                                  const TerminalImageOptions& options) {
    validate(image, options);
    const auto pixel_row_limit =
        options.mode == TerminalImageMode::ansi_truecolor
            ? (options.max_rows > std::numeric_limits<std::size_t>::max() / 2U
                   ? std::numeric_limits<std::size_t>::max()
                   : options.max_rows * 2U)
            : options.max_rows;
    const auto scale = std::min(
        {1.0, static_cast<double>(options.max_columns) / static_cast<double>(image.width),
         static_cast<double>(pixel_row_limit) / static_cast<double>(image.height)});
    const auto width = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::floor(static_cast<double>(image.width) * scale)));
    const auto height = std::max<std::size_t>(
        1U, static_cast<std::size_t>(std::floor(static_cast<double>(image.height) * scale)));
    const auto pixels = resize_area(image, width, height);

    std::ostringstream output;
    if (options.mode == TerminalImageMode::ascii) {
        constexpr std::string_view palette = " .:-=+*#%@";
        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                const auto value = pixels[y * width + x];
                const auto index = static_cast<std::size_t>(value) * (palette.size() - 1U) / 255U;
                output << palette[index];
            }
            output << '\n';
        }
        return output.str();
    }

    for (std::size_t y = 0; y < height; y += 2U) {
        for (std::size_t x = 0; x < width; ++x) {
            const auto upper = pixels[y * width + x];
            const auto lower = y + 1U < height ? pixels[(y + 1U) * width + x] : 0U;
            output << "\x1b[38;2;" << static_cast<unsigned int>(upper) << ';'
                   << static_cast<unsigned int>(upper) << ';' << static_cast<unsigned int>(upper)
                   << "m\x1b[48;2;" << static_cast<unsigned int>(lower) << ';'
                   << static_cast<unsigned int>(lower) << ';' << static_cast<unsigned int>(lower)
                   << "m▀";
        }
        output << "\x1b[0m\n";
    }
    return output.str();
}

}  // namespace cropsim
