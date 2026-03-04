/*******************************************************************************
 * Demo: QuickView as Debug Instrumentation
 *
 * Shows QuickView in its intended role: a temporary visualisation block
 * dropped *inside* existing computation code, not a full application.
 *
 * The "production" function computeTriangle() computes the vertices of a
 * triangle. A `debug` flag gates a QuickView block that visualises the triangle
 * before the rest of the pipeline runs. Flip debug = false to restore
 * production behaviour with zero overhead.
 *
 * QuickView::show() blocks until the window is closed, so leave debug = false
 * in production or CI builds.
 *
 * The same geometry and debug pattern are mirrored in
 * python/demos/demo_quick_view.py.
 *******************************************************************************/

#include "geodraw/ui/quick_view.hpp"
#include "geodraw/geometry/geometry.hpp"
#include <iostream>
#include <vector>

using namespace geodraw;

// ---------------------------------------------------------------------------
// "Production" computation — a simple triangle
// ---------------------------------------------------------------------------
static std::vector<Pos3> computeTriangle() {
    std::vector<Pos3> pts;
    pts.push_back(Pos3{1, 1, 0});
    pts.push_back(Pos3{3, 2, 0});
    pts.push_back(Pos3{1, 2, 1});
    return pts;
}

int main() {
    // Run the production computation.
    std::vector<Pos3> points = computeTriangle();

    // -------------------------------------------------------------------------
    // Debug block — flip this flag to false to skip the visualisation.
    // -------------------------------------------------------------------------
    bool debug = true;

    if (debug) {
        std::cout << "Debug: visualising computed triangle" << std::endl;

        Scene scene;

        // Draw the triangle.
        Triangle3 triangle;
        triangle.p0 = points[0];
        triangle.p1 = points[1];
        triangle.p2 = points[2];
        scene.Add(triangle, /*thickness=*/2.0f, /*linestyle=*/LineStyle::Solid,
                  /*alpha=*/1.0, /*color=*/Colors::ORANGE);

        // Reference frame at the origin.
        Pose3 axes;
        scene.Add(axes, /*scale=*/2.0f, /*thickness=*/5.0f);

        // show() blocks until the viewer window is closed.
        QuickView::show(scene, "Debugging: computed path");
    }

    // The rest of the (hypothetical) production pipeline continues here.
    std::cout << "Pipeline continues ..." << std::endl;

    return 0;
}
