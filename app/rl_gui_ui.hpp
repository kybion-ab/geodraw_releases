/*******************************************************************************
 * File: rl_gui_ui.hpp
 *
 * Description: UI helpers for rl_gui: data structures (ScenarioPin,
 * ScenarioFilter), free helper functions, and the RLGuiUI class.
 *
 * Extracted from rl_gui.cpp so that the main application file stays thin and
 * these pieces can be understood (and eventually tested) in isolation.
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2026-02-22
 *
 *******************************************************************************/

#pragma once

#include "geodraw/app/app.hpp"
#include "geodraw/modules/earth/earth.hpp"
#include "geodraw/modules/drive/scenario_plugin.hpp"

#include <string>
#include <vector>
#include <utility>
#include <memory>

// One ring of a GeoJSON polygon: [(lon, lat), ...]
using GeoRing = std::vector<std::pair<double, double>>;

// =============================================================================
// ScenarioPin — scenario location + computed metrics
// =============================================================================

struct ScenarioPin {
    std::string filepath;
    std::string name;
    geodraw::earth::GeoCoord location;
    double      radius = 0.0;
    bool        hasMetadata = false;

    int    vehicleCount       = 0;
    int    laneCount          = 0;
    double initialSpeedMin    = 0.0;
    double initialSpeedMax    = 0.0;
    double speedLimitEstimate = 0.0;
    double complexityScore    = 0.0;
};

// =============================================================================
// ScenarioFilter — UI-driven filter state
// =============================================================================

struct ScenarioFilter {
    bool includeMatching = true;

    bool  filterInitialSpeed = false;
    float initialSpeedMin = 0.0f, initialSpeedMax = 50.0f;

    bool  filterSpeedLimit = false;
    float speedLimitMin = 0.0f, speedLimitMax = 50.0f;

    bool filterVehicleCount = false;
    int  vehicleCountMin = 1, vehicleCountMax = 50;

    bool  filterComplexity = false;
    float complexityMin = 0.0f, complexityMax = 200.0f;

    bool filterShape = false;
    char geoJsonPath[512] = "";
};

// =============================================================================
// Free helper functions
// =============================================================================

ScenarioPin parseScenarioMetadata(const std::string& filepath);
double haversineDistance(const geodraw::earth::GeoCoord& a,
                         const geodraw::earth::GeoCoord& b);
std::vector<GeoRing> loadGeoJsonPolygons(const std::string& path);
bool pointInPolygons(double lat, double lon, const std::vector<GeoRing>& rings);
bool matchesFilter(const ScenarioPin& p, const ScenarioFilter& f,
                   const std::vector<GeoRing>& polys);

// =============================================================================
// RLGuiUI — ImGui panels for the rl_gui application
// =============================================================================

#ifdef GEODRAW_HAS_IMGUI

class RLGuiUI {
public:
    RLGuiUI(geodraw::App& app,
            geodraw::earth::EarthLayer& earth,
            std::vector<ScenarioPin>& pins,
            geodraw::ScenarioPlugin& scenario,
            bool& showPins,
            bool& terrainEnabled,
            std::string& currentLayerId,
            std::shared_ptr<geodraw::earth::TileProvider> provider);

    void draw();

private:
    void drawEarthPanel();
    void drawFilterPanel();
    void applyFilter();

    geodraw::App& app_;
    geodraw::earth::EarthLayer& earth_;
    std::vector<ScenarioPin>& scenarioPins_;
    geodraw::ScenarioPlugin& scenario_;
    bool& showPins_;
    bool& terrainEnabled_;
    std::string& currentLayerId_;
    std::shared_ptr<geodraw::earth::TileProvider> provider_;

    char directoryPath_[512] = "";
    bool showFileBrowser_    = false;
    std::string currentBrowserDir_;
    int selectedPinIndex_    = -1;

    std::vector<ScenarioPin> allScenarioPins_;
    ScenarioFilter           filter_;
    std::vector<GeoRing>     filterPolygons_;
    bool                     showFilterPanel_   = false;
    std::string              filterGeoJsonStatus_;
};

#endif // GEODRAW_HAS_IMGUI
