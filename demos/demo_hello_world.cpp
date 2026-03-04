/*******************************************************************************
 * Demo: Hello World
 *
 * START HERE. The most basic GeoDraw application.
 *
 * This demo walks through the fundamental pattern every GeoDraw app follows:
 *   1. Create an App window
 *   2. Add a scene  (the container for geometry)
 *   3. Set an update callback that populates the scene
 *   4. Call run() to enter the event loop
 *
 * Geometry demonstrated:
 *   Pose3     — coordinate axes (position + rotation matrix)
 *   Triangle  — triangle in 3D
 *
 * Controls: drag to orbit · scroll to zoom · right-drag to pan · H for help
 *******************************************************************************/

#include "geodraw/app/app.hpp"           // App, Scene
#include "geodraw/geometry/geometry.hpp" // Pose3, Triangle, Pos3, glm

using namespace geodraw;

std::string demoDocstring() {
  return R"(=== Demo Hello World ===

A minimal GeoDraw application.

In all GeoDraw applications, the user navigates the scene using the mouse or
track pad. The viewer camera can be in one of two states, PIVOT or PAN. Here is
how they work:

  | Action         \           State    | PIVOT                 | PAN                               |
  |-------------------------------------+-----------------------+-----------------------------------|
  | Right click + mouse drag up/down    | Zoom                  | Zoom                              |
  | Left click + mouse drag             | Pivot around target   | Pan along surface                 |
  | Double click                        | Select target         | Select target and set PIVOT state |
  | Right click + mouse drag left/right | Pan and set PAN state | Pan                               |

Use the mouse to acquaint yourself with the navigation system.)";
}

// Hold geometry in a struct.
struct DemoGeometry {
  Pose3 axes; // Drawn as color-labeled coordinate axes.
  Triangle3 triangle{Pos3{1, 1, 0}, Pos3{3, 2, 0}, Pos3{1, 2, 1}};
};

void updateDemoGeometry(Scene &scene, DemoGeometry &geometry) {
  // Standard pattern for update callback:
  //   1. scene.clear()   — remove drawn geometry from the scene
  //   2. scene.Add(...)  — add geometry to show
  scene.clear();
  scene.Add(geometry.axes, /*scale=*/2.0f, /*thickness=*/5.0f);
  scene.Add(geometry.triangle, /*thickness=*/2.0f, /*linestyle=*/LineStyle::Solid,
            /*alpha=*/1.0, /*color=*/Colors::ORANGE);
}

int main() {
  // -------------------------------------------------------------------------
  // Step 1: Create the application window.
  //
  // App(width, height, title) initialises the OpenGL window, the renderer,
  // and the event system. Orbit camera is active by default — drag to rotate.
  // -------------------------------------------------------------------------
  App app(1280, 720, "Demo Hello World");

  // -------------------------------------------------------------------------
  // Step 2: Add a scene.
  //
  // A Scene is the container that holds every piece of geometry you want
  // drawn. addScene() creates one and registers it with the app.
  // After this call, app.scene() returns a reference to it.
  // -------------------------------------------------------------------------
  app.addScene();

  // -------------------------------------------------------------------------
  // Prepare geometry (created once; reused every frame).
  // -------------------------------------------------------------------------
  DemoGeometry geometry;

  // -------------------------------------------------------------------------
  // Step 3: Register an update callback.
  //
  // The callback runs whenever the scene needs refreshing.
  // `dt` is elapsed time in seconds since the previous frame.
  // -------------------------------------------------------------------------
  app.addUpdateCallback([&](float /*dt*/) {
    updateDemoGeometry(app.scene(), geometry);
  });

  // Add a docstring to print when pressing 'H'.
  app.setDocstring(demoDocstring());

  // -------------------------------------------------------------------------
  // Step 4: Enter the event loop.
  //
  // run() blocks until the user closes the window (Q / ESC / window X).
  // -------------------------------------------------------------------------
  app.run();

  return 0;
}
