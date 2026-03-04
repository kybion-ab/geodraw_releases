/*******************************************************************************
 * File: goal_assignment.hpp
 *
 * Description: Goal position assignment for vehicles in scenario generation.
 * Assigns reachable goal points along lane paths for each vehicle.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/modules/drive/trajectory_data.hpp"
#include <vector>

namespace geodraw {

/**
 * Configuration for goal assignment.
 */
struct GoalAssignmentConfig {
    double targetDistance = 100.0;  // Target goal distance in meters
    double minDistance = 30.0;      // Minimum acceptable distance
};

/**
 * Result of goal assignment operation.
 */
struct GoalAssignmentResult {
    int goalsAssigned = 0;   // Number of vehicles with goals
    int goalsSkipped = 0;    // Vehicles with no valid path
};

/**
 * Assign goal positions to vehicles based on lane graph traversal.
 *
 * For each vehicle:
 * 1. Find the closest lane to the vehicle's position
 * 2. Build a lane graph from all lanes
 * 3. Traverse the graph forward from the vehicle's position
 * 4. Sample a goal position at target distance or at path end
 *
 * @param vehicles Vector of vehicles to assign goals to (modified in place)
 * @param lanes Vector of lane geometries for the scene
 * @param config Configuration options
 * @return Result with counts of assigned/skipped goals
 */
GEODRAW_API GoalAssignmentResult assignGoals(
    std::vector<VehicleTrajectory>& vehicles,
    const std::vector<LaneGeometry>& lanes,
    const GoalAssignmentConfig& config = {});

}  // namespace geodraw
