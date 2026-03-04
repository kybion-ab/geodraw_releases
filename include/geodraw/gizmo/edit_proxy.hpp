/*******************************************************************************
 * File: gizmo/edit_proxy.hpp
 *
 * Description: Base class for editable object proxies.
 *
 * EditProxy represents the "edit intent" for an object. Instead of modifying
 * raw data structures directly, gizmos modify proxies which then update the
 * underlying objects with rules and tolerances.
 *
 * Key concepts:
 * - Proxy owns a mutable transform that gizmos can modify
 * - syncFromOriginal() reads current state from the original object
 * - commitToOriginal() writes proxy state back to original
 * - Proxies declare which gizmo types they support
 *
 ******************************************************************************/

#pragma once

#include "geodraw/gizmo/gizmo.hpp"
#include "geodraw/geometry/geometry.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <string>
#include <vector>

namespace geodraw {

/**
 * Non-template base class for type-erased proxy storage
 */
class EditProxyBase {
public:
    virtual ~EditProxyBase() = default;

    //==========================================================================
    // Identity
    //==========================================================================

    /**
     * Get type name for this proxy (e.g., "Vehicle", "Lane", "Shape3")
     */
    virtual const std::string& getTypeName() const = 0;

    /**
     * Get pointer to the original object being edited (type-erased)
     * Used for deduplication in EditingContext
     */
    virtual void* getObjectPtr() const = 0;

    /**
     * Get unique ID for this proxy instance
     */
    uint32_t getID() const { return id_; }

    //==========================================================================
    // Lifecycle
    //==========================================================================

    /**
     * Sync proxy state from original object
     * Called when entering edit mode or resetting after cancel
     */
    virtual void syncFromOriginal() = 0;

    /**
     * Commit proxy changes back to original object
     * Called when user confirms edits (ENTER)
     */
    virtual void commitToOriginal() = 0;

    //==========================================================================
    // Transform
    //==========================================================================

    /**
     * Get current world transform for gizmo positioning
     */
    virtual glm::mat4 getTransform() const = 0;

    /**
     * Apply a delta transform to the proxy
     * Called during gizmo drag operations
     */
    virtual void applyTransform(const glm::mat4& deltaTransform) = 0;

    //==========================================================================
    // Gizmos
    //==========================================================================

    /**
     * Get list of gizmo types this proxy supports
     */
    virtual std::vector<GizmoType> getSupportedGizmos() const = 0;

    //==========================================================================
    // Visual representation
    //==========================================================================

    /**
     * Render proxy to scene (for visualization in edit mode)
     * @param scene     Scene to render to
     * @param isSelected True if this proxy is selected
     * @param isHovered  True if cursor is over this proxy
     */
    virtual void render(Scene& scene, bool isSelected, bool isHovered) = 0;

    /**
     * Get bounding box of the proxy geometry (for picking)
     */
    virtual BBox3 getBounds() const = 0;

    //==========================================================================
    // State
    //==========================================================================

    /**
     * Check if proxy has uncommitted changes
     */
    bool isDirty() const { return dirty_; }

    /**
     * Mark proxy as dirty (has unsaved changes)
     */
    void setDirty(bool dirty = true) { dirty_ = dirty; }

    /**
     * Check if this proxy is currently selected
     */
    bool isSelected() const { return selected_; }

    /**
     * Set selection state
     */
    void setSelected(bool selected) { selected_ = selected; }

    /**
     * Check if cursor is hovering over this proxy
     */
    bool isHovered() const { return hovered_; }

    /**
     * Set hover state
     */
    void setHovered(bool hovered) { hovered_ = hovered; }

protected:
    EditProxyBase() : id_(nextID_++) {}

    uint32_t id_;
    bool dirty_ = false;
    bool selected_ = false;
    bool hovered_ = false;

private:
    static inline uint32_t nextID_ = 1;
};

/**
 * Typed proxy base class with pointer to original object
 *
 * @tparam T Type of the original object being edited
 */
template<typename T>
class EditProxy : public EditProxyBase {
public:
    /**
     * Create proxy for an existing object
     * @param original  Pointer to the object to edit (must remain valid)
     * @param typeName  Human-readable type name
     */
    EditProxy(T* original, const std::string& typeName)
        : original_(original), typeName_(typeName) {}

    const std::string& getTypeName() const override { return typeName_; }

    /**
     * Get pointer to the original object (type-erased)
     */
    void* getObjectPtr() const override { return original_; }

    /**
     * Get pointer to original object (typed)
     */
    T* getOriginal() const { return original_; }

protected:
    T* original_;
    std::string typeName_;
};

//==============================================================================
// Concrete proxy for Shape3 objects
//==============================================================================

/**
 * Proxy for editing Shape3 geometry
 *
 * Supports translation via a position offset. The position is tracked
 * separately from the Shape3 vertices, allowing non-destructive editing.
 */
class Shape3Proxy : public EditProxy<Shape3> {
public:
    Shape3Proxy(Shape3* shape)
        : EditProxy(shape, "Shape3") {
        syncFromOriginal();
    }

    void syncFromOriginal() override;

    void commitToOriginal() override;

    glm::mat4 getTransform() const override {
        return glm::translate(glm::mat4(1.0f), glm::vec3(position_));
    }

    void applyTransform(const glm::mat4& deltaTransform) override;

    std::vector<GizmoType> getSupportedGizmos() const override {
        return {GizmoType::TRANSLATE};
    }

    void render(Scene& scene, bool isSelected, bool isHovered) override;

    BBox3 getBounds() const override;

private:
    glm::dvec3 computeCentroid() const;

    Shape3 editShape_;           // Copy for editing
    glm::dvec3 position_;        // Current position (centroid + edits)
    glm::dvec3 originalPosition_; // Position at last sync
};

} // namespace geodraw
