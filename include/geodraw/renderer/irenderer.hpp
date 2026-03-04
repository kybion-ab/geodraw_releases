/*******************************************************************************
 * File: irenderer.hpp
 *
 * Description: Abstract interface for rendering operations.
 * Allows dependency injection and mocking for testing components that
 * depend on rendering (e.g., EarthLayer, GltfLoader).
 *
 ******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"

#include <glm/glm.hpp>
#include <optional>
#include <string>

// Forward declare OpenGL types to avoid including GL headers
using GLuint = unsigned int;

namespace geodraw {

// Forward declarations
class Scene;
class Camera;

/**
 * IRenderer - Abstract rendering interface
 *
 * This interface defines the rendering operations that components can depend on.
 * The concrete Renderer class implements this interface with OpenGL.
 * For testing, a MockRenderer can be created that implements this interface
 * without requiring an OpenGL context.
 *
 * Usage for testable code:
 *   class EarthLayer {
 *   public:
 *       EarthLayer(const Config& config, IRenderer& renderer);
 *   };
 *
 *   // In production:
 *   Renderer renderer;
 *   EarthLayer earth(config, renderer);
 *
 *   // In tests:
 *   MockRenderer mockRenderer;
 *   EarthLayer earth(config, mockRenderer);
 */
class GEODRAW_API IRenderer {
public:
  virtual ~IRenderer() = default;

  // ==========================================================================
  // Core Rendering
  // ==========================================================================

  /**
   * Render a scene from the given camera viewpoint
   */
  virtual void render(const Scene &scene, const Camera &camera) = 0;

  /**
   * Clear the framebuffer
   */
  virtual void clear() = 0;

  /**
   * Set the viewport dimensions
   */
  virtual void setViewport(int width, int height) = 0;

  /**
   * Set the viewport region (position + size) for sub-viewport rendering
   */
  virtual void setViewportRegion(int x, int y, int width, int height) = 0;

  // ==========================================================================
  // Picking Support
  // ==========================================================================

  /**
   * Read depth value at screen coordinates
   * @return Depth value [0,1] or nullopt if reading failed
   */
  virtual std::optional<float> readDepthAt(int x, int y) = 0;

  /**
   * Get the last rendered scene (for auto-framing calculations)
   */
  virtual const Scene *getLastScene() const = 0;

  // ==========================================================================
  // Text Rendering
  // ==========================================================================

  /**
   * Render text at screen coordinates
   * @param text Text to render
   * @param screenX X position in screen pixels
   * @param screenY Y position in screen pixels
   * @param color RGB color (0-1 range)
   * @param scale Text size multiplier (1.0 = default 16px font)
   */
  virtual void renderText(const std::string &text, float screenX, float screenY,
                          const glm::vec3 &color = glm::vec3(1.0f, 1.0f, 1.0f),
                          float scale = 1.0f) = 0;

  // ==========================================================================
  // Texture Management
  // ==========================================================================

  /**
   * Load a texture from file
   * @param path Path to image file
   * @return OpenGL texture handle, or 0 on failure
   */
  virtual GLuint loadTexture(const std::string &path) = 0;

  /**
   * Unload a previously loaded texture
   * @param path Path used when loading the texture
   */
  virtual void unloadTexture(const std::string &path) = 0;

  // ==========================================================================
  // State Management
  // ==========================================================================

  /**
   * Enable or disable depth testing
   */
  virtual void setDepthTestEnabled(bool enabled) = 0;

  /**
   * Set the clear color
   */
  virtual void setClearColor(float r, float g, float b, float a) = 0;
};

} // namespace geodraw
