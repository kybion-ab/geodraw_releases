/*******************************************************************************
 * Demo: Video Capture
 *
 * Layers VideoCapturePlugin on top of CameraTrajectoryPlugin.
 * Together they let you:
 *   1. Record a camera path (CameraTrajectoryPlugin)
 *   2. Replay it and capture each frame to disk (VideoCapturePlugin)
 *
 * Pattern — two extra lines after camera-trajectory setup:
 *   VideoCapturePlugin videoCap(camTraj);          // requires camTraj reference
 *   app.addModule(videoCap);
 *   app.activateMinorMode(*videoCap.getMinorMode());
 *
 * Both plugins expose an ImGui panel; show them in the same callback.
 *******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/geometry/geometry.hpp"
#include "geodraw/ui/imgui_plugin.h"
#include "geodraw/modules/camera_trajectory/camera_trajectory_plugin.hpp"
#include "geodraw/modules/video_capture/video_capture_plugin.hpp"

using namespace geodraw;

std::string demoDocstring() {
  return R"(=== Demo Video Capture ===

Demonstrates video capture procedure.
1. Add keyframes
2. Bake interpolated frames
3. Capture video
4. Save as mp4)";
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

int main() {
    App app(1280, 720, "Demo Video Capture");
    app.addScene();

    DemoGeometry geometry;

    // Camera trajectory plugin (record / playback)
    CameraTrajectoryPlugin camTraj;
    app.addModule(camTraj);

    // -------------------------------------------------------------------------
    // VideoCapturePlugin — captures rendered frames during trajectory playback.
    // Requires a reference to CameraTrajectoryPlugin so it can hook into
    // playback start/stop events.
    // -------------------------------------------------------------------------
    VideoCapturePlugin videoCap(camTraj);
    app.addModule(videoCap);

    // drawPluginsPanel() shows a "Plugins" checklist, both plugin panels,
    // and handles minor mode activation automatically.
    ImGuiPlugin imgui(app);
    imgui.setImGuiCallback([&]() {
        imgui.drawPluginsPanel();
    });

    app.addUpdateCallback([&](float /*dt*/) {
        UpdateDemoGeometry(app.scene(), geometry);
    });

    app.setDocstring(demoDocstring());

    app.run();
    return 0;
}
