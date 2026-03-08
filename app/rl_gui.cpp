/*******************************************************************************
 * File: rl_gui.cpp
 *
 * Description: Combined application merging earth_viewer (major mode) with
 * scenario_viewer (minor mode). The earth serves as the base visualization
 * layer, with scenarios displayed as overlay pins that can be picked and
 * visualized.
 *
 * Long-term goal: To provide a GUI-based interface for training, evaluating,
 * and analyzing reinforcement learning policies for self driving.
 *
 * Features:
 * - Globe/satellite rendering with LOD management
 * - Scenario pins displayed at WGS84 locations
 * - Directory selection in ImGui panel for scanning JSON scenarios
 * - Scenario geometry overlaid on earth surface via ScenarioPlugin
 * - Playback controls for vehicles
 * - Visibility toggles (lanes, vehicles, etc.)
 *
 * Usage:
 *   Set MAPTILER_API_KEY environment variable or pass as command line argument.
 *   ./rl_gui [API_KEY]
 *
 ******************************************************************************/

#include "rl_gui_ui.hpp"

#include "geodraw/log/log.hpp"
#include <nfd.hpp>
#include "geodraw/modules/earth/providers/maptiler_provider.hpp"
#include "geodraw/modules/camera_trajectory/camera_trajectory_plugin.hpp"
#include "geodraw/modules/video_capture/video_capture_plugin.hpp"
#include "geodraw/modules/gltf/gltf_loader.hpp"
#include "geodraw/ui/imgui_plugin.h"
#include "geodraw/ui/tooltip_system.hpp"
#include "geodraw/modules/clustering/marker_clusterer.hpp"
#include "geodraw/modules/earth/earth_shape_edit.hpp"

#ifdef GEODRAW_HAS_IMGUI
#include <imgui.h>
#endif

#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <cmath>

using namespace geodraw;
using namespace geodraw::earth;

// =============================================================================
// Main Application
// =============================================================================

int main(int argc, char* argv[]) {
    NFD::Guard nfdGuard;

    geodraw::parse_log_args(argc, argv);

    std::string apiKey;
    if (argc > 1) apiKey = argv[1];
    else if (const char* env = std::getenv("MAPTILER_API_KEY")) apiKey = env;

    if (apiKey.empty()) {
        std::cerr << "Error: No MapTiler API key provided.\n"
                  << "Usage: " << argv[0] << " [API_KEY]\n"
                  << "Or set MAPTILER_API_KEY environment variable.\n"
                  << "\nGet a free API key at: https://www.maptiler.com/cloud/\n";
        return 1;
    }

    std::cout << "=== RL GUI ===" << std::endl;

    App app(1280, 720, "RL GUI - Earth + Scenario Viewer");
    app.camera.setInteractionState(Camera::InteractionState::PAN);

    auto provider = std::make_shared<MapTilerProvider>(apiKey);

    EarthLayerConfig config;
    config.provider   = provider;
    config.layerId    = maptiler::SATELLITE;
    config.reference  = GeoReference(GeoCoord(57.7085653, 11.9404429, 0.0));
    config.lodBias    = 1.0f;
    config.cache.cachePath = std::filesystem::path(std::getenv("HOME"))
                             / ".cache" / "geodraw" / "earth_tiles";
    config.cache.maxMemoryCacheTiles = 1024;
    config.vectorLayers = geodraw::earth::pbf::VectorLayerSet::roadsAndBuildings();

    EarthLayer earth(config, app.getRenderer());
    earth.setCoordinateOutput(CoordinateOutput::ECEF);

    // Vector overlay layer (streets)
    EarthLayerConfig vectorConfig = config;
    vectorConfig.layerId = maptiler::STREETS;
    EarthLayer vectorEarth(vectorConfig, app.getRenderer());
    vectorEarth.setCoordinateOutput(CoordinateOutput::ECEF);

    // App-level state
    std::vector<ScenarioPin> scenarioPins;
    bool   showPins           = true;
    bool   showEarth          = true;
    bool   terrainEnabled     = false;
    bool   showTextures       = true;
    bool   vectorOverlayEnabled = false;
    bool   showBuildings      = true;
    bool   showRoads          = true;
    bool   show3DBuildings    = false;
    bool   showTileDebug      = false;
    std::string currentLayerId = maptiler::SATELLITE;
    int    hoveredPinIndex = -1;

    MinorMode& layersMode = app.createMinorMode("Layers");
    app.activateMinorMode(layersMode);

    ShapeEditor shapeEditor;

    TooltipSystem tooltipSystem;
    clustering::MarkerClusterer markerClusterer;

    // -------------------------------------------------------------------------
    // ScenarioPlugin (embedded, shares app.scene())
    // -------------------------------------------------------------------------
    app.addScene("rl_gui");  // create scene before plugin uses it

    ScenarioPlugin scenario(app.scene());

    // Wire native file/folder pickers into the scenario plugin's browse buttons
    scenario.setFolderPickCallback([](const std::string& def) -> std::string {
        NFD::UniquePath out;
        if (NFD::PickFolder(out, def.empty() ? nullptr : def.c_str()) == NFD_OKAY)
            return out.get();
        return "";
    });
    scenario.setFilePickCallback([](const std::string& def) -> std::string {
        nfdfilteritem_t filters[] = {{"JSON scenario", "json"}};
        NFD::UniquePath out;
        std::string dir = def;
        // Use the directory of the current file as default path
        if (!dir.empty()) {
            namespace fs = std::filesystem;
            if (fs::is_regular_file(dir)) dir = fs::path(dir).parent_path().string();
        }
        if (NFD::OpenDialog(out, filters, 1, dir.empty() ? nullptr : dir.c_str()) == NFD_OKAY)
            return out.get();
        return "";
    });

    // Register tooltip providers before attach so they're ready
    tooltipSystem.registerProvider("ScenarioPin",
        [&scenarioPins](const TooltipContext& ctx) {
            TooltipContent content;
            if (ctx.objectId > 0 && ctx.objectId <= scenarioPins.size()) {
                const auto& pin = scenarioPins[ctx.objectId - 1];
                std::ostringstream ss;
                ss << pin.name << "\n"
                   << std::fixed << std::setprecision(4)
                   << "Location: " << pin.location.latitude
                   << ", " << pin.location.longitude << "\n"
                   << std::setprecision(0) << "Radius: " << pin.radius << "m";
                content.text  = ss.str();
                content.valid = true;
            }
            return content;
        });

    tooltipSystem.registerProvider("Vehicle",
        [&scenario](const TooltipContext& ctx) {
            std::string info = scenario.getVehicleInfo(ctx.objectId);
            if (info.empty()) return TooltipContent{};
            return TooltipContent::makeText(info);
        });

    // -------------------------------------------------------------------------
    // Camera
    // -------------------------------------------------------------------------
    app.camera.setMode(Camera::CameraMode::GLOBE);
    app.camera.setGlobeTarget(config.reference.origin.latitude,
                               config.reference.origin.longitude);
    app.camera.setGlobeAltitude(2000.0);
    app.camera.globePitch = glm::radians(45.0f);
    app.camera.globeYaw   = glm::radians(30.0f);

    struct Location { const char* name; double lat, lon; };
    std::vector<Location> locations = {
        {"Gothenburg",    57.7085653,  11.9404429},
        {"San Francisco", 37.7749,    -122.4194},
        {"New York",      40.7128,     -74.0060},
        {"London",        51.5074,      -0.1278},
        {"Tokyo",         35.6762,     139.6503},
        {"Sydney",       -33.8688,    151.2093},
        {"Paris",         48.8566,      2.3522},
        {"Cairo",         30.0444,     31.2357},
    };
    int curLoc = 0;

    // -------------------------------------------------------------------------
    // Global commands
    // -------------------------------------------------------------------------
    app.addCmd("next-location", [&]() {
        curLoc = (curLoc + 1) % int(locations.size());
        earth.setReference(GeoReference(GeoCoord(locations[curLoc].lat, locations[curLoc].lon, 0)));
        app.camera.setGlobeTarget(locations[curLoc].lat, locations[curLoc].lon);
        std::cout << "Jumped to: " << locations[curLoc].name << "\n";
    }, "Jump to next location", key(Key::N));

    app.addCmd("prev-location", [&]() {
        curLoc = (curLoc + int(locations.size()) - 1) % int(locations.size());
        earth.setReference(GeoReference(GeoCoord(locations[curLoc].lat, locations[curLoc].lon, 0)));
        app.camera.setGlobeTarget(locations[curLoc].lat, locations[curLoc].lon);
        std::cout << "Jumped to: " << locations[curLoc].name << "\n";
    }, "Jump to previous location", key(Key::M));

    app.addCmd("pick-scenario", [&]() {
        if (scenarioPins.empty()) {
            std::cout << "No scenarios loaded. Scan a directory first.\n";
            return;
        }
        glm::dvec3 tgt = app.camera.getGlobeTargetECEF();
        ECEFCoord  ec(tgt.x, tgt.y, tgt.z);
        GeoCoord   geo = ecefToGeo(ec).toDegrees();
        double minD = std::numeric_limits<double>::max();
        int    nearest = -1;
        for (size_t i = 0; i < scenarioPins.size(); ++i) {
            double d = haversineDistance(geo, scenarioPins[i].location);
            if (d < minD) { minD = d; nearest = int(i); }
        }
        if (nearest >= 0) {
            const auto& pin = scenarioPins[nearest];
            scenario.setGeoReference(GeoReference(pin.location));
            scenario.activateScenario(pin.filepath, app);
            app.camera.setGlobeTarget(pin.location.latitude, pin.location.longitude);
            app.camera.setGlobeAltitude(pin.radius * 3.0);
        }
    }, "Pick nearest scenario", key(Key::P));

    app.addCmd("close-scenario", [&]() {
        if (scenario.getMinorMode() &&
            app.isMinorModeActive(*scenario.getMinorMode())) {
            app.deactivateMinorMode(*const_cast<MinorMode*>(scenario.getMinorMode()));
            std::cout << "Closed scenario view\n";
        }
    }, "Close scenario view", key(Key::Escape));

    auto cycleRasterLayer = [&]() {
        auto all = provider->availableLayers();
        std::vector<TileLayerConfig> rasterLayers;
        for (auto& l : all) if (l.isRasterLayer()) rasterLayers.push_back(l);
        if (rasterLayers.empty()) return;
        int idx = 0;
        for (size_t i = 0; i < rasterLayers.size(); i++)
            if (rasterLayers[i].id == currentLayerId) { idx = int(i); break; }
        idx = (idx + 1) % int(rasterLayers.size());
        currentLayerId = rasterLayers[idx].id;
        earth.setLayerId(currentLayerId);
    };
    app.addCmd("cycle-raster-layer", cycleRasterLayer,
               "Cycle raster base layer", key(Key::L), 0, true, layersMode);

    app.addToggle("toggle-terrain",        terrainEnabled,       "Toggle 3D terrain",                key(Key::D3));
    app.addToggle("toggle-pins",           showPins,             "Toggle scenario pins",              key(Key::I));
    app.addToggle("toggle-earth",          showEarth,            "Toggle earth layer");
    app.addToggle("toggle-textures",       showTextures,         "Toggle texture rendering",          key(Key::X), 0, true, layersMode);
    app.addToggle("toggle-vector-overlay", vectorOverlayEnabled, "Toggle streets vector overlay",     key(Key::V), 0, true, layersMode);
    app.addToggle("toggle-buildings",      showBuildings,        "Toggle buildings",                  key(Key::B), 0, true, layersMode);
    app.addToggle("toggle-roads",          showRoads,            "Toggle roads/streets",              key(Key::R), 0, true, layersMode);
    app.addToggle("toggle-3d-buildings",   show3DBuildings,      "Toggle 3D building extrusion",
                  key(Key::E), 0, true, layersMode);

    app.addToggle("toggle-tile-debug", showTileDebug, "Toggle tile debug overlay", key(Key::D));

    app.addCmd("increase-lod", [&]() {
        float bias = std::min(earth.getLodBias() * 1.5f, 4.0f);
        earth.setLodBias(bias);
        std::cout << "LOD bias: " << bias << "\n";
        app.requestUpdate();
    }, "Increase LOD detail level", key(Key::Equal));

    app.addCmd("decrease-lod", [&]() {
        float bias = std::max(earth.getLodBias() / 1.5f, 0.25f);
        earth.setLodBias(bias);
        std::cout << "LOD bias: " << bias << "\n";
        app.requestUpdate();
    }, "Decrease LOD detail level", key(Key::Minus));

    app.addCmd("print-coords", [&]() {
        glm::dvec3 tgt = app.camera.getGlobeTargetECEF();
        ECEFCoord ec(tgt.x, tgt.y, tgt.z);
        GeoCoord geo = ecefToGeo(ec).toDegrees();
        std::cout << "\n=== Target Coordinates ===\n"
                  << "WGS84: lat=" << geo.latitude << ", lon=" << geo.longitude << "\n"
                  << "ECEF: (" << tgt.x << ", " << tgt.y << ", " << tgt.z << ") m\n"
                  << "Altitude: " << app.camera.globeAltitude << " m\n";
    }, "Print target coordinates (WGS84/ECEF)", key(Key::C));

    bool debugSceneInfo = false;
    app.addCmd("debug-scene-info", [&debugSceneInfo]() { debugSceneInfo = true; },
               "Print scene debug info", key(Key::F1));

    // -------------------------------------------------------------------------
    // Attach scenario plugin (registers OVERLAY update callback + minor mode)
    // -------------------------------------------------------------------------
    scenario.attach(app);

    CameraTrajectoryPlugin camTraj;
    app.addModule(camTraj);
    app.activateMinorMode(*camTraj.getMinorMode());

    VideoCapturePlugin videoCap(camTraj);
    videoCap.setScenarioSync(scenario);   // sync scenario timestep during capture
    app.addModule(videoCap);
    app.activateMinorMode(*videoCap.getMinorMode());

    app.addModule(shapeEditor);

    // Load car model and hand it to the scenario plugin for vehicle rendering.
    {
        namespace fs = std::filesystem;
        auto findDataRoot = []() -> fs::path {
            if (const char* root = getenv("GEODRAW_ROOT"))
                return fs::path(root);
            auto p = fs::current_path();
            for (int i = 0; i < 4; ++i) {
                if (fs::exists(p / "data" / "glb_files")) return p;
                p = p.parent_path();
            }
            return fs::current_path();
        };
        auto dataRoot = findDataRoot();
        auto mdl = gltf::loadGLB(
            (dataRoot / "data" / "glb_files" / "GreenCar.glb").string(),
            app.getRenderer());
        if (!mdl || !mdl->isValid())
            std::cerr << "Warning: Failed to load car model, using wireframe fallback\n";
        else
            scenario.setCarModel(std::move(*mdl));
    }

    app.setDocstring(R"(=== RL GUI - Earth + Scenario Viewer ===

Combines globe visualization with scenario viewing.
Scan directories for scenarios with WGS84 metadata,
view them as pins on the globe, and load them for playback.

  H  - Display help)");

    // -------------------------------------------------------------------------
    // Primary (earth) update callback at priority BASE (0)
    // -------------------------------------------------------------------------
    app.addUpdateCallback([&](float dt) {
        (void)dt;
        earth.setTerrainEnabled(terrainEnabled);
        earth.setShowTextures(showTextures);
        earth.setShowWireframe(showTileDebug);
        bool loading = earth.update(app.camera, app.getWidth(), app.getHeight());

        if (vectorOverlayEnabled) {
            vectorEarth.setShowBuildings(showBuildings);
            vectorEarth.setShowRoads(showRoads);
            vectorEarth.setShow3DBuildings(show3DBuildings);
            vectorEarth.setShowWireframe(showTileDebug);
            bool vLoading = vectorEarth.update(app.camera, app.getWidth(), app.getHeight());
            loading = loading || vLoading;
        }

        Scene& scene = app.scene();
        scene.clear();
        scene.setOrigin(app.camera.getGlobeTargetECEF());

        if (showEarth) earth.addToScene(scene);
        if (vectorOverlayEnabled) vectorEarth.addToScene(scene);

        // Render scenario pins with clustering
        if (showPins && !scenarioPins.empty()) {
            glm::dvec3 camECEF = app.camera.getGlobeCameraECEF();
            int fbW = app.getWidth(), fbH = app.getHeight();
            glm::dvec3 sceneOrigin = app.camera.getGlobeTargetECEF();

            std::vector<std::optional<glm::vec2>> screenPos;
            std::vector<glm::dvec3> ecefPos;

            for (const auto& pin : scenarioPins) {
                ECEFCoord pe = geoToECEF(pin.location.toRadians());
                glm::dvec3 pinE(pe.x, pe.y, pe.z);
                ecefPos.push_back(pinE);

                glm::dvec3 pn = glm::normalize(pinE);
                glm::dvec3 vd = glm::normalize(camECEF - pinE);
                if (glm::dot(pn, vd) < 0.0) { screenPos.push_back(std::nullopt); continue; }

                glm::vec3 local = glm::vec3(pinE - sceneOrigin);
                auto sp = app.camera.projectToScreen(local, float(fbW), float(fbH));
                if (sp) screenPos.push_back(glm::vec2(sp->x, sp->y));
                else    screenPos.push_back(std::nullopt);
            }

            markerClusterer.update(screenPos, ecefPos, app.camera.globeAltitude);
        }

        if (loading) app.requestUpdate();
    });

    // -------------------------------------------------------------------------
    // Draw callback
    // -------------------------------------------------------------------------
    app.addDrawCallback([&]() {
        if (earth.tick(app.camera, app.getWidth(), app.getHeight()) ||
            (vectorOverlayEnabled && vectorEarth.tick(app.camera, app.getWidth(), app.getHeight())))
            app.requestUpdate();

        // app.scene() is rendered automatically by renderScenePairs() — do NOT call render() here.

        // Tile debug labels
        if (showTileDebug) {
            auto tiles = earth.getVisibleTileDebugInfo();
            glm::dvec3 origin  = app.camera.getGlobeTargetECEF();
            glm::dvec3 camECEF = app.camera.getGlobeCameraECEF();
            for (const auto& t : tiles) {
                glm::dvec3 tileNormal = glm::normalize(t.centerECEF);
                glm::dvec3 viewDir    = glm::normalize(camECEF - t.centerECEF);
                if (glm::dot(tileNormal, viewDir) < 0.0) continue;
                glm::vec3 local = glm::vec3(t.centerECEF - origin);
                auto sp = app.camera.projectToScreen(local, float(app.getWidth()), float(app.getHeight()));
                if (!sp) continue;
                glm::vec3 color = t.hasTexture && !t.usingFallback
                                      ? glm::vec3(0.9f, 0.9f, 0.2f)
                                  : t.usingFallback
                                      ? glm::vec3(0.95f, 0.5f, 0.0f)
                                      : glm::vec3(1.0f, 0.4f, 0.0f);
                app.getRenderer().renderText(t.key, sp->x, sp->y, color, 1.5f);
            }
        }

        // Text info overlay (earth_viewer baseline)
        {
            char buf[256];
            snprintf(buf, sizeof(buf), "Location: %s", locations[curLoc].name);
            app.getRenderer().renderText(buf, 10, 30, glm::vec3(1.0f, 1.0f, 1.0f));

            snprintf(buf, sizeof(buf), "Tiles: %d visible, %d loading, %d cached",
                     earth.visibleTileCount(), earth.loadingTileCount(), earth.gpuTextureCount());
            app.getRenderer().renderText(buf, 10, 50, glm::vec3(0.8f, 0.8f, 0.8f));

            double alt = app.camera.globeAltitude;
            if (alt > 1000) snprintf(buf, sizeof(buf), "Altitude: %.1f km", alt / 1000.0);
            else            snprintf(buf, sizeof(buf), "Altitude: %.0f m", alt);
            app.getRenderer().renderText(buf, 10, 70, glm::vec3(0.8f, 0.8f, 0.8f));

            snprintf(buf, sizeof(buf), "Layer: %s", currentLayerId.c_str());
            app.getRenderer().renderText(buf, 10, 90, glm::vec3(0.8f, 0.8f, 0.8f));

            snprintf(buf, sizeof(buf), "Terrain: %s", terrainEnabled ? "ON" : "OFF");
            app.getRenderer().renderText(buf, 10, 110, glm::vec3(0.8f, 0.8f, 0.8f));

            app.getRenderer().renderText("Map data: MapTiler, OpenStreetMap contributors",
                                         10, app.getHeight() - 20, glm::vec3(0.5f, 0.5f, 0.5f));
        }
    });

    // -------------------------------------------------------------------------
    // UI
    // -------------------------------------------------------------------------
    RLGuiUI ui(app, earth, scenarioPins, scenario,
                showPins, terrainEnabled, currentLayerId, provider);

    auto drawLayersPanel = [&]() {
        std::string rasterName = currentLayerId;
        for (auto& l : provider->availableLayers())
            if (l.id == currentLayerId) { rasterName = l.displayName; break; }

        ImGui::SetNextWindowPos(ImVec2(240, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(220, 0), ImGuiCond_FirstUseEver);
        ImGui::Begin("Layers", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::SeparatorText("Raster");
        if (ImGui::Checkbox("Texture rendering", &showTextures))
            app.requestUpdate();
        ImGui::Text("%s", rasterName.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Next [L]")) { cycleRasterLayer(); app.requestUpdate(); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("cycle-raster-layer");
        if (ImGui::Checkbox("3D terrain mesh", &terrainEnabled))
            app.requestUpdate();

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

    app.registerPluginPanel({"Layers", &layersMode,
        [&](void* ctx) { ImGui::SetCurrentContext((ImGuiContext*)ctx); drawLayersPanel(); },
        /* panelOpen= */ true});

    app.registerPluginPanel({"Scenario", const_cast<MinorMode*>(scenario.getMinorMode()),
        [&](void* ctx) { scenario.drawImGuiPanel(app.camera, app, ctx); },
        /* panelOpen= */ false});

    ImGuiPlugin imgui(app);
    imgui.setImGuiCallback([&]() {
        imgui.drawPluginsPanel("geodraw");
        ui.draw();

        // Render pins / clusters as screen-space circles
        if (showPins && !scenarioPins.empty()) {
            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            glm::ivec2 fbSize = app.getFramebufferSize();
            int fbW = fbSize.x, fbH = fbSize.y;
            int winW = app.getWidth(), winH = app.getHeight();
            float sx = float(winW)/float(fbW), sy = float(winH)/float(fbH);

            for (const auto& c : markerClusterer.getClusters()) {
                ImVec2 ctr(c.screenCenter.x * sx, c.screenCenter.y * sy);
                constexpr float rad = 12.f;
                dl->AddCircleFilled(ctr, rad, IM_COL32(230,60,60,230), 16);
                dl->AddCircle(ctr, rad+1.f, IM_COL32_WHITE, 16, 2.f);
                if (c.count() > 1) {
                    std::string lbl = "(" + std::to_string(c.count()) + ")";
                    ImVec2 tp(ctr.x + rad + 4, ctr.y - 6);
                    dl->AddText(ImVec2(tp.x+1,tp.y+1), IM_COL32(0,0,0,180), lbl.c_str());
                    dl->AddText(tp, IM_COL32_WHITE, lbl.c_str());
                }
            }
        }

        // Ray picking for tooltips
        if (!ImGui::GetIO().WantCaptureMouse) {
            glm::dvec2 cur = app.getCursorPos();
            double mx = cur.x, my = cur.y;
            glm::ivec2 fbSize = app.getFramebufferSize();
            int fbW = fbSize.x, fbH = fbSize.y;
            int winW = app.getWidth(), winH = app.getHeight();
            float sx = float(fbW)/float(winW), sy = float(fbH)/float(winH);
            float fbmx = float(mx)*sx, fbmy = float(my)*sy;

            Camera::Ray ray = app.camera.getRayFromScreenPos(
                fbmx, fbmy, float(fbW), float(fbH));
            PickResult pick = app.scene().rayPick(ray, true);
            tooltipSystem.update(pick, mx, my);

            // Cluster click handling
            if (showPins && !scenarioPins.empty()) {
                const auto& clusters = markerClusterer.getClusters();
                const clustering::Cluster* hov = nullptr;
                constexpr float hr = 12.f;
                float wmx = float(mx), wmy = float(my);
                float scaleX = float(winW)/float(fbW);
                float scaleY = float(winH)/float(fbH);
                for (const auto& cl : clusters) {
                    if (!cl.isSingle()) {
                        float cx = cl.screenCenter.x / sx;
                        float cy = cl.screenCenter.y / sy;
                        float d  = std::sqrt((wmx-cx)*(wmx-cx) + (wmy-cy)*(wmy-cy));
                        if (d < hr*1.5f) { hov = &cl; break; }
                    }
                }
                (void)scaleX; (void)scaleY;
                static bool wasDown = false;
                bool down = app.isMouseButtonPressed(0);
                if (hov && down && !wasDown) {
                    ECEFCoord ce(hov->ecefCenter.x, hov->ecefCenter.y, hov->ecefCenter.z);
                    GeoCoord cg = ecefToGeo(ce).toDegrees();
                    app.camera.setGlobeTarget(cg.latitude, cg.longitude);
                    app.camera.setGlobeAltitude(app.camera.globeAltitude / 3.0);
                    app.requestUpdate();
                }
                wasDown = down;
            }
        }

        if (const auto* c = tooltipSystem.render()) {
            ImGui::BeginTooltip();
            if (c->customRender)        c->customRender();
            else if (!c->text.empty()) ImGui::TextUnformatted(c->text.c_str());
            ImGui::EndTooltip();
        }
    });

    std::cout << "Starting RL GUI...\n"
              << "Press N/M to change location, P to pick nearest scenario\n"
              << "Press D for tile debug, = / - for LOD bias, C to print coordinates\n"
              << "Press X to toggle earth layer, E to toggle 3D building extrusion\n";

    app.run();
    return 0;
}
