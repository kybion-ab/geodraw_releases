/*******************************************************************************
 * File: geojson.hpp
 *
 * Description: Header-only module for loading and saving GeoJSON files.
 * Converts between GeoJSON Features and GeoDraw's Shape3 format.
 * Provides coordinate conversion between WGS84 and local ENU coordinates.
 *
 * Why required: Enables loading and rendering of standard geospatial data
 * files in GeoJSON format, widely used for geographic data exchange.
 *
 *
 ******************************************************************************/

#pragma once

#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "geodraw/geometry/geometry.hpp"
#include "geodraw/modules/earth/earth_coords.hpp"

namespace geodraw {
namespace geojson {

// =============================================================================
// Types
// =============================================================================

/**
 * Result of loading a GeoJSON file
 */
struct LoadedGeoJSON {
    std::vector<Shape3> shapes;  // One per Feature
    BBox3 bounds;                // Combined bounds (in loaded coordinates)

    bool isValid() const { return !shapes.empty(); }
};

// =============================================================================
// Internal Helpers
// =============================================================================

namespace detail {

/**
 * Update bounding box with a position
 */
inline void updateBounds(BBox3& bbox, const Pos3& pos, bool& hasBounds) {
    if (!hasBounds) {
        bbox.min = pos;
        bbox.max = pos;
        hasBounds = true;
    } else {
        bbox.min.pos = glm::min(bbox.min.pos, pos.pos);
        bbox.max.pos = glm::max(bbox.max.pos, pos.pos);
    }
}

/**
 * Parse a GeoJSON coordinate array [lon, lat] or [lon, lat, alt]
 * Returns Pos3 where x=lon, y=lat, z=alt
 */
inline Pos3 parseCoordinate(const nlohmann::json& coord) {
    double lon = coord[0].get<double>();
    double lat = coord[1].get<double>();
    double alt = (coord.size() > 2) ? coord[2].get<double>() : 0.0;
    return Pos3(lon, lat, alt);
}

/**
 * Parse array of coordinates into a Polyline3
 */
inline Polyline3 parseCoordinateArray(const nlohmann::json& coords) {
    Polyline3 polyline;
    for (const auto& coord : coords) {
        polyline.points.push_back(parseCoordinate(coord));
    }
    return polyline;
}

/**
 * Parse a GeoJSON Point geometry
 */
inline void parsePoint(const nlohmann::json& geometry, Shape3& shape) {
    shape.points.push_back(parseCoordinate(geometry["coordinates"]));
}

/**
 * Parse a GeoJSON MultiPoint geometry
 */
inline void parseMultiPoint(const nlohmann::json& geometry, Shape3& shape) {
    for (const auto& coord : geometry["coordinates"]) {
        shape.points.push_back(parseCoordinate(coord));
    }
}

/**
 * Parse a GeoJSON LineString geometry
 */
inline void parseLineString(const nlohmann::json& geometry, Shape3& shape) {
    shape.lines.push_back(parseCoordinateArray(geometry["coordinates"]));
}

/**
 * Parse a GeoJSON MultiLineString geometry
 */
inline void parseMultiLineString(const nlohmann::json& geometry, Shape3& shape) {
    for (const auto& line : geometry["coordinates"]) {
        shape.lines.push_back(parseCoordinateArray(line));
    }
}

/**
 * Parse a GeoJSON Polygon geometry
 * First ring is outer boundary, rest are holes
 */
inline void parsePolygon(const nlohmann::json& geometry, Shape3& shape) {
    std::vector<Polyline3> polygon;
    for (const auto& ring : geometry["coordinates"]) {
        polygon.push_back(parseCoordinateArray(ring));
    }
    if (!polygon.empty()) {
        shape.polygons.push_back(polygon);
    }
}

/**
 * Parse a GeoJSON MultiPolygon geometry
 */
inline void parseMultiPolygon(const nlohmann::json& geometry, Shape3& shape) {
    for (const auto& poly : geometry["coordinates"]) {
        std::vector<Polyline3> polygon;
        for (const auto& ring : poly) {
            polygon.push_back(parseCoordinateArray(ring));
        }
        if (!polygon.empty()) {
            shape.polygons.push_back(polygon);
        }
    }
}

/**
 * Parse a GeoJSON geometry (any type)
 */
inline void parseGeometry(const nlohmann::json& geometry, Shape3& shape) {
    if (!geometry.contains("type")) {
        return;
    }

    std::string type = geometry["type"].get<std::string>();

    if (type == "Point") {
        parsePoint(geometry, shape);
    } else if (type == "MultiPoint") {
        parseMultiPoint(geometry, shape);
    } else if (type == "LineString") {
        parseLineString(geometry, shape);
    } else if (type == "MultiLineString") {
        parseMultiLineString(geometry, shape);
    } else if (type == "Polygon") {
        parsePolygon(geometry, shape);
    } else if (type == "MultiPolygon") {
        parseMultiPolygon(geometry, shape);
    } else if (type == "GeometryCollection") {
        if (geometry.contains("geometries")) {
            for (const auto& geom : geometry["geometries"]) {
                parseGeometry(geom, shape);
            }
        }
    }
}

/**
 * Compute bounding box for a shape
 */
inline void computeShapeBounds(Shape3& shape) {
    bool hasBounds = false;

    // Points
    for (const auto& pt : shape.points) {
        updateBounds(shape.bbox, pt, hasBounds);
    }

    // Lines
    for (const auto& line : shape.lines) {
        for (const auto& pt : line.points) {
            updateBounds(shape.bbox, pt, hasBounds);
        }
    }

    // Polygons
    for (const auto& polygon : shape.polygons) {
        for (const auto& ring : polygon) {
            for (const auto& pt : ring.points) {
                updateBounds(shape.bbox, pt, hasBounds);
            }
        }
    }
}

/**
 * Convert coordinate to GeoJSON array
 */
inline nlohmann::json coordinateToJson(const Pos3& pos) {
    if (std::abs(pos.pos.z) < 1e-10) {
        return nlohmann::json::array({pos.pos.x, pos.pos.y});
    }
    return nlohmann::json::array({pos.pos.x, pos.pos.y, pos.pos.z});
}

/**
 * Convert polyline to GeoJSON coordinate array
 */
inline nlohmann::json polylineToJson(const Polyline3& polyline) {
    nlohmann::json coords = nlohmann::json::array();
    for (const auto& pt : polyline.points) {
        coords.push_back(coordinateToJson(pt));
    }
    return coords;
}

/**
 * Determine geometry type and create GeoJSON geometry object
 */
inline nlohmann::json shapeToGeometry(const Shape3& shape) {
    bool hasPolygons = !shape.polygons.empty();
    bool hasLines = !shape.lines.empty();
    bool hasPoints = !shape.points.empty();

    int typeCount = (hasPolygons ? 1 : 0) + (hasLines ? 1 : 0) + (hasPoints ? 1 : 0);

    // GeometryCollection for mixed types
    if (typeCount > 1) {
        nlohmann::json geometries = nlohmann::json::array();

        // Add polygons
        for (const auto& polygon : shape.polygons) {
            nlohmann::json rings = nlohmann::json::array();
            for (const auto& ring : polygon) {
                rings.push_back(polylineToJson(ring));
            }
            geometries.push_back({{"type", "Polygon"}, {"coordinates", rings}});
        }

        // Add lines
        for (const auto& line : shape.lines) {
            geometries.push_back({{"type", "LineString"}, {"coordinates", polylineToJson(line)}});
        }

        // Add points
        for (const auto& pt : shape.points) {
            geometries.push_back({{"type", "Point"}, {"coordinates", coordinateToJson(pt)}});
        }

        return {{"type", "GeometryCollection"}, {"geometries", geometries}};
    }

    // Single type
    if (hasPolygons) {
        if (shape.polygons.size() == 1) {
            nlohmann::json rings = nlohmann::json::array();
            for (const auto& ring : shape.polygons[0]) {
                rings.push_back(polylineToJson(ring));
            }
            return {{"type", "Polygon"}, {"coordinates", rings}};
        } else {
            nlohmann::json multiPoly = nlohmann::json::array();
            for (const auto& polygon : shape.polygons) {
                nlohmann::json rings = nlohmann::json::array();
                for (const auto& ring : polygon) {
                    rings.push_back(polylineToJson(ring));
                }
                multiPoly.push_back(rings);
            }
            return {{"type", "MultiPolygon"}, {"coordinates", multiPoly}};
        }
    }

    if (hasLines) {
        if (shape.lines.size() == 1) {
            return {{"type", "LineString"}, {"coordinates", polylineToJson(shape.lines[0])}};
        } else {
            nlohmann::json multiLine = nlohmann::json::array();
            for (const auto& line : shape.lines) {
                multiLine.push_back(polylineToJson(line));
            }
            return {{"type", "MultiLineString"}, {"coordinates", multiLine}};
        }
    }

    if (hasPoints) {
        if (shape.points.size() == 1) {
            return {{"type", "Point"}, {"coordinates", coordinateToJson(shape.points[0])}};
        } else {
            nlohmann::json multiPoint = nlohmann::json::array();
            for (const auto& pt : shape.points) {
                multiPoint.push_back(coordinateToJson(pt));
            }
            return {{"type", "MultiPoint"}, {"coordinates", multiPoint}};
        }
    }

    // Empty geometry
    return {{"type", "Point"}, {"coordinates", nlohmann::json::array({0, 0})}};
}

} // namespace detail

// =============================================================================
// Loading Functions
// =============================================================================

/**
 * Load GeoJSON from string
 *
 * Parses GeoJSON and converts Features to Shape3 objects.
 * Coordinates stored as: x=longitude, y=latitude, z=altitude
 *
 * @param json GeoJSON string
 * @return LoadedGeoJSON with shapes in WGS84 coordinates
 */
inline std::optional<LoadedGeoJSON> loadGeoJSONFromString(const std::string& json) {
    LoadedGeoJSON result;
    bool hasBounds = false;

    try {
        nlohmann::json j = nlohmann::json::parse(json);

        if (!j.contains("type")) {
            return std::nullopt;
        }

        std::string type = j["type"].get<std::string>();

        // Handle FeatureCollection
        if (type == "FeatureCollection") {
            if (!j.contains("features")) {
                return std::nullopt;
            }

            for (const auto& feature : j["features"]) {
                Shape3 shape;

                // Parse geometry
                if (feature.contains("geometry") && !feature["geometry"].is_null()) {
                    detail::parseGeometry(feature["geometry"], shape);
                }

                // Store properties as JSON string in user_data
                if (feature.contains("properties") && !feature["properties"].is_null()) {
                    shape.user_data = feature["properties"].dump();
                }

                if (!shape.isEmpty()) {
                    detail::computeShapeBounds(shape);
                    detail::updateBounds(result.bounds, shape.bbox.min, hasBounds);
                    detail::updateBounds(result.bounds, shape.bbox.max, hasBounds);
                    result.shapes.push_back(std::move(shape));
                }
            }
        }
        // Handle single Feature
        else if (type == "Feature") {
            Shape3 shape;

            if (j.contains("geometry") && !j["geometry"].is_null()) {
                detail::parseGeometry(j["geometry"], shape);
            }

            if (j.contains("properties") && !j["properties"].is_null()) {
                shape.user_data = j["properties"].dump();
            }

            if (!shape.isEmpty()) {
                detail::computeShapeBounds(shape);
                result.bounds = shape.bbox;
                result.shapes.push_back(std::move(shape));
            }
        }
        // Handle bare Geometry
        else if (type == "Point" || type == "MultiPoint" ||
                 type == "LineString" || type == "MultiLineString" ||
                 type == "Polygon" || type == "MultiPolygon" ||
                 type == "GeometryCollection") {
            Shape3 shape;
            detail::parseGeometry(j, shape);

            if (!shape.isEmpty()) {
                detail::computeShapeBounds(shape);
                result.bounds = shape.bbox;
                result.shapes.push_back(std::move(shape));
            }
        }
        else {
            return std::nullopt;
        }

    } catch (const nlohmann::json::exception& e) {
        return std::nullopt;
    }

    if (result.shapes.empty()) {
        return std::nullopt;
    }

    return result;
}

/**
 * Load GeoJSON from file
 *
 * Coordinates stored as: x=longitude, y=latitude, z=altitude
 *
 * @param path Path to .geojson or .json file
 * @return LoadedGeoJSON with shapes in WGS84 coordinates
 */
inline std::optional<LoadedGeoJSON> loadGeoJSON(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    return loadGeoJSONFromString(content);
}

// =============================================================================
// Saving Functions
// =============================================================================

/**
 * Save shapes to GeoJSON string
 *
 * Assumes coordinates are: x=longitude, y=latitude, z=altitude
 *
 * @param shapes Vector of Shape3 to save (one Feature per Shape3)
 * @return GeoJSON string
 */
inline std::string saveGeoJSONToString(const std::vector<Shape3>& shapes) {
    nlohmann::json features = nlohmann::json::array();

    for (const auto& shape : shapes) {
        nlohmann::json feature;
        feature["type"] = "Feature";
        feature["geometry"] = detail::shapeToGeometry(shape);

        // Parse user_data as JSON for properties, or use empty object
        if (!shape.user_data.empty()) {
            try {
                feature["properties"] = nlohmann::json::parse(shape.user_data);
            } catch (const nlohmann::json::exception&) {
                feature["properties"] = nlohmann::json::object();
            }
        } else {
            feature["properties"] = nlohmann::json::object();
        }

        features.push_back(feature);
    }

    nlohmann::json geojson;
    geojson["type"] = "FeatureCollection";
    geojson["features"] = features;

    return geojson.dump(2);  // Pretty print with 2-space indent
}

/**
 * Save shapes to GeoJSON file
 *
 * Assumes coordinates are: x=longitude, y=latitude, z=altitude
 *
 * @param path Output file path
 * @param shapes Vector of Shape3 to save (one Feature per Shape3)
 * @return true on success
 */
inline bool saveGeoJSON(const std::string& path, const std::vector<Shape3>& shapes) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    file << saveGeoJSONToString(shapes);
    file.close();

    return file.good();
}

// =============================================================================
// Coordinate System Conversion (conv_cs pattern)
// =============================================================================

/**
 * Convert Shape3 from WGS84 to local ENU coordinates
 *
 * Input: x=longitude, y=latitude, z=altitude (WGS84)
 * Output: x=east, y=north, z=up (ENU relative to reference)
 *
 * @param shape Shape in WGS84 coordinates
 * @param ref GeoReference defining the ENU origin
 * @return New Shape3 in ENU coordinates
 */
inline Shape3 conv_cs_wgs84_to_enu(const Shape3& shape, const earth::GeoReference& ref) {
    Shape3 result;
    result.user_data = shape.user_data;

    // Convert all points
    for (const auto& pt : shape.points) {
        earth::GeoCoord geo(pt.pos.y, pt.pos.x, pt.pos.z);  // lat, lon, alt
        earth::ENUCoord enu = earth::geoToENU(geo.toRadians(), ref);
        result.points.push_back(Pos3(enu.east, enu.north, enu.up));
    }

    // Convert all polylines
    for (const auto& line : shape.lines) {
        Polyline3 converted;
        for (const auto& pt : line.points) {
            earth::GeoCoord geo(pt.pos.y, pt.pos.x, pt.pos.z);
            earth::ENUCoord enu = earth::geoToENU(geo.toRadians(), ref);
            converted.points.push_back(Pos3(enu.east, enu.north, enu.up));
        }
        result.lines.push_back(converted);
    }

    // Convert all polygons
    for (const auto& polygon : shape.polygons) {
        std::vector<Polyline3> convertedPoly;
        for (const auto& ring : polygon) {
            Polyline3 convertedRing;
            for (const auto& pt : ring.points) {
                earth::GeoCoord geo(pt.pos.y, pt.pos.x, pt.pos.z);
                earth::ENUCoord enu = earth::geoToENU(geo.toRadians(), ref);
                convertedRing.points.push_back(Pos3(enu.east, enu.north, enu.up));
            }
            convertedPoly.push_back(convertedRing);
        }
        result.polygons.push_back(convertedPoly);
    }

    // Recompute bounds in new coordinate system
    detail::computeShapeBounds(result);

    return result;
}

/**
 * Convert Shape3 from local ENU to WGS84 coordinates
 *
 * Input: x=east, y=north, z=up (ENU)
 * Output: x=longitude, y=latitude, z=altitude (WGS84)
 *
 * @param shape Shape in ENU coordinates
 * @param ref GeoReference defining the ENU origin
 * @return New Shape3 in WGS84 coordinates
 */
inline Shape3 conv_cs_enu_to_wgs84(const Shape3& shape, const earth::GeoReference& ref) {
    Shape3 result;
    result.user_data = shape.user_data;

    // Convert all points
    for (const auto& pt : shape.points) {
        earth::ENUCoord enu(pt.pos.x, pt.pos.y, pt.pos.z);  // east, north, up
        earth::WGS84d geo = earth::enuToGeo(enu, ref).toDegrees();
        result.points.push_back(Pos3(geo.longitude, geo.latitude, geo.altitude));
    }

    // Convert all polylines
    for (const auto& line : shape.lines) {
        Polyline3 converted;
        for (const auto& pt : line.points) {
            earth::ENUCoord enu(pt.pos.x, pt.pos.y, pt.pos.z);
            earth::WGS84d geo = earth::enuToGeo(enu, ref).toDegrees();
            converted.points.push_back(Pos3(geo.longitude, geo.latitude, geo.altitude));
        }
        result.lines.push_back(converted);
    }

    // Convert all polygons
    for (const auto& polygon : shape.polygons) {
        std::vector<Polyline3> convertedPoly;
        for (const auto& ring : polygon) {
            Polyline3 convertedRing;
            for (const auto& pt : ring.points) {
                earth::ENUCoord enu(pt.pos.x, pt.pos.y, pt.pos.z);
                earth::WGS84d geo = earth::enuToGeo(enu, ref).toDegrees();
                convertedRing.points.push_back(Pos3(geo.longitude, geo.latitude, geo.altitude));
            }
            convertedPoly.push_back(convertedRing);
        }
        result.polygons.push_back(convertedPoly);
    }

    // Recompute bounds in new coordinate system
    detail::computeShapeBounds(result);

    return result;
}

/**
 * Convenience: Load GeoJSON and convert to ENU in one step
 *
 * @param path Path to .geojson or .json file
 * @param ref GeoReference defining the ENU origin
 * @return LoadedGeoJSON with shapes in ENU coordinates
 */
inline std::optional<LoadedGeoJSON> loadGeoJSON_enu(const std::string& path,
                                                     const earth::GeoReference& ref) {
    auto wgs84Result = loadGeoJSON(path);
    if (!wgs84Result) {
        return std::nullopt;
    }

    LoadedGeoJSON result;
    bool hasBounds = false;

    for (const auto& shape : wgs84Result->shapes) {
        Shape3 enuShape = conv_cs_wgs84_to_enu(shape, ref);
        detail::updateBounds(result.bounds, enuShape.bbox.min, hasBounds);
        detail::updateBounds(result.bounds, enuShape.bbox.max, hasBounds);
        result.shapes.push_back(std::move(enuShape));
    }

    return result;
}

} // namespace geojson
} // namespace geodraw
