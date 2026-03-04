/*******************************************************************************
 * File: gizmo/editing_context.hpp
 *
 * Description: Central coordinator for gizmo-based editing.
 *
 * EditingContext manages:
 * - Edit mode state (on/off)
 * - Collection of editable proxies
 * - Selection (which proxy is selected)
 * - Active gizmo and gizmo cycling
 * - Input dispatch to gizmos
 * - Commit/cancel workflow
 *
 * Usage:
 *   EditingContext ctx;
 *   ctx.addProxy(std::make_unique<Shape3Proxy>(&myShape));
 *   ctx.setEditMode(true);
 *
 *   // In update callback:
 *   if (ctx.isEditMode()) {
 *       ctx.updateHover(ray);
 *       if (mouseClicked) ctx.handleClick(ray);
 *       if (mouseDragging) ctx.handleDrag(worldPoint);
 *   }
 *
 *   // In draw callback:
 *   ctx.render(scene, camera);
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"

#include "geodraw/gizmo/gizmo.hpp"
#include "geodraw/gizmo/edit_proxy.hpp"
#include "geodraw/gizmo/proxy_factory.hpp"
#include "geodraw/camera/camera.hpp"
#include "geodraw/scene/scene.hpp"
#include <memory>
#include <vector>
#include <unordered_map>

namespace geodraw {

/**
 * Central coordinator for gizmo-based editing
 */
class GEODRAW_API EditingContext {
public:
    EditingContext() = default;

    //==========================================================================
    // Mode Management
    //==========================================================================

    /**
     * Enable or disable edit mode
     * When enabled, camera controls should be conditionally disabled
     * based on isDraggingGizmo()
     */
     void setEditMode(bool enabled);

    /**
     * Check if edit mode is active
     */
    bool isEditMode() const { return editMode_; }

    /**
     * Toggle edit mode on/off
     */
    void toggleEditMode() { setEditMode(!editMode_); }

    //==========================================================================
    // Proxy Management
    //==========================================================================

    /**
     * Add a proxy to the editing context
     * Automatically deduplicates: if a proxy for this object already exists,
     * returns the existing proxy's ID instead of creating a new one.
     *
     * @param proxy Unique pointer to proxy (context takes ownership)
     * @return ID of the added (or existing) proxy
     */
    uint32_t addProxy(std::unique_ptr<EditProxyBase> proxy);

    /**
     * Remove a proxy by ID
     */
    void removeProxy(uint32_t id);

    /**
     * Clear all proxies
     */
  void clearProxies();

    /**
     * Try to register a proxy for a geometry object
     * Uses ProxyRegistry to check if the type is supported and create the proxy.
     * Deduplicates automatically - returns existing proxy ID if already registered.
     *
     * @tparam T The geometry type
     * @param obj Pointer to the geometry object
     * @return True if proxy was registered (or already existed), false if type not supported
     */
    template<typename T>
    bool tryRegisterProxy(T* obj) {
        if (!ProxyRegistry::canCreate<T>()) {
            return false;
        }
        auto proxy = ProxyRegistry::create(obj);
        if (proxy) {
            addProxy(std::move(proxy));
            return true;
        }
        return false;
    }

    /**
     * Get all proxies (const)
     */
    const std::vector<std::unique_ptr<EditProxyBase>>& getProxies() const {
        return proxies_;
    }

    /**
     * Find proxy by ID
     */
    EditProxyBase* findProxy(uint32_t id);

    //==========================================================================
    // Selection
    //==========================================================================

    /**
     * Select a proxy
     */
    void selectProxy(EditProxyBase* proxy);

    /**
     * Clear selection
     */
    void clearSelection();

    /**
     * Get currently selected proxy
     */
    EditProxyBase* getSelectedProxy() const { return selectedProxy_; }

    /**
     * Check if any proxy is selected
     */
    bool hasSelection() const { return selectedProxy_ != nullptr; }

    //==========================================================================
    // Gizmo Cycling
    //==========================================================================

    /**
     * Cycle to next gizmo type for selected proxy (TAB)
     */
    void cycleGizmo(bool reverse = false);

    /**
     * Get currently active gizmo (may be null)
     */
    Gizmo* getActiveGizmo() const { return activeGizmo_.get(); }

    //==========================================================================
    // Input Handling
    //==========================================================================

    /**
     * Update hover state based on cursor ray
     * Call this every frame in edit mode
     *
     * @param ray Camera ray through cursor position
     */
    void updateHover(const Camera::Ray& ray);

    /**
     * Handle mouse click
     * @param ray Camera ray through click position
     * @param worldPoint World-space point under cursor (from depth picking)
     * @return True if click was consumed (gizmo or selection)
     */
    bool handleClick(const Camera::Ray& ray, const glm::vec3& worldPoint);

    /**
     * Handle mouse drag
     * @param worldPoint Current world-space point under cursor
     * @param camera Camera for view-dependent calculations
     * @return True if drag was consumed by gizmo
     */
    bool handleDrag(const glm::vec3& worldPoint, const Camera& camera);

    /**
     * Handle mouse release
     * @return True if release ended a gizmo drag
     */
    bool handleRelease();

    /**
     * Check if a gizmo is currently being dragged
     * Use this to disable camera controls during gizmo manipulation
     */
    bool isDraggingGizmo() const { return draggingGizmo_; }

    /**
     * Get currently hovered handle index (-1 if none)
     */
    int getHoveredHandle() const { return hoveredHandle_; }

    //==========================================================================
    // Commit / Cancel
    //==========================================================================

    /**
     * Commit all dirty proxy changes to original objects
     */
    void commit();

    /**
     * Cancel all edits (revert to original state)
     */
    void cancel();

    /**
     * Check if any proxy has uncommitted changes
     */
    bool hasUnsavedChanges() const;

    //==========================================================================
    // Rendering
    //==========================================================================

    /**
     * Render all proxies and active gizmo
     * @param scene Scene to render to
     * @param camera Camera for gizmo sizing
     */
    void render(Scene& scene, const Camera& camera);

private:
    //==========================================================================
    // Internal Helpers
    //==========================================================================

    void updateActiveGizmo();

    // Simple ray-AABB intersection test
    bool rayAABBIntersect(const Camera::Ray& ray, const BBox3& box, float& t) const;

    //==========================================================================
    // State
    //==========================================================================

    bool editMode_ = false;
    std::vector<std::unique_ptr<EditProxyBase>> proxies_;
    std::unordered_map<void*, EditProxyBase*> objectToProxy_;  // For deduplication

    EditProxyBase* selectedProxy_ = nullptr;
    EditProxyBase* hoveredProxy_ = nullptr;

    std::unique_ptr<Gizmo> activeGizmo_;
    size_t currentGizmoIndex_ = 0;
    int hoveredHandle_ = -1;

    bool draggingGizmo_ = false;
    glm::mat4 dragStartTransform_;
};

} // namespace geodraw
