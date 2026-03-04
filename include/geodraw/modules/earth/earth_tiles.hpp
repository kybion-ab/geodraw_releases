/*******************************************************************************
 * File: earth_tiles.hpp
 *
 * Description: Tile addressing, quadtree structures, and LOD management for
 * the Earth visualization module. Provides the data structures for managing
 * map tiles in a hierarchical spatial structure.
 *
 * Why required: Efficient tile management requires spatial indexing (quadtree)
 * and LOD selection to load only visible tiles at appropriate resolutions.
 * This module provides the foundation for the tile fetching and rendering system.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/modules/earth/earth_coords.hpp"
#include <array>
#include <memory>
#include <string>

namespace geodraw {
namespace earth {

// =============================================================================
// Tile Coordinate
// =============================================================================

/**
 * XYZ tile coordinate in Web Mercator tiling scheme
 *
 * Standard "slippy map" tile addressing:
 * - z: Zoom level (0 = world in 1 tile, each level doubles tile count)
 * - x: Column (0 at prime meridian, increases eastward)
 * - y: Row (0 at top/north, increases southward - TMS convention is opposite)
 */
struct TileCoord {
    int z = 0;  // Zoom level (0-22)
    int x = 0;  // Column
    int y = 0;  // Row

    TileCoord() = default;
    TileCoord(int z_, int x_, int y_) : z(z_), x(x_), y(y_) {}

    /**
     * Generate unique string key for caching
     */
    std::string key() const {
        return std::to_string(z) + "/" + std::to_string(x) + "/" + std::to_string(y);
    }

    /**
     * Get parent tile (one zoom level up, contains this tile)
     */
    TileCoord parent() const {
        if (z <= 0) return *this;
        return TileCoord{z - 1, x / 2, y / 2};
    }

    /**
     * Get the four child tiles (one zoom level down)
     * Order: NW, NE, SW, SE
     */
    std::array<TileCoord, 4> children() const {
        const int cx = x * 2;
        const int cy = y * 2;
        return {{
            TileCoord{z + 1, cx,     cy},      // NW (top-left)
            TileCoord{z + 1, cx + 1, cy},      // NE (top-right)
            TileCoord{z + 1, cx,     cy + 1},  // SW (bottom-left)
            TileCoord{z + 1, cx + 1, cy + 1}   // SE (bottom-right)
        }};
    }

    /**
     * Get geographic bounds of this tile in WGS84
     */
    void bounds(double& west, double& south, double& east, double& north) const {
        tileBounds(z, x, y, west, south, east, north);
    }

    /**
     * Get center point of this tile
     */
    GeoCoord center() const {
        return tileCenter(z, x, y).toDegrees();
    }

    /**
     * Get corner coordinates in WGS84 (SW, SE, NE, NW order)
     */
    std::array<GeoCoord, 4> corners() const {
        double west, south, east, north;
        bounds(west, south, east, north);
        return {{
            GeoCoord(south, west, 0.0),  // SW
            GeoCoord(south, east, 0.0),  // SE
            GeoCoord(north, east, 0.0),  // NE
            GeoCoord(north, west, 0.0)   // NW
        }};
    }

    /**
     * Check if this tile contains a point
     */
    bool contains(const GeoCoord& point) const {
        double west, south, east, north;
        bounds(west, south, east, north);
        return point.longitude >= west && point.longitude <= east &&
               point.latitude >= south && point.latitude <= north;
    }

    /**
     * Equality comparison
     */
    bool operator==(const TileCoord& other) const {
        return z == other.z && x == other.x && y == other.y;
    }

    bool operator!=(const TileCoord& other) const {
        return !(*this == other);
    }

    /**
     * Less-than for use in ordered containers
     */
    bool operator<(const TileCoord& other) const {
        if (z != other.z) return z < other.z;
        if (x != other.x) return x < other.x;
        return y < other.y;
    }
};

// =============================================================================
// Tile Bounds (for external data fetching)
// =============================================================================

/**
 * Tile coordinate with its WGS84 bounding box
 * Useful for fetching external data for visible tiles
 */
struct TileBounds {
    int z, x, y;                         // Tile coordinates (for cache keys)
    double west, south, east, north;     // WGS84 bounds in degrees

    /**
     * Generate cache key string (e.g., "14/8529/5765")
     */
    std::string key() const {
        return std::to_string(z) + "/" + std::to_string(x) + "/" + std::to_string(y);
    }
};

// =============================================================================
// Quadtree Node (for LOD management)
// =============================================================================

/**
 * Quadtree node for hierarchical tile management
 *
 * The quadtree enables efficient:
 * - Frustum culling (skip entire subtrees outside view)
 * - LOD selection (render coarse tiles when far, detailed when close)
 * - Parent-tile fallback (show parent while children load)
 */
struct QuadTreeNode {
    TileCoord coord;
    float screenSpaceError = 0.0f;  // Error metric for LOD selection
    bool visible = false;            // Passes frustum test
    bool selected = false;           // Selected for rendering at this LOD

    std::array<std::unique_ptr<QuadTreeNode>, 4> children;

    /**
     * Check if this is a leaf node (no children)
     */
    bool isLeaf() const {
        return !children[0] && !children[1] && !children[2] && !children[3];
    }

    /**
     * Create children nodes
     */
    void createChildren() {
        auto childCoords = coord.children();
        for (int i = 0; i < 4; ++i) {
            children[i] = std::make_unique<QuadTreeNode>();
            children[i]->coord = childCoords[i];
        }
    }

    /**
     * Destroy children nodes
     */
    void destroyChildren() {
        for (auto& child : children) {
            child.reset();
        }
    }
};

// =============================================================================
// LOD Selection Helpers
// =============================================================================

/**
 * Calculate screen-space error for a tile
 *
 * This is the key metric for LOD selection. A higher error means the tile
 * covers more screen pixels and should be subdivided into children.
 *
 * @param coord Tile coordinate
 * @param cameraLat Camera latitude (for meters-per-pixel calculation)
 * @param cameraAlt Camera altitude/distance from surface
 * @param viewportHeight Viewport height in pixels
 * @param fovDegrees Vertical field of view in degrees
 * @return Screen-space error (pixels)
 */
inline float calculateScreenSpaceError(const TileCoord& coord,
                                        double cameraLat,
                                        double cameraAlt,
                                        int viewportHeight,
                                        float fovDegrees) {
    // Get geometric error of this tile (size in meters)
    const double mpp = metersPerPixel(cameraLat, coord.z);
    const double tileSize = 256.0;  // Standard tile size
    const double tileSizeMeters = mpp * tileSize;

    // Convert to screen-space error
    // screenError = geometricError * viewportHeight / (2 * distance * tan(fov/2))
    const double fovRad = deg2rad(fovDegrees);
    const double screenError = tileSizeMeters * viewportHeight /
                               (2.0 * cameraAlt * std::tan(fovRad / 2.0));

    return static_cast<float>(screenError);
}


} // namespace earth
} // namespace geodraw

// =============================================================================
// Hash function for TileCoord (for use in unordered containers)
// =============================================================================

namespace std {
template<>
struct hash<geodraw::earth::TileCoord> {
    size_t operator()(const geodraw::earth::TileCoord& coord) const {
        // Combine z, x, y into a single hash
        size_t h = 0;
        h ^= std::hash<int>{}(coord.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(coord.x) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(coord.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};
} // namespace std
