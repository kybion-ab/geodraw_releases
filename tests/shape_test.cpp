/*******************************************************************************
 * File: shape_test.cpp
 *
 * Description: Demonstrates Shape3 with polygon tesselation
 * Shows polygon with holes, isolated lines, and isolated points
 *
 *******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/scene/scene.hpp"
#include "geodraw/geometry/geometry.hpp"
#include "geodraw/geometry/colors.hpp"
#include <cmath>

using namespace geodraw;

Shape3 shape1()
{
    Shape3 shape1 = {};

    // Outer boundary: 10x10 square (CCW winding)
    Polyline3 outer1;
    outer1.points = {
      Pos3(-5, -5, 0), Pos3(5, -5, 0),
      Pos3(5, 5, 0), Pos3(-5, 5, 0),
      Pos3(-5, -5, 0)
    };

    // Inner hole: circular approximation (CW winding)
    Polyline3 circleHole;
    int segments = 20;
    float radius = 2.0f;
    for (int i = segments; i >= 0; --i) {  // Reverse for CW
      float angle = (i / (float)segments) * 2.0f * M_PI;
      circleHole.points.push_back(Pos3(
        radius * cos(angle),
        radius * sin(angle),
        0.0
      ));
    }

    // Add polygon with outer ring and hole
    shape1.polygons.push_back({outer1, circleHole});

    return shape1;
}

Shape3 shape2()
{
    Shape3 shape2;

    // Outer: L-shaped polygon
    Polyline3 outer2;
    outer2.points = {
      Pos3(8, -5, 0), Pos3(18, -5, 0),
      Pos3(18, 5, 0), Pos3(13, 5, 0),
      Pos3(13, 0, 0), Pos3(8, 0, 0),
      Pos3(8, -5, 0)
    };

    // Two rectangular holes
    Polyline3 hole1;
    hole1.points = {
      Pos3(9, -4, 0), Pos3(9, -1, 0),
      Pos3(12, -1, 0), Pos3(12, -4, 0),
      Pos3(9, -4, 0)
    };

    Polyline3 hole2;
    hole2.points = {
      Pos3(14, 1, 0), Pos3(14, 4, 0),
      Pos3(17, 4, 0), Pos3(17, 1, 0),
      Pos3(14, 1, 0)
    };

    // Add polygon with outer ring and two holes
    shape2.polygons.push_back({outer2, hole1, hole2});

    return shape2;
}

Shape3 shape3()
{
    Shape3 shape3;

    // First polygon: Small triangle
    Polyline3 triangle;
    triangle.points = {
      Pos3(-15, -3, 0), Pos3(-12, -3, 0),
      Pos3(-13.5, 0, 0), Pos3(-15, -3, 0)
    };
    shape3.polygons.push_back({triangle});

    // Second polygon: Square with triangular hole
    Polyline3 squareOuter;
    squareOuter.points = {
      Pos3(-15, 2, 0), Pos3(-10, 2, 0),
      Pos3(-10, 7, 0), Pos3(-15, 7, 0),
      Pos3(-15, 2, 0)
    };

    Polyline3 triangleHole;
    triangleHole.points = {
      Pos3(-14, 3, 0), Pos3(-11, 3, 0),
      Pos3(-12.5, 6, 0), Pos3(-14, 3, 0)
    };
    shape3.polygons.push_back({squareOuter, triangleHole});

    // Third polygon: Small circle (no holes)
    Polyline3 circle;
    int circleSegs = 16;
    float circleRadius = 1.5f;
    glm::vec2 circleCenter(-12.5, -7);
    for (int i = 0; i <= circleSegs; ++i) {
      float angle = (i / (float)circleSegs) * 2.0f * M_PI;
      circle.points.push_back(Pos3(
        circleCenter.x + circleRadius * cos(angle),
        circleCenter.y + circleRadius * sin(angle),
        0.0
      ));
    }
    shape3.polygons.push_back({circle});

    return shape3;
}

struct ShapeTestGui {
  std::vector<Shape3> shapes;
  std::vector<glm::vec3> shapeColors;
  bool showFilled = true;

  void init() {
    // Example 1: Square with circular hole
    shapes.push_back(shape1());
    shapeColors.push_back(Colors::BLUE);

    // Example 2: Complex polygon with multiple holes
    shapes.push_back(shape2());
    shapeColors.push_back(Colors::ORANGE);


    // Example 3: Multiple separate polygons in one shape
    shapes.push_back(shape3());
    shapeColors.push_back(Colors::GREEN);
  }

  void updateGeometry(Scene& scene) {
    scene.clear();

    // Render all shapes
    for (size_t i = 0; i < shapes.size(); ++i) {
      glm::vec3 color = shapeColors[i];
      scene.Add(shapes[i], 2.0f, LineStyle::Solid, 0.8f, color,
                true, showFilled);
    }
  }
};

std::string docstring()
{
  return R"(=== Shape3 Tesselation Test ===

Demonstrates polygon-with-holes using earcut triangulation library.
Shows three complex shapes: square with circular hole, L-shape with two
rectangular holes, and composite shape with multiple polygons.

Workflow:
  • Toggle between filled and wireframe modes to see tessellation
  • Observe how holes are correctly rendered as transparent regions
  • Rotate view to verify 3D structure and triangle mesh)";
}

int main() {
  App app(1200, 800, "Shape3 Tesselation Test");

  ShapeTestGui gui;
  gui.init();

  // Register render mode toggle
  app.addToggle("", gui.showFilled, "Toggle filled/wireframe mode", GLFW_KEY_F);

  app.setDocstring(docstring());

  static Scene scene;
  app.addUpdateCallback([&](float dt) {
    gui.updateGeometry(scene);
  });

  app.addDrawCallback([&]() {
    app.getRenderer().render(scene, app.camera);
  });

  app.run();
}
