/*******************************************************************************
 * File: earth_viewer.cpp
 *
 * Description: Example demonstrating the Earth visualization module.
 * Shows how to render satellite tile imagery with LOD management.
 *
 * Usage:
 *   Set MAPTILER_API_KEY environment variable or pass as command line argument.
 *   ./earth_viewer [API_KEY]
 *
 *
 ******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/scene/scene.hpp"
#include "geodraw/geometry/geometry.hpp"
#include "geodraw/log/log.hpp"
#include "geodraw/geometry/colors.hpp"
#include "geodraw/modules/earth/earth.hpp"
#include "geodraw/modules/earth/providers/maptiler_provider.hpp"
#include "geodraw/modules/earth/earth_shape_edit.hpp"
#include "geodraw/ui/imgui_plugin.h"
#include "geodraw/modules/camera_trajectory/camera_trajectory_plugin.hpp"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <set>

using namespace geodraw;
using namespace geodraw::earth;

// Helper class for ImGui rendering in earth_viewer
class EarthViewerUI {
public:
  EarthViewerUI(geodraw::App& app) : app_(app) {}
  void draw();

private:
  void drawCommandWindow();
  std::string formatKeyBinding(int key, int mods);

  geodraw::App& app_;
};

std::string EarthViewerUI::formatKeyBinding(int k, int mods) {
  if (k < 0) return "";

  std::string result;

  // Handle modifiers
  if (mods & Mod::Ctrl)  result += "Ctrl+";
  if (mods & Mod::Alt)   result += "Alt+";
  if (mods & Mod::Shift) result += "Shift+";
  if (mods & Mod::Super) result += "Super+";

  // Handle key codes
  if (k >= key(Key::A) && k <= key(Key::Z)) {
    result += static_cast<char>('A' + (k - key(Key::A)));
  } else if (k >= key(Key::D0) && k <= key(Key::D9)) {
    result += static_cast<char>('0' + (k - key(Key::D0)));
  } else if (k >= key(Key::F1) && k <= key(Key::F12)) {
    result += "F" + std::to_string(k - key(Key::F1) + 1);
  } else {
    switch (k) {
      case key(Key::Space):     result += "Space"; break;
      case key(Key::Enter):     result += "Enter"; break;
      case key(Key::Tab):       result += "Tab"; break;
      case key(Key::Escape):    result += "Esc"; break;
      case key(Key::Backspace): result += "Backspace"; break;
      case key(Key::Delete):    result += "Delete"; break;
      case key(Key::Up):        result += "Up"; break;
      case key(Key::Down):      result += "Down"; break;
      case key(Key::Left):      result += "Left"; break;
      case key(Key::Right):     result += "Right"; break;
      case key(Key::Equal):     result += "="; break;
      case key(Key::Minus):     result += "-"; break;
      default: result += "?"; break;
    }
  }
  return result;
}

void EarthViewerUI::draw() {
  drawCommandWindow();
}

void EarthViewerUI::drawCommandWindow() {
  ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(220, 0), ImGuiCond_FirstUseEver);

  if (!ImGui::Begin("Commands", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::End();
    return;
  }

  // Collect minor mode command names to filter them out from global commands
  std::set<std::string> minorModeCommandNames;
  for (const auto& mode : app_.getMinorModes()) {
    for (const auto& cmd : mode->commands) {
      minorModeCommandNames.insert(cmd.name);
    }
  }

  ImGui::Text("Global Commands");
  ImGui::Separator();

  auto commands = app_.getCommands();
  for (const auto& cmd : commands) {
    // Skip minor mode commands (they'll appear in their own windows)
    if (minorModeCommandNames.count(cmd.name)) continue;
    // Skip empty command names
    if (cmd.name.empty()) continue;

    if (cmd.type == "toggle" && cmd.togglePtr) {
      // Render as checkbox
      bool value = *cmd.togglePtr;
      if (ImGui::Checkbox(cmd.docstring.c_str(), &value)) {
        cmd.callback();
        app_.requestUpdate();
      }
    } else {
      // Render as button
      if (ImGui::Button(cmd.docstring.c_str())) {
        cmd.callback();
        app_.requestUpdate();
      }
    }

    // Tooltip with command name and key binding
    if (ImGui::IsItemHovered()) {
      std::string tip = cmd.name;
      std::string key = formatKeyBinding(cmd.key, cmd.mods);
      if (!key.empty()) {
        tip += " [" + key + "]";
      }
      ImGui::SetTooltip("%s", tip.c_str());
    }
  }

  ImGui::End();
}


int main(int argc, char* argv[]) {
    // Parse logging flags (--verbose, --spam)
    geodraw::parse_log_args(argc, argv);

    // Get API key from environment or command line
    std::string apiKey;
    if (argc > 1) {
        apiKey = argv[1];
    } else {
        const char* envKey = std::getenv("MAPTILER_API_KEY");
        if (envKey) {
            apiKey = envKey;
        }
    }

    if (apiKey.empty()) {
        std::cerr << "Error: No MapTiler API key provided.\n";
        std::cerr << "Usage: " << argv[0] << " [API_KEY]\n";
        std::cerr << "Or set MAPTILER_API_KEY environment variable.\n";
        std::cerr << "\nGet a free API key at: https://www.maptiler.com/cloud/\n";
        return 1;
    }

    std::cout << "=== Earth Viewer Demo ===" << std::endl;

    // Create application
    App app(1280, 720, "Earth Viewer");

    // Set PAN as default interaction state
    app.camera.setInteractionState(Camera::InteractionState::PAN);

    // Create MapTiler provider with API key
    auto provider = std::make_shared<MapTilerProvider>(apiKey);

    // Configure earth layer - Gothenburg as default location
    EarthLayerConfig config;
    config.provider = provider;
    config.layerId = maptiler::SATELLITE;  // Use satellite layer
    config.reference = GeoReference(GeoCoord(57.7085653, 11.9404429, 0.0));  // Gothenburg
    config.lodBias = 1.0f;
    config.cache.cachePath = std::filesystem::path(std::getenv("HOME")) / ".cache" / "geodraw" / "earth_tiles";
    config.cache.maxMemoryCacheTiles = 1024;

    // Configure which MVT layers to parse for vector layers (like streets)
    // Only roads and buildings - skip water for better performance
    config.vectorLayers = geodraw::earth::pbf::VectorLayerSet::roadsAndBuildings();

    // Create earth layer (always use ECEF output for GLOBE mode)
    EarthLayer earth(config, app.getRenderer());
    earth.setCoordinateOutput(CoordinateOutput::ECEF);

    // Create vector overlay layer (streets)
    EarthLayerConfig vectorConfig = config;
    vectorConfig.layerId = maptiler::STREETS;
    EarthLayer vectorEarth(vectorConfig, app.getRenderer());
    vectorEarth.setCoordinateOutput(CoordinateOutput::ECEF);

    // Some interesting locations to jump to
    struct Location {
        const char* name;
        double lat;
        double lon;
    };

    std::vector<Location> locations = {
        {"Gothenburg", 57.7085653, 11.9404429},
        {"San Francisco", 37.7749, -122.4194},
        {"New York", 40.7128, -74.0060},
        {"London", 51.5074, -0.1278},
        {"Tokyo", 35.6762, 139.6503},
        {"Sydney", -33.8688, 151.2093},
        {"Paris", 48.8566, 2.3522},
        {"Cairo", 30.0444, 31.2357}
    };

    int currentLocation = 0;

    // Configure camera for GLOBE mode (always ECEF-based)
    app.camera.setMode(Camera::CameraMode::GLOBE);
    app.camera.setGlobeTarget(config.reference.origin.latitude, config.reference.origin.longitude);
    app.camera.setGlobeAltitude(2000.0);  // Start at 2km altitude
    app.camera.globePitch = glm::radians(45.0f);
    app.camera.globeYaw = glm::radians(30.0f);

    // State
    bool showInfo = true;
    bool showTileDebug = false;
    bool terrainEnabled = false;
    std::string currentRasterLayerId = maptiler::SATELLITE;
    bool vectorOverlayEnabled = false;
    bool showBuildings = true;
    bool showRoads = true;
    bool show3DBuildings = false;
    bool showTextures = true;

    // Camera trajectory plugin
    CameraTrajectoryPlugin camTraj;
    app.addModule(camTraj);

    MinorMode& layersMode = app.createMinorMode("Layers");
    app.activateMinorMode(layersMode);

    // Shape editor
    ShapeEditor shapeEditor;

    // Register commands
    app.addToggle("toggle-info", showInfo, "Toggle info overlay", key(Key::I));

    app.addCmd("next-location", [&]() {
        currentLocation = (currentLocation + 1) % locations.size();
        auto& loc = locations[currentLocation];
        earth.setReference(GeoReference(GeoCoord(loc.lat, loc.lon, 0.0)));
        app.camera.setGlobeTarget(loc.lat, loc.lon);
        std::cout << "Jumped to: " << loc.name << std::endl;
    }, "Jump to next location", key(Key::N));

    app.addCmd("prev-location", [&]() {
        currentLocation = (currentLocation + locations.size() - 1) % locations.size();
        auto& loc = locations[currentLocation];
        earth.setReference(GeoReference(GeoCoord(loc.lat, loc.lon, 0.0)));
        app.camera.setGlobeTarget(loc.lat, loc.lon);
        std::cout << "Jumped to: " << loc.name << std::endl;
    }, "Jump to previous location", key(Key::P));

    auto cycleRasterLayer = [&]() {
        auto all = provider->availableLayers();
        std::vector<TileLayerConfig> rasterLayers;
        for (auto& l : all) if (l.isRasterLayer()) rasterLayers.push_back(l);
        if (rasterLayers.empty()) return;
        int idx = 0;
        for (size_t i = 0; i < rasterLayers.size(); i++)
            if (rasterLayers[i].id == currentRasterLayerId) { idx = static_cast<int>(i); break; }
        idx = (idx + 1) % static_cast<int>(rasterLayers.size());
        currentRasterLayerId = rasterLayers[idx].id;
        earth.setLayerId(currentRasterLayerId);
        std::cout << "Raster layer: " << rasterLayers[idx].displayName << std::endl;
    };
    app.addCmd("cycle-raster-layer", cycleRasterLayer,
               "Cycle raster base layer", key(Key::L), 0, true, layersMode);

    app.addToggle("toggle-vector-overlay", vectorOverlayEnabled,
                  "Toggle streets vector overlay", key(Key::V), 0, true, layersMode);
    app.addToggle("toggle-buildings", showBuildings, "Toggle buildings", key(Key::B), 0, true, layersMode);
    app.addToggle("toggle-roads", showRoads, "Toggle roads/streets", key(Key::R), 0, true, layersMode);
    app.addToggle("toggle-3d-buildings", show3DBuildings, "Toggle 3D building extrusion", key(Key::E), 0, true, layersMode);

    app.addCmd("increase-lod", [&]() {
        float bias = earth.getLodBias() * 1.5f;
        if (bias > 4.0f) bias = 4.0f;
        earth.setLodBias(bias);
        std::cout << "LOD bias: " << bias << std::endl;
    }, "Increase LOD detail", key(Key::Equal));

    app.addCmd("decrease-lod", [&]() {
        float bias = earth.getLodBias() / 1.5f;
        if (bias < 0.25f) bias = 0.25f;
        earth.setLodBias(bias);
        std::cout << "LOD bias: " << bias << std::endl;
    }, "Decrease LOD detail", key(Key::Minus));

    app.addToggle("toggle-textures", showTextures,
                  "Toggle texture rendering", key(Key::T), 0, true, layersMode);

    app.addToggle("toggle-tile-debug", showTileDebug, "Toggle tile debug (wireframe + labels)", key(Key::D));
    app.addToggle("toggle-terrain", terrainEnabled, "Toggle 3D terrain mesh", key(Key::D3), 0, true, layersMode);

    app.addCmd("print-coords", [&]() {
        // In GLOBE mode, get target position from camera
        glm::dvec3 targetECEF = app.camera.getGlobeTargetECEF();
        earth::ECEFCoord ecef(targetECEF.x, targetECEF.y, targetECEF.z);
        earth::GeoCoord geo = earth::ecefToGeo(ecef).toDegrees();

        std::cout << "\n=== Target Point Coordinates ===" << std::endl;
        std::cout << "WGS84: lat=" << geo.latitude << ", lon=" << geo.longitude
                  << ", alt=" << geo.altitude << " m" << std::endl;
        std::cout << "ECEF: (" << ecef.x << ", " << ecef.y << ", " << ecef.z << ") m" << std::endl;
        std::cout << "Altitude: " << app.camera.globeAltitude << " m" << std::endl;
    }, "Print target point coordinates (WGS84/ECEF)", key(Key::C));

    // Register shape editor as a module (registers commands, OVERLAY callbacks, plugin panel)
    app.addModule(shapeEditor);

    app.setDocstring(R"(=== Earth Viewer Demo ===

Demonstrates satellite tile rendering with LOD management.
Uses GLOBE mode with ECEF coordinates for seamless Earth viewing.

Controls:
  Mouse drag   - Rotate view around target
  Right drag   - Pan (move target on Earth's surface)
  Scroll       - Zoom in/out
  Double-click - Set camera target to clicked point
  H            - Show help with more details

Features:
  - Full Earth visible when zoomed out
  - Double-click to target any point on globe
  - Seamless zoom from orbital to ground level

Layer Controls:
  L            - Cycle raster base layer (satellite / hybrid / terrain)
  V            - Toggle streets vector overlay
  B            - Toggle buildings (when vector overlay is active)
  R            - Toggle roads/streets (when vector overlay is active)
  E            - Toggle 3D building extrusion

3D Terrain (press 3):
  - Uses quantized mesh terrain data (available zoom 0-13)
  - Satellite imagery is draped on terrain mesh
  - Shows real elevation for mountains and valleys

Locations: Gothenburg, San Francisco, New York, London, Tokyo, Sydney, Paris, Cairo)");

    // Use app.scene() so ray picking can access the geometry
    app.addScene("earth");

    // Debug: print first tile positions once
    static bool debugPrinted = false;

    app.addUpdateCallback([&](float dt) {
        // Sync toggle state with earth layer
        earth.setShowWireframe(showTileDebug);
        earth.setTerrainEnabled(terrainEnabled);
        earth.setShowTextures(showTextures);

        // Update earth layer (handles tile loading, texture uploads, LOD)
        bool loading = earth.update(app.camera, app.getWidth(), app.getHeight());

        if (vectorOverlayEnabled) {
            vectorEarth.setShowBuildings(showBuildings);
            vectorEarth.setShowRoads(showRoads);
            vectorEarth.setShowWireframe(showTileDebug);
            vectorEarth.setShow3DBuildings(show3DBuildings);
            bool vLoading = vectorEarth.update(app.camera, app.getWidth(), app.getHeight());
            loading = loading || vLoading;
        }

        // Rebuild the scene
        // This callback is only triggered when tick() detects a change
        Scene& scene = app.scene();
        scene.clear();

        // Set scene origin for target-relative rendering (GLOBE mode uses target ECEF)
        glm::dvec3 sceneOrigin = app.camera.getGlobeTargetECEF();
        scene.setOrigin(sceneOrigin);

        // Add earth tiles to scene (raster first, then vector overlay)
        earth.addToScene(scene);
        if (vectorOverlayEnabled) vectorEarth.addToScene(scene);

        // Request update while still loading tiles
        if (loading) {
            app.requestUpdate();
        }
    });

    app.addDrawCallback([&]() {
        // Tick every frame: checks camera delta and async tile completions
        // Returns true when scene needs rebuilding
        if (earth.tick(app.camera, app.getWidth(), app.getHeight()) ||
            (vectorOverlayEnabled && vectorEarth.tick(app.camera, app.getWidth(), app.getHeight()))) {
            app.requestUpdate();
        }

        // Always sync scene origin with camera target before rendering (prevents jitter)
        // This runs every frame, independent of the update callback's tile loading threshold
        glm::dvec3 sceneOrigin = app.camera.getGlobeTargetECEF();
        app.scene().setOrigin(sceneOrigin);

        app.getRenderer().render(app.scene(), app.camera);

        // Render tile debug labels if enabled
        if (showTileDebug) {
            auto tileDebugInfo = earth.getVisibleTileDebugInfo();
            glm::dvec3 sceneOrigin = app.camera.getGlobeTargetECEF();
            glm::dvec3 cameraECEF = app.camera.getGlobeCameraECEF();

            for (const auto& tile : tileDebugInfo) {
                // Backface culling: check if tile surface faces camera
                // The surface normal at tile center points outward from Earth's center
                glm::dvec3 tileNormal = glm::normalize(tile.centerECEF);
                // View direction: from tile center toward camera
                glm::dvec3 viewDir = glm::normalize(cameraECEF - tile.centerECEF);
                // If dot product is negative, tile faces away from camera
                if (glm::dot(tileNormal, viewDir) < 0.0) {
                    continue;  // Skip backfacing tiles
                }

                // Convert tile center from ECEF to scene-relative coordinates
                glm::vec3 localPos = glm::vec3(tile.centerECEF - sceneOrigin);

                // Project to screen coordinates
                auto screenPos = app.camera.projectToScreen(localPos,
                    static_cast<float>(app.getWidth()),
                    static_cast<float>(app.getHeight()));

                if (screenPos.has_value()) {
                    // Choose color based on tile state
                    glm::vec3 color;
                    if (tile.hasTexture && !tile.usingFallback) {
                      color = Colors::YELLOW;
                    } else if (tile.usingFallback) {
                      color = Colors::ORANGE;
                    } else {
                        color = glm::vec3(1.0f, 0.4f, 0.0f);  // Dark orange: loading
                    }

                    // Render with 1.5x scale for better visibility
                    app.getRenderer().renderText(tile.key,
                        screenPos->x, screenPos->y, color, 1.5f);
                }
            }
        }

        // Render info overlay
        if (showInfo) {
            auto& ref = earth.getReference();
            char buf[256];

            // Location info
            snprintf(buf, sizeof(buf), "Location: %s (%.4f, %.4f)",
                     locations[currentLocation].name,
                     ref.origin.latitude, ref.origin.longitude);
            app.getRenderer().renderText(buf, 10, 30, glm::vec3(1, 1, 1));

            // Tile stats
            snprintf(buf, sizeof(buf), "Tiles: %d visible, %d loading, %d cached",
                     earth.visibleTileCount(),
                     earth.loadingTileCount(),
                     earth.gpuTextureCount());
            app.getRenderer().renderText(buf, 10, 50, glm::vec3(0.8f, 0.8f, 0.8f));

            // Camera altitude
            double altitude = app.camera.globeAltitude;
            if (altitude > 1000) {
                snprintf(buf, sizeof(buf), "Altitude: %.1f km", altitude / 1000.0);
            } else {
                snprintf(buf, sizeof(buf), "Altitude: %.0f m", altitude);
            }
            app.getRenderer().renderText(buf, 10, 70, glm::vec3(0.8f, 0.8f, 0.8f));

            // Active layer info
            if (vectorOverlayEnabled) {
                const char* parts;
                if (showRoads && showBuildings)  parts = show3DBuildings ? "roads+buildings (3D)" : "roads+buildings";
                else if (showRoads)              parts = "roads";
                else if (showBuildings)          parts = show3DBuildings ? "buildings (3D)" : "buildings";
                else                             parts = "(overlay off)";
                snprintf(buf, sizeof(buf), "Layer: %s + %s", currentRasterLayerId.c_str(), parts);
            } else {
                snprintf(buf, sizeof(buf), "Layer: %s", currentRasterLayerId.c_str());
            }
            app.getRenderer().renderText(buf, 10, 90, glm::vec3(0.8f, 0.8f, 0.8f));

            // Terrain status
            snprintf(buf, sizeof(buf), "Terrain: %s", terrainEnabled ? "ON" : "OFF");
            app.getRenderer().renderText(buf, 10, 110, glm::vec3(0.8f, 0.8f, 0.8f));

            // Attribution (required by MapTiler TOS)
            app.getRenderer().renderText("Map data: MapTiler, OpenStreetMap contributors",
                                         10, app.getHeight() - 20,
                                         glm::vec3(0.5f, 0.5f, 0.5f));
        }
    });

    auto drawLayersPanel = [&]() {
        // Compute display name of current raster layer
        std::string rasterName = currentRasterLayerId;
        for (auto& l : provider->availableLayers())
            if (l.id == currentRasterLayerId) { rasterName = l.displayName; break; }

        ImGui::SetNextWindowPos(ImVec2(240, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(220, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Layers", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        // --- Raster ---
        ImGui::SeparatorText("Raster");

        if (ImGui::Checkbox("Texture rendering", &showTextures))
            app.requestUpdate();

        ImGui::Text("%s", rasterName.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Next [L]")) { cycleRasterLayer(); app.requestUpdate(); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("cycle-raster-layer");

        if (ImGui::Checkbox("3D terrain mesh", &terrainEnabled))
            app.requestUpdate();

        // --- Vector ---
        ImGui::SeparatorText("Vector");

        if (ImGui::Checkbox("Streets overlay", &vectorOverlayEnabled))
            app.requestUpdate();

        if (vectorOverlayEnabled) {
            ImGui::Indent(12.0f);

            if (ImGui::Checkbox("Buildings", &showBuildings))
                app.requestUpdate();
            ImGui::SameLine();
            if (!showBuildings) ImGui::BeginDisabled();
            if (ImGui::Checkbox("3D extrude", &show3DBuildings))
                app.requestUpdate();
            if (!showBuildings) ImGui::EndDisabled();

            if (ImGui::Checkbox("Streets", &showRoads))
                app.requestUpdate();

            ImGui::Unindent(12.0f);
        }

        ImGui::End();
    };

    // Register Layers as a plugin panel so drawPluginsPanel() can manage it
    app.registerPluginPanel({"Layers", &layersMode,
        [&](void* ctx) { ImGui::SetCurrentContext((ImGuiContext*)ctx); drawLayersPanel(); },
        /* panelOpen= */ true});

    // Create UI helper
    EarthViewerUI ui(app);

    // Initialize ImGui plugin (wraps draw callback)
    ImGuiPlugin imgui(app);
    imgui.setImGuiCallback([&]() {
        imgui.drawPluginsPanel("geodraw");
        ui.draw();
    });

    std::cout << "Starting Earth Viewer..." << std::endl;
    std::cout << "Press H for help, N/P to change location, L to cycle raster layer, V to toggle streets overlay" << std::endl;

    app.run();

    return 0;
}
