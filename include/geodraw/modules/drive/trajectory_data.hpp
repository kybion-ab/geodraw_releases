/*******************************************************************************
 * File : trajectory_data.hpp
 *
 * Description :
 *
 * Author  : Magnus Nilsson
 * Date    : 2026-01-29
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"

#include "geodraw/modules/drive/geometry_road.hpp"

#include <optional>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace geodraw {

// VehicleTrajectory - Time-series data for a single vehicle
struct GEODRAW_API VehicleTrajectory {
    int id;
    std::vector<glm::dvec3> positions;
    std::vector<double> headings;
    std::vector<glm::dvec2> velocities;
    std::vector<bool> valid;
    double width, length, height;
    std::optional<glm::dvec3> goalPosition;  // Target destination
    int mark_as_expert = 0;  // PufferDrive: 1 if this is an expert trajectory

  bool isValidAt(size_t timestep) const;

  /// Returns velocities[ts] if ts is in range, otherwise glm::dvec2(0.0).
  /// Eliminates scatter-site bounds checks against velocities.size().
  glm::dvec2 velocityAt(size_t ts) const;

  size_t numTimesteps() const;
};

// LaneGeometry - Geometric representation of a lane
struct LaneGeometry {
    int id;
    std::vector<glm::dvec3> centerline;
    double width;
    double length = 0.0;   // PufferDrive: road segment length (0.0 = auto/default)
    double height = 0.0;   // PufferDrive: road segment height (0.0 = flat)

    // Convert to Lane for mesh generation
    Lane toRoadLane() const;
};

// RoadLine - Road marking line geometry
struct GEODRAW_API RoadLine {
    int id;
    int map_element_id;
    std::vector<glm::dvec3> geometry;
    double width = 0.1;    // PufferDrive: line width
    double length = 0.0;   // PufferDrive: road segment length (0.0 = auto/default)
    double height = 0.0;   // PufferDrive: road segment height (0.0 = flat)

    Polyline3 toPolyline3() const;
};

// RoadEdge - Road boundary edge geometry
struct GEODRAW_API RoadEdge {
    int id;
    int map_element_id;
    std::vector<glm::dvec3> geometry;
    double width = 0.1;    // PufferDrive: edge width
    double length = 0.0;   // PufferDrive: road segment length (0.0 = auto/default)
    double height = 0.0;   // PufferDrive: road segment height (0.0 = flat)

    Polyline3 toPolyline3() const;
};

// SceneData - Complete trajectory scene
struct GEODRAW_API SceneData {
    std::string name;  // PufferDrive: scenario name identifier
    std::vector<VehicleTrajectory> vehicles;
    std::vector<LaneGeometry> lanes;
    std::vector<RoadLine> roadLines;
    std::vector<RoadEdge> roadEdges;

    size_t maxTimesteps() const;
    size_t minTimesteps() const;
    BBox3 computeBounds() const;
};

}  // namespace geodraw
