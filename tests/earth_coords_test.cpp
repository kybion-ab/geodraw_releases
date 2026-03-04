/*******************************************************************************
 * File: earth_coords_test.cpp
 *
 * Description: Test for earth coordinate conversion functions.
 * Verifies round-trip accuracy and known reference points.
 *
 *
 ******************************************************************************/

#include "geodraw/modules/earth/earth_coords.hpp"
#include "geodraw/modules/earth/earth_tiles.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>

using namespace geodraw::earth;

static bool approxEqual(double a, double b, double tolerance = 1e-6) {
    return std::abs(a - b) < tolerance;
}

static bool approxEqual(const GeoCoord& a, const GeoCoord& b, double tolerance = 1e-6) {
    return approxEqual(a.latitude, b.latitude, tolerance) &&
           approxEqual(a.longitude, b.longitude, tolerance) &&
           approxEqual(a.altitude, b.altitude, tolerance);
}

TEST_CASE("Earth coords - WGS84 to ECEF round-trip") {
    GeoCoord original(37.7749, -122.4194, 100.0); // San Francisco (degrees)

    ECEFCoord ecef = geoToECEF(original.toRadians());
    GeoCoord recovered = ecefToGeo(ecef).toDegrees();

    CHECK(approxEqual(original, recovered, 1e-5));
}

TEST_CASE("Earth coords - known ECEF value at equator/prime meridian") {
    GeoCoord origin(0.0, 0.0, 0.0);
    ECEFCoord ecef = geoToECEF(origin.toRadians());

    CHECK(approxEqual(ecef.x, WGS84_A, 1.0));
    CHECK(approxEqual(ecef.y, 0.0, 1.0));
    CHECK(approxEqual(ecef.z, 0.0, 1.0));
}

TEST_CASE("Earth coords - WGS84 to ENU round-trip") {
    GeoReference ref(GeoCoord(37.7749, -122.4194, 0.0)); // San Francisco
    GeoCoord point(37.7849, -122.4094, 50.0);            // ~1km NE, 50m up

    ENUCoord enu = geoToENU(point.toRadians(), ref);
    GeoCoord recovered = enuToGeo(enu, ref).toDegrees();

    CHECK(approxEqual(point, recovered, 1e-5));
}

TEST_CASE("Earth coords - ENU at reference point is (0, 0, 0)") {
    GeoReference ref(GeoCoord(59.3293, 18.0686, 0.0)); // Stockholm
    ENUCoord enu = geoToENU(ref.origin.toRadians(), ref);

    CHECK(approxEqual(enu.east, 0.0, 0.01));
    CHECK(approxEqual(enu.north, 0.0, 0.01));
    CHECK(approxEqual(enu.up, 0.0, 0.01));
}

TEST_CASE("Earth coords - ENU directions") {
    GeoReference ref(GeoCoord(45.0, 0.0, 0.0));

    GeoCoord northPoint(45.01, 0.0, 0.0);
    ENUCoord enuNorth = geoToENU(northPoint.toRadians(), ref);

    GeoCoord eastPoint(45.0, 0.01, 0.0);
    ENUCoord enuEast = geoToENU(eastPoint.toRadians(), ref);

    // North point should have positive north, near-zero east
    CHECK(enuNorth.north > 1000.0);
    CHECK(std::abs(enuNorth.east) < 10.0);

    // East point should have positive east, near-zero north
    CHECK(enuEast.east > 500.0);
    CHECK(std::abs(enuEast.north) < 10.0);
}

TEST_CASE("Earth coords - tile coordinate addressing") {
    GeoCoord sf(37.7749, -122.4194, 0.0); // San Francisco (degrees)

    int x, y;
    geoToTileXY(sf.toRadians(), 10, x, y);

    // Verify tile bounds contain the point
    double west, south, east, north;
    tileBounds(10, x, y, west, south, east, north);

    CHECK(sf.longitude >= west);
    CHECK(sf.longitude <= east);
    CHECK(sf.latitude >= south);
    CHECK(sf.latitude <= north);
}

TEST_CASE("Earth coords - TileCoord parent/children relationships") {
    TileCoord tile(10, 163, 395);
    TileCoord parent = tile.parent();

    CHECK(parent.z == tile.z - 1);

    auto children = parent.children();
    bool found = false;
    for (const auto& child : children) {
        if (child == tile) { found = true; break; }
    }
    CHECK(found);
}

TEST_CASE("Earth coords - meters per pixel") {
    // At equator, zoom 0: entire world (~40,075 km) in 256 pixels
    double mpp0 = metersPerPixel(0.0, 0);
    double expected0 = 40075016.686 / 256.0; // ~156543 m/px

    double mpp10 = metersPerPixel(0.0, 10);
    double expected10 = expected0 / 1024.0; // 2^10 = 1024

    CHECK(approxEqual(mpp0, expected0, expected0 * 0.01));   // 1% tolerance
    CHECK(approxEqual(mpp10, expected10, expected10 * 0.01));
}

