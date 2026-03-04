/*******************************************************************************
 * Demo: Textured Ground and Triangle
 *
 * Demonstrates loading PNG textures and applying them to geometry.
 * The demo falls back gracefully to solid colours if texture files are missing.
 *
 * Texture files expected (relative to the build directory at run time):
 *   ../data/textures/triangle.png  — applied to the triangle
 *
 * Pattern:
 *   GLuint tex = app.getRenderer().loadTexture("path/to/file.png");
 *   scene.Add(triangle, ..., textureHandle=tex);
 *
 * loadTexture() returns 0 if the file cannot be opened; passing 0 as the
 * textureHandle causes the geometry to render with its solid colour instead,
 * so no special fallback logic is required in the scene update.
 *
 * Note: this demo uses a manual draw callback (app.addDrawCallback) rather than
 * app.addScene() because the textured Add() overloads are only available when
 * rendering is driven explicitly through the Renderer.
 *******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/scene/scene.hpp"
#include "geodraw/geometry/geometry.hpp"
#include <iostream>

using namespace geodraw;

struct DemoGeometry {
  Pose3 axes;
  // Note: UV coordinates are set to enable texture sampling.
  Triangle3 triangle{Pos3{1, 1, 0}, Pos3{3, 2, 0}, Pos3{1, 2, 1},
                     glm::vec2{0.0f, 0.0f}, glm::vec2{1.0f, 0.0f}, glm::vec2{0.5f, 1.0f}};
  // Load textures through the renderer.hpp
  // Returns 0 if the file cannot be opened — the demo still runs without them.
  GLuint triangleTex;
};

void demoGeometryInit(App& app, DemoGeometry& geometry) {
  geometry.triangleTex = app.getRenderer().loadTexture("../data/textures/brick.png");
  if (geometry.triangleTex == 0) {
    std::cout << "Note: brick.png not found — triangle will be solid orange.\n";
  }
}

void UpdateDemoGeometry(Scene &scene, DemoGeometry &geometry) {
  scene.clear();
  scene.Add(geometry.axes, /*scale=*/2.0f, /*thickness=*/5.0f);
  scene.Add(geometry.triangle, /*thickness=*/2.0f, /*linestyle=*/LineStyle::Solid,
  /*alpha=*/1.0, /*color=*/Colors::ORANGE, /*depthTest=*/true, /*filled=*/true, /*textureHandle=*/geometry.triangleTex);
}

int main() {
    App app(1280, 720, "Demo Textured Geometry");
    app.addScene();
    DemoGeometry geometry;
    demoGeometryInit(app, geometry);

    app.addUpdateCallback([&](float /*dt*/) {
      UpdateDemoGeometry(app.scene(), geometry);
    });

    app.addDrawCallback([&]() {
      app.getRenderer().render(app.scene(), app.camera);
    });

    app.run();

    // Release textures when done.
    if (geometry.triangleTex != 0) app.getRenderer().unloadTexture("../data/textures/brick.png");

    return 0;
}
