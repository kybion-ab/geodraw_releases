/*******************************************************************************
 * File: osm_to_scenario.hpp
 *
 * Description: Extracts road data from OpenStreetMap (via MapTiler vector tiles)
 * within a specified radius of a WGS84 coordinate and converts it to the scenario
 * format compatible with scenario_viewer.
 *
 * This is a reusable library module - core logic can be used from any application.
 *
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/modules/earth/earth_coords.hpp"
#include "geodraw/modules/earth/earth_pbf.hpp"
#include "geodraw/modules/drive/trajectory_data.hpp"

#include <optional>
#include <string>
#include <vector>

namespace geodraw {
namespace earth {

// =============================================================================
// Configuration
// =============================================================================

/**
 * Configuration for scenario generation
 */
struct ScenarioGeneratorConfig {
    GeoCoord center;      // WGS84 center point
    double radius;        // Radius in meters
    std::string apiKey;   // MapTiler API key
    int zoomLevel = 14;   // Tile zoom level (max for vector tiles)
};

// =============================================================================
// Result
// =============================================================================

/**
 * Result of scenario generation
 */
struct ScenarioGeneratorResult {
    SceneData scene;                     // Generated scene data
    GeoReference reference;              // Geographic reference for ENU conversion
    std::vector<std::string> warnings;   // Non-fatal warnings during generation
    bool success = false;                // True if generation succeeded
    std::string errorMessage;            // Error message if generation failed

    // Statistics
    int tilesProcessed = 0;
    int roadsProcessed = 0;
    int lanesGenerated = 0;
    int roadEdgesGenerated = 0;
};

// =============================================================================
// Main Entry Point
// =============================================================================

/**
 * Generate a scenario from OpenStreetMap data within a radius of a point
 *
 * Fetches vector tiles from MapTiler, parses road geometry, and converts
 * to scenario format compatible with scenario_viewer.
 *
 * @param config Configuration specifying center, radius, and API key
 * @return Result containing scene data or error information
 */
GEODRAW_API ScenarioGeneratorResult generateScenario(const ScenarioGeneratorConfig& config);

// =============================================================================
// Conversion Utilities
// =============================================================================

/**
 * Convert a parsed road to LaneGeometry
 *
 * @param road Parsed road from PBF
 * @param id Unique ID for the lane
 * @return LaneGeometry with centerline and width
 */
GEODRAW_API LaneGeometry roadToLane(const pbf::Road& road, int id);

/**
 * Convert a parsed PATH road to RoadEdge
 *
 * @param road Parsed road from PBF (PATH class)
 * @param id Unique ID for the road edge
 * @return RoadEdge with geometry
 */
GEODRAW_API RoadEdge roadToRoadEdge(const pbf::Road& road, int id);

/**
 * Get lane width for a road class
 *
 * Standard lane widths by road class:
 * - MOTORWAY:    3.7m
 * - PRIMARY:     3.5m
 * - SECONDARY:   3.5m
 * - TERTIARY:    3.2m
 * - RESIDENTIAL: 3.0m
 * - SERVICE:     2.5m
 * - OTHER:       3.0m
 *
 * @param roadClass Road classification
 * @return Lane width in meters
 */
GEODRAW_API double getLaneWidth(pbf::RoadClass roadClass);

// =============================================================================
// Extended JSON Output
// =============================================================================

/**
 * Save scenario to JSON with extended metadata
 *
 * Writes the scene data along with metadata about the generation parameters.
 * The output format includes:
 * - metadata: origin, radius, source, zoom_level
 * - objects: vehicles (empty for generated scenarios)
 * - roads: lanes and road edges
 *
 * @param filepath Output file path
 * @param scene Scene data to save
 * @param origin WGS84 origin point
 * @param radius Extraction radius in meters
 * @param zoomLevel Tile zoom level used
 */
GEODRAW_API void saveScenarioWithMetadata(
    const std::string& filepath,
    const SceneData& scene,
    const GeoCoord& origin,
    double radius,
    int zoomLevel);

// =============================================================================
// Batch Scenario Generation (GeoJSON input)
// =============================================================================

/**
 * Configuration for batch scenario generation from GeoJSON polygon(s)
 */
struct BatchScenarioConfig {
    std::string geojsonPath;           // Path to GeoJSON file containing polygon(s)
    std::string outputDir;             // Output directory for generated scenarios
    std::string apiKey;                // MapTiler API key
    int zoomLevel = 14;                // Tile zoom level (max for vector tiles)
    std::optional<int> numScenarios;   // If set, distribute exactly N scenarios
    double radius = 300.0;             // Circle radius for extraction + packing

    // Vehicle generation options (from single-scenario mode)
    bool generateVehicles = false;
    double vehicleDensity = 0.01;
    int minVehicles = 0;
    int maxVehicles = 100;
    unsigned int vehicleSeed = 0;
    double goalDistance = 100.0;

    // PufferDrive options
    bool pufferdriveFormat = false;
    size_t targetTimesteps = 91;
};

/**
 * Result of batch scenario generation
 */
struct BatchScenarioResult {
    int scenariosGenerated = 0;
    std::vector<std::string> outputFiles;
    std::vector<std::string> errors;
    bool success = false;
};

/**
 * Generate multiple scenarios from a GeoJSON polygon
 *
 * Loads a GeoJSON file, extracts polygon(s), and generates scenario files
 * at distributed center points within the polygon.
 *
 * Two modes:
 * 1. If numScenarios is set: Distribute exactly N scenario centers evenly
 * 2. Otherwise: Pack non-overlapping circles using radius for spacing
 *
 * @param config Configuration specifying GeoJSON path, output dir, and options
 * @return Result containing generated file paths or error information
 */
GEODRAW_API BatchScenarioResult generateBatchScenarios(const BatchScenarioConfig& config);

} // namespace earth
} // namespace geodraw
