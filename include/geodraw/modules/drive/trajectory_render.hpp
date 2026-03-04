/*******************************************************************************
 * File : trajectory_render.hpp
 *
 * Description :
 *
 * Author  : Magnus Nilsson
 * Date    : 2026-01-29
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"

#include "geodraw/geometry/geometry.hpp"
#include "geodraw/scene/scene.hpp"
#include "geodraw/modules/drive/trajectory_data.hpp"
#include "geodraw/graph/lane_graph.hpp"

// Forward declaration to avoid circular dependency
namespace geodraw::earth {
struct GeoReference;
}

namespace geodraw {

// LaneArrow - Single direction arrow on a lane
struct GEODRAW_API LaneArrow {
    Pos3 position;           // Center position on lane
    glm::dvec3 direction;    // Unit vector in lane direction
    double size;             // Arrow size (derived from lane width)
};

// LaneArrows - All arrows for a single lane
struct GEODRAW_API LaneArrows {
    int laneId;
    std::vector<LaneArrow> arrows;
};

// Generate arrows for a single lane at arc-length intervals
GEODRAW_API std::vector<LaneArrow> generateLaneArrows(
    const LaneGeometry& lane,
    double intervalMeters = 15.0,
    double arrowSizeRatio = 0.5);

// Generate arrows for all lanes
GEODRAW_API std::vector<LaneArrows> generateAllLaneArrows(
    const std::vector<LaneGeometry>& lanes,
    double intervalMeters = 15.0,
    double arrowSizeRatio = 0.5);

// Render chevron arrow to scene
GEODRAW_API void renderLaneArrow(Scene& scene,
                                 const LaneArrow& arrow,
                                 const glm::vec3& color,
                                 float thickness = 2.0f);

// Render all arrows for a lane
GEODRAW_API void renderLaneArrows(Scene& scene,
                                  const LaneArrows& arrows,
                                  const glm::vec3& color,
                                  float thickness = 2.0f);

// Options for lane rendering
struct GEODRAW_API LaneRenderOptions {
    bool showLaneMeshes = true;   // Set to false if app renders lanes separately (e.g., edit mode)
    bool showArrows = true;
    glm::vec3 laneColor = glm::vec3(0.4f, 0.4f, 0.4f);
    glm::vec3 arrowColor = glm::vec3(1.0f, 1.0f, 1.0f);
    float laneAlpha = 0.8f;
    float laneThickness = 2.0f;
    float arrowThickness = 2.0f;
    double altitudeOffset = 0.0;  // Z offset for globe mode (avoids z-fighting)
};

// Unified lane rendering function
// For local/ENU coordinates: pass geoRef = nullptr
// For globe mode (ENU->ECEF transform): pass a valid GeoReference pointer
GEODRAW_API void renderLanes(Scene& scene,
                             const std::vector<Mesh3>& laneMeshes,
                             const std::vector<LaneArrows>& laneArrows,
                             const LaneRenderOptions& options = {},
                             const earth::GeoReference* geoRef = nullptr);

GEODRAW_API RBox3 generateVehicleBox(const VehicleTrajectory& vehicle, size_t timestep);

GEODRAW_API Line3 generateVelocityArrow(const VehicleTrajectory& vehicle,
                                   size_t timestep,
                            double arrowScaleFactor);

GEODRAW_API std::vector<Mesh3> generateLaneMeshes(const std::vector<LaneGeometry>& lanes);

GEODRAW_API glm::vec3 velocityToColor(const glm::dvec2& velocity,
                          double minSpeed,
                          double maxSpeed);

GEODRAW_API void renderOrientedBox(Scene& scene,
                       const RBox3& box,
                       const glm::vec3& color,
                       float thickness = 2.0f);

GEODRAW_API Mesh3 generateRoadEdgeRibbon(const std::vector<glm::dvec3>& geometry, double width = 0.1);

// Options for lane graph rendering
struct GEODRAW_API LaneGraphRenderOptions {
    bool showEdges = true;
    bool showNodes = true;
    bool showArrows = true;
    glm::vec3 edgeColor = glm::vec3(0.0f, 0.8f, 1.0f);       // Cyan
    glm::vec3 startNodeColor = glm::vec3(0.2f, 0.9f, 0.3f);  // Green (direction indicator)
    glm::vec3 endNodeColor = glm::vec3(0.9f, 0.3f, 0.2f);    // Red
    glm::vec3 mergeNodeColor = glm::vec3(1.0f, 0.6f, 0.0f);  // Orange
    glm::vec3 splitNodeColor = glm::vec3(0.0f, 0.8f, 0.8f);  // Cyan
    float edgeThickness = 2.5f;
    float nodeMarkerSize = 1.2f;
    double altitudeOffset = 0.3;
};

// Pre-computed visualization data for lane graph
struct GEODRAW_API LaneGraphViz {
    struct NodeViz {
        glm::dvec3 position;
        bool isMerge;
        bool isSplit;
        size_t inDegree;   // Number of incoming edges
        size_t outDegree;  // Number of outgoing edges
        size_t nodeIndex;  // Index in the nodes vector (for tooltip ID)
    };
    struct EdgeViz {
        glm::dvec3 start;  // Source node position
        glm::dvec3 end;    // Target node position
        std::vector<glm::dvec3> polyline;  // Full lane centerline
        int64_t laneId;
    };
    std::vector<NodeViz> nodes;
    std::vector<EdgeViz> edges;
};

// Build visualization data from lane graph
GEODRAW_API LaneGraphViz buildLaneGraphViz(
    const graph::LaneGraphResult& graphResult,
    const std::vector<LaneGeometry>& lanes);

// Render lane graph to scene
// geoRef = nullptr for LOCAL, valid pointer for ECEF
GEODRAW_API void renderLaneGraph(
    Scene& scene,
    const LaneGraphViz& viz,
    const LaneGraphRenderOptions& options = {},
    const earth::GeoReference* geoRef = nullptr);

// Result of computing a lane-following path from vehicle to goal
struct GEODRAW_API GoalPathResult {
    bool found = false;
    std::vector<glm::dvec3> polyline;  // Full path as connected points
    graph::NodeId startNode = graph::InvalidNodeId;
    graph::NodeId endNode = graph::InvalidNodeId;
};

// Options for rendering goal paths
struct GEODRAW_API GoalPathRenderOptions {
    glm::vec3 color = glm::vec3(0.3f, 0.8f, 0.4f);
    float thickness = 2.0f;
    float alpha = 0.6f;
    double altitudeOffset = 0.1;
};

// Find the closest graph node to a given position
// Returns InvalidNodeId if no node within maxDistance
GEODRAW_API graph::NodeId findClosestGraphNode(
    const glm::dvec3& position,
    const graph::LaneGraphResult& graphResult,
    double maxDistance = 50.0);

// Compute a lane-following path from vehicle position to goal position
// Uses Dijkstra's algorithm with lane length weights
GEODRAW_API GoalPathResult computeGoalPath(
    const glm::dvec3& vehiclePos,
    const glm::dvec3& goalPos,
    const graph::LaneGraphResult& graphResult,
    const LaneGraphViz& viz,
    const std::vector<LaneGeometry>& lanes);

// Render a goal path to the scene as a dashed polyline
GEODRAW_API void renderGoalPath(
    Scene& scene,
    const GoalPathResult& path,
    const GoalPathRenderOptions& options = {},
    const earth::GeoReference* geoRef = nullptr);

}
