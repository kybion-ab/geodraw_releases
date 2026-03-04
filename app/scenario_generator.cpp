/*******************************************************************************
 * File: scenario_generator.cpp
 *
 * Description: CLI tool for generating scenario files from OpenStreetMap data.
 * Extracts road geometry within a radius of a WGS84 coordinate and saves
 * to JSON format compatible with scenario_viewer.
 *
 * Usage:
 *   scenario_generator --lat 57.7 --lon 11.9 --radius 300 --output test.json
 *
 * The API key can be provided via:
 *   1. --api-key command line argument
 *   2. MAPTILER_API_KEY environment variable
 *
 *
 ******************************************************************************/

#include "geodraw/modules/earth/osm_to_scenario.hpp"
#include "geodraw/modules/drive/vehicle_generator.hpp"
#include "geodraw/modules/drive/goal_assignment.hpp"
#include "geodraw/modules/drive/trajectory_generator.hpp"
#include "geodraw/modules/drive/trajectory_json.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

using namespace geodraw;
using namespace geodraw::earth;

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n\n"
              << "Generate scenario files from OpenStreetMap road data.\n\n"
              << "Single-point mode (required options):\n"
              << "  --lat <latitude>    Center latitude in degrees (WGS84)\n"
              << "  --lon <longitude>   Center longitude in degrees (WGS84)\n"
              << "  --radius <meters>   Extraction radius in meters\n"
              << "  --output <file>     Output JSON file path\n\n"
              << "GeoJSON mode (alternative to --lat/--lon):\n"
              << "  --geojson <file>        Path to GeoJSON file containing polygon(s)\n"
              << "  --output-dir <dir>      Output directory for generated scenarios (required with --geojson)\n"
              << "  --num-scenarios <N>     Generate exactly N scenarios (optional)\n"
              << "                          Without this flag: pack scenarios using --radius for spacing\n\n"
              << "Common options:\n"
              << "  --radius <meters>   Extraction radius in meters (default: 300)\n"
              << "  --api-key <key>     MapTiler API key (or set MAPTILER_API_KEY env var)\n"
              << "  --zoom <level>      Tile zoom level (default: 14, max for vector tiles)\n"
              << "  --name <name>       Scenario name (default: derived from output filename)\n"
              << "  --help              Show this help message\n\n"
              << "Vehicle generation:\n"
              << "  --vehicles              Enable vehicle generation\n"
              << "  --vehicle-density <d>   Vehicles per meter (default: 0.01)\n"
              << "  --min-vehicles <n>      Minimum count (default: 0)\n"
              << "  --max-vehicles <n>      Maximum count (default: 100)\n"
              << "  --vehicle-seed <n>      Random seed (default: time-based)\n"
              << "  --goal-distance <m>     Target goal distance in meters (default: 100)\n"
              << "  --gen-trajs             Generate full trajectories from start to goal\n"
              << "  --traj-timestep <s>     Trajectory timestep in seconds (default: 0.1)\n\n"
              << "PufferDrive compatibility:\n"
              << "  --pufferdrive           Export in PufferDrive format (with trajectory padding)\n"
              << "  --timesteps <n>         Target timesteps for trajectories (default: 91)\n\n"
              << "Examples:\n"
              << "  Single-point mode:\n"
              << "  " << programName << " --lat 57.7 --lon 11.9 --radius 300 --output gothenburg.json\n\n"
              << "  GeoJSON mode with radius packing:\n"
              << "  " << programName << " --geojson area.geojson --output-dir scenarios/ --radius 200\n\n"
              << "  GeoJSON mode with fixed number of scenarios:\n"
              << "  " << programName << " --geojson area.geojson --output-dir scenarios/ --radius 200 --num-scenarios 5\n\n"
              << "The generated JSON files can be viewed with scenario_viewer.\n";
}

int main(int argc, char** argv) {
    // Default values
    double lat = 0.0;
    double lon = 0.0;
    double radius = 300.0;  // Default radius for GeoJSON mode
    std::string outputPath;
    std::string apiKey;
    std::string scenarioName;
    int zoomLevel = 14;

    // GeoJSON batch mode options
    std::string geojsonPath;
    std::string outputDir;
    int numScenarios = -1;  // -1 means not set (use radius packing)

    // Vehicle generation options
    bool generateVehiclesFlag = false;
    double vehicleDensity = 0.01;
    int minVehicles = 0;
    int maxVehicles = 100;
    unsigned int vehicleSeed = 0;
    double goalDistance = 100.0;
    bool generateTrajectoriesFlag = false;
    double trajectoryTimestep = 0.1;

    // PufferDrive options
    bool pufferdriveFormat = false;
    size_t targetTimesteps = 91;

    bool hasLat = false;
    bool hasLon = false;
    bool hasRadius = false;
    bool hasOutput = false;
    bool hasGeoJSON = false;
    bool hasOutputDir = false;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "--lat" && i + 1 < argc) {
            lat = std::stod(argv[++i]);
            hasLat = true;
        }
        else if (arg == "--lon" && i + 1 < argc) {
            lon = std::stod(argv[++i]);
            hasLon = true;
        }
        else if (arg == "--radius" && i + 1 < argc) {
            radius = std::stod(argv[++i]);
            hasRadius = true;
        }
        else if (arg == "--output" && i + 1 < argc) {
            outputPath = argv[++i];
            hasOutput = true;
        }
        else if (arg == "--api-key" && i + 1 < argc) {
            apiKey = argv[++i];
        }
        else if (arg == "--zoom" && i + 1 < argc) {
            zoomLevel = std::stoi(argv[++i]);
        }
        else if (arg == "--vehicles") {
            generateVehiclesFlag = true;
        }
        else if (arg == "--vehicle-density" && i + 1 < argc) {
            vehicleDensity = std::stod(argv[++i]);
        }
        else if (arg == "--min-vehicles" && i + 1 < argc) {
            minVehicles = std::stoi(argv[++i]);
        }
        else if (arg == "--max-vehicles" && i + 1 < argc) {
            maxVehicles = std::stoi(argv[++i]);
        }
        else if (arg == "--vehicle-seed" && i + 1 < argc) {
            vehicleSeed = static_cast<unsigned int>(std::stoul(argv[++i]));
        }
        else if (arg == "--goal-distance" && i + 1 < argc) {
            goalDistance = std::stod(argv[++i]);
        }
        else if (arg == "--gen-trajs") {
            generateTrajectoriesFlag = true;
        }
        else if (arg == "--traj-timestep" && i + 1 < argc) {
            trajectoryTimestep = std::stod(argv[++i]);
        }
        else if (arg == "--name" && i + 1 < argc) {
            scenarioName = argv[++i];
        }
        else if (arg == "--pufferdrive") {
            pufferdriveFormat = true;
        }
        else if (arg == "--timesteps" && i + 1 < argc) {
            targetTimesteps = static_cast<size_t>(std::stoul(argv[++i]));
        }
        else if (arg == "--geojson" && i + 1 < argc) {
            geojsonPath = argv[++i];
            hasGeoJSON = true;
        }
        else if (arg == "--output-dir" && i + 1 < argc) {
            outputDir = argv[++i];
            hasOutputDir = true;
        }
        else if (arg == "--num-scenarios" && i + 1 < argc) {
            numScenarios = std::stoi(argv[++i]);
        }
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // Determine mode and check required arguments
    bool geojsonMode = hasGeoJSON;

    if (geojsonMode) {
        // GeoJSON mode: require --output-dir
        if (!hasOutputDir) {
            std::cerr << "Error: --output-dir is required when using --geojson.\n\n";
            printUsage(argv[0]);
            return 1;
        }
    } else {
        // Single-point mode: require --lat, --lon, --radius, --output
        if (!hasLat || !hasLon || !hasRadius || !hasOutput) {
            std::cerr << "Error: Missing required arguments.\n\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // Get API key from environment if not provided
    if (apiKey.empty()) {
        const char* envKey = std::getenv("MAPTILER_API_KEY");
        if (envKey) {
            apiKey = envKey;
        }
    }

    if (apiKey.empty()) {
        std::cerr << "Error: MapTiler API key is required.\n"
                  << "Provide via --api-key or set MAPTILER_API_KEY environment variable.\n"
                  << "Get a free API key at https://cloud.maptiler.com/\n";
        return 1;
    }

    // Common validation
    if (radius <= 0.0) {
        std::cerr << "Error: Radius must be positive.\n";
        return 1;
    }

    if (zoomLevel < 0 || zoomLevel > 14) {
        std::cerr << "Error: Zoom level must be between 0 and 14.\n";
        return 1;
    }

    // Branch based on mode
    if (geojsonMode) {
        // =====================================================================
        // GeoJSON Batch Mode
        // =====================================================================
        std::cout << "=== Scenario Generator (GeoJSON Mode) ===\n";
        std::cout << "GeoJSON: " << geojsonPath << "\n";
        std::cout << "Output directory: " << outputDir << "\n";
        std::cout << "Radius: " << radius << " meters\n";
        if (numScenarios > 0) {
            std::cout << "Target scenarios: " << numScenarios << "\n";
        } else {
            std::cout << "Mode: Hexagonal packing\n";
        }
        std::cout << "Zoom level: " << zoomLevel << "\n\n";

        try {
            BatchScenarioConfig batchConfig;
            batchConfig.geojsonPath = geojsonPath;
            batchConfig.outputDir = outputDir;
            batchConfig.apiKey = apiKey;
            batchConfig.zoomLevel = zoomLevel;
            batchConfig.radius = radius;
            if (numScenarios > 0) {
                batchConfig.numScenarios = numScenarios;
            }

            // Vehicle generation options
            batchConfig.generateVehicles = generateVehiclesFlag;
            batchConfig.vehicleDensity = vehicleDensity;
            batchConfig.minVehicles = minVehicles;
            batchConfig.maxVehicles = maxVehicles;
            batchConfig.vehicleSeed = vehicleSeed;
            batchConfig.goalDistance = goalDistance;

            // PufferDrive options
            batchConfig.pufferdriveFormat = pufferdriveFormat;
            batchConfig.targetTimesteps = targetTimesteps;

            BatchScenarioResult result = generateBatchScenarios(batchConfig);

            if (!result.success) {
                std::cerr << "Error: Batch generation failed.\n";
                for (const auto& error : result.errors) {
                    std::cerr << "  - " << error << "\n";
                }
                return 1;
            }

            std::cout << "\nGenerated " << result.scenariosGenerated << " scenario(s):\n";
            for (const auto& file : result.outputFiles) {
                std::cout << "  " << file << "\n";
            }

            if (!result.errors.empty()) {
                std::cerr << "\nWarnings:\n";
                for (const auto& error : result.errors) {
                    std::cerr << "  - " << error << "\n";
                }
            }

            return 0;

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
    }

    // =========================================================================
    // Single-Point Mode
    // =========================================================================

    // Validate single-point inputs
    if (lat < -90.0 || lat > 90.0) {
        std::cerr << "Error: Latitude must be between -90 and 90 degrees.\n";
        return 1;
    }

    if (lon < -180.0 || lon > 180.0) {
        std::cerr << "Error: Longitude must be between -180 and 180 degrees.\n";
        return 1;
    }

    // Configure and run scenario generation
    ScenarioGeneratorConfig config;
    config.center = GeoCoord(lat, lon, 0.0);
    config.radius = radius;
    config.apiKey = apiKey;
    config.zoomLevel = zoomLevel;

    std::cout << "=== Scenario Generator ===\n";
    std::cout << "Center: " << lat << ", " << lon << "\n";
    std::cout << "Radius: " << radius << " meters\n";
    std::cout << "Zoom level: " << zoomLevel << "\n";
    std::cout << "Output: " << outputPath << "\n\n";

    try {
        // Generate scenario
        ScenarioGeneratorResult result = generateScenario(config);

        if (!result.success) {
            std::cerr << "Error: " << result.errorMessage << "\n";
            return 1;
        }

        // Print warnings
        for (const auto& warning : result.warnings) {
            std::cerr << "Warning: " << warning << "\n";
        }

        // Generate vehicles if requested
        if (generateVehiclesFlag) {
            VehicleGeneratorConfig vconfig;
            vconfig.density = vehicleDensity;
            vconfig.minVehicles = minVehicles;
            vconfig.maxVehicles = maxVehicles;
            vconfig.seed = vehicleSeed;

            auto vresult = generateVehicles(result.scene.lanes, vconfig);
            result.scene.vehicles = std::move(vresult.vehicles);

            std::cout << "Generated " << vresult.vehiclesGenerated << " vehicles";
            if (vresult.vehiclesSkipped > 0) {
                std::cout << " (" << vresult.vehiclesSkipped << " skipped due to spacing)";
            }
            std::cout << "\n";

            // Assign goals to vehicles
            GoalAssignmentConfig gconfig;
            gconfig.targetDistance = goalDistance;
            auto gresult = assignGoals(result.scene.vehicles, result.scene.lanes, gconfig);
            std::cout << "Assigned goals to " << gresult.goalsAssigned << " vehicles";
            if (gresult.goalsSkipped > 0) {
                std::cout << " (" << gresult.goalsSkipped << " skipped, no valid path)";
            }
            std::cout << "\n";

            // Generate full trajectories if requested
            if (generateTrajectoriesFlag) {
                TrajectoryGeneratorConfig tconfig;
                tconfig.timestep = trajectoryTimestep;

                auto tresult = generateTrajectories(
                    result.scene.vehicles, result.scene.lanes, tconfig);

                std::cout << "Generated trajectories for " << tresult.trajectoriesGenerated
                          << " vehicles";
                if (tresult.trajectoriesSkipped > 0) {
                    std::cout << " (" << tresult.trajectoriesSkipped
                              << " skipped, no valid path)";
                }
                std::cout << "\n";
            }
        }

        // Set scenario name if provided
        if (!scenarioName.empty()) {
            result.scene.name = scenarioName;
        }

        // Save to file - use PufferDrive format if requested
        if (pufferdriveFormat) {
            PufferDriveExportOptions pdOptions;
            pdOptions.targetTimesteps = targetTimesteps;
            pdOptions.padTrajectories = true;
            pdOptions.includeMetadata = true;

            // Create a modified scene with metadata embedded
            SceneData sceneWithMetadata = result.scene;
            if (sceneWithMetadata.name.empty()) {
                // Generate name from filepath
                std::string name = outputPath;
                size_t lastSlash = name.find_last_of("/\\");
                if (lastSlash != std::string::npos) {
                    name = name.substr(lastSlash + 1);
                }
                size_t lastDot = name.find_last_of('.');
                if (lastDot != std::string::npos) {
                    name = name.substr(0, lastDot);
                }
                sceneWithMetadata.name = name;
            }

            saveToPufferDrive(outputPath, sceneWithMetadata, pdOptions);
        } else {
            saveScenarioWithMetadata(
                outputPath,
                result.scene,
                config.center,
                config.radius,
                config.zoomLevel);
        }

        // Print summary
        std::cout << "\n=== Generation Complete ===\n";
        std::cout << "Tiles processed: " << result.tilesProcessed << "\n";
        std::cout << "Roads found: " << result.roadsProcessed << "\n";
        std::cout << "Lanes generated: " << result.lanesGenerated << "\n";
        std::cout << "Road edges generated: " << result.roadEdgesGenerated << "\n";
        std::cout << "\nOutput file: " << outputPath << "\n";
        std::cout << "View with: ./scenario_viewer " << outputPath << "\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
