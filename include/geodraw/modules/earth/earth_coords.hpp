/*******************************************************************************
 * File: earth_coords.hpp
 *
 * Description: Coordinate conversion functions for Earth visualization.
 * Provides WGS84 geodetic, ECEF (Earth-Centered Earth-Fixed), and local ENU
 * (East-North-Up) coordinate systems with conversion functions between them.
 *
 * Also includes Web Mercator projection functions for tile addressing.
 *
 * Why required: Foundation for placing scenario geometry at global positions
 * while rendering in local coordinates. Essential for the Earth visualization
 * module to convert between GPS/WGS84 coordinates and the local rendering frame.
 *
 *
 ******************************************************************************/

#pragma once

#include <algorithm>
#include <cmath>
#include <utility>
#include <glm/glm.hpp>

namespace geodraw {
namespace earth {

// =============================================================================
// WGS84 Ellipsoid Constants
// =============================================================================

constexpr double WGS84_A = 6378137.0;                           // Semi-major axis (meters)
constexpr double WGS84_F = 1.0 / 298.257223563;                 // Flattening
constexpr double WGS84_B = WGS84_A * (1.0 - WGS84_F);           // Semi-minor axis (meters)
constexpr double WGS84_E2 = 2.0 * WGS84_F - WGS84_F * WGS84_F;  // First eccentricity squared

// =============================================================================
// Angle Conversion Utilities
// =============================================================================

inline constexpr double deg2rad(double deg) { return deg * M_PI / 180.0; }
inline constexpr double rad2deg(double rad) { return rad * 180.0 / M_PI; }

// =============================================================================
// Coordinate Types
// =============================================================================

/**
 * WGS84 geodetic coordinates in degrees (GPS / external format)
 *
 * Use for data at system boundaries: GPS input, JSON parsing, UI display.
 * - latitude:  Degrees north (+) or south (-) of equator, range [-90, +90]
 * - longitude: Degrees east (+) or west (-) of prime meridian, range [-180, +180]
 * - altitude:  Meters above the WGS84 reference ellipsoid
 */
struct WGS84;  // forward declaration for toRadians()

struct WGS84d {
    double latitude = 0.0;   // degrees, [-90, +90]
    double longitude = 0.0;  // degrees, [-180, +180]
    double altitude = 0.0;   // meters above WGS84 ellipsoid

    WGS84d() = default;
    WGS84d(double lat, double lon, double alt = 0.0)
        : latitude(lat), longitude(lon), altitude(alt) {}

    WGS84 toRadians() const;  // defined after WGS84
};

/**
 * WGS84 geodetic coordinates in radians (internal math format)
 *
 * Use for all internal coordinate calculations and math functions.
 * - latitude:  Radians, [-π/2, +π/2]
 * - longitude: Radians, [-π, +π]
 * - altitude:  Meters above the WGS84 reference ellipsoid
 */
struct WGS84 {
    double latitude = 0.0;   // radians, [-π/2, +π/2]
    double longitude = 0.0;  // radians, [-π, +π]
    double altitude = 0.0;   // meters above WGS84 ellipsoid

    WGS84() = default;
    WGS84(double lat, double lon, double alt = 0.0)
        : latitude(lat), longitude(lon), altitude(alt) {}

    WGS84d toDegrees() const {
        return WGS84d(rad2deg(latitude), rad2deg(longitude), altitude);
    }
};

inline WGS84 WGS84d::toRadians() const {
    return WGS84(deg2rad(latitude), deg2rad(longitude), altitude);
}

// Backward-compatibility alias: existing code using GeoCoord continues to compile
using GeoCoord = WGS84d;

/**
 * Earth-Centered Earth-Fixed (ECEF) coordinates
 *
 * Cartesian coordinates with origin at Earth's center of mass:
 * - X axis: Points from center through intersection of equator and prime meridian
 * - Y axis: Points from center through intersection of equator and 90E longitude
 * - Z axis: Points from center through North Pole
 *
 * All values in meters.
 */
struct ECEFCoord {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    ECEFCoord() = default;
    ECEFCoord(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
};

/**
 * Local East-North-Up (ENU) coordinates relative to a reference point
 *
 * A local tangent plane coordinate system:
 * - east: Meters east of reference point (positive = east)
 * - north: Meters north of reference point (positive = north)
 * - up: Meters above reference point (positive = up, away from Earth center)
 *
 * This coordinate system is used for rendering, where the reference point
 * is the scene origin and geometry is placed relative to it.
 */
struct ENUCoord {
    double east = 0.0;   // Meters east of reference
    double north = 0.0;  // Meters north of reference
    double up = 0.0;     // Meters above reference

    ENUCoord() = default;
    ENUCoord(double e, double n, double u) : east(e), north(n), up(u) {}

    /**
     * Convert to glm::vec3 for rendering
     * Maps ENU to GeoDraw's Z-up coordinate system:
     * - X = east
     * - Y = north
     * - Z = up
     */
    glm::vec3 toVec3() const {
        return glm::vec3(
            static_cast<float>(east),
            static_cast<float>(north),
            static_cast<float>(up)
        );
    }

    /**
     * Convert to glm::dvec3 (double precision)
     */
    glm::dvec3 toDVec3() const {
        return glm::dvec3(east, north, up);
    }
};

/**
 * Reference point for ENU coordinate system
 *
 * Defines the origin of the local coordinate system.
 * The origin WGS84 position maps to ENU (0, 0, 0).
 */
struct GeoReference {
    GeoCoord origin;  // WGS84 origin point

    GeoReference() = default;
    GeoReference(const GeoCoord& o) : origin(o) {}
};

// =============================================================================
// WGS84 <-> ECEF Conversion
// =============================================================================

/**
 * Convert WGS84 geodetic coordinates to ECEF
 *
 * Uses the standard geodetic-to-ECEF transformation with WGS84 ellipsoid.
 *
 * @param geo WGS84 coordinates (latitude/longitude in radians, altitude in meters)
 * @return ECEF coordinates in meters
 */
inline ECEFCoord geoToECEF(const WGS84& geo) {
    const double lat = geo.latitude;
    const double lon = geo.longitude;
    const double alt = geo.altitude;

    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double sinLon = std::sin(lon);
    const double cosLon = std::cos(lon);

    // Radius of curvature in the prime vertical
    const double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);

    return ECEFCoord{
        (N + alt) * cosLat * cosLon,
        (N + alt) * cosLat * sinLon,
        (N * (1.0 - WGS84_E2) + alt) * sinLat
    };
}

/**
 * Convert ECEF coordinates to WGS84 geodetic
 *
 * Uses iterative method (Bowring's method) for accurate conversion.
 * Converges within ~10 iterations for typical accuracy requirements.
 *
 * @param ecef ECEF coordinates in meters
 * @return WGS84 coordinates (latitude/longitude in radians, altitude in meters)
 */
inline WGS84 ecefToGeo(const ECEFCoord& ecef) {
    const double x = ecef.x;
    const double y = ecef.y;
    const double z = ecef.z;

    // Longitude is straightforward
    const double lon = std::atan2(y, x);

    // Distance from Z axis
    const double p = std::sqrt(x * x + y * y);

    // Initial latitude estimate
    double lat = std::atan2(z, p * (1.0 - WGS84_E2));

    // Iterate to convergence (Bowring's method)
    for (int i = 0; i < 10; ++i) {
        const double sinLat = std::sin(lat);
        const double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);
        lat = std::atan2(z + WGS84_E2 * N * sinLat, p);
    }

    // Compute altitude
    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double N = WGS84_A / std::sqrt(1.0 - WGS84_E2 * sinLat * sinLat);

    double alt;
    if (std::abs(cosLat) > 1e-10) {
        alt = p / cosLat - N;
    } else {
        // Near poles, use Z component
        alt = std::abs(z) / std::abs(sinLat) - N * (1.0 - WGS84_E2);
    }

    return WGS84(lat, lon, alt);
}

// =============================================================================
// WGS84 <-> ENU Conversion
// =============================================================================

/**
 * Convert WGS84 geodetic coordinates to local ENU relative to reference point
 *
 * The ENU coordinate system is a local tangent plane at the reference point.
 * This is useful for rendering where the reference is the scene origin.
 *
 * @param point WGS84 coordinates of the point to convert (in radians)
 * @param ref Reference point defining the ENU origin (origin stored in degrees)
 * @return ENU coordinates in meters relative to reference
 */
inline ENUCoord geoToENU(const WGS84& point, const GeoReference& ref) {
    const WGS84 refRad = ref.origin.toRadians();

    // Convert both points to ECEF
    const ECEFCoord pointECEF = geoToECEF(point);
    const ECEFCoord refECEF = geoToECEF(refRad);

    // Difference vector in ECEF
    const double dx = pointECEF.x - refECEF.x;
    const double dy = pointECEF.y - refECEF.y;
    const double dz = pointECEF.z - refECEF.z;

    // Rotation matrix from ECEF to ENU at reference point
    const double lat = refRad.latitude;
    const double lon = refRad.longitude;

    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double sinLon = std::sin(lon);
    const double cosLon = std::cos(lon);

    // Apply ECEF-to-ENU rotation
    // ENU = R * (ECEF_point - ECEF_ref)
    // where R is the rotation matrix from ECEF to ENU
    const double east  = -sinLon * dx + cosLon * dy;
    const double north = -sinLat * cosLon * dx - sinLat * sinLon * dy + cosLat * dz;
    const double up    =  cosLat * cosLon * dx + cosLat * sinLon * dy + sinLat * dz;

    return ENUCoord(east, north, up);
}

/**
 * Convert local ENU coordinates to WGS84 geodetic
 *
 * Inverse of geoToENU - converts from local tangent plane back to GPS coordinates.
 *
 * @param enu ENU coordinates in meters relative to reference
 * @param ref Reference point defining the ENU origin (origin stored in degrees)
 * @return WGS84 coordinates in radians
 */
inline WGS84 enuToGeo(const ENUCoord& enu, const GeoReference& ref) {
    const double east = enu.east;
    const double north = enu.north;
    const double up = enu.up;

    const WGS84 refRad = ref.origin.toRadians();

    // Reference point trigonometry
    const double lat = refRad.latitude;
    const double lon = refRad.longitude;

    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double sinLon = std::sin(lon);
    const double cosLon = std::cos(lon);

    // Inverse of ECEF-to-ENU rotation (transpose of rotation matrix)
    // ECEF_delta = R^T * ENU
    const double dx = -sinLon * east - sinLat * cosLon * north + cosLat * cosLon * up;
    const double dy =  cosLon * east - sinLat * sinLon * north + cosLat * sinLon * up;
    const double dz =                  cosLat * north          + sinLat * up;

    // Add reference ECEF position
    const ECEFCoord refECEF = geoToECEF(refRad);
    const ECEFCoord pointECEF(refECEF.x + dx, refECEF.y + dy, refECEF.z + dz);

    return ecefToGeo(pointECEF);
}

// =============================================================================
// Web Mercator Projection (for Tile Addressing)
// =============================================================================

/**
 * Convert WGS84 geodetic to Web Mercator projection coordinates
 *
 * Web Mercator (EPSG:3857) is used by most web mapping services.
 * Returns normalized coordinates in the range [-PI, PI] for x and
 * approximately [-PI, PI] for y (clamped to valid latitude range).
 *
 * @param geo WGS84 coordinates in radians
 * @return Pair of (x, y) in Web Mercator projection
 */
inline std::pair<double, double> geoToWebMercator(const WGS84& geo) {
    const double x = geo.longitude;

    // Clamp latitude to valid Web Mercator range (~85.051 degrees = 1.4844 rad)
    const double maxLat = deg2rad(85.051128779806604);
    const double lat = std::clamp(geo.latitude, -maxLat, maxLat);
    const double y = std::log(std::tan(M_PI / 4.0 + lat / 2.0));

    return {x, y};
}

/**
 * Convert Web Mercator projection coordinates to WGS84 geodetic
 *
 * @param x Web Mercator x coordinate (radians)
 * @param y Web Mercator y coordinate
 * @return WGS84 coordinates in radians (altitude = 0)
 */
inline WGS84 webMercatorToGeo(double x, double y) {
    const double lon = x;  // already radians
    const double lat = 2.0 * std::atan(std::exp(y)) - M_PI / 2.0;  // result in radians
    return WGS84(lat, lon, 0.0);
}

// =============================================================================
// Tile Coordinate Addressing
// =============================================================================

// Forward declaration - full definition in earth_tiles.hpp
struct TileCoord;

/**
 * Convert WGS84 geodetic coordinates to XYZ tile coordinate at given zoom level
 *
 * Uses the standard Web Mercator tiling scheme (Google/OSM/MapTiler).
 * Tile (0,0) is at the top-left (northwest) corner.
 *
 * @param geo WGS84 coordinates in radians
 * @param zoom Zoom level (0-22 typically, higher = more detail)
 * @return Tile coordinate {z, x, y}
 */
inline void geoToTileXY(const WGS84& geo, int zoom, int& tileX, int& tileY) {
    const double n = std::pow(2.0, zoom);
    const double latRad = geo.latitude;

    // X: longitude to tile column (longitude in radians [-π,π] → tile column)
    tileX = static_cast<int>((geo.longitude + M_PI) / (2.0 * M_PI) * n);

    // Y: latitude to tile row (Web Mercator projection, Y increases southward)
    tileY = static_cast<int>((1.0 - std::asinh(std::tan(latRad)) / M_PI) / 2.0 * n);

    // Clamp to valid range
    const int maxTile = static_cast<int>(n) - 1;
    tileX = std::clamp(tileX, 0, maxTile);
    tileY = std::clamp(tileY, 0, maxTile);
}

/**
 * Get the WGS84 bounding box of a tile
 *
 * @param zoom Zoom level
 * @param tileX Tile column
 * @param tileY Tile row
 * @param west Output: western longitude (degrees)
 * @param south Output: southern latitude (degrees)
 * @param east Output: eastern longitude (degrees)
 * @param north Output: northern latitude (degrees)
 */
inline void tileBounds(int zoom, int tileX, int tileY,
                       double& west, double& south, double& east, double& north) {
    const double n = std::pow(2.0, zoom);

    // Longitude bounds
    west = tileX / n * 360.0 - 180.0;
    east = (tileX + 1) / n * 360.0 - 180.0;

    // Latitude bounds (inverse Web Mercator)
    const double yNorth = M_PI * (1.0 - 2.0 * tileY / n);
    const double ySouth = M_PI * (1.0 - 2.0 * (tileY + 1) / n);

    north = rad2deg(std::atan(std::sinh(yNorth)));
    south = rad2deg(std::atan(std::sinh(ySouth)));
}

/**
 * Get the center point of a tile in WGS84 coordinates
 *
 * @param zoom Zoom level
 * @param tileX Tile column
 * @param tileY Tile row
 * @return WGS84 coordinates of tile center in radians (altitude = 0)
 */
inline WGS84 tileCenter(int zoom, int tileX, int tileY) {
    double west, south, east, north;
    tileBounds(zoom, tileX, tileY, west, south, east, north);  // returns degrees
    return WGS84(deg2rad((north + south) / 2.0), deg2rad((east + west) / 2.0), 0.0);
}

/**
 * Calculate approximate meters per pixel at given latitude and zoom
 *
 * Useful for determining appropriate zoom level for a given view.
 *
 * @param latitude Latitude in radians
 * @param zoom Zoom level
 * @return Approximate meters per pixel at this latitude/zoom
 */
inline double metersPerPixel(double latitude, int zoom) {
    // Earth circumference at equator / (tile size * number of tiles)
    const double tileSize = 256.0;  // Standard tile size
    const double n = std::pow(2.0, zoom);
    const double metersAtEquator = 2.0 * M_PI * WGS84_A / (tileSize * n);

    // Scale by latitude (Web Mercator distortion)
    return metersAtEquator * std::cos(latitude);
}

/**
 * Calculate appropriate zoom level for given meters-per-pixel target
 *
 * @param latitude Latitude in radians
 * @param targetMetersPerPixel Desired meters per pixel
 * @return Recommended zoom level (clamped to 0-22)
 */
inline int zoomForMetersPerPixel(double latitude, double targetMetersPerPixel) {
    const double tileSize = 256.0;
    const double earthCircum = 2.0 * M_PI * WGS84_A;
    const double cosLat = std::cos(latitude);

    // Solve: targetMPP = earthCircum * cosLat / (tileSize * 2^zoom)
    // 2^zoom = earthCircum * cosLat / (tileSize * targetMPP)
    // zoom = log2(earthCircum * cosLat / (tileSize * targetMPP))
    const double zoom = std::log2(earthCircum * cosLat / (tileSize * targetMetersPerPixel));

    return std::clamp(static_cast<int>(std::round(zoom)), 0, 22);
}

// =============================================================================
// Coordinate Output Mode
// =============================================================================

/**
 * Coordinate output mode for Earth tiles
 *
 * - ENU: Local East-North-Up coordinates (default for backward compatibility)
 * - ECEF: Earth-Centered Earth-Fixed coordinates
 */
enum class CoordinateOutput {
    ENU,   // Local tangent plane (default for backward compatibility)
    ECEF   // Earth-centered (for seamless multi-scale rendering)
};

} // namespace earth
} // namespace geodraw
