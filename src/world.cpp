#include "cropsim/world.hpp"

#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace cropsim {

bool EllipseLeaf::operator==(const EllipseLeaf& other) const noexcept {
    return x == other.x && y == other.y && radius_x == other.radius_x &&
           radius_y == other.radius_y && rotation == other.rotation;
}

Crop::Crop(const std::uint64_t crop_id, const double crop_x, const double crop_y,
           const double radius)
    : id(crop_id), x(crop_x), y(crop_y), leaves{{0.0, 0.0, radius, radius, 0.0}} {}

Crop::Crop(const std::uint64_t crop_id, const double crop_x, const double crop_y,
           std::vector<EllipseLeaf> crop_leaves)
    : id(crop_id), x(crop_x), y(crop_y), leaves(std::move(crop_leaves)) {}

bool Crop::operator==(const Crop& other) const noexcept {
    return id == other.id && x == other.x && y == other.y && leaves == other.leaves;
}

bool Aabb::operator==(const Aabb& other) const noexcept {
    return min_x == other.min_x && min_y == other.min_y && max_x == other.max_x &&
           max_y == other.max_y;
}

bool RailPoint::operator==(const RailPoint &other) const noexcept {
    return x == other.x && y == other.y;
}

Rail::Rail(const std::uint64_t id, const std::uint64_t field_index,
           const std::uint64_t row_index, std::vector<RailPoint> points)
    : id_(id), field_index_(field_index), row_index_(row_index),
      points_(std::move(points)) {
    if (points_.size() < 2U) {
        throw std::invalid_argument("rail must contain at least two points");
    }
    cumulative_lengths_.reserve(points_.size());
    cumulative_lengths_.push_back(0.0);
    for (std::size_t index = 1U; index < points_.size(); ++index) {
        const auto &previous = points_[index - 1U];
        const auto &current = points_[index];
        if (!std::isfinite(previous.x) || !std::isfinite(previous.y) ||
            !std::isfinite(current.x) || !std::isfinite(current.y)) {
            throw std::invalid_argument("rail points must be finite");
        }
        const auto segment = std::hypot(current.x - previous.x,
                                        current.y - previous.y);
        if (!std::isfinite(segment) || segment <= 0.0 ||
            segment > std::numeric_limits<double>::max() - cumulative_lengths_.back()) {
            throw std::invalid_argument("rail segments must have positive finite length");
        }
        cumulative_lengths_.push_back(cumulative_lengths_.back() + segment);
    }
}

Rail::Rail(const std::uint64_t id, const RailKind kind,
           std::optional<std::uint64_t> traversal_id,
           std::optional<std::uint64_t> bed_id, std::vector<RailPoint> points)
    : Rail(id, 0U, 0U, std::move(points)) {
    kind_ = kind;
    traversal_id_ = traversal_id;
    bed_id_ = bed_id;
    const auto bed_kind = kind == RailKind::nominal_bed_center ||
                          kind == RailKind::real_bed_center;
    if ((bed_kind && (!bed_id_ || !traversal_id_)) ||
        (!bed_kind && (!traversal_id_ || bed_id_))) {
        throw std::invalid_argument("rail ownership does not match its kind");
    }
}

std::size_t Rail::segment_at(const double distance) const {
    if (!std::isfinite(distance) || distance < 0.0 || distance > length()) {
        throw std::out_of_range("rail distance is outside its length");
    }
    if (distance == length()) {
        return points_.size() - 2U;
    }
    const auto upper = std::upper_bound(cumulative_lengths_.begin(),
                                        cumulative_lengths_.end(), distance);
    return static_cast<std::size_t>(upper - cumulative_lengths_.begin() - 1);
}

RailPoint Rail::position_at(const double distance) const {
    const auto segment = segment_at(distance);
    const auto segment_length = cumulative_lengths_[segment + 1U] -
                                cumulative_lengths_[segment];
    const auto fraction = (distance - cumulative_lengths_[segment]) / segment_length;
    return {points_[segment].x + fraction * (points_[segment + 1U].x - points_[segment].x),
            points_[segment].y + fraction * (points_[segment + 1U].y - points_[segment].y)};
}

RailPoint Rail::tangent_at(const double distance) const {
    const auto segment = segment_at(distance);
    const auto dx = points_[segment + 1U].x - points_[segment].x;
    const auto dy = points_[segment + 1U].y - points_[segment].y;
    const auto magnitude = std::hypot(dx, dy);
    return {dx / magnitude, dy / magnitude};
}

bool Rail::operator==(const Rail &other) const noexcept {
    return id_ == other.id_ && field_index_ == other.field_index_ &&
           row_index_ == other.row_index_ && kind_ == other.kind_ &&
           traversal_id_ == other.traversal_id_ && bed_id_ == other.bed_id_ &&
           points_ == other.points_;
}

CropLine::CropLine(const std::uint64_t id, std::optional<std::uint64_t> bed_id,
                   const std::uint64_t field_index, const std::uint64_t row_index,
                   std::vector<RailPoint> points,
                   std::optional<std::uint64_t> first_crop_id,
                   std::optional<std::uint64_t> crop_count)
    : id_(id), bed_id_(bed_id), field_index_(field_index), row_index_(row_index),
      points_(std::move(points)), first_crop_id_(first_crop_id), crop_count_(crop_count) {
    if (first_crop_id_.has_value() != crop_count_.has_value() ||
        (crop_count_ && *crop_count_ == 0U) || points_.size() < 2U) {
        throw std::invalid_argument("invalid crop line metadata");
    }
    cumulative_lengths_.reserve(points_.size()); cumulative_lengths_.push_back(0.0);
    for (std::size_t i = 1; i < points_.size(); ++i) {
        const auto d = std::hypot(points_[i].x-points_[i-1].x, points_[i].y-points_[i-1].y);
        if (!std::isfinite(points_[i-1].x) || !std::isfinite(points_[i-1].y) ||
            !std::isfinite(points_[i].x) || !std::isfinite(points_[i].y) ||
            !std::isfinite(d) || d <= 0.0) throw std::invalid_argument("invalid crop line geometry");
        cumulative_lengths_.push_back(cumulative_lengths_.back()+d);
    }
}
std::size_t CropLine::segment_at(const double d) const {
    if (!std::isfinite(d) || d < 0 || d > length()) throw std::out_of_range("crop-line distance is outside its length");
    if (d == length()) return points_.size()-2U;
    return static_cast<std::size_t>(std::upper_bound(cumulative_lengths_.begin(), cumulative_lengths_.end(), d)-cumulative_lengths_.begin()-1);
}
RailPoint CropLine::position_at(const double d) const { const auto i=segment_at(d); const auto f=(d-cumulative_lengths_[i])/(cumulative_lengths_[i+1]-cumulative_lengths_[i]); return {points_[i].x+f*(points_[i+1].x-points_[i].x), points_[i].y+f*(points_[i+1].y-points_[i].y)}; }
RailPoint CropLine::tangent_at(const double d) const { const auto i=segment_at(d); const auto dx=points_[i+1].x-points_[i].x, dy=points_[i+1].y-points_[i].y, n=std::hypot(dx,dy); return {dx/n,dy/n}; }
bool CropLine::operator==(const CropLine &o) const noexcept { return id_==o.id_ && bed_id_==o.bed_id_ && field_index_==o.field_index_ && row_index_==o.row_index_ && points_==o.points_ && first_crop_id_==o.first_crop_id_ && crop_count_==o.crop_count_; }
bool Traversal::operator==(const Traversal &o) const noexcept { return id==o.id && field_index==o.field_index && pass_index==o.pass_index && nominal_rail_ids==o.nominal_rail_ids && actual_rail_ids==o.actual_rail_ids && bed_ids==o.bed_ids; }
bool Bed::operator==(const Bed &o) const noexcept { return id==o.id && traversal_id==o.traversal_id && nominal_rail_ids==o.nominal_rail_ids && real_rail_ids==o.real_rail_ids && crop_line_ids==o.crop_line_ids; }

World::World(const std::uint64_t seed, std::vector<Crop> crops,
             std::vector<Rail> rails, std::vector<CropLine> crop_lines,
             std::vector<Traversal> traversals, std::vector<Bed> beds)
    : seed_(seed), crops_(std::move(crops)), rails_(std::move(rails)),
      crop_lines_(std::move(crop_lines)), traversals_(std::move(traversals)), beds_(std::move(beds)),
      spatial_index_(crops_) {
    for (const auto& crop : crops_) {
        static_cast<void>(crop_bounds(crop));
    }
    for (std::size_t i=0;i<crop_lines_.size();++i) if(crop_lines_[i].id()!=i) throw std::invalid_argument("crop-line IDs must be sequential");
    for (std::size_t i=0;i<traversals_.size();++i) if(traversals_[i].id!=i) throw std::invalid_argument("traversal IDs must be sequential");
    for (std::size_t i=0;i<beds_.size();++i) if(beds_[i].id!=i || beds_[i].traversal_id>=traversals_.size()) throw std::invalid_argument("invalid bed ownership");
    for (const auto &r: rails_) { if(!r.traversal_id() || *r.traversal_id()>=traversals_.size()) throw std::invalid_argument("invalid rail traversal"); if(r.bed_id() && *r.bed_id()>=beds_.size()) throw std::invalid_argument("invalid rail bed"); }
    for (const auto &l: crop_lines_) { if(l.bed_id() && *l.bed_id()>=beds_.size()) throw std::invalid_argument("invalid crop-line bed"); if(l.first_crop_id() && (*l.first_crop_id()>=crops_.size() || *l.crop_count()>crops_.size()-*l.first_crop_id())) throw std::invalid_argument("invalid crop range"); }
    const auto ordered_unique=[](const std::vector<std::uint64_t>& ids,const std::size_t limit){if(!std::is_sorted(ids.begin(),ids.end())||std::adjacent_find(ids.begin(),ids.end())!=ids.end())return false;return std::all_of(ids.begin(),ids.end(),[limit](auto id){return id<limit;});};
    for(const auto &t:traversals_){if(!ordered_unique(t.nominal_rail_ids,rails_.size())||!ordered_unique(t.actual_rail_ids,rails_.size())||!ordered_unique(t.bed_ids,beds_.size()))throw std::invalid_argument("invalid traversal associations");for(auto id:t.nominal_rail_ids)if(rails_[id].kind()!=RailKind::nominal_traversal||rails_[id].traversal_id()!=t.id)throw std::invalid_argument("invalid nominal traversal rail");for(auto id:t.actual_rail_ids)if(rails_[id].kind()!=RailKind::actual_traversal||rails_[id].traversal_id()!=t.id)throw std::invalid_argument("invalid actual traversal rail");for(auto id:t.bed_ids)if(beds_[id].traversal_id!=t.id)throw std::invalid_argument("invalid traversal bed");}
    for(const auto &b:beds_){if(!ordered_unique(b.nominal_rail_ids,rails_.size())||!ordered_unique(b.real_rail_ids,rails_.size())||!ordered_unique(b.crop_line_ids,crop_lines_.size()))throw std::invalid_argument("invalid bed associations");for(auto id:b.nominal_rail_ids)if(rails_[id].kind()!=RailKind::nominal_bed_center||rails_[id].bed_id()!=b.id)throw std::invalid_argument("invalid nominal bed rail");for(auto id:b.real_rail_ids)if(rails_[id].kind()!=RailKind::real_bed_center||rails_[id].bed_id()!=b.id)throw std::invalid_argument("invalid real bed rail");for(auto id:b.crop_line_ids)if(crop_lines_[id].bed_id()!=b.id)throw std::invalid_argument("invalid bed crop line");}
    for (std::size_t index = 0; index < rails_.size(); ++index) {
        if (rails_[index].id() != index) {
            throw std::invalid_argument("rail IDs must be sequential");
        }
    }
}

std::vector<std::size_t> World::query(const Aabb& bounds) const {
    return spatial_index_.query(crops_, bounds);
}

bool World::operator==(const World& other) const noexcept {
    return seed_ == other.seed_ && crops_ == other.crops_ && rails_ == other.rails_ && crop_lines_ == other.crop_lines_ && traversals_ == other.traversals_ && beds_ == other.beds_ &&
           spatial_index_ == other.spatial_index_;
}

} // namespace cropsim
