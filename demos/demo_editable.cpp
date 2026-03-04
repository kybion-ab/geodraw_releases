/*******************************************************************************
 * Demo: Editable Geometry
 *
 * Demonstrates the two-line editing workflow built into App:
 *   app.addScene();       // creates a scene pair: normal + editable
 *   app.enableEditing();  // activates click-to-select + drag editing for Shape3
 *
 * Only Shape3 objects matter for editing:
 *   - Add via scene.Add(shape) using a non-const Shape3& variable.
 *   - Tag with .withId(n).withType("Name") so the editor can track the object.
 *
 * Edit session controls (press TAB to enter edit mode):
 *   TAB   — enter / exit edit mode
 *   ENTER — commit edits (propagate changes to the source object)
 *   ESC   — cancel edits (revert to the last committed state)
 *******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/geometry/geometry.hpp"

using namespace geodraw;

struct DemoGeometry {
  Pose3 axes;
  Triangle3 triangle{Pos3{1, 1, 0}, Pos3{3, 2, 0}, Pos3{1, 2, 1}};

  // Editable Shape3 — an L-shaped polygon.
  // Must be a non-const variable: scene.Add(Shape3&) selects the editable overload.
  Shape3 editableShape;
};

void initDemoGeometry(DemoGeometry &geometry) {
  Polyline3 lshape;
  lshape.points = {
    Pos3{-5, -5, 0}, Pos3{-5,  0, 0}, Pos3{-3,  0, 0},
    Pos3{-3, -3, 0}, Pos3{ 0, -3, 0}, Pos3{ 0, -5, 0},
    Pos3{-5, -5, 0}  // close the ring
  };
  geometry.editableShape.polygons.push_back({lshape});
}

void UpdateDemoGeometry(Scene &scene, DemoGeometry &geometry) {
  scene.clear();
  scene.Add(geometry.axes, /*scale=*/2.0f, /*thickness=*/5.0f);
  scene.Add(geometry.triangle, /*thickness=*/2.0f, /*linestyle=*/LineStyle::Solid,
  /*alpha=*/1.0, /*color=*/Colors::ORANGE);

  // Passing a non-const Shape3& selects the editable rendering path.
  // .withId() and .withType() are required for the editor to track this object.
  scene.Add(geometry.editableShape)
    .withId(1).withType("EditableShape")
    ->color = Colors::GREEN;
}

int main() {
    App app(1280, 720, "Demo Editable Geometry");
    app.addScene();

    // enableEditing() wires click/drag handlers into the event system.
    app.enableEditing();

    DemoGeometry geometry;
    initDemoGeometry(geometry);

    app.addUpdateCallback([&](float /*dt*/) {
      UpdateDemoGeometry(app.scene(), geometry);
    });

    app.run();
    return 0;
}
