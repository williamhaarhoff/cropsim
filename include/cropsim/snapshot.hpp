#pragma once

#include "cropsim/world.hpp"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace cropsim {

inline constexpr std::uint32_t world_snapshot_version = 2;

[[nodiscard]] std::vector<std::byte> serialise_snapshot(const World& world);
[[nodiscard]] World deserialise_snapshot(const std::vector<std::byte>& snapshot);
void save_snapshot(const World& world, const std::filesystem::path& path);
[[nodiscard]] World load_snapshot(const std::filesystem::path& path);

}  // namespace cropsim
