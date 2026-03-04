/*******************************************************************************
 * File: lane_graph.hpp
 *
 * Description: Lane-to-graph conversion for road network topology analysis.
 * Converts lane geometry from JSON scenario files into a directed graph where
 * nodes represent lane junctions and edges represent lane segments.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/graph/graph.hpp"
#include <unordered_map>
#include <vector>

namespace geodraw {
namespace graph {

// =============================================================================
// Data Types
// =============================================================================

/**
 * 3D point representing a position in lane geometry.
 */
struct LanePoint {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/**
 * Lane data extracted from JSON scenario files.
 */
struct LaneData {
    int64_t id = 0;                   ///< Lane ID from JSON
    int64_t mapElementId = 0;         ///< Map element ID from JSON
    std::vector<LanePoint> geometry;  ///< Polyline geometry points
};

/**
 * Configuration options for lane graph construction.
 */
struct LaneGraphOptions {
    double connectionThreshold = 2.0;  ///< Max distance (meters) to consider endpoints connected
};

/**
 * Result of lane graph construction.
 */
struct GEODRAW_API LaneGraphResult {
    Graph graph;                                    ///< The constructed directed graph
    std::unordered_map<EdgeId, int64_t> edgeToLaneId;  ///< Map edge back to JSON lane ID
};

/**
 * Complexity analysis result for a lane network.
 */
struct GEODRAW_API ComplexityScore {
    double score = 0.0;        ///< Overall normalized score [0, 1]
    int mergeCount = 0;        ///< Number of merge points (in_degree > 1)
    int splitCount = 0;        ///< Number of split points (out_degree > 1)
    int turnCount = 0;         ///< Number of sharp turns (heading change > 45 deg)
    double avgCurvature = 0.0; ///< Average curvature across all lanes (1/m)
    double networkDensity = 0.0; ///< Edges / nodes ratio
};

// =============================================================================
// Functions
// =============================================================================

/**
 * Build a directed graph from lane geometry.
 *
 * Nodes represent lane junctions (points where lane endpoints cluster).
 * Edges represent lane segments connecting junctions.
 *
 * Node metadata:
 *   - x, y, z (double): 3D position of junction
 *   - is_merge (bool): True if in_degree > 1
 *   - is_split (bool): True if out_degree > 1
 *
 * Edge metadata:
 *   - lane_id (int64_t): Original lane ID from JSON
 *   - length (double): Lane segment length in meters
 *   - max_curvature (double): Maximum curvature along segment (1/m)
 *   - avg_curvature (double): Average curvature along segment (1/m)
 *   - total_heading_change (double): Total heading change in radians
 *   - is_turn (bool): True if total_heading_change > PI/4 (45 degrees)
 *
 * @param lanes Vector of lane data (from JSON)
 * @param options Configuration options
 * @return Graph with nodes at junctions and edges for lane segments
 */
GEODRAW_API LaneGraphResult buildLaneGraph(
    const std::vector<LaneData>& lanes,
    const LaneGraphOptions& options = {});

/**
 * Compute complexity score for a lane network graph.
 *
 * The complexity score combines multiple factors:
 *   - Merge count: Number of points where multiple lanes converge
 *   - Split count: Number of points where lanes diverge
 *   - Turn count: Number of sharp turns (heading change > 45 deg)
 *   - Average curvature: Mean curvature across all lanes
 *   - Network density: Ratio of edges to nodes
 *
 * Formula:
 *   score = w1 * normalized(mergeCount)
 *         + w2 * normalized(splitCount)
 *         + w3 * normalized(turnCount)
 *         + w4 * normalized(avgCurvature)
 *         + w5 * normalized(networkDensity)
 *
 * Default weights: w1=0.25, w2=0.25, w3=0.20, w4=0.15, w5=0.15
 *
 * @param laneGraph Graph built from buildLaneGraph
 * @return ComplexityScore with individual metrics and overall score
 */
GEODRAW_API ComplexityScore computeComplexity(const Graph& laneGraph);

} // namespace graph
} // namespace geodraw
