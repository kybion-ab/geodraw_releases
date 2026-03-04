/*******************************************************************************
 * File: vehicle_glb_demo.cpp
 *
 * Description: Demonstrates Vehicle struct with .glb model rendering
 *
 * Features:
 * - Load and render vehicles with .glb models
 * - Toggle between .glb rendering and wireframe box fallback
 * - Display velocity vectors
 * - Interactive controls
 *
 *******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/scene/scene.hpp"
#include "geodraw/modules/drive/vehicle.hpp"
#include "geodraw/modules/gltf/gltf_loader.hpp"
#include "geodraw/geometry/colors.hpp"
#include <cmath>

using namespace geodraw;

int main() {
    App app(1280, 720, "Vehicle GLB Demo");
    Renderer &renderer = app.getRenderer();

    // Load GLB models (cached for reuse)
    auto whiteCarModel = gltf::loadGLB("data/glb_files/WhiteCar.glb", renderer);
    auto greenCarModel = gltf::loadGLB("data/glb_files/GreenCar.glb", renderer);
    auto redCarModel = gltf::loadGLB("data/glb_files/RedCar.glb", renderer);

    if (!whiteCarModel || !greenCarModel || !redCarModel) {
        std::cerr << "Failed to load one or more .glb models\n";
        return 1;
    }

    // Create test vehicles with different poses and velocities
    std::vector<Vehicle> vehicles = {
        // Stationary red car facing north (heading=0)
        {Pos3{0, 0, 0}, orientationFromHeading(0.0), glm::dvec3(0, 0, 0), 4.5, 1.8, 1.5,
         Colors::RED},

        // Green car moving northeast
        {Pos3{10, 5, 0}, orientationFromHeading(M_PI/4), glm::dvec3(8, 8, 0), 4.2, 1.7, 1.4,
         Colors::GREEN},

        // Blue car moving sideways (drift!)
        {Pos3{-8, 8, 0}, orientationFromHeading(M_PI/2), glm::dvec3(0, 5, 0), 4.8, 1.9, 1.6,
         Colors::BLUE},

        // White compact car
        {Pos3{-5, -10, 0}, orientationFromHeading(-M_PI/3), glm::dvec3(-3, -5, 0), 3.8, 1.6, 1.4,
         Colors::WHITE},
    };

    // State variables
    bool useGLB = true;
    bool showVelocity = true;
    bool showBounds = false;
    static Scene queue;

    // Update callback: rebuild scene each frame
    app.addUpdateCallback([&](float dt) {
        queue.clear();

        for (const auto &v : vehicles) {
            // Render vehicle (glb or box)
            const gltf::LoadedModel *model = (useGLB && whiteCarModel) ? &(*whiteCarModel) : nullptr;
            addVehicle(queue, v, model,
                               2.0f, LineStyle::Solid, 1.0f, true);

            // Velocity arrow (optional)
            if (showVelocity && glm::length(v.velocity) > 0.1) {
                glm::dvec3 arrowEnd = v.position.pos + v.velocity;
                Line3 velArrow{v.position, Pos3{arrowEnd.x, arrowEnd.y, arrowEnd.z}};
                queue.Add(velArrow, 3.0f, LineStyle::Solid, 1.0f,
                         glm::vec3(1.0f, 1.0f, 0.0f), true);
            }

            // Bounding box (optional)
            if (showBounds) {
                RBox3 box = v.toRBox3();
                queue.Add(box, 1.0f, LineStyle::Solid, 0.5f,
                         glm::vec3(0.7f, 0.7f, 0.7f), true);
            }
        }

        // Ground reference grid
        for (int x = -20; x <= 20; x += 5) {
            queue.Add(Line3{Pos3{(double)x, -20.0, 0.0}, Pos3{(double)x, 20.0, 0.0}},
                     1.0f, LineStyle::Solid, 0.3f,
                     glm::vec3(0.5f, 0.5f, 0.5f), true);
        }
        for (int y = -20; y <= 20; y += 5) {
            queue.Add(Line3{Pos3{-20.0, (double)y, 0.0}, Pos3{20.0, (double)y, 0.0}},
                     1.0f, LineStyle::Solid, 0.3f,
                     glm::vec3(0.5f, 0.5f, 0.5f), true);
        }
    });

    // Draw callback: render all queued geometry
    app.addDrawCallback([&]() {
        renderer.render(queue, app.camera);
    });

    // Commands and controls
    app.addToggle("", useGLB, "Toggle GLB rendering"        , GLFW_KEY_G);
    app.addToggle("", showVelocity, "Toggle velocity arrows", GLFW_KEY_V);
    app.addToggle("", showBounds, "Toggle bounding boxes"   , GLFW_KEY_B);

    app.setDocstring(R"(=== Vehicle GLB Demo ===

Demonstrates Vehicle struct with .glb model rendering:
- Four vehicles with different poses and velocities
- Adaptive scaling: .glb models scaled to vehicle dimensions
- Velocity independent of heading (see blue car drift)
- Fallback to wireframe box when .glb disabled

Controls:
  G - Toggle GLB rendering
  V - Toggle velocity arrows
  B - Toggle bounding boxes
  H - Show help)");

    app.run();
    return 0;
}
