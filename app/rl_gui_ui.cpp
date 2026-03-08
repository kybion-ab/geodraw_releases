/*******************************************************************************
 * File: rl_gui_ui.cpp
 *
 * Description: Implementation of rl_gui UI helpers.
 * See rl_gui_ui.hpp.
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2026-02-22
 *
 *******************************************************************************/

#include "rl_gui_ui.hpp"

#include "geodraw/modules/earth/earth_coords.hpp"

#ifdef GEODRAW_HAS_IMGUI
#include <imgui.h>
#include <nfd.hpp>
#endif

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace geodraw;
using namespace geodraw::earth;

// =============================================================================
// Helper functions
// =============================================================================

static void computeScenarioMetrics(const nlohmann::json& j, ScenarioPin& pin) {
    try {
        if (j.contains("objects") && j["objects"].is_array()) {
            pin.vehicleCount = (int)j["objects"].size();

            std::vector<double> initialSpeeds;
            for (const auto& obj : j["objects"]) {
                if (!obj.contains("vx") || !obj.contains("vy") || !obj.contains("valid"))
                    continue;
                const auto& vx    = obj["vx"];
                const auto& vy    = obj["vy"];
                const auto& valid = obj["valid"];
                for (size_t t = 0; t < valid.size(); ++t) {
                    if (valid[t].get<bool>() && t < vx.size() && t < vy.size()) {
                        double vxt = vx[t].get<double>(), vyt = vy[t].get<double>();
                        initialSpeeds.push_back(std::sqrt(vxt*vxt + vyt*vyt));
                        break;
                    }
                }
            }
            if (!initialSpeeds.empty()) {
                pin.initialSpeedMin = *std::min_element(initialSpeeds.begin(), initialSpeeds.end());
                pin.initialSpeedMax = *std::max_element(initialSpeeds.begin(), initialSpeeds.end());
            }
        }

        if (j.contains("roads") && j["roads"].is_array())
            pin.laneCount = (int)j["roads"].size();

        if (j.contains("metadata")) {
            pin.speedLimitEstimate = j["metadata"].value("speed_limit", 0.0);
            pin.complexityScore    = j["metadata"].value("complexity",  0.0);
        }
        if (pin.complexityScore == 0.0)
            pin.complexityScore = pin.vehicleCount * 2.0 + pin.laneCount;
    } catch (...) {}
}

ScenarioPin parseScenarioMetadata(const std::string& filepath) {
    ScenarioPin pin;
    pin.filepath = filepath;
    pin.name = fs::path(filepath).filename().string();
    pin.hasMetadata = false;

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) return pin;

        nlohmann::json j;
        file >> j;

        if (j.contains("metadata") && j["metadata"].contains("origin")) {
            auto origin = j["metadata"]["origin"];
            pin.location = GeoCoord(
                origin.value("latitude",  0.0),
                origin.value("longitude", 0.0),
                origin.value("altitude",  0.0));
            pin.radius = j["metadata"].value("radius", 100.0);
            pin.hasMetadata = true;
        }
        if (j.contains("metadata") && j["metadata"].contains("coordinate")) {
            auto origin = j["metadata"]["coordinate"];
            pin.location = GeoCoord(origin[0], origin[1], 0.0);
            pin.radius = j["metadata"].value("radius", 100.0);
            pin.hasMetadata = true;
        }
        if (pin.hasMetadata) computeScenarioMetrics(j, pin);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing metadata from " << filepath << ": " << e.what() << "\n";
    }

    return pin;
}

double haversineDistance(const GeoCoord& a, const GeoCoord& b) {
    constexpr double R = 6371000.0;
    double lat1 = deg2rad(a.latitude),  lat2 = deg2rad(b.latitude);
    double dLat = deg2rad(b.latitude - a.latitude);
    double dLon = deg2rad(b.longitude - a.longitude);
    double h = std::sin(dLat/2)*std::sin(dLat/2) +
               std::cos(lat1)*std::cos(lat2)*std::sin(dLon/2)*std::sin(dLon/2);
    return R * 2 * std::atan2(std::sqrt(h), std::sqrt(1-h));
}

std::vector<GeoRing> loadGeoJsonPolygons(const std::string& path) {
    std::vector<GeoRing> rings;
    try {
        std::ifstream f(path);
        if (!f.is_open()) return rings;
        nlohmann::json j;
        f >> j;

        std::function<void(const nlohmann::json&)> processGeometry =
            [&](const nlohmann::json& geom) {
                if (!geom.contains("type")) return;
                std::string type = geom["type"];
                if (type == "Polygon" && geom.contains("coordinates")) {
                    const auto& coords = geom["coordinates"];
                    if (!coords.empty() && coords[0].is_array()) {
                        GeoRing ring;
                        for (const auto& pt : coords[0])
                            if (pt.is_array() && pt.size() >= 2)
                                ring.emplace_back(pt[0].get<double>(), pt[1].get<double>());
                        rings.push_back(std::move(ring));
                    }
                } else if (type == "MultiPolygon" && geom.contains("coordinates")) {
                    for (const auto& poly : geom["coordinates"])
                        if (!poly.empty() && poly[0].is_array()) {
                            GeoRing ring;
                            for (const auto& pt : poly[0])
                                if (pt.is_array() && pt.size() >= 2)
                                    ring.emplace_back(pt[0].get<double>(), pt[1].get<double>());
                            rings.push_back(std::move(ring));
                        }
                }
            };

        std::string topType = j.value("type", "");
        if (topType == "Feature") {
            if (j.contains("geometry")) processGeometry(j["geometry"]);
        } else if (topType == "FeatureCollection") {
            if (j.contains("features"))
                for (const auto& feat : j["features"])
                    if (feat.contains("geometry")) processGeometry(feat["geometry"]);
        } else {
            processGeometry(j);
        }
    } catch (...) {}
    return rings;
}

bool pointInPolygons(double lat, double lon, const std::vector<GeoRing>& rings) {
    for (const auto& ring : rings) {
        if (ring.size() < 3) continue;
        bool inside = false;
        size_t n = ring.size();
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            double xi = ring[i].first, yi = ring[i].second;
            double xj = ring[j].first, yj = ring[j].second;
            if (((yi > lat) != (yj > lat)) &&
                (lon < (xj - xi) * (lat - yi) / (yj - yi) + xi))
                inside = !inside;
        }
        if (inside) return true;
    }
    return false;
}

bool matchesFilter(const ScenarioPin& p, const ScenarioFilter& f,
                   const std::vector<GeoRing>& polys) {
    if (f.filterInitialSpeed) {
        if (!(p.initialSpeedMin <= (double)f.initialSpeedMax &&
              p.initialSpeedMax >= (double)f.initialSpeedMin))
            return false;
    }
    if (f.filterSpeedLimit) {
        if (p.speedLimitEstimate < (double)f.speedLimitMin ||
            p.speedLimitEstimate > (double)f.speedLimitMax)
            return false;
    }
    if (f.filterVehicleCount) {
        if (p.vehicleCount < f.vehicleCountMin || p.vehicleCount > f.vehicleCountMax)
            return false;
    }
    if (f.filterComplexity) {
        if (p.complexityScore < (double)f.complexityMin ||
            p.complexityScore > (double)f.complexityMax)
            return false;
    }
    if (f.filterShape) {
        if (!pointInPolygons(p.location.latitude, p.location.longitude, polys))
            return false;
    }
    return true;
}

// =============================================================================
// RLGuiUI
// =============================================================================

#ifdef GEODRAW_HAS_IMGUI

RLGuiUI::RLGuiUI(geodraw::App& app, EarthLayer& earth,
                 std::vector<ScenarioPin>& pins, ScenarioPlugin& scenario,
                 bool& showPins, bool& terrainEnabled,
                 std::string& currentLayerId, std::shared_ptr<TileProvider> provider)
    : app_(app), earth_(earth), scenarioPins_(pins),
      scenario_(scenario),
      showPins_(showPins), terrainEnabled_(terrainEnabled),
      currentLayerId_(currentLayerId), provider_(provider) {}

void RLGuiUI::applyFilter() {
    scenarioPins_.clear();
    for (const auto& pin : allScenarioPins_) {
        bool matches = matchesFilter(pin, filter_, filterPolygons_);
        if (matches == filter_.includeMatching)
            scenarioPins_.push_back(pin);
    }
    app_.requestUpdate();
}

void RLGuiUI::drawFilterPanel() {
    ImGui::SetNextWindowPos(ImVec2(300, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Scenario Filters", &showFilterPanel_,
                      ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    // Mode
    ImGui::Text("Mode:");
    ImGui::SameLine();
    int mode = filter_.includeMatching ? 0 : 1;
    if (ImGui::RadioButton("Include matching", &mode, 0)) filter_.includeMatching = true;
    ImGui::SameLine();
    if (ImGui::RadioButton("Exclude matching", &mode, 1)) filter_.includeMatching = false;

    ImGui::Separator();

    // Initial Speed
    ImGui::Checkbox("Initial Speed [m/s]", &filter_.filterInitialSpeed);
    if (filter_.filterInitialSpeed) {
        ImGui::SliderFloat("Min##spd", &filter_.initialSpeedMin, 0.0f, 80.0f, "%.1f");
        ImGui::SliderFloat("Max##spd", &filter_.initialSpeedMax, 0.0f, 80.0f, "%.1f");
    }

    ImGui::Separator();

    // Speed Limit
    ImGui::Checkbox("Speed Limit [m/s]", &filter_.filterSpeedLimit);
    if (filter_.filterSpeedLimit) {
        ImGui::SliderFloat("Min##lim", &filter_.speedLimitMin, 0.0f, 80.0f, "%.1f");
        ImGui::SliderFloat("Max##lim", &filter_.speedLimitMax, 0.0f, 80.0f, "%.1f");
    }

    ImGui::Separator();

    // Vehicle Count
    ImGui::Checkbox("Vehicle Count", &filter_.filterVehicleCount);
    if (filter_.filterVehicleCount) {
        ImGui::SliderInt("Min##vc", &filter_.vehicleCountMin, 0, 200);
        ImGui::SliderInt("Max##vc", &filter_.vehicleCountMax, 0, 200);
    }

    ImGui::Separator();

    // Complexity Score
    ImGui::Checkbox("Complexity Score", &filter_.filterComplexity);
    if (filter_.filterComplexity) {
        ImGui::SliderFloat("Min##cx", &filter_.complexityMin, 0.0f, 500.0f, "%.0f");
        ImGui::SliderFloat("Max##cx", &filter_.complexityMax, 0.0f, 500.0f, "%.0f");
    }

    ImGui::Separator();

    // Geographic Shape
    ImGui::Checkbox("Geographic Shape (GeoJSON)", &filter_.filterShape);
    if (filter_.filterShape) {
        ImGui::InputText("GeoJSON path", filter_.geoJsonPath, sizeof(filter_.geoJsonPath));
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            filterPolygons_ = loadGeoJsonPolygons(filter_.geoJsonPath);
            if (filterPolygons_.empty())
                filterGeoJsonStatus_ = "No rings loaded (invalid path or format)";
            else
                filterGeoJsonStatus_ = "Loaded " + std::to_string(filterPolygons_.size()) + " ring(s)";
        }
        if (!filterGeoJsonStatus_.empty())
            ImGui::TextUnformatted(filterGeoJsonStatus_.c_str());
    }

    ImGui::Separator();

    if (ImGui::Button("Apply Filter", ImVec2(-1, 0)))
        applyFilter();

    if (ImGui::Button("Reset / Show All", ImVec2(-1, 0))) {
        filter_ = ScenarioFilter{};
        filterPolygons_.clear();
        filterGeoJsonStatus_.clear();
        scenarioPins_ = allScenarioPins_;
        app_.requestUpdate();
    }

    ImGui::End();
}

void RLGuiUI::draw() {
    drawEarthPanel();
    if (showFilterPanel_) drawFilterPanel();
}

void RLGuiUI::drawEarthPanel() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 0), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Scenario Loader", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    // Scenario Directory section
    ImGui::Text("Scenario Directory:");
    ImGui::InputText("##dirpath", directoryPath_, sizeof(directoryPath_));
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        NFD::UniquePath out;
        std::string def = directoryPath_[0] ? directoryPath_ : fs::current_path().string();
        if (NFD::PickFolder(out, def.c_str()) == NFD_OKAY) {
            std::strncpy(directoryPath_, out.get(), sizeof(directoryPath_) - 1);
            directoryPath_[sizeof(directoryPath_) - 1] = '\0';
        }
    }

    if (ImGui::Button("Scan Directory", ImVec2(-1, 0))) {
        if (std::strlen(directoryPath_) > 0 && fs::exists(directoryPath_)) {
            allScenarioPins_.clear();
            int count = 0, skipped = 0;
            try {
                for (auto& e : fs::recursive_directory_iterator(
                         directoryPath_, fs::directory_options::skip_permission_denied)) {
                    std::string ext = e.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    if (ext == ".json") {
                        ScenarioPin pin = parseScenarioMetadata(e.path().string());
                        if (pin.hasMetadata) {
                            allScenarioPins_.push_back(pin); ++count;
                        } else {
                            ++skipped;
                            std::cerr << "Warning: no location metadata in "
                                      << e.path().filename().string() << "\n";
                        }
                    }
                }
            } catch (const std::exception& ex) {
                std::cerr << "Scan error: " << ex.what() << "\n";
            }
            skippedCount_ = skipped;
            std::cout << "Found " << count << " scenarios with location metadata";
            if (skipped > 0)
                std::cout << " (" << skipped << " JSON files skipped — no location metadata)";
            std::cout << "\n";
            applyFilter();
        }
    }

    ImGui::Separator();
    if (allScenarioPins_.size() != scenarioPins_.size())
        ImGui::Text("Showing: %zu / %zu scenarios", scenarioPins_.size(), allScenarioPins_.size());
    else if (skippedCount_ > 0)
        ImGui::Text("Found: %zu scenarios (%d skipped, no location)", scenarioPins_.size(), skippedCount_);
    else
        ImGui::Text("Found: %zu scenarios", scenarioPins_.size());
    if (ImGui::Button(showFilterPanel_ ? "Filters (open)" : "Filters...", ImVec2(-1, 0)))
        showFilterPanel_ = !showFilterPanel_;

    if (!scenarioPins_.empty()) {
        ImGui::BeginChild("ScenarioList", ImVec2(0, 120), true);
        for (size_t i = 0; i < scenarioPins_.size(); ++i) {
            const auto& pin = scenarioPins_[i];
            bool sel = (selectedPinIndex_ == int(i));
            if (ImGui::Selectable(pin.name.c_str(), sel)) {
                selectedPinIndex_ = int(i);
                app_.camera.setGlobeTarget(pin.location.latitude, pin.location.longitude);
                app_.camera.setGlobeAltitude(pin.radius * 5.0);
                app_.requestUpdate();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Lat: %.4f, Lon: %.4f\nRadius: %.0fm",
                                  pin.location.latitude, pin.location.longitude, pin.radius);
        }
        ImGui::EndChild();
    }

    if (ImGui::Checkbox("Show Pins",  &showPins_))     app_.requestUpdate();

    ImGui::End();
}

#endif // GEODRAW_HAS_IMGUI
