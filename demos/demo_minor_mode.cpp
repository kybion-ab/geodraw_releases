/*******************************************************************************
 * Demo: Minor mode
 *
 * Demonstrates minor modes.
 *
 *******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/geometry/geometry.hpp"

#include <iostream>

using namespace geodraw;

std::string demoDocstring() {
  return R"(=== Demo Minor Mode ===

  A minor mode is just a container for keybindings — it has no draw/update logic of
  its own. Multiple minor modes can be active simultaneously. When a key is pressed,
  the app checks active minor modes in reverse activation order (last activated
  wins).

  Press '1' and '2' to toggle two minor modes.
  Press 'H' to display help for active minor modes.)";
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

void addMinorMode1(App &app, MinorMode &myMode) {
  myMode = app.createMinorMode("My minor mode 1");
  app.addCmd("say-hello", [](){ std::cout << "Hello world!" << std::endl; },
             "Say Hello World", GLFW_KEY_M, 0, true, myMode);
  app.addCmd("toggle-myMode1", [&app, &myMode](){ app.toggleMinorMode(myMode); }, "Toggle hello minor mode", GLFW_KEY_1);
}

void addMinorMode2(App &app, MinorMode &myMode) {
  myMode = app.createMinorMode("My minor mode 2");
  app.addCmd("say-hi", [](){ std::cout << "Hi world!" << std::endl; },
             "Say Hi World", GLFW_KEY_M, 0, true, myMode);
  app.addCmd("toggle-myMode2", [&app, &myMode](){ app.toggleMinorMode(myMode); }, "Toggle hi minormode", GLFW_KEY_2);
}

int main() {
    App app(1280, 720, "Demo Minor mode");
    app.addScene();

    DemoGeometry geometry;

    app.addUpdateCallback([&](float /*dt*/) {
        UpdateDemoGeometry(app.scene(), geometry);
    });

    MinorMode myMode1, myMode2;
    addMinorMode1(app, myMode1);
    addMinorMode2(app, myMode2);

    app.setDocstring(demoDocstring());

    app.run();
    return 0;
}
