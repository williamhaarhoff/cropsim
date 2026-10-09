#pragma once

#include "cropsim/world.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace cropsim {

struct View2D {
    double min_x{};
    double min_y{};
    double max_x{};
    double max_y{};
};

struct GrayscaleImage {
    std::size_t width{};
    std::size_t height{};
    std::vector<std::uint8_t> pixels;

    [[nodiscard]] bool operator==(const GrayscaleImage& other) const noexcept;
};

[[nodiscard]] GrayscaleImage render(const World& world, View2D view,
                                    std::size_t width, std::size_t height);
void write_pgm(const GrayscaleImage& image, const std::filesystem::path& path);

}  // namespace cropsim
