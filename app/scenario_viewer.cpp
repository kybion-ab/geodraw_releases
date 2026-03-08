/*******************************************************************************
 * File: scenario_viewer.cpp
 *
 * Description: Standalone entry point for the vehicle trajectory viewer.
 *
 * All application logic lives in ScenarioPlugin::runStandalone().
 *
 *******************************************************************************/

#include "geodraw/modules/drive/scenario_plugin.hpp"
#include <cstdlib>
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

    geodraw::ScenarioPlugin::runStandalone(filepath);
    return 0;
}
