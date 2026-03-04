/*******************************************************************************
 * Demo: Tooltip System
 *
 * Demonstrates hovering over geometry to display a tooltip popup.
 *
 * Three ingredients:
 *   1. imgui.enableTooltips(scene)
 *        Hooks into the ImGui frame to perform ray-picking on the given scene
 *        and draw a tooltip popup when the ray hits tagged geometry.
 *
 *   2. scene.Add(...).withId(n).withType("Name")
 *        Tags a rendered item so the picking system can identify it.
 *        id   — unique integer within the scene (used in TooltipContext)
 *        type — string key matched to a registered provider
 *
 *   3. app.tooltips().registerProvider("Name", lambda)
 *        Maps a type string to content.  The lambda receives a TooltipContext
 *        (objectId, hit position, …) and returns the text to display.
 *
 * Hover the mouse over any tagged geometry to see its tooltip.
 *******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/geometry/geometry.hpp"
#include "geodraw/ui/imgui_plugin.h"

using namespace geodraw;

std::string demoDocstring() {
  return R"(=== Demo Tooltip ===

Demonstrates a tooltip when hovering the mouse over the orange triangle.)";
}

struct DemoGeometry {
  Pose3 axes;
  Triangle3 triangle{Pos3{1, 1, 0}, Pos3{3, 2, 0}, Pos3{1, 2, 1}};
};

void UpdateDemoGeometry(Scene &scene, DemoGeometry &geometry) {
  scene.clear();
  scene.Add(geometry.axes, /*scale=*/2.0f, /*thickness=*/5.0f);
  scene.Add(geometry.triangle, /*thickness=*/2.0f, /*linestyle=*/LineStyle::Solid,
  /*alpha=*/1.0, /*color=*/Colors::ORANGE)
    .withId(1).withType("Triangle");  // Tag the triangle.
}

int main() {
    App app(1280, 720, "Demo Tooltip System");
    app.addScene();

    DemoGeometry geometry;

    // -------------------------------------------------------------------------
    // ImGuiPlugin — required to host the tooltip overlay.
    // enableTooltips() activates ray-picking and popup rendering for the scene.
    // -------------------------------------------------------------------------
    ImGuiPlugin imgui(app);
    imgui.enableTooltips(app.scene());

    // -------------------------------------------------------------------------
    // Register tooltip content providers.
    // The string key must match what you pass to .withType("...") below.
    // -------------------------------------------------------------------------
    app.tooltips().registerProvider("Triangle", [](const TooltipContext&) {
        return TooltipContent::makeText("Orange Triangle)");
    });

    app.addUpdateCallback([&](float /*dt*/) {
      UpdateDemoGeometry(app.scene(), geometry);
    });

    app.setDocstring(demoDocstring());

    app.run();
    return 0;
}
