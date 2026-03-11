/*******************************************************************************
 * Demo: Camera Trajectory
 *
 * Demonstrates the CameraTrajectoryPlugin for recording and replaying camera paths.
 *
 * Pattern — three lines after App setup:
 *   CameraTrajectoryPlugin camTraj;
 *   app.addModule(camTraj);                        // register update/draw hooks
 *   app.activateMinorMode(*camTraj.getMinorMode()); // install keyboard bindings
 *
 * The plugin's ImGui panel (shown via drawImGuiPanel()) provides record /
 * playback / speed controls.
 *
 * Note on ImGui context forwarding:
 *   Both the main binary (rl_gui) and the shared library (libgeodraw) compile
 *   their own copy of ImGui, each with its own GImGui global.  Any ImGui call
 *   inside shared-library code must use the main binary's context; pass it
 *   explicitly via ImGuiCtx(ImGui::GetCurrentContext()) at the call site.
 *******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/geometry/geometry.hpp"
#include "geodraw/ui/imgui_plugin.h"
#include "geodraw/modules/camera_trajectory/camera_trajectory_plugin.hpp"

using namespace geodraw;

std::string demoDocstring() {
  return R"(=== Demo Camera Trajectory ===

Demonstrates camera trajectory.
1. Add keyframes by positioning viewer and press 'Add'
2. Bake interpolated frames
3. View trajectory
4. Save as CSV file)";
}

struct DemoGeometry {
  Pose3 axes;
  Triangle3 triangle{Pos3{1, 1, 0}, Pos3{3, 2, 0}, Pos3{1, 2, 1}};
};

void UpdateDemoGeometry(Scene &scene, DemoGeometry &gui) {
  scene.clear();
  scene.Add(gui.axes, /*scale=*/2.0f, /*thickness=*/5.0f);
  scene.Add(gui.triangle, /*thickness=*/2.0f, /*linestyle=*/LineStyle::Solid,
  /*alpha=*/1.0, /*color=*/Colors::ORANGE);
}

void DemoImGuiCallback(App& app, DemoGeometry &geometry, CameraTrajectoryPlugin& camTraj) {
  // Pass ImGui::GetCurrentContext() so the shared-library code uses the
  // correct GImGui global (see note at the top of this file).
  camTraj.drawImGuiPanel(app.camera, app, geodraw::ImGuiCtx(ImGui::GetCurrentContext()));
}


int main() {
    App app(1280, 720, "Demo Camera Trajectory");
    app.addScene();

    DemoGeometry geometry;

    // -------------------------------------------------------------------------
    // CameraTrajectoryPlugin — record keyframes, then play them back smoothly.
    // -------------------------------------------------------------------------
    CameraTrajectoryPlugin camTraj;
    app.addModule(camTraj);
    app.activateMinorMode(*camTraj.getMinorMode());

    // ImGuiPlugin hosts the camera-trajectory control panel.
    ImGuiPlugin imgui(app);
    imgui.setImGuiCallback([&]() {
        DemoImGuiCallback(app, geometry, camTraj);
    });

    app.addUpdateCallback([&](float /*dt*/) {
        UpdateDemoGeometry(app.scene(), geometry);
    });

    app.setDocstring(demoDocstring());

    app.run();
    return 0;
}
