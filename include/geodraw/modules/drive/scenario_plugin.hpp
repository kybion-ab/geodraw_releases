/*******************************************************************************
 * File: scenario_plugin.hpp
 *
 * Description: IAppModule plugin for vehicle trajectory scenario visualization.
 *
 * Can operate in two modes:
 *   Standalone – owns its own Scene, acts as the primary app renderer.
 *   Embedded   – shares the host app's Scene and adds geometry at
 *                Priority::OVERLAY (e.g. on top of an earth layer).
 *
 * Usage (standalone):
 *   ScenarioPlugin::runStandalone("data/scene.json");
 *
 * Usage (embedded in rl_gui):
 *   ScenarioPlugin scenario(app.scene());
 *   scenario.attach(app);
 *   // …when user picks a pin:
 *   scenario.setGeoReference(earth::GeoReference(pin.location));
 *   scenario.activateScenario(pin.filepath, app);
 *   // …in ImGui callback:
 *   if (app.isMinorModeActive(*scenario.getMinorMode()))
 *       scenario.drawImGuiPanel(app.camera, app);
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2026-02-20
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/app/app_module.hpp"
#include <functional>
#include <memory>
#include <string>

namespace geodraw {

class App;
class Camera;
class Scene;
struct MinorMode;

namespace earth {
struct GeoReference;
}

namespace gltf {
struct LoadedModel;
}

/**
 * Plugin for vehicle trajectory scenario visualization.
 *
 * Implements IAppModule so it integrates with App via attach()/detach().
 */
class GEODRAW_API ScenarioPlugin : public IAppModule {
public:
    /// Create standalone plugin (owns a private Scene).
    ScenarioPlugin();

    /// Create embedded plugin (uses an externally owned Scene).
    explicit ScenarioPlugin(Scene& sharedScene);

    ~ScenarioPlugin();

    ScenarioPlugin(const ScenarioPlugin&) = delete;
    ScenarioPlugin& operator=(const ScenarioPlugin&) = delete;
    ScenarioPlugin(ScenarioPlugin&&) noexcept;
    ScenarioPlugin& operator=(ScenarioPlugin&&) noexcept;

    //=========================================================================
    // IAppModule interface
    //=========================================================================

    /// Register callbacks and keybindings with \p app.
    /// Standalone mode: registers primary update/draw callbacks (priority BASE).
    /// Embedded mode:   registers update callback at priority OVERLAY and
    ///                  creates a "Scenario" minor mode for keybindings.
    void attach(App& app) override;

    /// Deactivate minor mode (embedded) and clear the App pointer.
    void detach(App& app) override;

    //=========================================================================
    // Domain configuration
    //=========================================================================

    /// Set a geo-reference for ENU→ECEF coordinate transformation (globe mode).
    /// Can be called before or after attach().
    void setGeoReference(const earth::GeoReference& ref);

    /// Clear the geo-reference (revert to local ENU coordinates).
    void clearGeoReference();

    //=========================================================================
    // Scenario management
    //=========================================================================

    /// Load and activate a scenario from a JSON file.
    /// In embedded mode also activates the "Scenario" minor mode.
    void activateScenario(const std::string& filepath, App& app);

    /// Returns true if a scenario has been loaded.
    bool isLoaded() const;

    /// Set the 3D car model used to render vehicles.
    /// Call this after gltf::loadGLB() and before activating scenarios.
    void setCarModel(gltf::LoadedModel model);

    //=========================================================================
    // Access for host applications
    //=========================================================================

    /// Minor mode created during attach() for embedded usage.
    /// Returns nullptr in standalone mode or before attach() is called.
    const MinorMode* getMinorMode() const;

    // Playback control for external synchronization (e.g. video capture)
    void setCurrentTimestep(int ts);   // clamps to [0, maxTimesteps-1]
    int  currentTimestep() const;
    int  maxTimesteps() const;
    void setPlaying(bool playing);
    bool isPlaying() const;

    //=========================================================================
    // File/folder picker callbacks (optional — set by host app)
    //=========================================================================

    /// Callback invoked when the user clicks a "Browse..." button for a folder.
    /// Receives the current path as default; returns selected path or "" on cancel.
    /// If not set, an inline ImGui dir browser is shown as fallback.
    using FolderPickFn = std::function<std::string(const std::string& defaultPath)>;
    void setFolderPickCallback(FolderPickFn cb);

    /// Callback invoked when the user clicks a "Browse..." button for a .json file.
    /// Receives the current path as default; returns selected path or "" on cancel.
    /// If not set, an inline ImGui file browser is shown as fallback.
    using FilePickFn = std::function<std::string(const std::string& defaultPath)>;
    void setFilePickCallback(FilePickFn cb);

    //=========================================================================
    // ImGui panel
    //=========================================================================

#ifdef GEODRAW_HAS_IMGUI
    /// Draw the scenario ImGui control panel.
    /// Call this inside an imgui.setImGuiCallback() lambda.
    /// Pass ImGui::GetCurrentContext() as imguiCtx so that the shared library
    /// can call ImGui::SetCurrentContext() before making any ImGui calls.
    /// (Required when ImGui is compiled separately into the host binary and
    /// the shared library — each has its own GImGui global.)
    void drawImGuiPanel(Camera& camera, App& app, void* imguiCtx = nullptr);
#endif

    //=========================================================================
    // Tooltip support
    //=========================================================================

    /// Returns a formatted info string for the vehicle with the given scene
    /// object ID (as assigned by addVehicle: vi+1 for vehicle at index vi).
    /// Returns an empty string if the scenario is not loaded, the ID is out of
    /// range, or the vehicle is not valid at the current timestep.
    std::string getVehicleInfo(uint64_t objectId) const;

    //=========================================================================
    // Standalone factory
    //=========================================================================

    /// Create a self-contained App, load a scenario, and run to completion.
    void runStandalone(const std::string& filepath,
                       int width = 1280, int height = 720);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace geodraw
