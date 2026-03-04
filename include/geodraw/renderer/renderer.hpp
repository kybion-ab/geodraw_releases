/*******************************************************************************
 * File: renderer.hpp
 *
 * Description: OpenGL renderer implementation.
 * Implements IRenderer interface for production use.
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/renderer/irenderer.hpp"
#include "geodraw/scene/scene.hpp"

#include <memory>

namespace geodraw {

/**
 * Renderer - OpenGL implementation of IRenderer
 *
 * This is the concrete renderer that uses OpenGL for all rendering operations.
 * For testing components that depend on rendering, use a mock implementation
 * of IRenderer instead.
 */
class GEODRAW_API Renderer : public IRenderer {
public:
  Renderer();
  ~Renderer() override;

  // Disable copy (Impl contains OpenGL resources)
  Renderer(const Renderer &) = delete;
  Renderer &operator=(const Renderer &) = delete;

  // ==========================================================================
  // IRenderer interface implementation
  // ==========================================================================

  void render(const Scene &scene, const Camera &camera) override;
  void clear() override;
  void setViewport(int width, int height) override;
  void setViewportRegion(int x, int y, int width, int height) override;

  std::optional<float> readDepthAt(int x, int y) override;
  const Scene *getLastScene() const override;

  void renderText(const std::string &text, float screenX, float screenY,
                  const glm::vec3 &color = glm::vec3(1.0f, 1.0f, 1.0f),
                  float scale = 1.0f) override;

  GLuint loadTexture(const std::string &path) override;
  void unloadTexture(const std::string &path) override;

  void setDepthTestEnabled(bool enabled) override;
  void setClearColor(float r, float g, float b, float a) override;

private:
  class Impl; // PIMPL - hides OpenGL details
  std::unique_ptr<Impl> impl;
};

} // namespace geodraw
