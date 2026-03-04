/*******************************************************************************
 * File: tooltip_system.hpp
 *
 * Description: Tooltip system for 3D scene objects. Uses the existing ray
 * picking infrastructure and ImGui tooltips to display contextual information
 * when hovering over scene objects (pins, vehicles, lanes, etc.).
 *
 * Why required: Provides visual feedback when interacting with 3D objects,
 * showing relevant metadata (names, positions, speeds) without cluttering
 * the main UI.
 *
 * Author: Magnus Nilsson, Kybion AB
 * Date: 2026-02-13
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/scene/scene.hpp"

#include <functional>
#include <string>
#include <unordered_map>

namespace geodraw {

/**
 * Content to display in a tooltip
 */
struct GEODRAW_API TooltipContent {
    std::string text;                        // Simple text content
    std::function<void()> customRender;      // Optional custom ImGui rendering
    bool valid = false;                      // True if content should be shown

    /**
     * Convenience factory for simple text tooltips
     *
     * Usage: return TooltipContent::text("Vehicle " + std::to_string(ctx.objectId));
     */
    static TooltipContent makeText(const std::string& t) {
        TooltipContent c;
        c.text = t;
        c.valid = true;
        return c;
    }
};

/**
 * Context passed to tooltip providers
 */
struct GEODRAW_API TooltipContext {
    uint64_t objectId;           // Object ID from DrawCmd
    std::string objectType;      // Object type from DrawCmd
    glm::vec3 worldPoint;        // World position of the pick hit
};

/**
 * Function type for providing tooltip content
 */
using TooltipProvider = std::function<TooltipContent(const TooltipContext&)>;

/**
 * TooltipSystem - Manages tooltips for 3D scene objects
 *
 * Usage:
 *   1. Create TooltipSystem instance
 *   2. Register providers for each object type
 *   3. Call update() with pick results in update callback
 *   4. Call render() in ImGui callback
 *
 * Example:
 *   TooltipSystem tooltips;
 *   tooltips.registerProvider("Vehicle", [&](const TooltipContext& ctx) {
 *       TooltipContent content;
 *       content.text = fmt::format("Vehicle {}", ctx.objectId);
 *       content.valid = true;
 *       return content;
 *   });
 */
class GEODRAW_API TooltipSystem {
public:
    TooltipSystem() = default;

    /**
     * Register a tooltip provider for an object type
     *
     * @param objectType Object type string (e.g., "Vehicle", "ScenarioPin")
     * @param provider Function that generates tooltip content
     */
    void registerProvider(const std::string& objectType, TooltipProvider provider);

    /**
     * Update tooltip state based on pick result
     * Call this in the update callback with the current pick result.
     *
     * @param pickResult Result from Scene::rayPick()
     * @param mouseX Current mouse X position (screen coords)
     * @param mouseY Current mouse Y position (screen coords)
     */
    void update(const PickResult& pickResult, double mouseX, double mouseY);

    /**
     * Update render state (call this every frame)
     * Prepares tooltip content but doesn't render ImGui.
     * Returns pointer to content if tooltip should be shown, nullptr otherwise.
     */
    const TooltipContent* render();

    /**
     * Set hover delay before showing tooltip
     *
     * @param seconds Delay in seconds (default: 0.3)
     */
    void setHoverDelay(float seconds) { hoverDelay_ = seconds; }

    /**
     * Enable or disable the tooltip system
     */
    void setEnabled(bool enabled) { enabled_ = enabled; }

    /**
     * Check if tooltips are enabled
     */
    bool isEnabled() const { return enabled_; }

    /**
     * Reset tooltip state (e.g., when scene changes)
     */
    void reset();

private:
    std::unordered_map<std::string, TooltipProvider> providers_;

    // Current hover state
    uint64_t hoveredObjectId_ = 0;
    std::string hoveredObjectType_;
    glm::vec3 hoveredWorldPoint_{0.0f};
    double hoverStartTime_ = 0.0;
    double lastMouseX_ = 0.0;
    double lastMouseY_ = 0.0;

    // Cached tooltip content
    TooltipContent cachedContent_;
    bool contentCached_ = false;

    // Configuration
    float hoverDelay_ = 0.3f;
    bool enabled_ = true;

    // Mouse movement threshold for cache invalidation (pixels)
    static constexpr double MOUSE_MOVE_THRESHOLD = 2.0;
};

} // namespace geodraw
