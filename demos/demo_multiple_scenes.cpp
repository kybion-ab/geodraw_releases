/*******************************************************************************
 * Demo: Multiple Scenes
 *
 * 2×3 grid of scenes, each showing a triangle in a different color.
 *
 * Demonstrates: App::setSceneGrid, App::sceneCamera, multiple scenes.
 *
 * Controls: H for help
 *******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/geometry/geometry.hpp"

using namespace geodraw;

static const glm::vec3 COLORS[6] = {
    Colors::RED,    Colors::GREEN,   Colors::BLUE,
    Colors::ORANGE, Colors::CYAN,    Colors::MAGENTA,
};
static const Triangle3 TRIANGLE{Pos3{1, 1, 0}, Pos3{3, 2, 0}, Pos3{1, 2, 1}};
static const Pose3 AXES;

int main() {
    App app(1280, 720, "Demo Multiple Scenes");

    for (int i = 0; i < 6; ++i) app.addScene();
    app.setSceneGrid(2, 3);

    // Set a static camera for each sub-scene
    for (int i = 0; i < 6; ++i) {
        app.sceneCamera(i).autoFrame(
            glm::vec3(1.0f, 1.0f, 0.0f),
            glm::vec3(3.0f, 2.0f, 1.0f));
    }

    app.addUpdateCallback([&](float) {
        for (int i = 0; i < 6; ++i) {
            app.scene(i).clear();
            app.scene(i).Add(TRIANGLE, 2.0f, LineStyle::Solid, 1.0f, COLORS[i]);
            app.scene(i).Add(AXES, 2.0f, 5.0f);
        }
    });

    app.run();
    return 0;
}
