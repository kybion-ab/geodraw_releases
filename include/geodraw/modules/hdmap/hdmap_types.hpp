/*******************************************************************************
 * File: hdmap_types.hpp
 *
 * Description: GeoDraw semantic HD map types.
 * These are provider-agnostic types that represent HD map objects for
 * autonomous vehicle applications. HD map providers adapt their native
 * structures to these GeoDraw types.
 *
 * Supports:
 * - Scene visualization with picking
 * - Semantic queries and command execution
 * - Provider-agnostic representation
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/modules/earth/earth_coords.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace geodraw {
namespace hdmap {

// =============================================================================
// Common Types
// =============================================================================

/**
 * Unique identifier for HD map objects
 * Used for picking and reference tracking
 */
using HDObjectId = uint64_t;

/**
 * 3D point with geographic coordinates
 * Wrapper around earth::GeoCoord with altitude support
 */
struct HDPoint3 {
    double latitude = 0.0;   // WGS84 latitude in degrees
    double longitude = 0.0;  // WGS84 longitude in degrees
    double altitude = 0.0;   // Altitude in meters (above WGS84 ellipsoid)

    HDPoint3() = default;
    HDPoint3(double lat, double lon, double alt = 0.0)
        : latitude(lat), longitude(lon), altitude(alt) {}

    earth::GeoCoord toGeoCoord() const {
        return earth::GeoCoord(latitude, longitude, altitude);
    }
};

/**
 * 3D polyline geometry for HD map objects
 */
using HDPolyline3 = std::vector<HDPoint3>;

// =============================================================================
// Lane Types
// =============================================================================

/**
 * Lane type classification
 */
enum class HDLaneType {
    Unknown,
    Normal,         // Regular driving lane
    OffRamp,        // Exit ramp
    OnRamp,         // Entry ramp
    Emergency,      // Emergency/shoulder lane
    HOV,            // High-occupancy vehicle lane
    Parking,        // Parking lane
    Bicycle,        // Bicycle lane
    Bus,            // Bus-only lane
    Turn,           // Turn lane
    Merge,          // Merge area
    Reversible,     // Reversible lane (direction changes)
    Acceleration,   // Acceleration lane
    Deceleration    // Deceleration lane
};

/**
 * Traffic direction for a lane
 */
enum class HDTrafficDirection {
    Unknown,
    Forward,        // Same as link direction
    Backward,       // Opposite to link direction
    Bidirectional   // Both directions allowed
};

/**
 * Lane object representing a single lane in the HD map
 */
struct HDLane {
    HDObjectId id = 0;                          // Unique object ID
    HDLaneType type = HDLaneType::Unknown;
    HDTrafficDirection direction = HDTrafficDirection::Unknown;

    // Geometry
    HDPolyline3 centerline;                     // Lane centerline
    HDPolyline3 leftBoundary;                   // Left boundary (optional)
    HDPolyline3 rightBoundary;                  // Right boundary (optional)

    // Lane properties
    std::optional<double> width;                // Lane width in meters
    std::optional<double> speedLimitKmh;        // Speed limit in km/h
    std::optional<double> speedRecommendedKmh;  // Recommended speed in km/h

    // Associations
    std::optional<HDObjectId> leftLaneId;       // Adjacent lane to the left
    std::optional<HDObjectId> rightLaneId;      // Adjacent lane to the right
    std::optional<HDObjectId> parentLinkId;     // Parent road link

    // Provider-specific metadata
    std::string providerObjectId;               // Original ID from provider
    std::string metadata;                       // JSON metadata for extensions
};

// =============================================================================
// Lane Marker Types
// =============================================================================

/**
 * Lane marker type classification
 */
enum class HDLaneMarkerType {
    Unknown,
    SolidLine,          // Continuous solid line
    DashedLine,         // Dashed/broken line
    DoubleSolid,        // Double solid lines
    DoubleDashed,       // Double dashed lines
    SolidDashed,        // Solid + dashed combination
    DashedSolid,        // Dashed + solid combination
    BotsDots,           // Raised pavement markers
    Curb,               // Physical curb
    Edge,               // Road edge
    Virtual             // No physical marking (implicit boundary)
};

/**
 * Lane marker color
 */
enum class HDLaneMarkerColor {
    Unknown,
    White,
    Yellow,
    Blue,
    Red,
    Green,
    Orange
};

/**
 * Lane marker object
 */
struct HDLaneMarker {
    HDObjectId id = 0;
    HDLaneMarkerType type = HDLaneMarkerType::Unknown;
    HDLaneMarkerColor color = HDLaneMarkerColor::Unknown;

    // Geometry
    HDPolyline3 geometry;                       // Marker centerline

    // Properties
    std::optional<double> width;                // Marker width in meters

    // Provider-specific metadata
    std::string providerObjectId;
    std::string metadata;
};

// =============================================================================
// Barrier Types
// =============================================================================

/**
 * Barrier type classification
 */
enum class HDBarrierType {
    Unknown,
    Guardrail,          // Metal guardrail
    Concrete,           // Concrete barrier (Jersey barrier)
    Fence,              // Wire/chain fence
    Curb,               // Raised curb
    Wall,               // Solid wall
    SoundBarrier,       // Noise barrier
    Vegetation,         // Hedge/vegetation barrier
    Ditch,              // Ditch or channel
    PostsAndRope        // Posts connected by rope/cable
};

/**
 * Barrier object
 */
struct HDBarrier {
    HDObjectId id = 0;
    HDBarrierType type = HDBarrierType::Unknown;

    // Geometry
    HDPolyline3 geometry;                       // Barrier centerline

    // Properties
    std::optional<double> width;                // Barrier width in meters
    std::optional<double> height;               // Barrier height in meters

    // Provider-specific metadata
    std::string providerObjectId;
    std::string metadata;
};

// =============================================================================
// Link Types (Road Network)
// =============================================================================

/**
 * Road functional class
 */
enum class HDRoadClass {
    Unknown,
    Motorway,           // Motorway/Interstate
    Trunk,              // Major highway
    Primary,            // Primary road
    Secondary,          // Secondary road
    Tertiary,           // Tertiary road
    Residential,        // Residential street
    Service,            // Service road
    Unclassified        // Other
};

/**
 * Link object representing a road segment in the network
 */
struct HDLink {
    HDObjectId id = 0;
    HDRoadClass roadClass = HDRoadClass::Unknown;
    HDTrafficDirection direction = HDTrafficDirection::Unknown;

    // Geometry
    HDPolyline3 geometry;                       // Link reference line

    // Road properties
    std::optional<double> speedLimitKmh;        // Posted speed limit
    std::optional<int> laneCount;               // Number of lanes
    std::optional<double> length;               // Link length in meters

    // Network connectivity
    std::vector<HDObjectId> laneIds;            // Lanes belonging to this link
    std::optional<HDObjectId> startNodeId;      // Start junction/node
    std::optional<HDObjectId> endNodeId;        // End junction/node

    // Provider-specific metadata
    std::string providerObjectId;
    std::string metadata;
};

// =============================================================================
// Generic HD Map Object (for polymorphic handling)
// =============================================================================

/**
 * Variant type for any HD map object
 */
using HDMapObject = std::variant<HDLane, HDLaneMarker, HDBarrier, HDLink>;

/**
 * Get the object ID from any HD map object
 */
inline HDObjectId getObjectId(const HDMapObject& obj) {
    return std::visit([](const auto& o) { return o.id; }, obj);
}

/**
 * Get the geometry from any HD map object (returns centerline/main geometry)
 */
inline const HDPolyline3& getGeometry(const HDMapObject& obj) {
    return std::visit([](const auto& o) -> const HDPolyline3& {
        if constexpr (std::is_same_v<std::decay_t<decltype(o)>, HDLane>) {
            return o.centerline;
        } else {
            return o.geometry;
        }
    }, obj);
}

/**
 * Get a human-readable type name for an HD map object
 */
inline std::string getTypeName(const HDMapObject& obj) {
    return std::visit([](const auto& o) -> std::string {
        using T = std::decay_t<decltype(o)>;
        if constexpr (std::is_same_v<T, HDLane>) return "Lane";
        else if constexpr (std::is_same_v<T, HDLaneMarker>) return "LaneMarker";
        else if constexpr (std::is_same_v<T, HDBarrier>) return "Barrier";
        else if constexpr (std::is_same_v<T, HDLink>) return "Link";
        else return "Unknown";
    }, obj);
}

// =============================================================================
// HD Map Tile
// =============================================================================

/**
 * A tile of HD map data
 * Contains all HD map objects within a geographic region
 */
struct HDMapTile {
    int64_t tileId = 0;                         // Provider tile identifier

    // Contained objects
    std::vector<HDLane> lanes;
    std::vector<HDLaneMarker> laneMarkers;
    std::vector<HDBarrier> barriers;
    std::vector<HDLink> links;

    // Tile bounds (optional, for spatial queries)
    std::optional<HDPoint3> minBound;
    std::optional<HDPoint3> maxBound;

    /**
     * Get total number of objects in this tile
     */
    size_t objectCount() const {
        return lanes.size() + laneMarkers.size() + barriers.size() + links.size();
    }

    /**
     * Check if tile is empty
     */
    bool empty() const {
        return objectCount() == 0;
    }
};

// =============================================================================
// Selection and Picking Support
// =============================================================================

/**
 * Result of picking an HD map object
 */
struct HDPickResult {
    bool hit = false;
    HDObjectId objectId = 0;
    std::string objectType;                     // "Lane", "LaneMarker", etc.
    HDPoint3 hitPoint;                          // 3D point where pick occurred
    double distance = 0.0;                      // Distance from pick ray origin

    // Pointer to the actual object (valid while source data exists)
    const HDMapObject* object = nullptr;
};

} // namespace hdmap
} // namespace geodraw
