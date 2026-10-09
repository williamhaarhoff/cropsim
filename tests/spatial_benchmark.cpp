#include "cropsim/world.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

void run_case(const std::size_t count) {
    const auto side = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(count))));
    std::vector<cropsim::Crop> crops;
    crops.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        crops.push_back({index, static_cast<double>(index % side),
                         static_cast<double>(index / side), 0.2});
    }
    const auto construction_start = Clock::now();
    const cropsim::World world(1234, std::move(crops));
    const auto construction_end = Clock::now();

    std::vector<double> latencies;
    latencies.reserve(101U);
    std::size_t returned = 0U;
    for (std::size_t iteration = 0; iteration < 101U; ++iteration) {
        const auto coordinate = static_cast<double>((iteration * 7919U) % side);
        const cropsim::Aabb query{coordinate, coordinate, coordinate + 10.0,
                                  coordinate + 10.0};
        const auto start = Clock::now();
        const auto result = world.query(query);
        const auto end = Clock::now();
        returned += result.size();
        latencies.push_back(std::chrono::duration<double, std::nano>(end - start).count());
    }
    std::sort(latencies.begin(), latencies.end());
    const auto construction_ms =
        std::chrono::duration<double, std::milli>(construction_end - construction_start).count();
    const auto area = static_cast<double>(side) * static_cast<double>(side);
    const auto& index = world.spatial_index();
    std::cout << std::fixed << std::setprecision(2) << "crops=" << count
              << " density=" << static_cast<double>(count) / area
              << " construction_ms=" << construction_ms
              << " median_lookup_ns=" << latencies[latencies.size() / 2U]
              << " mean_returned_candidates=" << static_cast<double>(returned) / 101.0
              << " cells=" << index.cell_count() << " references=" << index.reference_count()
              << " index_bytes=" << index.allocated_bytes() << '\n';
}

}  // namespace

int main() {
    for (const auto count : {std::size_t{50}, std::size_t{1'000}, std::size_t{1'000'000},
                             std::size_t{10'000'000}}) {
        run_case(count);
    }
}
