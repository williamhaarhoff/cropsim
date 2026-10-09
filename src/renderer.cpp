#include "cropsim/renderer.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace cropsim {

bool GrayscaleImage::operator==(const GrayscaleImage& other) const noexcept {
    return width == other.width && height == other.height && pixels == other.pixels;
}

GrayscaleImage render(const World& world, const View2D view, const std::size_t width,
                      const std::size_t height) {
    if (width == 0 || height == 0 || view.max_x <= view.min_x || view.max_y <= view.min_y) {
        throw std::invalid_argument("render dimensions and view bounds must be positive");
    }
    if (width > std::numeric_limits<std::size_t>::max() / height) {
        throw std::invalid_argument("render dimensions are too large");
    }
    GrayscaleImage image{width, height, std::vector<std::uint8_t>(width * height, 0U)};
    const auto scale_x = (view.max_x - view.min_x) / static_cast<double>(width);
    const auto scale_y = (view.max_y - view.min_y) / static_cast<double>(height);

    for (const auto& crop : world.crops()) {
        const auto first_column = static_cast<std::size_t>(std::clamp(
            std::floor((crop.x - crop.radius - view.min_x) / scale_x), 0.0,
            static_cast<double>(width - 1U)));
        const auto last_column = static_cast<std::size_t>(std::clamp(
            std::floor((crop.x + crop.radius - view.min_x) / scale_x), 0.0,
            static_cast<double>(width - 1U)));
        const auto first_row = static_cast<std::size_t>(std::clamp(
            std::floor((view.max_y - crop.y - crop.radius) / scale_y), 0.0,
            static_cast<double>(height - 1U)));
        const auto last_row = static_cast<std::size_t>(std::clamp(
            std::floor((view.max_y - crop.y + crop.radius) / scale_y), 0.0,
            static_cast<double>(height - 1U)));

        if (crop.x + crop.radius < view.min_x || crop.x - crop.radius > view.max_x ||
            crop.y + crop.radius < view.min_y || crop.y - crop.radius > view.max_y) {
            continue;
        }
        for (std::size_t row = first_row; row <= last_row; ++row) {
            const auto y = view.max_y - (static_cast<double>(row) + 0.5) * scale_y;
            for (std::size_t column = first_column; column <= last_column; ++column) {
                const auto x = view.min_x + (static_cast<double>(column) + 0.5) * scale_x;
                const auto dx = x - crop.x;
                const auto dy = y - crop.y;
                if (dx * dx + dy * dy <= crop.radius * crop.radius) {
                    image.pixels[row * width + column] = 255U;
                }
            }
        }
    }
    return image;
}

void write_pgm(const GrayscaleImage& image, const std::filesystem::path& path) {
    if (image.width == 0 || image.height == 0 ||
        image.pixels.size() != image.width * image.height) {
        throw std::invalid_argument("invalid grayscale image");
    }
    std::ofstream stream(path, std::ios::binary);
    stream << "P5\n" << image.width << ' ' << image.height << "\n255\n";
    stream.write(reinterpret_cast<const char*>(image.pixels.data()),
                 static_cast<std::streamsize>(image.pixels.size()));
    if (!stream) {
        throw std::runtime_error("failed to write PGM image");
    }
}

}  // namespace cropsim
