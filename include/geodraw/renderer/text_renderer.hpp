/*******************************************************************************
 * File: text_renderer.hpp
 *
 * Description: See text_renderer.cpp.
 *
 *
 *******************************************************************************/

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace geodraw {

class TextRenderer {
public:
  TextRenderer();
  ~TextRenderer();

  // Delete copy/move to prevent resource issues
  TextRenderer(const TextRenderer &) = delete;
  TextRenderer &operator=(const TextRenderer &) = delete;

  // Load font from file path (e.g., system font or embedded font)
  // fontSize: size in pixels for the atlas
  bool loadFont(const char *fontPath, float fontSize = 16.0f);

  // Render text at screen coordinates (top-left origin)
  // x, y: screen coordinates in pixels
  // color: RGB color (default white)
  // scale: text size multiplier (1.0 = default size)
  void renderText(const std::string &text, float x, float y,
                  const glm::vec3 &color = glm::vec3(1.0f, 1.0f, 1.0f),
                  float scale = 1.0f);

  // Set viewport dimensions for orthographic projection
  // Should be called when window is resized
  void setViewport(int width, int height);

private:
  class Impl; // PIMPL - hides STB and OpenGL details
  std::unique_ptr<Impl> impl;
};

} // namespace geodraw
