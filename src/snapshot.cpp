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

template <typename Integer> void append_integer(std::vector<std::byte>& output, Integer value) {
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

    template <typename Integer> Integer integer() {
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
    std::uint8_t byte() { require(1U); return static_cast<std::uint8_t>(std::to_integer<unsigned int>(input_[offset_++])); }

    void expect_magic() {
        require(magic.size());
        for (const auto expected : magic) {
            if (input_[offset_++] != expected) {
                throw std::runtime_error("invalid world snapshot magic");
            }
        }
    }

    [[nodiscard]] bool finished() const noexcept { return offset_ == input_.size(); }
    [[nodiscard]] std::size_t remaining() const noexcept {
        return input_.size() - offset_;
    }

  private:
    void require(const std::size_t amount) const {
        if (amount > input_.size() - offset_) {
            throw std::runtime_error("truncated world snapshot");
        }
    }

    const std::vector<std::byte>& input_;
    std::size_t offset_{};
};

} // namespace

std::vector<std::byte> serialise_snapshot(const World& world) {
    std::vector<std::byte> output;
    output.reserve(magic.size() + 20U + world.size() * 40U);
    output.insert(output.end(), magic.begin(), magic.end());
    append_integer(output, world_snapshot_version);
    append_integer(output, world.seed());
    append_integer(output, static_cast<std::uint64_t>(world.size()));
    for (const auto& crop : world.crops()) {
        append_integer(output, crop.id);
        append_double(output, crop.x);
        append_double(output, crop.y);
        append_integer(output, static_cast<std::uint64_t>(crop.leaves.size()));
        for (const auto& leaf : crop.leaves) {
            append_double(output, leaf.x);
            append_double(output, leaf.y);
            append_double(output, leaf.radius_x);
            append_double(output, leaf.radius_y);
            append_double(output, leaf.rotation);
        }
    }
    const auto& index = world.spatial_index();
    append_double(output, index.bounds().min_x);
    append_double(output, index.bounds().min_y);
    append_double(output, index.bounds().max_x);
    append_double(output, index.bounds().max_y);
    append_double(output, index.cell_size());
    append_integer(output, static_cast<std::uint64_t>(index.columns()));
    append_integer(output, static_cast<std::uint64_t>(index.rows()));
    append_integer(output, static_cast<std::uint64_t>(index.cell_offsets().size()));
    for (const auto offset : index.cell_offsets()) {
        append_integer(output, static_cast<std::uint64_t>(offset));
    }
    append_integer(output, static_cast<std::uint64_t>(index.references().size()));
    for (const auto reference : index.references()) {
        append_integer(output, static_cast<std::uint64_t>(reference));
    }
    append_integer(output, static_cast<std::uint64_t>(world.traversals().size()));
    for (const auto &traversal : world.traversals()) {
        append_integer(output,traversal.id); append_integer(output,traversal.field_index); append_integer(output,traversal.pass_index);
        for (const auto *ids : {&traversal.nominal_rail_ids,&traversal.actual_rail_ids,&traversal.bed_ids}) { append_integer(output,static_cast<std::uint64_t>(ids->size())); for(auto id:*ids) append_integer(output,id); }
    }
    append_integer(output, static_cast<std::uint64_t>(world.beds().size()));
    for (const auto &bed : world.beds()) {
        append_integer(output,bed.id); append_integer(output,bed.traversal_id);
        for (const auto *ids : {&bed.nominal_rail_ids,&bed.real_rail_ids,&bed.crop_line_ids}) { append_integer(output,static_cast<std::uint64_t>(ids->size())); for(auto id:*ids) append_integer(output,id); }
    }
    append_integer(output, static_cast<std::uint64_t>(world.rails().size()));
    for (const auto& rail : world.rails()) {
        append_integer(output, rail.id());
        append_integer(output, static_cast<std::uint8_t>(rail.kind()));
        append_integer(output, *rail.traversal_id());
        append_integer(output, rail.bed_id() ? *rail.bed_id() : UINT64_MAX);
        append_integer(output, static_cast<std::uint64_t>(rail.points().size()));
        for (const auto& point : rail.points()) {
            append_double(output, point.x);
            append_double(output, point.y);
        }
    }
    append_integer(output, static_cast<std::uint64_t>(world.crop_lines().size()));
    for (const auto &line: world.crop_lines()) {
        append_integer(output,line.id()); append_integer(output,line.bed_id()?*line.bed_id():UINT64_MAX);
        append_integer(output,line.field_index()); append_integer(output,line.row_index());
        append_integer(output,line.first_crop_id()?*line.first_crop_id():UINT64_MAX);
        append_integer(output,line.crop_count()?*line.crop_count():UINT64_MAX);
        append_integer(output,static_cast<std::uint64_t>(line.points().size()));
        for(const auto &p:line.points()){append_double(output,p.x);append_double(output,p.y);}
    }
    return output;
}

World deserialise_snapshot(const std::vector<std::byte>& snapshot) {
    Reader reader(snapshot);
    reader.expect_magic();
    const auto version = reader.integer<std::uint32_t>();
    if (version != 1U && version != 2U && version != 3U &&
        version != 4U && version != world_snapshot_version) {
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
        const auto id = reader.integer<std::uint64_t>();
        const auto x = reader.floating_point();
        const auto y = reader.floating_point();
        if (version <= 2U) {
            crops.emplace_back(id, x, y, reader.floating_point());
            continue;
        }
        const auto leaf_count = reader.integer<std::uint64_t>();
        if (leaf_count == 0U ||
            leaf_count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
            throw std::runtime_error("world snapshot leaf count is invalid");
        }
        std::vector<EllipseLeaf> leaves;
        leaves.reserve(static_cast<std::size_t>(leaf_count));
        for (std::uint64_t leaf_index = 0; leaf_index < leaf_count; ++leaf_index) {
            leaves.push_back({reader.floating_point(), reader.floating_point(),
                              reader.floating_point(), reader.floating_point(),
                              reader.floating_point()});
        }
        crops.emplace_back(id, x, y, std::move(leaves));
    }
    if (version == 1U) {
        if (!reader.finished()) {
            throw std::runtime_error("world snapshot has trailing data");
        }
        return World(seed, std::move(crops));
    }

    const Aabb stored_bounds{reader.floating_point(), reader.floating_point(),
                             reader.floating_point(), reader.floating_point()};
    const auto stored_cell_size = reader.floating_point();
    const auto columns = reader.integer<std::uint64_t>();
    const auto rows = reader.integer<std::uint64_t>();
    const auto offsets_count = reader.integer<std::uint64_t>();
    const SpatialIndex expected(crops);
    if (!(stored_bounds == expected.bounds()) || stored_cell_size != expected.cell_size() ||
        columns != expected.columns() || rows != expected.rows() ||
        offsets_count != expected.cell_offsets().size()) {
        throw std::runtime_error("world snapshot spatial index metadata is invalid");
    }
    for (const auto expected_offset : expected.cell_offsets()) {
        if (reader.integer<std::uint64_t>() != expected_offset) {
            throw std::runtime_error("world snapshot spatial index offsets are invalid");
        }
    }
    const auto references_count = reader.integer<std::uint64_t>();
    if (references_count != expected.references().size()) {
        throw std::runtime_error("world snapshot spatial index reference count is invalid");
    }
    for (const auto expected_reference : expected.references()) {
        if (reader.integer<std::uint64_t>() != expected_reference) {
            throw std::runtime_error("world snapshot spatial index references are invalid");
        }
    }
    if (version <= 3U) {
        if (!reader.finished()) {
            throw std::runtime_error("world snapshot has trailing data");
        }
        return World(seed, std::move(crops));
    }
    if (version == 4U) {
      const auto rail_count=reader.integer<std::uint64_t>(); std::vector<CropLine> lines; lines.reserve(static_cast<std::size_t>(rail_count));
      for(std::uint64_t i=0;i<rail_count;++i){ const auto id=reader.integer<std::uint64_t>(); const auto field=reader.integer<std::uint64_t>(); const auto row=reader.integer<std::uint64_t>(); const auto n=reader.integer<std::uint64_t>(); if(id!=i||n<2U||n>reader.remaining()/16U) throw std::runtime_error("world snapshot rail metadata is invalid"); std::vector<RailPoint> p; p.reserve(static_cast<std::size_t>(n)); for(std::uint64_t j=0;j<n;++j)p.push_back({reader.floating_point(),reader.floating_point()}); try{lines.emplace_back(i,std::nullopt,field,row,std::move(p));}catch(const std::invalid_argument&){throw std::runtime_error("world snapshot rail geometry is invalid");}}
      if (!reader.finished()) {
          throw std::runtime_error("world snapshot has trailing data");
      }
      return World(seed,std::move(crops),{},std::move(lines));
    }
    auto read_ids=[&reader](){ const auto n=reader.integer<std::uint64_t>(); if(n>reader.remaining()/8U) throw std::runtime_error("world snapshot association count is invalid"); std::vector<std::uint64_t> v;v.reserve(static_cast<std::size_t>(n));while(v.size()<n)v.push_back(reader.integer<std::uint64_t>());return v;};
    const auto traversal_count=reader.integer<std::uint64_t>(); std::vector<Traversal> traversals; traversals.reserve(static_cast<std::size_t>(traversal_count));
    for(std::uint64_t i=0;i<traversal_count;++i){Traversal t; t.id=reader.integer<std::uint64_t>();t.field_index=reader.integer<std::uint64_t>();t.pass_index=reader.integer<std::uint64_t>();t.nominal_rail_ids=read_ids();t.actual_rail_ids=read_ids();t.bed_ids=read_ids();if(t.id!=i)throw std::runtime_error("invalid traversal ID");traversals.push_back(std::move(t));}
    const auto bed_count=reader.integer<std::uint64_t>(); std::vector<Bed> beds;beds.reserve(static_cast<std::size_t>(bed_count));
    for(std::uint64_t i=0;i<bed_count;++i){Bed b;b.id=reader.integer<std::uint64_t>();b.traversal_id=reader.integer<std::uint64_t>();b.nominal_rail_ids=read_ids();b.real_rail_ids=read_ids();b.crop_line_ids=read_ids();if(b.id!=i)throw std::runtime_error("invalid bed ID");beds.push_back(std::move(b));}
    const auto rail_count = reader.integer<std::uint64_t>();
    constexpr std::size_t minimum_rail_bytes = 4U * sizeof(std::uint64_t) +
                                               4U * sizeof(double);
    if (rail_count > static_cast<std::uint64_t>(std::vector<Rail>{}.max_size()) ||
        rail_count > reader.remaining() / minimum_rail_bytes) {
        throw std::runtime_error("world snapshot rail count is invalid");
    }
    std::vector<Rail> rails;
    rails.reserve(static_cast<std::size_t>(rail_count));
    for (std::uint64_t rail_index = 0; rail_index < rail_count; ++rail_index) {
        const auto id = reader.integer<std::uint64_t>();
        const auto kind_value = reader.byte();
        const auto traversal_id = reader.integer<std::uint64_t>();
        const auto bed_value = reader.integer<std::uint64_t>();
        const auto point_count = reader.integer<std::uint64_t>();
        if (id != rail_index || point_count < 2U ||
            point_count > static_cast<std::uint64_t>(std::vector<RailPoint>{}.max_size()) ||
            point_count > reader.remaining() / (2U * sizeof(double))) {
            throw std::runtime_error("world snapshot rail metadata is invalid");
        }
        std::vector<RailPoint> points;
        points.reserve(static_cast<std::size_t>(point_count));
        for (std::uint64_t point_index = 0; point_index < point_count;
             ++point_index) {
            points.push_back({reader.floating_point(), reader.floating_point()});
        }
        try {
            if(kind_value>static_cast<std::uint8_t>(RailKind::real_bed_center)) throw std::invalid_argument("kind");
            rails.emplace_back(id, static_cast<RailKind>(kind_value), traversal_id,
                               bed_value==UINT64_MAX?std::optional<std::uint64_t>{}:std::optional<std::uint64_t>{bed_value}, std::move(points));
        } catch (const std::invalid_argument&) {
            throw std::runtime_error("world snapshot rail geometry is invalid");
        }
    }
    const auto line_count=reader.integer<std::uint64_t>(); std::vector<CropLine> lines;lines.reserve(static_cast<std::size_t>(line_count));
    for(std::uint64_t i=0;i<line_count;++i){const auto id=reader.integer<std::uint64_t>(),bed=reader.integer<std::uint64_t>(),field=reader.integer<std::uint64_t>(),row=reader.integer<std::uint64_t>(),first=reader.integer<std::uint64_t>(),count_value=reader.integer<std::uint64_t>(),n=reader.integer<std::uint64_t>();if(id!=i||n<2||n>reader.remaining()/16U)throw std::runtime_error("invalid crop-line metadata");std::vector<RailPoint> p;p.reserve(static_cast<std::size_t>(n));for(std::uint64_t j=0;j<n;++j)p.push_back({reader.floating_point(),reader.floating_point()});try{lines.emplace_back(id,bed==UINT64_MAX?std::optional<std::uint64_t>{}:std::optional<std::uint64_t>{bed},field,row,std::move(p),first==UINT64_MAX?std::optional<std::uint64_t>{}:std::optional<std::uint64_t>{first},count_value==UINT64_MAX?std::optional<std::uint64_t>{}:std::optional<std::uint64_t>{count_value});}catch(const std::invalid_argument&){throw std::runtime_error("invalid crop-line geometry");}}
    if (!reader.finished()) {
        throw std::runtime_error("world snapshot has trailing data");
    }
    try { return World(seed, std::move(crops), std::move(rails), std::move(lines), std::move(traversals), std::move(beds)); }
    catch(const std::invalid_argument &) { throw std::runtime_error("invalid world hierarchy"); }
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

} // namespace cropsim
