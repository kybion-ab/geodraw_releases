/*******************************************************************************
 * File : trajectory_json.hpp
 *
 * Description :
 *
 * Author  : Magnus Nilsson
 * Date    : 2026-01-29
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"

#include "geodraw/modules/drive/trajectory_data.hpp"
#include <nlohmann/json.hpp>

namespace geodraw {

GEODRAW_API VehicleTrajectory parseVehicle(const nlohmann::json& obj);

GEODRAW_API LaneGeometry parseLane(const nlohmann::json& obj);


GEODRAW_API RoadLine parseRoadLine(const nlohmann::json& obj);

GEODRAW_API RoadEdge parseRoadEdge(const nlohmann::json& obj);

GEODRAW_API SceneData loadFromJson(const std::string& filepath);

GEODRAW_API nlohmann::json vehicleToJson(const VehicleTrajectory& vehicle);

GEODRAW_API nlohmann::json laneToJson(const LaneGeometry& lane);

GEODRAW_API nlohmann::json roadLineToJson(const RoadLine& roadLine);

GEODRAW_API nlohmann::json roadEdgeToJson(const RoadEdge& roadEdge);

GEODRAW_API void saveToJson(const std::string& filepath, const SceneData& scene);

// PufferDrive export options
struct PufferDriveExportOptions {
    size_t targetTimesteps = 91;  // PufferDrive expects 91 timesteps
    bool padTrajectories = true;  // Pad short trajectories with zeros
    bool includeMetadata = true;  // Include metadata section
};

// Save in PufferDrive-compatible format
// Ensures required fields: name, tl_states, objects, roads
// Pads trajectories to targetTimesteps if padTrajectories is true
GEODRAW_API void saveToPufferDrive(const std::string& filepath,
                                    const SceneData& scene,
                                    const PufferDriveExportOptions& options = {});
}
