/*******************************************************************************
 * File: gizmo_demo.cpp
 *
 * Description: Demo showcasing the gizmo-based editing system.
 *
 * Features demonstrated:
 * - Edit mode toggle (TAB)
 * - Object selection by clicking
 * - TranslateGizmo with axis and plane handles
 * - Commit (ENTER) and cancel (ESC) workflow
 * - Hover highlighting for proxies and gizmo handles
 *
 * Controls:
 *   TAB       - Toggle edit mode
 *   Click     - Select object (in edit mode)
 *   Drag      - Move selected object via gizmo handles
 *   ENTER     - Commit changes
 *   ESC       - Cancel changes / deselect
 *   H         - Show help
 *
 ******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/scene/scene.hpp"
#include "geodraw/geometry/geometry.hpp"
#include "geodraw/geometry/colors.hpp"

using namespace geodraw;

/**
 * Create a simple rectangle Shape3 at a position
 */
Shape3 createRectangle(double x, double y, double width, double height) {
    Shape3 shape;
    Polyline3 ring;
    ring.addPoint(Pos3{x - width/2, y - height/2, 0});
    ring.addPoint(Pos3{x + width/2, y - height/2, 0});
    ring.addPoint(Pos3{x + width/2, y + height/2, 0});
    ring.addPoint(Pos3{x - width/2, y + height/2, 0});
    shape.polygons.push_back({ring});
    return shape;
}

/**
 * Create a simple triangle Shape3 at a position
 */
Shape3 createTriangle(double x, double y, double size) {
    Shape3 shape;
    Polyline3 ring;
    double h = size * std::sqrt(3.0) / 2.0;
    ring.addPoint(Pos3{x, y + h * 2.0/3.0, 0});
    ring.addPoint(Pos3{x - size/2, y - h/3.0, 0});
    ring.addPoint(Pos3{x + size/2, y - h/3.0, 0});
    shape.polygons.push_back({ring});
    return shape;
}

/**
 * Create an L-shaped polygon
 */
Shape3 createLShape(double x, double y, double size) {
    Shape3 shape;
    Polyline3 ring;
    double s = size / 3.0;
    ring.addPoint(Pos3{x, y, 0});
    ring.addPoint(Pos3{x + size, y, 0});
    ring.addPoint(Pos3{x + size, y + s, 0});
    ring.addPoint(Pos3{x + s, y + s, 0});
    ring.addPoint(Pos3{x + s, y + size, 0});
    ring.addPoint(Pos3{x, y + size, 0});
    shape.polygons.push_back({ring});
    return shape;
}

struct GizmoGui {
  Shape3 rectangle;
  Shape3 triangle;
  Shape3 l_shape;

  void init() {
    // Create some shapes to edit
    std::vector<Shape3> shapes;
    rectangle = createRectangle(-15, 0, 8, 6);   // Red rectangle
    triangle = createTriangle(0, 0, 8);          // Green triangle
    l_shape = createLShape(10, -5, 10);         // Blue L-shape
  }

  void updateCallback(Scene &scene) {
    scene.clear();

    scene.Add(triangle)->color = Colors::RED;
    scene.Add(rectangle)->color = Colors::GREEN;
    scene.Add(l_shape)->color =  Colors::BLUE;
  };

};


int main() {
    App app(1280, 720, "Gizmo Demo");

    GizmoGui gui;
    gui.init();

    // Scene for normal mode rendering
    app.addScene();
    app.enableEditing();

    // Update callback
    app.addUpdateCallback([&](float dt) {
      gui.updateCallback(app.scene()); });


    app.setDocstring(R"(=== Gizmo Demo ===

Demonstrates the gizmo-based editing system:

Objects:
  - Red rectangle (left)
  - Green triangle (center)
  - Blue L-shape (right)

Controls:
  TAB    - Toggle edit mode
  Click  - Select object (in edit mode)
  Drag   - Move via gizmo handles (axis arrows or plane squares)
  G      - Cycle gizmo type (currently only translate)
  ENTER  - Commit changes
  ESC    - Cancel / clear selection
  H      - Show help

Gizmo handles:
  Red arrow    - Move along X axis
  Green arrow  - Move along Y axis
  Blue arrow   - Move along Z axis
  Yellow quad  - Move in XY plane
  Cyan quad    - Move in XZ plane
  Magenta quad - Move in YZ plane

Note: Handles highlight when hovered.)");

    app.run();
    return 0;
}
