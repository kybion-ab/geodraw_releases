/*******************************************************************************
 * Demo: Add keyboard commands and toggles
 *
 * Demonstrates the addCmd and addToggle methods.
 *
 *******************************************************************************/

#include "geodraw/app/app.hpp"           // App, Scene
#include "geodraw/geometry/geometry.hpp" // Pose3, Triangle, Pos3, glm
#include <iostream>

using namespace geodraw;

std::string demoDocstring() {
  return R"(=== Demo ImGui Panel ===

Demonstrates keyboard commands and toggles.)";
}

struct DemoGeometry {
  Pose3 axes;
  Triangle3 triangle{Pos3{1, 1, 0}, Pos3{3, 2, 0}, Pos3{1, 2, 1}};
  bool showTriangle{true};
};

void rotateSlightly(Triangle3& triangle) {
  glm::dvec3 zAxis(0, 0, 1);
  glm::dvec3 origin(0, 0, 0);
  constexpr double angle = 0.1; // radians per keypress
  triangle.p0 = triangle.p0.rotateAround(origin, zAxis, angle);
  triangle.p1 = triangle.p1.rotateAround(origin, zAxis, angle);
  triangle.p2 = triangle.p2.rotateAround(origin, zAxis, angle);
  std::cout << "Rotating slightly" << std::endl;
}

void UpdateDemoGeometry(Scene &scene, DemoGeometry &geometry) {
  scene.clear();
  scene.Add(geometry.axes, /*scale=*/2.0f, /*thickness=*/5.0f);
  if(geometry.showTriangle){
    scene.Add(geometry.triangle, /*thickness=*/2.0f, /*linestyle=*/LineStyle::Solid,
        /*alpha=*/1.0, /*color=*/Colors::ORANGE);
  }
}

int main() {
  App app(1280, 720, "Demo Add Commands and Toggles");
  app.addScene();
  DemoGeometry geometry;

  // -------------------------------------------------------------------------
  // Add keyboard commands and toggles.
  //
  // These automatically get registered to the help system. Press 'H' to
  // check.
  // -------------------------------------------------------------------------
  app.addCmd("rotate-slightly", [&](){rotateSlightly(geometry.triangle);},
             "Rotate the triangle slightly",
             key(Key::R), 0, true);
  app.addToggle("toggle-triangle", geometry.showTriangle, "Toggle triangle", key(Key::T));

  app.addUpdateCallback([&](float /*dt*/) {
    UpdateDemoGeometry(app.scene(), geometry);
  });

  app.setDocstring(demoDocstring());

  app.run();

  return 0;
}
