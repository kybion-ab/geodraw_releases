/*******************************************************************************
 * Demo: ImGui Panel
 *
 * Adds a floating ImGui control window to the hello-world scene.
 *
 * Pattern — two lines on top of any existing App:
 *   ImGuiPlugin imgui(app);                // 1. construct (wires into render loop)
 *   imgui.setImGuiCallback([&]() { ... }); // 2. set callback with ImGui:: calls
 *
 * Everything inside the callback runs inside a valid ImGui frame.
 * State mutated by the UI is read by the update callback every frame,
 * so the scene responds to controls in real time.
 *******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/geometry/geometry.hpp"
#include "geodraw/ui/imgui_plugin.h"

using namespace geodraw;

std::string demoDocstring() {
  return R"(=== Demo ImGui Panel ===

Demonstrates an ImGui panel added to the scene.)";
}

struct DemoGeometry {
  Pose3 axes; // Drawn as color-labeled coordinate axes.
  Triangle3 triangle{Pos3{1, 1, 0}, Pos3{3, 2, 0}, Pos3{1, 2, 1}};
  float triangleColor[3] = {0.95f, 0.5f, 0.0f}; // Orange
};

void UpdateDemoGeometry(Scene &scene, DemoGeometry &geometry) {
  scene.clear();
  scene.Add(geometry.axes, /*scale=*/2.0f, /*thickness=*/5.0f);
  scene.Add(geometry.triangle, /*thickness=*/2.0f, /*linestyle=*/LineStyle::Solid,
            /*alpha=*/1.0, /*color=*/glm::vec3(geometry.triangleColor[0],
                                               geometry.triangleColor[1],
                                               geometry.triangleColor[2]));
}

void DemoImGuiCallback(App& app, DemoGeometry &geometry) {
  ImGui::Begin("Color picking");
  ImGui::Separator();

  // ColorEdit3 writes directly into our float[3] array.
  if(ImGui::ColorEdit3("Triangle color", geometry.triangleColor)){
    app.requestUpdate(); // Make sure we get updates in real time.
  }
  ImGui::End();
}

int main() {
    App app(1280, 720, "Demo ImGui Panel");
    app.addScene();

    DemoGeometry geometry;

    // -------------------------------------------------------------------------
    // ImGuiPlugin: wires ImGui into the rendering loop automatically.
    // Constructing it is enough — no manual frame begin/end needed.
    // -------------------------------------------------------------------------
    ImGuiPlugin imgui(app);

    // setImGuiCallback() runs every frame inside a valid ImGui context.
    // Call any ImGui:: function here — Begin/End windows, sliders, buttons, etc.
    imgui.setImGuiCallback([&]() {
      DemoImGuiCallback(app, geometry);
    });

    // The update callback reads the state set by the ImGui callback above.
    app.addUpdateCallback([&](float /*dt*/) {
      UpdateDemoGeometry(app.scene(), geometry);
    });

    app.setDocstring(demoDocstring());

    app.run();
    return 0;
}
