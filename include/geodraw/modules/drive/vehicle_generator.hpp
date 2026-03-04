/*******************************************************************************
 * File: vehicle_generator.hpp
 *
 * Description: Generate random vehicles placed along lanes for scenario
 * generation. Vehicles are distributed based on density with count limits,
 * and headings matching lane direction.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/modules/drive/trajectory_data.hpp"
#include "geodraw/graph/lane_graph.hpp"
#include <vector>

namespace geodraw {

struct VehicleGeneratorConfig {
    double density = 0.01;        // Vehicles per meter (1 per 100m default)
    int minVehicles = 0;
    int maxVehicles = 100;
    double minLength = 4.0;       // Vehicle length range (meters)
    double maxLength = 5.5;
    double minWidth = 1.8;        // Vehicle width range (meters)
    double maxWidth = 2.1;
    double height = 1.5;
    double minSpacing = 5.0;      // Min gap between vehicles on same lane
    unsigned int seed = 0;        // 0 = time-based seed
    bool setInitialSpeed = true;  // Calculate initial speed from lane properties
};

/**
 * Configuration for initial speed calculation.
 */
struct InitialSpeedConfig {
    double baseSpeedHighway = 30.0;     // m/s (~108 km/h) for wide lanes
    double baseSpeedUrban = 14.0;       // m/s (~50 km/h) for narrow lanes
    double minSpeed = 5.0;              // m/s (~18 km/h) minimum
    double maxSpeed = 35.0;             // m/s (~126 km/h) maximum
    double mergeSlowdownFactor = 0.7;   // Speed reduction near merges
    double splitSlowdownFactor = 0.8;   // Speed reduction near splits
    double junctionProximity = 30.0;    // Distance (m) to consider junction "nearby"
};

/**
 * Calculate reasonable initial speed for a vehicle based on lane properties.
 *
 * Factors considered:
 * - Lane curvature: High curvature → lower speed (centripetal acceleration limit)
 * - Nearby merges: Reduce speed when approaching merge points
 * - Nearby splits: Reduce speed when approaching decision points
 * - Lane width: Wider lanes suggest higher-class roads with higher limits
 *
 * @param lane The lane geometry the vehicle is on
 * @param arcLength Position along the lane (meters from start)
 * @param laneGraph Optional lane graph for merge/split detection
 * @param config Speed calculation parameters
 * @return Initial speed in m/s
 */
GEODRAW_API double calculateInitialSpeed(
    const LaneGeometry& lane,
    double arcLength,
    const graph::LaneGraphResult* laneGraph = nullptr,
    const InitialSpeedConfig& config = {});

struct VehicleGeneratorResult {
    std::vector<VehicleTrajectory> vehicles;
    int vehiclesGenerated = 0;
    int vehiclesSkipped = 0;      // Due to spacing constraints
    double totalLaneLength = 0.0;
};

GEODRAW_API VehicleGeneratorResult generateVehicles(
    const std::vector<LaneGeometry>& lanes,
    const VehicleGeneratorConfig& config = {});

} // namespace geodraw
