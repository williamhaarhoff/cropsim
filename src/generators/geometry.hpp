#pragma once

#include "cropsim/generators/generator.hpp"

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/linestring.hpp>
#include <boost/geometry/geometries/multi_polygon.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

#include <vector>

namespace cropsim::generators::geometry {

namespace bg = boost::geometry;
using BoostPoint = bg::model::d2::point_xy<double>;
using BoostPolygon = bg::model::polygon<BoostPoint, false, true>;
using BoostLine = bg::model::linestring<BoostPoint>;
using BoostMultiPolygon = bg::model::multi_polygon<BoostPolygon>;

[[nodiscard]] BoostPolygon to_boost(const Polygon2 &polygon);
[[nodiscard]] Polygon2 from_boost(const BoostPolygon &polygon);
[[nodiscard]] Polygon2 validate_polygon(const Polygon2 &polygon,
                                        const char *field);
[[nodiscard]] double area(const Polygon2 &polygon);
[[nodiscard]] bool covered_by(const Point2 &point, const Polygon2 &polygon);
[[nodiscard]] std::vector<Polygon2> intersect(const Polygon2 &first,
                                              const Polygon2 &second);
[[nodiscard]] std::vector<Polygon2> inset(const Polygon2 &polygon,
                                          double distance);
[[nodiscard]] std::vector<RowSegment>
clip_line(const Polygon2 &polygon, Point2 begin, Point2 end,
          std::uint64_t field_index, std::uint64_t row_index,
          double row_spacing);
[[nodiscard]] double principal_orientation(const Polygon2 &polygon);
[[nodiscard]] double minimum_oriented_width(const Polygon2 &polygon);
[[nodiscard]] Polygon2 canonical(Polygon2 polygon);

} // namespace cropsim::generators::geometry
