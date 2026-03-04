/*******************************************************************************
 * File : scene.hpp
 *
 * Description: Library providing the Scene system for building renderable
 * geometry collections. Contains Add() methods that convert geometry primitives
 * (Line3, Triangle3, Ground, Mesh3) into DrawCmd structures with rendering
 * parameters (color, thickness, transparency, textures).
 *
 * Why required: Bridge between geometry and rendering. Users add primitives to
 * Scenes which are then passed to the Renderer. Essential for the rendering
 * architecture that separates scene description from OpenGL execution.
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2025-12-21
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"

#include "geodraw/geometry/colors.hpp"
#include "geodraw/geometry/geometry.hpp"
#include "geodraw/geometry/ray.hpp"
#include <glm/glm.hpp>
#include <limits>
#include <optional>
#include <set>
#include <vector>

using GLuint = unsigned int;

namespace geodraw {

// Forward declarations
struct DrawCmd;
struct CameraNearFrustum;

// Forward declaration for editing
class EditingContext;

// Result of ray picking against scene geometry
struct GEODRAW_API PickResult {
  bool hit = false;
  float distance = 0.0f;
  glm::vec3 worldPoint{0.0f};
  uint64_t objectId = 0;         // Object ID if the hit DrawCmd has one
  std::string objectType;        // Object type if the hit DrawCmd has one
};

// Supported line styles
enum class LineStyle { Solid, Dashed, Dotted };

// Primitive types for rendering
enum class PrimitiveType {
  LINE_STRIP, // Lines/wireframes (default)
  LINES,      // Independent line segments (pairs of vertices, GL_LINES)
  TRIANGLES   // Filled triangles
};

// Vertex data for textured rendering (position + UV coordinates)
struct GEODRAW_API VertexData {
  glm::vec3 pos;
  glm::vec2 uv;

  VertexData(const glm::vec3 &p, const glm::vec2 &t) : pos(p), uv(t) {}
};

// Draw command stored in the scene
struct GEODRAW_API DrawCmd {
  std::vector<glm::vec3> vertices; // Non-textured vertices (backward compat)
  std::vector<glm::vec3> normals;  // Optional per-vertex normals (for flat
                                   // shading: 3 identical per triangle)
  float thickness = 1.0f;
  LineStyle lineStyle = LineStyle::Solid;
  float transparency = 1.0f;
  bool depthTest = true; // whether to respect depth buffer
  glm::vec3 color = Colors::WHITE; // Color (default white)
  PrimitiveType primitiveType = PrimitiveType::LINE_STRIP; // Primitive type

  // Textured rendering support
  std::vector<VertexData> texturedVertices; // Textured vertices (position + UV)
  GLuint textureHandle = 0;                 // 0 = no texture, use solid color

  bool hasTexture() const { return textureHandle != 0; }
  bool hasNormals() const { return !normals.empty(); }

  // Rendering order control
  bool alwaysOnTop = false;  // Render after all regular objects, ignoring depth

  // Object identification for picking
  uint64_t objectId = 0;       // Application-defined object ID (0 = no ID)
  std::string objectType;      // Optional type name for identification
  bool hasObjectId() const { return objectId != 0; }

  // Persistent GPU resources (managed by renderer, freed by Scene::clear())
  // mutable so the renderer can cache VBOs on a const DrawCmd reference
  mutable GLuint vao = 0;     // VAO (stores attrib layout + VBO binding)
  mutable GLuint vbo = 0;     // Position (or interleaved pos+UV) VBO; 0 = not yet uploaded
  mutable GLuint normVbo = 0; // Normals VBO (flat shader path only)
};

/**
 * TooltipBuilder - Fluent API for setting tooltip metadata on DrawCmds
 *
 * Returned by Scene::Add() methods to allow chaining tooltip configuration:
 *   scene.Add(mesh, ...).withId(vehicleId).withType("Vehicle");
 *
 * Backward compatible: implicitly converts to DrawCmd* for existing code.
 */
class GEODRAW_API TooltipBuilder {
public:
    explicit TooltipBuilder(DrawCmd* cmd) : cmd_(cmd) {}

    /**
     * Set the object ID for this DrawCmd (used for tooltip lookup)
     */
    TooltipBuilder& withId(uint64_t id) {
        if (cmd_) cmd_->objectId = id;
        return *this;
    }

    /**
     * Set the object type for this DrawCmd (used to find tooltip provider)
     */
    TooltipBuilder& withType(const std::string& type) {
        if (cmd_) cmd_->objectType = type;
        return *this;
    }

    /**
     * Backward compatibility: implicit conversion to DrawCmd*
     * @deprecated Use .get() or operator-> to avoid stale-pointer hazard
     * when a subsequent scene.Add() call reallocates the command vector.
     */
    [[deprecated("Use .get() or operator-> instead; implicit conversion is a stale-pointer hazard")]]
    operator DrawCmd*() const { return cmd_; }

    /**
     * Get the underlying DrawCmd pointer
     */
    DrawCmd* get() const { return cmd_; }

    /**
     * Arrow operator for direct DrawCmd member access.
     * Allows: scene.Add(mesh).withId(1)->alwaysOnTop = true;
     */
    DrawCmd* operator->() const { return cmd_; }

private:
    DrawCmd* cmd_;
};

// Scene - collection of geometry to render
class GEODRAW_API Scene {
public:
  // Free CPU geometry and GPU buffers (VAO/VBOs) for all DrawCmds.
  // Must be called with an active OpenGL context.
  void clear();
  const std::vector<DrawCmd> &getCommands() const { return cmds; }

  //==========================================================================
  // ECEF Origin Support (for camera-relative rendering)
  //==========================================================================

  /**
   * Set ECEF origin for camera-relative rendering.
   * Typically set to camera's ECEF position each frame.
   * All Add() methods will subtract this origin before converting to float.
   *
   * @param ecefOrigin ECEF position in meters (typically camera position)
   */
  void setOrigin(const glm::dvec3& ecefOrigin) { origin_ = ecefOrigin; }

  /**
   * Get current ECEF origin
   */
  glm::dvec3 getOrigin() const { return origin_; }

protected:
  /**
   * Convert world coordinates to local float coordinates for GPU.
   * Subtracts origin and converts from double to float.
   *
   * @param worldPos World position (ECEF or any coordinate system)
   * @return Local float coordinates relative to origin
   */
  glm::vec3 toLocal(const glm::dvec3& worldPos) const {
    return glm::vec3(worldPos - origin_);
  }

public:

  //==========================================================================
  // Editing Support
  //==========================================================================

  /**
   * Connect this scene to an editing context for auto-proxy registration.
   * When set, calls to Add() for supported types (e.g., Shape3) will
   * automatically register editable proxies in the context.
   *
   * @param ctx Pointer to EditingContext (nullptr to disable)
   */
  void setEditingContext(EditingContext *ctx) { editCtx_ = ctx; }

  /**
   * Get the connected editing context (may be nullptr)
   */
  EditingContext *editingContext() { return editCtx_; }

  /**
   * Get tracked editable shapes (for proxy registration by App)
   */
  const std::vector<Shape3 *> &getEditableShapes() const {
    return editableShapes_;
  }

  // Compute axis-aligned bounding box of all geometry in the scene
  // Returns std::nullopt if scene is empty
  std::optional<BBox3> getBounds() const;

  // Ray picking against scene geometry
  // Iterates over all TRIANGULAR DrawCmds and tests ray-triangle intersection
  // Returns the closest hit (if any)
  // If requireObjectId is true, only considers objects with objectId != 0
  PickResult rayPick(const Ray& ray, bool requireObjectId = false) const;

  TooltipBuilder Add(const Pos3 &point, float radius = 0.5f,
               float alpha = 1.0f,
               const glm::vec3 &color = Colors::WHITE,
               bool depthTest = true,
               int segments = 16);
  TooltipBuilder Add(const Line3 &line, float thickness = 1.0f,
               LineStyle style = LineStyle::Solid, float alpha = 1.0f,
               const glm::vec3 &color = Colors::WHITE,
               bool depthTest = true);
  TooltipBuilder AddLines(const std::vector<Line3> &lines, float thickness = 1.0f,
               LineStyle style = LineStyle::Solid, float alpha = 1.0f,
               const glm::vec3 &color = Colors::WHITE,
               bool depthTest = true);
  TooltipBuilder Add(const Polyline3 &polyline, float thickness = 1.0f,
               LineStyle style = LineStyle::Solid, float alpha = 1.0f,
               const glm::vec3 &color = Colors::WHITE,
               bool depthTest = true);
  TooltipBuilder Add(const Triangle3 &tri, float thickness = 1.0f,
               LineStyle style = LineStyle::Solid, float alpha = 1.0f,
               const glm::vec3 &color = Colors::WHITE,
               bool depthTest = true, bool filled = true,
               GLuint textureHandle = 0, bool useShading = true);
  TooltipBuilder Add(const Quad3 &quad, float thickness = 1.0f,
               LineStyle style = LineStyle::Solid, float alpha = 1.0f,
               const glm::vec3 &color = Colors::WHITE,
               bool depthTest = true, bool filled = true);
  TooltipBuilder Add(const BBox3 &bbox, float thickness = 1.0f,
               LineStyle style = LineStyle::Solid, float alpha = 1.0f,
               const glm::vec3 &color = Colors::WHITE,
               bool depthTest = true, bool filled = true, bool useShading = true);
  TooltipBuilder Add(const RBox3 &rbox, float thickness = 1.0f,
               LineStyle style = LineStyle::Solid, float alpha = 1.0f,
               const glm::vec3 &color = Colors::WHITE,
               bool depthTest = true, bool filled = true, bool useShading = true);
  TooltipBuilder Add(const Pose3 &pose, float scale = 1.0f, float thickness = 2.0f,
               LineStyle style = LineStyle::Solid, float alpha = 1.0f,
               bool depthTest = true);
  TooltipBuilder Add(const CameraNearFrustum &camera, float thickness = 1.0f, LineStyle style = LineStyle::Solid, float alpha = 1.0f, const glm::vec3 &color = Colors::WHITE, bool depthTest = true, bool filled = true, bool useShading = true);
  TooltipBuilder Add(const Ground &ground, float thickness = 1.0f,
               float normalScale = 1.0f, LineStyle style = LineStyle::Solid,
               float alpha = 1.0f,
                     const glm::vec3 &quadColor = Colors::LIGHT_GRAY,
                     const glm::vec3 &normalColor = Colors::BLUE,
               bool depthTest = true, bool filled = true,
               GLuint textureHandle = 0, bool useShading = true);
  TooltipBuilder Add(const Mesh3 &mesh, float thickness = 1.0f,
               LineStyle style = LineStyle::Solid, float alpha = 1.0f,
               const glm::vec3 &color = Colors::WHITE,
               bool depthTest = true, bool filled = true,
               GLuint textureHandle = 0, bool useShading = true);
  TooltipBuilder Add(Shape3& shape, float thickness = 1.0f,
               LineStyle style = LineStyle::Solid, float alpha = 1.0f,
               const glm::vec3& color = Colors::WHITE,
               bool depthTest = true, bool filled = true, bool useShading = true);
  TooltipBuilder Add(const Shape3& shape, float thickness = 1.0f,
               LineStyle style = LineStyle::Solid, float alpha = 1.0f,
               const glm::vec3& color = Colors::WHITE,
               bool depthTest = true, bool filled = true, bool useShading = true);
private:
std::vector<DrawCmd> cmds;
EditingContext *editCtx_ =
    nullptr; // Optional editing context for auto-proxy registration
std::vector<Shape3 *> editableShapes_; // Tracked shapes for proxy registration
glm::dvec3 origin_{0.0, 0.0, 0.0}; // ECEF origin for camera-relative rendering
};

} // namespace geodraw

// Note: Include scene_hdmap.hpp separately when HD map visualization support is needed.
// Include modules/drive/scene_trajectory.hpp for RoadLine/RoadEdge rendering.
