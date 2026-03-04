/*******************************************************************************
 * Demo: Shape Edit (local coordinates)
 *
 * Demonstrates shape editing in a local (non-geographic) coordinate system.
 * No earth tiles, API keys, or network access required.
 *
 * Controls:
 *   CTRL+S       - Activate/deactivate shape editing mode
 *   CTRL+Click   - Add point
 *   CTRL+R       - Commit points as ring (polygon)
 *   CTRL+L       - Commit points as line
 *   CTRL+P       - Commit points as isolated points
 *   CTRL+Z / Y   - Undo / Redo
 *   CTRL+D       - Delete nearest point
 *   CTRL+I       - Split nearest edge (insert point)
 *   CTRL+M       - Move nearest point (hold and drag)
 *   CTRL+O       - Save shape to edited_shape.shape
 *   CTRL+F       - Load shape from edited_shape.shape
 *   H            - Show all keybindings
 *
 * The shape is stored in scene-local XY-plane coordinates (z ≈ 0).
 * CTRL+O/F save and load the raw coordinates using the .shape format.
 *******************************************************************************/

#include "geodraw/app/app.hpp"
#include "geodraw/geometry/geometry.hpp"
#include "geodraw/modules/earth/earth_shape_edit.hpp"

using namespace geodraw;

// Draw a ground plane + reference grid on the XY plane.
//
// The Ground mesh is fully opaque (transparency=1.0) so it writes to the
// depth buffer and depth-based picking works. Grid lines are drawn on top
// for visual reference. The ground color is dark gray to contrast with the
// default black background.
static void drawGroundAndGrid(Scene& scene, float extent = 10.0f, float step = 1.0f) {
    // Opaque ground plane — writes to depth buffer, enabling CTRL+click picking.
    Ground ground;
    ground.pose = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};  // centered at origin, flat
    ground.width  = extent * 2.0f;
    ground.height = extent * 2.0f;
    scene.Add(ground, 1.0f, 1.0f, LineStyle::Solid, 1.0f,
              glm::vec3(0.12f, 0.12f, 0.12f),  // dark gray surface
              glm::vec3(0.0f),                  // no normal arrows
              true, true, 0, false);

    // Grid lines drawn on top of the ground for spatial reference.
    glm::vec3 gridColor(0.28f, 0.28f, 0.28f);
    for (float x = -extent; x <= extent + 0.01f; x += step) {
        Polyline3 line;
        line.addPoint(Pos3(x, -extent, 0.0));
        line.addPoint(Pos3(x,  extent, 0.0));
        scene.Add(line, 1.0f, LineStyle::Solid, 1.0f, gridColor, true);
    }
    for (float y = -extent; y <= extent + 0.01f; y += step) {
        Polyline3 line;
        line.addPoint(Pos3(-extent, y, 0.0));
        line.addPoint(Pos3( extent, y, 0.0));
        scene.Add(line, 1.0f, LineStyle::Solid, 1.0f, gridColor, true);
    }
}

int main() {
    App app(1280, 720, "Demo Shape Edit");

    // Default orbit camera — no GLOBE mode, coords are scene-local XY plane
    app.addScene();

    ShapeEditor shapeEditor;
    shapeEditor.registerCommands(app);

    app.addUpdateCallback([&](float /*dt*/) {
        app.scene().clear();
        drawGroundAndGrid(app.scene());
        shapeEditor.addToScene(app.scene());
    });

    app.addDrawCallback([&]() {
        shapeEditor.handleInput(app);
    });

    app.run();

    return 0;
}
