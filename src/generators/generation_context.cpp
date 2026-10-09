#include "cropsim/generators/generation_context.hpp"

#include <stdexcept>
#include <utility>

namespace cropsim::generators {
namespace {

constexpr std::uint64_t splitmix_increment = 0x9e3779b97f4a7c15ULL;
constexpr std::uint64_t splitmix_first_multiplier = 0xbf58476d1ce4e5b9ULL;
constexpr std::uint64_t splitmix_second_multiplier = 0x94d049bb133111ebULL;
constexpr unsigned int splitmix_first_shift = 30U;
constexpr unsigned int splitmix_second_shift = 27U;
constexpr unsigned int splitmix_final_shift = 31U;
constexpr unsigned int double_precision_shift = 11U;
constexpr std::uint64_t morphology_domain = 0x43524f5053494d47ULL;

std::uint64_t splitmix64(std::uint64_t &state) noexcept {
  state += splitmix_increment;
  auto value = state;
  value = (value ^ (value >> splitmix_first_shift)) * splitmix_first_multiplier;
  value =
      (value ^ (value >> splitmix_second_shift)) * splitmix_second_multiplier;
  return value ^ (value >> splitmix_final_shift);
}

} // namespace

MorphologyContext::MorphologyContext(const std::uint64_t seed) noexcept
    : random_state_(seed) {}

double MorphologyContext::uniform_open() noexcept {
  constexpr double denominator = 9007199254740992.0;
  const auto value = splitmix64(random_state_) >> double_precision_shift;
  return (static_cast<double>(value) + 0.5) / denominator;
}

GenerationContext::GenerationContext(const std::uint64_t seed) noexcept
    : random_state_(seed), world_seed_(seed) {}

double GenerationContext::symmetric_unit() noexcept {
  constexpr double denominator = 9007199254740992.0;
  constexpr double range = 2.0;
  const auto value = splitmix64(random_state_) >> double_precision_shift;
  return (static_cast<double>(value) / denominator) * range - 1.0;
}

MorphologyContext
GenerationContext::morphology(const std::uint64_t crop_id) const noexcept {
  auto state = world_seed_ ^ morphology_domain;
  state ^= crop_id + splitmix_increment + (state << 6U) + (state >> 2U);
  return MorphologyContext(splitmix64(state));
}

Crop GenerationContext::make_crop(const double x, const double y,
                                  const double radius) {
  return make_crop(x, y, {{0.0, 0.0, radius, radius, 0.0}});
}

Crop GenerationContext::make_crop(const double x, const double y,
                                  std::vector<EllipseLeaf> leaves) {
  if (next_id_ == UINT64_MAX) {
    throw std::overflow_error("crop entity ID allocation overflow");
  }
  return Crop{next_id_++, x, y, std::move(leaves)};
}

} // namespace cropsim::generators
