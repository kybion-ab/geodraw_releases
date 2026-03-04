/*******************************************************************************
 * File: scenario_viewer.cpp
 *
 * Description: Standalone entry point for the vehicle trajectory viewer.
 *
 * All application logic lives in ScenarioPlugin::runStandalone().
 *
 *******************************************************************************/

#include "geodraw/modules/drive/scenario_plugin.hpp"

int main(int argc, char** argv) {
    const std::string filepath = (argc > 1)
        ? argv[1]
        : "../data/examples/tfrecord-00000-of-01000_4.json";

    geodraw::ScenarioPlugin::runStandalone(filepath);
    return 0;
}
