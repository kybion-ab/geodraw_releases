/*******************************************************************************
 * File: imgui_plugin.h
 *
 * Description: See imgui_plugin.cpp.
 *
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"

#include "geodraw/external/glad/include/glad.h"  // Include glad before ImGui to avoid OpenGL header conflicts
#include <imgui.h>
struct GLFWwindow;
#include <functional>
#include <string>

namespace geodraw {
class App;
class Scene;
}

// ImGuiPlugin: Optional plugin for integrating ImGui with App
// This class handles ImGui initialization, frame lifecycle, and input focus
// management
class GEODRAW_API ImGuiPlugin {
public:
  using ImGuiDrawCallback = std::function<void()>;

  // Constructor: Initializes ImGui and registers callbacks with App
  explicit ImGuiPlugin(geodraw::App &app);

  // Destructor: Shuts down ImGui
  ~ImGuiPlugin();

  // Set the callback that draws ImGui content (e.g., windows, widgets)
  void setImGuiCallback(ImGuiDrawCallback cb) { imguiDrawCallback = cb; }

  // Get ImGuiIO for direct access if needed (advanced usage)
  ImGuiIO &getIO() { return ImGui::GetIO(); }

  /**
   * Enable automatic tooltip handling for a scene.
   * When enabled, ImGuiPlugin will:
   * - Perform ray picking on mouse hover (when ImGui doesn't want mouse)
   * - Update app.tooltips() with pick results
   * - Render tooltip content automatically within ImGui frame
   *
   * @param scene The scene to pick against for tooltips
   */
  /// Draw a master window listing all registered minor modes as checkboxes.
  /// Call inside setImGuiCallback().
  void drawMasterWindow(const std::string& title = "geodraw");

  /// Draw the "Plugins" checklist window and all registered plugin panels.
  /// Manages open/close, collapse/expand, and minor mode sync automatically.
  /// Call once per ImGui frame inside setImGuiCallback().
  void drawPluginsPanel(const std::string& title = "Plugins");

  void enableTooltips(geodraw::Scene& scene);

  /**
   * Disable automatic tooltip handling.
   */
  void disableTooltips();

private:
  void initialize(GLFWwindow *window);
  void shutdown();
  void beginFrame();
  void endFrame();

  // Callbacks registered with App
  void drawCallback();
  bool inputFocusCallback();
  float scrollCallback();

  geodraw::App &app;
  ImGuiDrawCallback imguiDrawCallback = nullptr;

  // Tooltip automation state
  geodraw::Scene* tooltipScene_ = nullptr;  // Scene to pick against (nullptr = disabled)

  bool pluginsInitialized_ = false;
  void drawRegisteredPlugins();
};
