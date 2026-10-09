#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace cropsim {

namespace generators {
class GeneratorFactory;
}

struct Crop {
    std::uint64_t id{};
    double x{};
    double y{};
    double radius{};

    [[nodiscard]] bool operator==(const Crop& other) const noexcept;
};

class World final {
public:
    World(std::uint64_t seed, std::vector<Crop> crops);

    [[nodiscard]] std::uint64_t seed() const noexcept { return seed_; }
    [[nodiscard]] const std::vector<Crop>& crops() const noexcept { return crops_; }
    [[nodiscard]] std::size_t size() const noexcept { return crops_.size(); }
    [[nodiscard]] bool operator==(const World& other) const noexcept;

private:
    std::uint64_t seed_{};
    std::vector<Crop> crops_;
};

[[nodiscard]] World world_from_yaml(std::string_view yaml);
[[nodiscard]] World world_from_yaml(std::string_view yaml,
                                    const generators::GeneratorFactory& factory);

}  // namespace cropsim
