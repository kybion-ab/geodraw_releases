/*******************************************************************************
 * File: mock_renderer.hpp
 *
 * Description: Mock implementation of IRenderer for testing.
 * Does not require an OpenGL context and records all calls for verification.
 *
 ******************************************************************************/

#pragma once

#include "geodraw/renderer/irenderer.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace geodraw {

/**
 * MockRenderer - Test double for IRenderer
 *
 * This implementation allows testing components that depend on IRenderer
 * without requiring an OpenGL context. It records all method calls and
 * provides configurable return values.
 *
 * Usage in tests:
 *   MockRenderer renderer;
 *   EarthLayer earth(config, renderer);
 *
 *   // Configure mock behavior
 *   renderer.setNextTextureId(42);
 *
 *   // Execute code under test
 *   earth.update(camera, 800, 600);
 *
 *   // Verify interactions
 *   ASSERT(renderer.loadTextureCallCount() == 5);
 *   ASSERT(renderer.wasCleared());
 */
class MockRenderer : public IRenderer {
public:
  MockRenderer() = default;
  ~MockRenderer() override = default;

  // ==========================================================================
  // IRenderer implementation
  // ==========================================================================

  void render(const Scene &scene, const Camera &camera) override {
    renderCallCount_++;
    lastScene_ = &scene;
  }

  void clear() override { clearCallCount_++; }

  void setViewport(int width, int height) override {
    viewportWidth_ = width;
    viewportHeight_ = height;
    setViewportCallCount_++;
  }

  std::optional<float> readDepthAt(int x, int y) override {
    readDepthAtCallCount_++;
    lastDepthReadX_ = x;
    lastDepthReadY_ = y;
    return depthValue_;
  }

  const Scene *getLastScene() const override { return lastScene_; }

  void renderText(const std::string &text, float screenX, float screenY,
                  const glm::vec3 &color, float scale) override {
    renderTextCallCount_++;
    renderedTexts_.push_back(text);
  }

  GLuint loadTexture(const std::string &path) override {
    loadTextureCallCount_++;
    loadedTexturePaths_.push_back(path);

    // Return pre-configured ID or generate one
    if (nextTextureId_ > 0) {
      GLuint id = nextTextureId_;
      texturePathToId_[path] = id;
      nextTextureId_ = 0; // Reset after use
      return id;
    }

    // Auto-generate unique ID
    GLuint id = ++autoTextureIdCounter_;
    texturePathToId_[path] = id;
    return id;
  }

  void unloadTexture(const std::string &path) override {
    unloadTextureCallCount_++;
    unloadedTexturePaths_.push_back(path);
    texturePathToId_.erase(path);
  }

  void setDepthTestEnabled(bool enabled) override {
    depthTestEnabled_ = enabled;
    setDepthTestCallCount_++;
  }

  void setClearColor(float r, float g, float b, float a) override {
    clearColor_ = glm::vec4(r, g, b, a);
    setClearColorCallCount_++;
  }

  // ==========================================================================
  // Mock configuration
  // ==========================================================================

  /**
   * Set the texture ID to return on next loadTexture() call
   */
  void setNextTextureId(GLuint id) { nextTextureId_ = id; }

  /**
   * Set the depth value to return from readDepthAt()
   */
  void setDepthValue(std::optional<float> depth) { depthValue_ = depth; }

  /**
   * Reset all call counts and state
   */
  void reset() {
    renderCallCount_ = 0;
    clearCallCount_ = 0;
    setViewportCallCount_ = 0;
    readDepthAtCallCount_ = 0;
    renderTextCallCount_ = 0;
    loadTextureCallCount_ = 0;
    unloadTextureCallCount_ = 0;
    setDepthTestCallCount_ = 0;
    setClearColorCallCount_ = 0;

    lastScene_ = nullptr;
    viewportWidth_ = 0;
    viewportHeight_ = 0;
    depthTestEnabled_ = true;
    clearColor_ = glm::vec4(0.0f);

    renderedTexts_.clear();
    loadedTexturePaths_.clear();
    unloadedTexturePaths_.clear();
    texturePathToId_.clear();
    autoTextureIdCounter_ = 0;
    nextTextureId_ = 0;
    depthValue_ = std::nullopt;
  }

  // ==========================================================================
  // Verification helpers
  // ==========================================================================

  int renderCallCount() const { return renderCallCount_; }
  int clearCallCount() const { return clearCallCount_; }
  int setViewportCallCount() const { return setViewportCallCount_; }
  int readDepthAtCallCount() const { return readDepthAtCallCount_; }
  int renderTextCallCount() const { return renderTextCallCount_; }
  int loadTextureCallCount() const { return loadTextureCallCount_; }
  int unloadTextureCallCount() const { return unloadTextureCallCount_; }
  int setDepthTestCallCount() const { return setDepthTestCallCount_; }
  int setClearColorCallCount() const { return setClearColorCallCount_; }

  bool wasCleared() const { return clearCallCount_ > 0; }
  bool wasRendered() const { return renderCallCount_ > 0; }

  int getViewportWidth() const { return viewportWidth_; }
  int getViewportHeight() const { return viewportHeight_; }

  bool isDepthTestEnabled() const { return depthTestEnabled_; }
  glm::vec4 getClearColor() const { return clearColor_; }

  const std::vector<std::string> &getRenderedTexts() const {
    return renderedTexts_;
  }

  const std::vector<std::string> &getLoadedTexturePaths() const {
    return loadedTexturePaths_;
  }

  const std::vector<std::string> &getUnloadedTexturePaths() const {
    return unloadedTexturePaths_;
  }

  /**
   * Get texture ID for a given path (0 if not loaded)
   */
  GLuint getTextureId(const std::string &path) const {
    auto it = texturePathToId_.find(path);
    return (it != texturePathToId_.end()) ? it->second : 0;
  }

  int getLastDepthReadX() const { return lastDepthReadX_; }
  int getLastDepthReadY() const { return lastDepthReadY_; }

private:
  // Call counters
  int renderCallCount_ = 0;
  int clearCallCount_ = 0;
  int setViewportCallCount_ = 0;
  int readDepthAtCallCount_ = 0;
  int renderTextCallCount_ = 0;
  int loadTextureCallCount_ = 0;
  int unloadTextureCallCount_ = 0;
  int setDepthTestCallCount_ = 0;
  int setClearColorCallCount_ = 0;

  // State
  const Scene *lastScene_ = nullptr;
  int viewportWidth_ = 0;
  int viewportHeight_ = 0;
  bool depthTestEnabled_ = true;
  glm::vec4 clearColor_{0.0f};
  int lastDepthReadX_ = 0;
  int lastDepthReadY_ = 0;

  // Recorded data
  std::vector<std::string> renderedTexts_;
  std::vector<std::string> loadedTexturePaths_;
  std::vector<std::string> unloadedTexturePaths_;
  std::unordered_map<std::string, GLuint> texturePathToId_;

  // Configurable responses
  GLuint autoTextureIdCounter_ = 0;
  GLuint nextTextureId_ = 0;
  std::optional<float> depthValue_;
};

} // namespace geodraw
