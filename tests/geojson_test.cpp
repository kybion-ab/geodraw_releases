/*******************************************************************************
 * File: geojson_test.cpp
 *
 * Description: Tests for GeoJSON loading, saving, and coordinate conversion.
 *
 *
 ******************************************************************************/

#include "geodraw/modules/geojson/geojson.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

using namespace geodraw;
using namespace geodraw::geojson;
using Catch::Approx;

// Test helper: check if two doubles are approximately equal
static bool approxEqual(double a, double b, double epsilon = 1e-9) {
    return std::abs(a - b) < epsilon;
}

TEST_CASE("Load FeatureCollection with Point, LineString, Polygon") {
    std::string geojson = R"({
        "type": "FeatureCollection",
        "features": [
            {
                "type": "Feature",
                "properties": {"name": "Test Point"},
                "geometry": {
                    "type": "Point",
                    "coordinates": [18.0686, 59.3293, 10.0]
                }
            },
            {
                "type": "Feature",
                "properties": {"name": "Test Line"},
                "geometry": {
                    "type": "LineString",
                    "coordinates": [[18.0, 59.0], [18.1, 59.1], [18.2, 59.2]]
                }
            },
            {
                "type": "Feature",
                "properties": {"name": "Test Polygon"},
                "geometry": {
                    "type": "Polygon",
                    "coordinates": [[[18.0, 59.0], [18.1, 59.0], [18.1, 59.1], [18.0, 59.1], [18.0, 59.0]]]
                }
            }
        ]
    })";

    auto result = loadGeoJSONFromString(geojson);
    REQUIRE(result.has_value());
    REQUIRE(result->shapes.size() == 3);

    // Check Point
    REQUIRE(result->shapes[0].points.size() == 1);
    CHECK(approxEqual(result->shapes[0].points[0].pos.x, 18.0686));
    CHECK(approxEqual(result->shapes[0].points[0].pos.y, 59.3293));
    CHECK(approxEqual(result->shapes[0].points[0].pos.z, 10.0));

    // Check LineString
    REQUIRE(result->shapes[1].lines.size() == 1);
    CHECK(result->shapes[1].lines[0].points.size() == 3);

    // Check Polygon
    REQUIRE(result->shapes[2].polygons.size() == 1);
    CHECK(result->shapes[2].polygons[0].size() == 1);          // One ring (outer)
    CHECK(result->shapes[2].polygons[0][0].points.size() == 5); // Closed ring

    // Check properties stored in user_data
    CHECK(result->shapes[0].user_data.find("Test Point") != std::string::npos);
}

TEST_CASE("Load Multi geometries") {
    std::string geojson = R"({
        "type": "FeatureCollection",
        "features": [
            {
                "type": "Feature",
                "properties": {},
                "geometry": {
                    "type": "MultiPoint",
                    "coordinates": [[1, 2], [3, 4], [5, 6]]
                }
            },
            {
                "type": "Feature",
                "properties": {},
                "geometry": {
                    "type": "MultiLineString",
                    "coordinates": [[[0, 0], [1, 1]], [[2, 2], [3, 3]]]
                }
            },
            {
                "type": "Feature",
                "properties": {},
                "geometry": {
                    "type": "MultiPolygon",
                    "coordinates": [
                        [[[0, 0], [1, 0], [1, 1], [0, 1], [0, 0]]],
                        [[[2, 2], [3, 2], [3, 3], [2, 3], [2, 2]]]
                    ]
                }
            }
        ]
    })";

    auto result = loadGeoJSONFromString(geojson);
    REQUIRE(result.has_value());
    REQUIRE(result->shapes.size() == 3);

    CHECK(result->shapes[0].points.size() == 3);  // MultiPoint -> 3 points
    CHECK(result->shapes[1].lines.size() == 2);   // MultiLineString -> 2 lines
    CHECK(result->shapes[2].polygons.size() == 2); // MultiPolygon -> 2 polygons
}

TEST_CASE("Load Polygon with hole") {
    std::string geojson = R"({
        "type": "Feature",
        "properties": {},
        "geometry": {
            "type": "Polygon",
            "coordinates": [
                [[0, 0], [10, 0], [10, 10], [0, 10], [0, 0]],
                [[2, 2], [8, 2], [8, 8], [2, 8], [2, 2]]
            ]
        }
    })";

    auto result = loadGeoJSONFromString(geojson);
    REQUIRE(result.has_value());
    REQUIRE(result->shapes.size() == 1);
    REQUIRE(result->shapes[0].polygons.size() == 1);
    CHECK(result->shapes[0].polygons[0].size() == 2); // Outer + 1 hole
}

TEST_CASE("Coordinate conversion round-trip") {
    std::string geojson = R"({
        "type": "Feature",
        "properties": {"name": "Stockholm"},
        "geometry": {
            "type": "Point",
            "coordinates": [18.0686, 59.3293, 10.0]
        }
    })";

    auto result = loadGeoJSONFromString(geojson);
    REQUIRE(result.has_value());

    const auto& original = result->shapes[0];
    double origLon = original.points[0].pos.x;
    double origLat = original.points[0].pos.y;
    double origAlt = original.points[0].pos.z;

    // Convert to ENU using a reference point
    earth::GeoReference ref(earth::GeoCoord(59.0, 18.0, 0));
    auto enu = conv_cs_wgs84_to_enu(original, ref);

    // ENU coordinates should be non-zero (point is offset from reference)
    CHECK(std::abs(enu.points[0].pos.x) > 100); // East offset
    CHECK(std::abs(enu.points[0].pos.y) > 100); // North offset

    // Convert back to WGS84
    auto roundtrip = conv_cs_enu_to_wgs84(enu, ref);
    CHECK(approxEqual(roundtrip.points[0].pos.x, origLon, 1e-10));
    CHECK(approxEqual(roundtrip.points[0].pos.y, origLat, 1e-10));
    CHECK(approxEqual(roundtrip.points[0].pos.z, origAlt, 1e-6));
}

TEST_CASE("Save and reload") {
    std::vector<Shape3> shapes;

    // Point
    Shape3 pointShape;
    pointShape.points.push_back(Pos3(18.0686, 59.3293, 0));
    pointShape.user_data = R"({"name": "Stockholm"})";
    shapes.push_back(pointShape);

    // LineString
    Shape3 lineShape;
    Polyline3 line;
    line.points.push_back(Pos3(18.0, 59.0, 0));
    line.points.push_back(Pos3(18.1, 59.1, 0));
    lineShape.lines.push_back(line);
    shapes.push_back(lineShape);

    std::string output = saveGeoJSONToString(shapes);
    CHECK(!output.empty());
    CHECK(output.find("FeatureCollection") != std::string::npos);

    auto reloaded = loadGeoJSONFromString(output);
    REQUIRE(reloaded.has_value());
    REQUIRE(reloaded->shapes.size() == 2);

    // Verify point
    REQUIRE(reloaded->shapes[0].points.size() == 1);
    CHECK(approxEqual(reloaded->shapes[0].points[0].pos.x, 18.0686));
    CHECK(approxEqual(reloaded->shapes[0].points[0].pos.y, 59.3293));

    // Verify line
    REQUIRE(reloaded->shapes[1].lines.size() == 1);
    CHECK(reloaded->shapes[1].lines[0].points.size() == 2);

    // Verify properties preserved
    CHECK(reloaded->shapes[0].user_data.find("Stockholm") != std::string::npos);
}

TEST_CASE("Load bare geometry (not wrapped in Feature)") {
    std::string geojson = R"({
        "type": "LineString",
        "coordinates": [[0, 0], [1, 1], [2, 2]]
    })";

    auto result = loadGeoJSONFromString(geojson);
    REQUIRE(result.has_value());
    REQUIRE(result->shapes.size() == 1);
    REQUIRE(result->shapes[0].lines.size() == 1);
    CHECK(result->shapes[0].lines[0].points.size() == 3);
}

TEST_CASE("Invalid JSON handling") {
    CHECK(!loadGeoJSONFromString("not json at all").has_value());
    CHECK(!loadGeoJSONFromString("{}").has_value());
    CHECK(!loadGeoJSONFromString(R"({"type": "Unknown"})").has_value());
}

TEST_CASE("Bounding box computation") {
    std::string geojson = R"({
        "type": "FeatureCollection",
        "features": [
            {
                "type": "Feature",
                "properties": {},
                "geometry": {
                    "type": "Point",
                    "coordinates": [0, 0]
                }
            },
            {
                "type": "Feature",
                "properties": {},
                "geometry": {
                    "type": "Point",
                    "coordinates": [10, 20]
                }
            }
        ]
    })";

    auto result = loadGeoJSONFromString(geojson);
    REQUIRE(result.has_value());

    CHECK(approxEqual(result->bounds.min.pos.x, 0));
    CHECK(approxEqual(result->bounds.min.pos.y, 0));
    CHECK(approxEqual(result->bounds.max.pos.x, 10));
    CHECK(approxEqual(result->bounds.max.pos.y, 20));
}

TEST_CASE("loadGeoJSON_enu convenience conversion") {
    std::string geojson = R"({
        "type": "Feature",
        "properties": {},
        "geometry": {
            "type": "Point",
            "coordinates": [18.0686, 59.3293]
        }
    })";

    auto wgs84 = loadGeoJSONFromString(geojson);
    REQUIRE(wgs84.has_value());

    earth::GeoReference ref(earth::GeoCoord(59.3293, 18.0686, 0)); // Reference at same point
    auto enu = conv_cs_wgs84_to_enu(wgs84->shapes[0], ref);

    // Point at reference should be at origin
    CHECK(std::abs(enu.points[0].pos.x) < 1.0); // Within 1m of origin
    CHECK(std::abs(enu.points[0].pos.y) < 1.0);
}
