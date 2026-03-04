/*******************************************************************************
 * File: quick_view.hpp
 *
 * Description: QuickView utility for rapid debugging and visualization.
 * Provides a minimal API to display geometry without boilerplate App setup.
 * Perfect for quick debugging, testing, and exploratory visualization.
 *
 * Why required: Reduces friction for debugging - no need to create App,
 * set up callbacks, manage draw loops. Just add geometry to Scene and
 * call show().
 *
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"

#include "geodraw/app/app.hpp"
#include "geodraw/scene/scene.hpp"
#include "geodraw/geometry/geometry.hpp"
#include <iostream>
#include <memory>
#include <vector>
#include <string>

namespace geodraw {

/**
 * QuickView - Minimal API for rapid geometry visualization
 *
 * Provides static show() methods that handle all App boilerplate:
 * - Window creation and management
 * - Auto-framing camera to fit geometry
 * - Basic orbit camera controls
 * - Keyboard shortcuts (Q/ESC to quit, H for help)
 * - Render loop
 *
 * Example usage:
 *   Scene scene;
 *   scene.Add(BBox3{Pos3(-10,-10,0), Pos3(10,10,5)}, 2.0f);
 *   QuickView::show(scene);  // Done!
 */
class GEODRAW_API QuickView {
public:
    /**
     * Show single Scene in a window (blocks until closed)
     *
     * Creates window, auto-frames camera, and enters render loop.
     * Window closes when user presses Q, ESC, or clicks close button.
     *
     * @param scene Scene with geometry to display
     * @param title Window title
     * @param width Window width in pixels (default 1280)
     * @param height Window height in pixels (default 720)
     */
    static void show(const Scene& scene,
                     const std::string& title = "GeoDraw Quick View",
                     int width = 1280,
                     int height = 720);

    /**
     * Show multiple labeled Scenes with toggle controls
     *
     * Displays multiple scenes that can be toggled on/off with number keys.
     * Useful for comparing different geometric representations or debug layers.
     *
     * @param scenes Vector of (label, Scene) pairs
     * @param title Window title
     * @param width Window width in pixels
     * @param height Window height in pixels
     */
    static void show(const std::vector<std::pair<std::string, Scene>>& scenes,
                     const std::string& title = "GeoDraw Quick View",
                     int width = 1280,
                     int height = 720);

    /**
     * Show geometry with custom camera settings
     *
     * Provides control over initial camera position instead of auto-framing.
     *
     * @param scene Scene with geometry
     * @param cameraPos Initial camera position
     * @param lookAt Point camera looks at
     * @param title Window title
     */
    static void show(const Scene& scene,
                     const glm::vec3& cameraPos,
                     const glm::vec3& lookAt,
                     const std::string& title = "GeoDraw Quick View");

private:
    /**
     * Auto-frame camera to fit all geometry in scene
     */
    static void autoFrameCamera(Camera& camera, const Scene& scene);

    /**
     * Auto-frame camera to fit multiple scenes
     */
    template<typename EntryType>
    static void autoFrameMultipleScenes(Camera& camera, const std::vector<EntryType>& entries) {
        // Compute combined bounds of all scenes
        glm::vec3 minBound(std::numeric_limits<float>::max());
        glm::vec3 maxBound(std::numeric_limits<float>::lowest());
        bool hasGeometry = false;

        for (const auto& entry : entries) {
            auto bounds = entry.scene.getBounds();
            if (bounds) {
                glm::vec3 minB = glm::vec3(bounds->min.pos);
                glm::vec3 maxB = glm::vec3(bounds->max.pos);
                minBound = glm::min(minBound, minB);
                maxBound = glm::max(maxBound, maxB);
                hasGeometry = true;
            }
        }

        if (hasGeometry) {
            camera.setMode(Camera::CameraMode::ORBIT);
            camera.autoFrame(minBound, maxBound, glm::radians(45.0f), glm::radians(30.0f));

            std::cout << "Auto-framed to combined bounds: ["
                     << minBound.x << "," << minBound.y << "," << minBound.z << "] to ["
                     << maxBound.x << "," << maxBound.y << "," << maxBound.z << "]\n";
        } else {
            // No geometry - default view
            camera.setMode(Camera::CameraMode::ORBIT);
            camera.target = glm::vec3(0, 0, 0);
            camera.distance = 50.0f;
            camera.orbitYaw = glm::radians(45.0f);
            camera.orbitPitch = glm::radians(30.0f);

            std::cout << "No geometry in scenes - using default camera\n";
        }
    }

    /**
     * Set up standard keyboard commands
     */
    static void setupCommands(App& app);

    /**
     * Print usage instructions (single scene)
     */
    static void printUsage();

    /**
     * Print usage instructions (multiple scenes)
     */
    template<typename EntryType>
    static void printUsageMultiple(const std::vector<EntryType>& entries) {
        std::cout << "\n=== GeoDraw QuickView ===\n";
        std::cout << "Controls:\n";
        std::cout << "  Mouse drag: Rotate camera (orbit)\n";
        std::cout << "  Mouse scroll: Zoom in/out\n";
        std::cout << "  Right-click drag: Pan camera\n";
        std::cout << "  Q / ESC: Close window\n";
        std::cout << "  H: Show all commands\n";
        std::cout << "\nScenes:\n";

        for (size_t i = 0; i < entries.size() && i < 9; ++i) {
            std::cout << "  " << (i + 1) << ": Toggle '"
                     << entries[i].label << "'\n";
        }

        std::cout << "========================\n\n";
    }
};

/**
 * Global convenience function for even simpler usage
 *
 * Example:
 *   Scene q;
 *   q.Add(bbox);
 *   show(q);  // That's it!
 */
inline void show(const Scene& scene, const std::string& title = "GeoDraw") {
    QuickView::show(scene, title);
}

/**
 * Global convenience function for multiple scenes
 */
inline void show(const std::vector<std::pair<std::string, Scene>>& scenes,
                 const std::string& title = "GeoDraw") {
    QuickView::show(scenes, title);
}

} // namespace geodraw
