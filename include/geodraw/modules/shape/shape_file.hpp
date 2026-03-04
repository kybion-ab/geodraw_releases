/*******************************************************************************
 * File: shape_file.hpp
 *
 * Description: Header-only module for saving and loading .shape files.
 * Stores raw 3D coordinates (no coordinate system conversion).
 * Format is JSON-based and coordinate-system agnostic.
 *
 * File format:
 *   {
 *     "version": 1,
 *     "coordSystem": "LOCAL",
 *     "polygons": [ [ [[x,y,z],...], [[x,y,z],...] ] ],
 *     "lines":    [ [[x,y,z],...] ],
 *     "points":   [[x,y,z],...],
 *     "user_data": ""
 *   }
 *
 ******************************************************************************/

#pragma once

#include <fstream>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "geodraw/geometry/geometry.hpp"

namespace geodraw {
namespace shapefile {

// =============================================================================
// Internal helpers
// =============================================================================

namespace detail {

inline nlohmann::json pointToJson(const glm::dvec3& p) {
    return nlohmann::json::array({p.x, p.y, p.z});
}

inline nlohmann::json pos3ToJson(const Pos3& p) {
    return nlohmann::json::array({p.pos.x, p.pos.y, p.pos.z});
}

inline glm::dvec3 jsonToPoint(const nlohmann::json& j) {
    return glm::dvec3(j[0].get<double>(), j[1].get<double>(), j[2].get<double>());
}

inline Pos3 jsonToPos3(const nlohmann::json& j) {
    return Pos3(j[0].get<double>(), j[1].get<double>(), j[2].get<double>());
}

inline nlohmann::json polylineToJson(const Polyline3& line) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& pt : line.points) {
        arr.push_back(pos3ToJson(pt));
    }
    return arr;
}

inline Polyline3 jsonToPolyline(const nlohmann::json& j) {
    Polyline3 line;
    for (const auto& pt : j) {
        line.points.push_back(jsonToPos3(pt));
    }
    return line;
}

} // namespace detail

// =============================================================================
// Public API
// =============================================================================

/**
 * Save a Shape3 to a .shape file (raw coordinates, no conversion).
 *
 * @param path   Output file path
 * @param shape  Shape to save
 * @return true on success
 */
inline bool saveShape(const std::string& path, const Shape3& shape) {
    nlohmann::json j;
    j["version"] = 1;
    j["coordSystem"] = "LOCAL";
    j["user_data"] = shape.user_data;

    // Polygons: array of polygons, each polygon is array of rings,
    // each ring is array of [x,y,z] points.
    nlohmann::json polygons = nlohmann::json::array();
    for (const auto& polygon : shape.polygons) {
        nlohmann::json rings = nlohmann::json::array();
        for (const auto& ring : polygon) {
            rings.push_back(detail::polylineToJson(ring));
        }
        polygons.push_back(rings);
    }
    j["polygons"] = polygons;

    // Lines
    nlohmann::json lines = nlohmann::json::array();
    for (const auto& line : shape.lines) {
        lines.push_back(detail::polylineToJson(line));
    }
    j["lines"] = lines;

    // Isolated points
    nlohmann::json points = nlohmann::json::array();
    for (const auto& pt : shape.points) {
        points.push_back(detail::pos3ToJson(pt));
    }
    j["points"] = points;

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << j.dump(2);
    file.close();
    return file.good();
}

/**
 * Load a Shape3 from a .shape file.
 *
 * @param path  Input file path
 * @return Shape3 on success, nullopt on failure
 */
inline std::optional<Shape3> loadShape(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }

    Shape3 shape;

    if (j.contains("user_data") && j["user_data"].is_string()) {
        shape.user_data = j["user_data"].get<std::string>();
    }

    // Polygons
    if (j.contains("polygons") && j["polygons"].is_array()) {
        for (const auto& polygon : j["polygons"]) {
            std::vector<Polyline3> rings;
            for (const auto& ring : polygon) {
                rings.push_back(detail::jsonToPolyline(ring));
            }
            if (!rings.empty()) {
                shape.polygons.push_back(rings);
            }
        }
    }

    // Lines
    if (j.contains("lines") && j["lines"].is_array()) {
        for (const auto& line : j["lines"]) {
            shape.lines.push_back(detail::jsonToPolyline(line));
        }
    }

    // Isolated points
    if (j.contains("points") && j["points"].is_array()) {
        for (const auto& pt : j["points"]) {
            shape.points.push_back(detail::jsonToPos3(pt));
        }
    }

    return shape;
}

} // namespace shapefile
} // namespace geodraw
