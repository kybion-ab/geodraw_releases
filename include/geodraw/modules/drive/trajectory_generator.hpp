/*******************************************************************************
 * File: trajectory_generator.hpp
 *
 * Description: Generate full trajectories for vehicles by following lane
 * geometry from start position to goal position. Integrates velocity at each
 * timestep with speed adjusted based on lane curvature, merges, and splits.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/modules/drive/trajectory_data.hpp"
#include "geodraw/modules/drive/vehicle_generator.hpp"
#include "geodraw/graph/lane_graph.hpp"

namespace geodraw {

/**
 * Configuration for trajectory generation.
 */
struct TrajectoryGeneratorConfig {
    double timestep = 0.1;            // Time between samples (seconds)
    double maxDuration = 60.0;        // Maximum trajectory duration (seconds)
    InitialSpeedConfig speedConfig;   // Speed calculation parameters
};

/**
 * Result of trajectory generation.
 */
struct TrajectoryGeneratorResult {
    int trajectoriesGenerated = 0;    // Vehicles with full trajectories
    int trajectoriesSkipped = 0;      // Vehicles without valid paths
};

/**
 * Generate full trajectories for vehicles by following lanes to their goals.
 *
 * For each vehicle that has a goalPosition set:
 * 1. Find the path through the lane graph from current position to goal
 * 2. Build a combined polyline along that path
 * 3. Integrate velocity at each timestep:
 *    - Speed is calculated based on local curvature, nearby merges/splits, lane width
 *    - Position is updated: pos += velocity * timestep
 *    - Heading follows the lane direction
 *
 * Vehicles without goals or without valid paths keep their original single-timestep data.
 *
 * @param vehicles Vector of vehicles to generate trajectories for (modified in place)
 * @param lanes Lane geometries for the scene
 * @param config Configuration parameters
 * @return Result with counts of generated and skipped trajectories
 */
GEODRAW_API TrajectoryGeneratorResult generateTrajectories(
    std::vector<VehicleTrajectory>& vehicles,
    const std::vector<LaneGeometry>& lanes,
    const TrajectoryGeneratorConfig& config = {});

} // namespace geodraw
