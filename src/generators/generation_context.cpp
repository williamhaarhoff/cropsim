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
constexpr std::uint64_t hierarchy_morphology_domain = 0x484d4f5250484f4cULL;

std::uint64_t splitmix64(std::uint64_t &state) noexcept {
  state += splitmix_increment;
  auto value = state;
  value = (value ^ (value >> splitmix_first_shift)) * splitmix_first_multiplier;
  value =
      (value ^ (value >> splitmix_second_shift)) * splitmix_second_multiplier;
  return value ^ (value >> splitmix_final_shift);
}

std::uint64_t mix(std::uint64_t value) noexcept { return splitmix64(value); }

} // namespace

GenerationKey GenerationKey::child(const std::uint64_t domain,
                                   const std::uint64_t index) const noexcept {
  auto state = value ^ domain;
  state ^= index + splitmix_increment + (state << 6U) + (state >> 2U);
  return {mix(state)};
}

RandomStream::RandomStream(const std::uint64_t seed) noexcept
    : random_state_(seed) {}

double RandomStream::uniform_open() noexcept {
  constexpr double denominator = 9007199254740992.0;
  const auto value = splitmix64(random_state_) >> double_precision_shift;
  return (static_cast<double>(value) + 0.5) / denominator;
}

double RandomStream::symmetric_unit() noexcept {
  return uniform_open() * 2.0 - 1.0;
}

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

MorphologyContext
GenerationContext::morphology(const GenerationKey key_value) const noexcept {
  auto state = world_seed_ ^ hierarchy_morphology_domain ^ key_value.value;
  return MorphologyContext(splitmix64(state));
}

GenerationKey
GenerationContext::key(const std::uint64_t domain,
                       const std::initializer_list<std::uint64_t> indices) const
    noexcept {
  GenerationKey result{mix(world_seed_ ^ domain)};
  for (const auto index : indices) {
    result = result.child(domain, index);
  }
  return result;
}

RandomStream GenerationContext::random(const GenerationKey key_value) const
    noexcept {
  return RandomStream(mix(world_seed_ ^ key_value.value));
}

Crop GenerationContext::make_crop(const double x, const double y,
                                  const double radius) {
  return make_crop(x, y, {{0.0, 0.0, radius, radius, 0.0}});
}

void GenerationContext::add_rail(const std::uint64_t field_index,
                                 const std::uint64_t row_index,
                                 std::vector<RailPoint> points) {
  if (rails_.size() >= static_cast<std::size_t>(UINT64_MAX)) {
    throw std::overflow_error("rail ID allocation overflow");
  }
  rails_.emplace_back(static_cast<std::uint64_t>(rails_.size()), field_index,
                      row_index, std::move(points));
}

std::vector<Rail> GenerationContext::take_rails() noexcept {
  return std::move(rails_);
}
std::uint64_t GenerationContext::add_crop_line(std::optional<std::uint64_t> bed_id,
                                      const std::uint64_t field_index,
                                      const std::uint64_t row_index,
                                      std::vector<RailPoint> points,
                                      std::optional<std::uint64_t> first,
                                      std::optional<std::uint64_t> count) {
  crop_lines_.emplace_back(static_cast<std::uint64_t>(crop_lines_.size()), bed_id,
                           field_index,row_index,std::move(points),first,count);
  return static_cast<std::uint64_t>(crop_lines_.size()-1U);
}
std::vector<CropLine> GenerationContext::take_crop_lines() noexcept { return std::move(crop_lines_); }
std::uint64_t GenerationContext::add_traversal(const std::uint64_t f,const std::uint64_t p){const auto id=static_cast<std::uint64_t>(traversals_.size());traversals_.push_back({id,f,p,{},{},{}});return id;}
std::uint64_t GenerationContext::add_bed(const std::uint64_t t){const auto id=static_cast<std::uint64_t>(beds_.size());beds_.push_back({id,t,{},{},{}});return id;}
void GenerationContext::add_attachment_rail(const RailKind k,const std::uint64_t t,const std::optional<std::uint64_t> b,std::vector<RailPoint> p){const auto id=static_cast<std::uint64_t>(rails_.size());rails_.emplace_back(id,k,t,b,std::move(p));if(b){(k==RailKind::nominal_bed_center?beds_[*b].nominal_rail_ids:beds_[*b].real_rail_ids).push_back(id);}else{(k==RailKind::nominal_traversal?traversals_[t].nominal_rail_ids:traversals_[t].actual_rail_ids).push_back(id);}}
void GenerationContext::traversal_add_bed(const std::uint64_t t,const std::uint64_t b){traversals_[t].bed_ids.push_back(b);}
void GenerationContext::bed_add_crop_line(const std::uint64_t b,const std::uint64_t l){beds_[b].crop_line_ids.push_back(l);}
std::vector<Traversal> GenerationContext::take_traversals() noexcept{return std::move(traversals_);}
std::vector<Bed> GenerationContext::take_beds() noexcept{return std::move(beds_);}

Crop GenerationContext::make_crop(const double x, const double y,
                                  std::vector<EllipseLeaf> leaves) {
  if (next_id_ == UINT64_MAX) {
    throw std::overflow_error("crop entity ID allocation overflow");
  }
  return Crop{next_id_++, x, y, std::move(leaves)};
}

} // namespace cropsim::generators
