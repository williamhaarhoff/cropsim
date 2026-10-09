#include "cropsim/snapshot.hpp"

#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace cropsim {
namespace {

constexpr std::array<std::byte, 8> magic{std::byte{'C'}, std::byte{'R'}, std::byte{'O'},
                                         std::byte{'P'}, std::byte{'S'}, std::byte{'I'},
                                         std::byte{'M'}, std::byte{0}};

template <typename Integer>
void append_integer(std::vector<std::byte>& output, Integer value) {
    static_assert(std::is_unsigned_v<Integer>);
    for (std::size_t index = 0; index < sizeof(Integer); ++index) {
        output.push_back(static_cast<std::byte>(value & 0xffU));
        value >>= 8U;
    }
}

void append_double(std::vector<std::byte>& output, const double value) {
    static_assert(sizeof(double) == sizeof(std::uint64_t));
    std::uint64_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    append_integer(output, bits);
}

class Reader final {
public:
    explicit Reader(const std::vector<std::byte>& input) : input_(input) {}

    template <typename Integer>
    Integer integer() {
        static_assert(std::is_unsigned_v<Integer>);
        require(sizeof(Integer));
        Integer value{};
        for (std::size_t index = 0; index < sizeof(Integer); ++index) {
            value |= static_cast<Integer>(std::to_integer<unsigned int>(input_[offset_++]))
                     << (index * 8U);
        }
        return value;
    }

    double floating_point() {
        const auto bits = integer<std::uint64_t>();
        double value{};
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    void expect_magic() {
        require(magic.size());
        for (const auto expected : magic) {
            if (input_[offset_++] != expected) {
                throw std::runtime_error("invalid world snapshot magic");
            }
        }
    }

    [[nodiscard]] bool finished() const noexcept { return offset_ == input_.size(); }

private:
    void require(const std::size_t amount) const {
        if (amount > input_.size() - offset_) {
            throw std::runtime_error("truncated world snapshot");
        }
    }

    const std::vector<std::byte>& input_;
    std::size_t offset_{};
};

}  // namespace

std::vector<std::byte> serialise_snapshot(const World& world) {
    std::vector<std::byte> output;
    output.reserve(magic.size() + 20U + world.size() * 32U);
    output.insert(output.end(), magic.begin(), magic.end());
    append_integer(output, world_snapshot_version);
    append_integer(output, world.seed());
    append_integer(output, static_cast<std::uint64_t>(world.size()));
    for (const auto& crop : world.crops()) {
        append_integer(output, crop.id);
        append_double(output, crop.x);
        append_double(output, crop.y);
        append_double(output, crop.radius);
    }
    return output;
}

World deserialise_snapshot(const std::vector<std::byte>& snapshot) {
    Reader reader(snapshot);
    reader.expect_magic();
    if (reader.integer<std::uint32_t>() != world_snapshot_version) {
        throw std::runtime_error("unsupported world snapshot version");
    }
    const auto seed = reader.integer<std::uint64_t>();
    const auto count = reader.integer<std::uint64_t>();
    if (count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw std::runtime_error("world snapshot is too large");
    }
    std::vector<Crop> crops;
    crops.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0; index < count; ++index) {
        crops.push_back(Crop{reader.integer<std::uint64_t>(), reader.floating_point(),
                             reader.floating_point(), reader.floating_point()});
    }
    if (!reader.finished()) {
        throw std::runtime_error("world snapshot has trailing data");
    }
    return World(seed, std::move(crops));
}

void save_snapshot(const World& world, const std::filesystem::path& path) {
    const auto bytes = serialise_snapshot(world);
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error("failed to write world snapshot");
    }
}

World load_snapshot(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error("failed to open world snapshot");
    }
    const auto end = stream.tellg();
    if (end < 0) {
        throw std::runtime_error("failed to determine world snapshot size");
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    stream.seekg(0);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error("failed to read world snapshot");
    }
    return deserialise_snapshot(bytes);
}

}  // namespace cropsim
