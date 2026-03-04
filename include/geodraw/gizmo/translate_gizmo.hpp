/*******************************************************************************
 * File: gizmo/translate_gizmo.hpp
 *
 * Description: Translation gizmo with axis and plane handles.
 *
 * Provides 6 handles for constrained translation:
 * - 3 axis handles (X, Y, Z) for single-axis movement
 * - 3 plane handles (XY, XZ, YZ) for two-axis movement
 *
 * Visual representation:
 * - Colored lines with arrow tips (Red=X, Green=Y, Blue=Z)
 * - Small colored quads at axis intersections for plane handles
 * - Handles highlight when hovered or active
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"

#include "gizmo.hpp"

namespace geodraw {

/**
 * Translation gizmo with axis arrows and plane handles
 */
class GEODRAW_API TranslateGizmo : public Gizmo {
public:
    //==========================================================================
    // Handle indices
    //==========================================================================

    static constexpr int HANDLE_X = 0;   // X-axis (red)
    static constexpr int HANDLE_Y = 1;   // Y-axis (green)
    static constexpr int HANDLE_Z = 2;   // Z-axis (blue)
    static constexpr int HANDLE_XY = 3;  // XY plane (yellow)
    static constexpr int HANDLE_XZ = 4;  // XZ plane (cyan)
    static constexpr int HANDLE_YZ = 5;  // YZ plane (magenta)
    static constexpr int NUM_HANDLES = 6;

    //==========================================================================
    // Configuration
    //==========================================================================

    // Visual sizing (relative to scale_)
    float arrowLengthRatio = 1.0f;     // Axis length as fraction of scale
    float arrowTipRatio = 0.15f;       // Arrow tip size as fraction of axis length
    float handleRadiusRatio = 0.08f;   // Clickable sphere radius at axis ends
    float planeSizeRatio = 0.25f;      // Plane handle size as fraction of axis length
    float planeOffsetRatio = 0.35f;    // Plane handle offset from origin

    // Line thickness
    float axisThickness = 3.0f;
    float highlightThickness = 4.5f;

    //==========================================================================
    // Gizmo interface
    //==========================================================================

    GizmoType getType() const override { return GizmoType::TRANSLATE; }
    std::string getName() const override { return "Translate"; }

    void render(Scene& scene,
               const glm::mat4& transform,
               const Camera& camera,
               bool isActive,
               int hoveredHandle = -1) override;

    GizmoHitResult hitTest(const Camera::Ray& ray,
                           const glm::mat4& transform) const override;

    void beginDrag(int handleIndex,
                  const glm::vec3& startPoint,
                  const glm::mat4& transform) override;

    glm::mat4 updateDrag(const glm::vec3& currentPoint,
                         const Camera& camera) override;

    void endDrag() override;
    void cancelDrag() override;

private:
    //==========================================================================
    // Internal helpers
    //==========================================================================

    // Get world-space axis direction from transform
    glm::vec3 getAxisDirection(const glm::mat4& transform, int axis) const;

    // Get world-space origin from transform
    glm::vec3 getOrigin(const glm::mat4& transform) const;

    // Get handle color (with highlighting support)
    glm::vec3 getHandleColor(int handleIndex, int hoveredHandle, bool isActive) const;

    // Hit test individual axis (sphere at tip + cylinder along shaft)
    bool hitTestAxis(const Camera::Ray& ray, const glm::mat4& transform,
                     int axis, float& t) const;

    // Hit test plane handle (small quad)
    bool hitTestPlane(const Camera::Ray& ray, const glm::mat4& transform,
                      int planeHandle, float& t) const;

    // Project cursor to constraint axis/plane during drag
    glm::vec3 projectToConstraint(const glm::vec3& cursorPoint,
                                  const Camera& camera) const;

    // Constraint info for current drag
    glm::vec3 constraintOrigin_;
    glm::vec3 constraintAxis1_;      // Primary axis (for axis handles)
    glm::vec3 constraintAxis2_;      // Secondary axis (for plane handles)
    glm::vec3 constraintNormal_;     // Plane normal for plane handles
    bool isPlaneConstraint_ = false;
};


} // namespace geodraw
