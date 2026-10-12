#pragma once

#include "cropsim/generators/generator.hpp"
#include "cropsim/renderer.hpp"
#include "cropsim/terminal_image.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace YAML {
class Node;
}

namespace cropsim::generators {

enum class CorridorType { lane, track };
enum class BoundaryType { cadastral, hedge, ditch };

struct LandscapeCorridor final {
  CorridorType type{CorridorType::lane};
  double width{};
  std::vector<Point2> centerline;
  Polygon2 footprint;
};

struct LandscapeBoundary final {
  BoundaryType type{BoundaryType::cadastral};
  std::size_t field_index{};
  Point2 begin;
  Point2 end;
};

struct LandscapeAccess final {
  std::size_t field_index{};
  std::size_t corridor_index{};
  Point2 point;
};

struct LandscapeTerrain final {
  double relief{};
  double scale{};
  std::uint64_t seed{};
};

struct LandscapeLayout final {
  Polygon2 domain;
  std::vector<FieldRegion> fields;
  std::vector<LandscapeCorridor> corridors;
  std::vector<LandscapeBoundary> boundaries;
  std::vector<LandscapeAccess> access;
  LandscapeTerrain terrain;
};

[[nodiscard]] LandscapeLayout generate_landscape_layout(
    const YAML::Node &node, GenerationKey key);
[[nodiscard]] GrayscaleImage render_landscape_layout(
    const LandscapeLayout &layout, std::size_t width, std::size_t height);
[[nodiscard]] std::string landscape_layout_svg(const LandscapeLayout &layout,
                                               std::size_t width,
                                               std::size_t height);
void write_landscape_svg(const LandscapeLayout &layout,
                         const std::filesystem::path &path,
                         std::size_t width, std::size_t height);
[[nodiscard]] std::string format_landscape_layout_terminal(
    const LandscapeLayout &layout, std::size_t width, std::size_t height,
    const TerminalImageOptions &options = {});

} // namespace cropsim::generators
