/*******************************************************************************
 * File: earth_tile_cover.hpp
 *
 * Description: Adaptive quadtree tile cover computation for shapes.
 * Computes the minimal set of web mercator tiles needed to cover a given shape
 * (polygons with holes) defined in WGS84 coordinates.
 *
 * Uses adaptive quadtree subdivision with min/max LOD constraints.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/geometry/geometry.hpp"
#include "geodraw/geometry/geometry_coords.hpp"
#include "geodraw/modules/earth/earth_coords.hpp"
#include "geodraw/modules/earth/earth_tiles.hpp"
#include <stdexcept>
#include <vector>

namespace geodraw {
namespace earth {

// =============================================================================
// WGS84 Bounds Helper
// =============================================================================

/**
 * Compute WGS84 bounding box from a Shape3 in WGS84 coordinates
 *
 * The shape must have coordSystem = CoordSystem::WGS84 with:
 * - x = longitude (degrees, -180 to +180)
 * - y = latitude (degrees, -90 to +90)
 * - z = altitude (meters)
 *
 * @return BBox2(west, south, east, north) in degrees
 */
inline BBox2 computeWGS84Bounds(const Shape3& shape) {
    if (shape.polygons.empty() && shape.lines.empty() && shape.points.empty()) {
        return BBox2();
    }

    double west = 180.0, south = 90.0, east = -180.0, north = -90.0;

    auto updateBounds = [&](const Pos3& pt) {
        double lon = pt.pos.x;
        double lat = pt.pos.y;
        west = std::min(west, lon);
        east = std::max(east, lon);
        south = std::min(south, lat);
        north = std::max(north, lat);
    };

    for (const auto& polygon : shape.polygons) {
        for (const auto& ring : polygon) {
            for (const auto& pt : ring.points) {
                updateBounds(pt);
            }
        }
    }

    for (const auto& line : shape.lines) {
        for (const auto& pt : line.points) {
            updateBounds(pt);
        }
    }

    for (const auto& pt : shape.points) {
        updateBounds(pt);
    }

    return BBox2(Pos2(west, south), Pos2(east, north));
}

// =============================================================================
// Tile-Shape Classification
// =============================================================================

/**
 * Relationship between a tile and a shape
 */
enum class TileShapeRelation {
    OUTSIDE,    // Tile completely outside shape - skip
    INSIDE,     // Tile entirely inside shape - add without subdivision
    INTERSECTS  // Tile partially overlaps shape - subdivide or add at maxLOD
};

/**
 * Classify the relationship between a tile and a Shape3 in WGS84 coordinates
 *
 * Uses existing geometry functions (pointInPolygon2D, edgeIntersection2D)
 * with lon/lat stored as x/y in Pos3 (x=longitude, y=latitude).
 *
 * @param tile The tile to classify
 * @param shape Shape3 in WGS84 coordinates
 * @param shapeBounds Pre-computed WGS84 bounds of the shape (for efficiency)
 */
inline TileShapeRelation classifyTileShape(const TileCoord& tile, const Shape3& shape,
                                           const BBox2& shapeBounds) {
    // Get tile bounds as BBox2(west, south, east, north)
    double west, south, east, north;
    tile.bounds(west, south, east, north);
    BBox2 tileBbox(Pos2(west, south), Pos2(east, north));

    // Quick reject: no intersection with shape bounds
    if (!tileBbox.intersects(shapeBounds)) {
        return TileShapeRelation::OUTSIDE;
    }

    // Tile corners as Pos3 (lon=x, lat=y, z=0)
    Pos3 corners[4] = {
        Pos3(west, south, 0.0),   // SW
        Pos3(east, south, 0.0),   // SE
        Pos3(east, north, 0.0),   // NE
        Pos3(west, north, 0.0)    // NW
    };

    // Tile edges as 2D line segments
    glm::dvec2 tileEdges[4][2] = {
        {{west, south}, {east, south}},   // S edge
        {{east, south}, {east, north}},   // E edge
        {{east, north}, {west, north}},   // N edge
        {{west, north}, {west, south}}    // W edge
    };

    for (const auto& polygon : shape.polygons) {
        if (polygon.empty()) continue;
        const Polyline3& outerRing = polygon[0];

        // Count corners inside the polygon (outer minus holes)
        int cornersInside = 0;
        for (int i = 0; i < 4; ++i) {
            if (pointInPolygon2D(corners[i], outerRing)) {
                // Check if inside any hole
                bool inHole = false;
                for (size_t h = 1; h < polygon.size(); ++h) {
                    if (pointInPolygon2D(corners[i], polygon[h])) {
                        inHole = true;
                        break;
                    }
                }
                if (!inHole) {
                    cornersInside++;
                }
            }
        }

        // All 4 corners inside polygon
        if (cornersInside == 4) {
            // Check that no tile edges cross any holes
            bool crossesHole = false;
            for (size_t h = 1; h < polygon.size() && !crossesHole; ++h) {
                const Polyline3& hole = polygon[h];
                for (size_t j = 0; j < hole.points.size() && !crossesHole; ++j) {
                    size_t k = (j + 1) % hole.points.size();
                    glm::dvec2 h0(hole.points[j].pos.x, hole.points[j].pos.y);
                    glm::dvec2 h1(hole.points[k].pos.x, hole.points[k].pos.y);

                    for (int e = 0; e < 4; ++e) {
                        if (edgeIntersection2D(tileEdges[e][0], tileEdges[e][1], h0, h1)) {
                            crossesHole = true;
                            break;
                        }
                    }
                }
            }

            if (!crossesHole) {
                return TileShapeRelation::INSIDE;
            }
            // Crosses hole - treat as intersecting
            return TileShapeRelation::INTERSECTS;
        }

        // Some corners inside - definitely intersects
        if (cornersInside > 0) {
            return TileShapeRelation::INTERSECTS;
        }

        // No corners inside - check if any polygon vertex is inside tile
        for (const auto& pt : outerRing.points) {
            if (tileBbox.containsPoint(Pos2(pt.pos.x, pt.pos.y))) {
                return TileShapeRelation::INTERSECTS;
            }
        }

        // Check if any outer ring edge intersects tile edges
        for (size_t i = 0; i < outerRing.points.size(); ++i) {
            size_t j = (i + 1) % outerRing.points.size();
            glm::dvec2 p0(outerRing.points[i].pos.x, outerRing.points[i].pos.y);
            glm::dvec2 p1(outerRing.points[j].pos.x, outerRing.points[j].pos.y);

            for (int e = 0; e < 4; ++e) {
                if (edgeIntersection2D(tileEdges[e][0], tileEdges[e][1], p0, p1)) {
                    return TileShapeRelation::INTERSECTS;
                }
            }
        }

        // Check if tile center is inside polygon (handles case where tile is
        // entirely inside a large polygon but no vertices are inside tile)
        Pos3 tileCenter(tileBbox.center().x, tileBbox.center().y, 0.0);
        if (pointInPolygon2D(tileCenter, outerRing)) {
            bool inHole = false;
            for (size_t h = 1; h < polygon.size(); ++h) {
                if (pointInPolygon2D(tileCenter, polygon[h])) {
                    inHole = true;
                    break;
                }
            }
            if (!inHole) {
                return TileShapeRelation::INTERSECTS;
            }
        }
    }

    return TileShapeRelation::OUTSIDE;
}

// =============================================================================
// Adaptive Tile Cover Computation
// =============================================================================

namespace detail {

/**
 * Recursive quadtree traversal for tile cover computation
 *
 * @param tile Current tile to process
 * @param shape Shape3 in WGS84 coordinates
 * @param shapeBounds Pre-computed WGS84 bounds
 * @param maxZoom Maximum zoom level
 * @param result Output vector of tiles
 */
inline void processNode(const TileCoord& tile,
                        const Shape3& shape,
                        const BBox2& shapeBounds,
                        int maxZoom,
                        std::vector<TileCoord>& result) {
    TileShapeRelation relation = classifyTileShape(tile, shape, shapeBounds);

    if (relation == TileShapeRelation::OUTSIDE) {
        return;  // Skip this subtree
    }

    if (relation == TileShapeRelation::INSIDE || tile.z >= maxZoom) {
        result.push_back(tile);
        return;
    }

    // INTERSECTS: subdivide into children
    for (const auto& child : tile.children()) {
        processNode(child, shape, shapeBounds, maxZoom, result);
    }
}

} // namespace detail

/**
 * Compute adaptive tile cover for a shape
 *
 * Returns the minimal set of tiles that cover the given shape using
 * adaptive quadtree subdivision. Tiles entirely inside the shape are
 * added at their current LOD without further subdivision.
 *
 * The shape is converted to WGS84 coordinates internally if needed.
 * Supported coordinate systems: WGS84, ENU (requires ref), ECEF.
 *
 * @param shape Shape3 in any supported coordinate system
 * @param ref GeoReference for ENU conversion (ignored for WGS84/ECEF)
 * @param minZoom Minimum zoom level to start traversal (typically 0-4)
 * @param maxZoom Maximum zoom level for subdivision (typically 14-18)
 * @return Vector of TileCoord covering the shape
 * @throws std::invalid_argument if coordinate system is not supported
 */
inline std::vector<TileCoord> computeAdaptiveTileCover(
    const Shape3& shape,
    const GeoReference& ref,
    int minZoom,
    int maxZoom)
{
    std::vector<TileCoord> result;

    // Convert to WGS84 coordinates
    Shape3 wgs84Shape;
    if (shape.coordSystem == CoordSystem::WGS84) {
        wgs84Shape = shape;
    } else if (shape.coordSystem == CoordSystem::ENU) {
        ConversionContext ctx(ref);
        wgs84Shape = shape.convertTo(CoordSystem::WGS84, ctx);
    } else if (shape.coordSystem == CoordSystem::ECEF) {
        ConversionContext ctx;  // Empty context OK for ECEF->WGS84
        wgs84Shape = shape.convertTo(CoordSystem::WGS84, ctx);
    } else {
        throw std::invalid_argument("Unsupported coordinate system for tile cover");
    }

    // Check if shape is empty
    if (wgs84Shape.polygons.empty() && wgs84Shape.lines.empty() && wgs84Shape.points.empty()) {
        return result;
    }

    // Get shape bounds (x=longitude, y=latitude)
    BBox2 bbox = computeWGS84Bounds(wgs84Shape);

    // Find tiles at minZoom that intersect the bounding box
    // bbox: min=(west, south), max=(east, north) in degrees
    // geoToTileXY expects (lat, lon, alt)
    int tileXMin, tileYMin, tileXMax, tileYMax;
    geoToTileXY(GeoCoord(bbox.max.pos.y, bbox.min.pos.x, 0.0).toRadians(), minZoom, tileXMin, tileYMin);  // NW corner (lat=north, lon=west)
    geoToTileXY(GeoCoord(bbox.min.pos.y, bbox.max.pos.x, 0.0).toRadians(), minZoom, tileXMax, tileYMax);  // SE corner (lat=south, lon=east)

    // Process each starting tile
    for (int y = tileYMin; y <= tileYMax; ++y) {
        for (int x = tileXMin; x <= tileXMax; ++x) {
            TileCoord tile(minZoom, x, y);
            detail::processNode(tile, wgs84Shape, bbox, maxZoom, result);
        }
    }

    return result;
}

} // namespace earth
} // namespace geodraw
