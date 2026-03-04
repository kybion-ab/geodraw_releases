/*******************************************************************************
 * File: gizmo/gizmo.hpp
 *
 * Description: Base class for interactive 3D manipulation gizmos.
 *
 * Gizmos provide visual handles for transforming objects in the scene.
 * Each gizmo type (translate, rotate, scale) implements this interface
 * to provide consistent interaction patterns.
 *
 ******************************************************************************/

#pragma once

#include "geodraw/camera/camera.hpp"
#include "geodraw/scene/scene.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

namespace geodraw {

// Forward declarations
class Scene;

/**
 * Types of gizmos available for manipulation
 */
enum class GizmoType {
    TRANSLATE,    // 3-axis translation with axis/plane handles
    ROTATE,       // 3-axis rotation with arc handles
    SCALE,        // 3-axis scaling (uniform or per-axis)
    POINT_EDIT,   // Edit individual control points
    CUSTOM        // Application-defined gizmo
};

/**
 * Result of hit testing a gizmo against a ray
 */
struct GizmoHitResult {
    bool hit = false;           // Whether the ray hit any handle
    int handleIndex = -1;       // Index of hit handle (-1 = none)
    glm::vec3 hitPoint;         // World-space intersection point
    float distance = 0.0f;      // Distance from ray origin to hit

    // Convenience factory for no-hit result
    static GizmoHitResult noHit() { return GizmoHitResult{}; }
};

/**
 * Abstract base class for interactive gizmos
 *
 * Gizmos are rendered as overlays on selected objects and provide
 * visual handles for manipulation. The interaction model is:
 *
 * 1. render() - Draw gizmo handles at the object's transform
 * 2. hitTest() - Check if a ray intersects any handle
 * 3. beginDrag() - Start manipulation when handle clicked
 * 4. updateDrag() - Compute new transform during mouse drag
 * 5. endDrag() / cancelDrag() - Finish or abort manipulation
 */
class Gizmo {
public:
    virtual ~Gizmo() = default;

    //==========================================================================
    // Identity
    //==========================================================================

    /**
     * Get the type of this gizmo
     */
    virtual GizmoType getType() const = 0;

    /**
     * Get human-readable name for this gizmo
     */
    virtual std::string getName() const = 0;

    //==========================================================================
    // Rendering
    //==========================================================================

    /**
     * Render gizmo handles to the scene
     *
     * @param scene         Scene to add gizmo geometry to
     * @param transform     World transform of the object being manipulated
     * @param camera        Camera for view-dependent sizing/orientation
     * @param isActive      True if this gizmo is the current active gizmo
     * @param hoveredHandle Index of handle under cursor (-1 = none)
     */
    virtual void render(Scene& scene,
                       const glm::mat4& transform,
                       const Camera& camera,
                       bool isActive,
                       int hoveredHandle = -1) = 0;

    //==========================================================================
    // Interaction
    //==========================================================================

    /**
     * Test if a ray intersects any gizmo handle
     *
     * @param ray       Ray in world space (from camera through cursor)
     * @param transform World transform of the object being manipulated
     * @return Hit result with handle index and intersection point
     */
    virtual GizmoHitResult hitTest(const Camera::Ray& ray,
                                   const glm::mat4& transform) const = 0;

    /**
     * Begin dragging a gizmo handle
     *
     * @param handleIndex   Index of the handle being dragged
     * @param startPoint    World-space point where drag started
     * @param transform     Current transform of the object
     */
    virtual void beginDrag(int handleIndex,
                          const glm::vec3& startPoint,
                          const glm::mat4& transform) = 0;

    /**
     * Update transform during drag operation
     *
     * @param currentPoint  Current world-space point under cursor
     * @param camera        Camera for view-dependent calculations
     * @return Delta transform to apply (relative to drag start)
     */
    virtual glm::mat4 updateDrag(const glm::vec3& currentPoint,
                                 const Camera& camera) = 0;

    /**
     * End drag operation successfully (apply changes)
     */
    virtual void endDrag() = 0;

    /**
     * Cancel drag operation (revert to original)
     */
    virtual void cancelDrag() = 0;

    //==========================================================================
    // State
    //==========================================================================

    /**
     * Check if gizmo is currently being dragged
     */
    bool isDragging() const { return dragging_; }

    /**
     * Get the index of the currently active handle (-1 if not dragging)
     */
    int getActiveHandle() const { return activeHandle_; }

    /**
     * Get visual scale factor for gizmo size
     */
    float getScale() const { return scale_; }

    /**
     * Set visual scale factor for gizmo size
     */
    void setScale(float scale) { scale_ = scale; }

protected:
    bool dragging_ = false;
    int activeHandle_ = -1;
    glm::vec3 dragStartPoint_;
    glm::mat4 dragStartTransform_;
    float scale_ = 5.0f;  // Default gizmo size in world units
};

//==============================================================================
// Utility functions for gizmo implementations
//==============================================================================

/**
 * Test ray-sphere intersection
 *
 * @param ray       Ray to test
 * @param center    Sphere center in world space
 * @param radius    Sphere radius
 * @param t         Output: distance along ray to intersection (if hit)
 * @return True if ray intersects sphere
 */
inline bool raySphereIntersect(const Camera::Ray& ray,
                               const glm::vec3& center,
                               float radius,
                               float& t) {
    glm::vec3 oc = ray.origin - center;
    float a = glm::dot(ray.direction, ray.direction);
    float b = 2.0f * glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4 * a * c;

    if (discriminant < 0) {
        return false;
    }

    t = (-b - std::sqrt(discriminant)) / (2.0f * a);
    return t >= 0;
}

/**
 * Test ray-plane intersection
 *
 * @param ray       Ray to test
 * @param planePoint    A point on the plane
 * @param planeNormal   Plane normal (must be normalized)
 * @param t         Output: distance along ray to intersection (if hit)
 * @return True if ray intersects plane (not parallel)
 */
inline bool rayPlaneIntersect(const Camera::Ray& ray,
                              const glm::vec3& planePoint,
                              const glm::vec3& planeNormal,
                              float& t) {
    float denom = glm::dot(planeNormal, ray.direction);
    if (std::abs(denom) < 1e-6f) {
        return false;  // Ray parallel to plane
    }

    t = glm::dot(planePoint - ray.origin, planeNormal) / denom;
    return t >= 0;
}

/**
 * Project a point onto a line (closest point on line)
 *
 * @param point     Point to project
 * @param lineStart Start of line segment
 * @param lineDir   Direction of line (must be normalized)
 * @return Closest point on line to the input point
 */
inline glm::vec3 projectPointOnLine(const glm::vec3& point,
                                    const glm::vec3& lineStart,
                                    const glm::vec3& lineDir) {
    float t = glm::dot(point - lineStart, lineDir);
    return lineStart + lineDir * t;
}

/**
 * Get a point on a ray at distance t
 */
inline glm::vec3 rayPointAt(const Camera::Ray& ray, float t) {
    return ray.origin + ray.direction * t;
}

} // namespace geodraw
