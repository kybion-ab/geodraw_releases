/*******************************************************************************
 * File: scenario_viewer.cpp
 *
 * Description: Standalone entry point for the vehicle trajectory viewer.
 *
 * All application logic lives in ScenarioPlugin::runStandalone().
 *
 *******************************************************************************/

#include "geodraw/modules/drive/scenario_plugin.hpp"
#include <nfd.hpp>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>

int main(int argc, char** argv) {
    std::string filepath;
    if (argc > 1) {
        filepath = argv[1];
    } else {
        const char* root = getenv("GEODRAW_ROOT");
        if (!root) {
            throw std::runtime_error("GEODRAW_ROOT not set");
        }
        filepath = std::string(root) + "/data/examples/tfrecord-00000-of-01000_4.json";
    }

    NFD::Guard nfdGuard;

    geodraw::ScenarioPlugin scenario;

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
        if (!dir.empty()) {
            namespace fs = std::filesystem;
            if (fs::is_regular_file(dir)) dir = fs::path(dir).parent_path().string();
        }
        if (NFD::OpenDialog(out, filters, 1, dir.empty() ? nullptr : dir.c_str()) == NFD_OKAY)
            return out.get();
        return "";
    });

    scenario.runStandalone(filepath);
    return 0;
}
