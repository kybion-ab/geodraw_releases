/*******************************************************************************
 * File : app.hpp
 *
 * Description: Implementation of the App class, which manages the main
 * application loop, window creation, input handling, and camera controls.
 *
 * Why required: Core application framework that ties together windowing,
 * input, rendering, and user code. Essential for creating interactive 3D
 * visualization applications with geodraw.
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2025-12-21
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/app/app_module.hpp"
#include "geodraw/app/imgui_ctx.hpp"

#include <functional>
#include <optional>
#include <vector>
#include <set>
#include <string>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <filesystem>
#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif
#include "geodraw/external/glad/include/glad.h"
#include "geodraw/app/key.hpp"
struct GLFWwindow;
#include <glm/glm.hpp>

#include "geodraw/camera/camera.hpp"
#include "geodraw/renderer/renderer.hpp"
#include "geodraw/scene/scene.hpp"
#include "geodraw/gizmo/editing_context.hpp"
#include "geodraw/ui/tooltip_system.hpp"


namespace geodraw {

// Forward declaration
class App;

//==============================================================================
// CommandInfo - unified command descriptor used by MinorMode and App
//==============================================================================

/// Describes a registered command or toggle.
/// Used by MinorMode, App internal storage, and the getCommands() public API.
struct GEODRAW_API CommandInfo {
  int key = -1;       // Hotkey (-1 = none)
  int mods = 0;       // Required modifier keys (geodraw::Mod::* flags, 0 = none)
  std::string name;
  std::string docstring;
  std::string type;               // "command" or "toggle"
  std::function<void()> callback;
  bool* togglePtr = nullptr;      // For toggles: pointer to the bool variable
};

//==============================================================================
// MinorMode - Emacs-style keybinding mode that can be activated/deactivated
//==============================================================================

/**
 * A minor mode provides a set of keybindings that take precedence over global
 * bindings when the mode is active. Multiple minor modes can be active
 * simultaneously; precedence is determined by activation order (last-activated
 * wins, like Emacs).
 */
struct GEODRAW_API MinorMode {
  std::string name;
  std::vector<CommandInfo> commands;

  // Note: active state is tracked by App via activeMinorModes_ list
};

//==============================================================================
// PluginPanelInfo - Plugin ImGui panel registration for automated show/hide
//==============================================================================

/// Describes a plugin's ImGui panel. Registered via App::registerPluginPanel().
/// State fields are managed by ImGuiPlugin::drawPluginsPanel().
struct GEODRAW_API PluginPanelInfo {
    std::string name;                              ///< Display name (title bar + checklist)
    MinorMode*  mode        = nullptr;             ///< Associated minor mode (may be null)
    std::function<void(ImGuiCtx)> drawContents;    ///< Draw panel contents (no Begin/End)
    bool panelOpen   = true;  ///< Persisted open/hidden state (managed by ImGuiPlugin)
    bool wasExpanded = true;  ///< Was window expanded last frame? (collapse detection)
    bool forceExpand = false; ///< Force-expand on next draw (set after checkbox re-enables)
};

//==============================================================================
// ScenePair - Normal scene + editable scene with shared EditingContext
//==============================================================================

/**
 * A scene pair manages two scenes:
 * - normal: The scene used for normal rendering
 * - editable: The scene used when edit mode is active (shows proxies + gizmos)
 *
 * The EditingContext coordinates selection, gizmos, and editing state.
 */
struct ScenePair {
  Scene normal;              // User populates this via app.scene()
  Scene editable;            // Used for edit mode rendering
  EditingContext editCtx;    // Editing state for this scene
  bool editingEnabled = false;  // Whether editing is enabled for this scene
  std::string name;          // Optional name for this scene pair
  Camera camera;             // Per-scene camera (opt-in via App::sceneCamera())
  bool useDedicatedCamera = false;  // If true, use camera instead of App::camera

  ScenePair(const std::string& n = "default") : name(n) {
    // Connect normal scene to editing context for auto-proxy registration
    normal.setEditingContext(&editCtx);
  }
};

// Ruler state for distance measurement
struct RulerState {
  bool active = false;
  glm::vec3 tailWorld;     // World space position of ruler start
  glm::vec3 tipWorld;      // World space position of ruler end
  bool tipSnapped = false; // Whether tip is snapped to geometry
};

// Input mode for M-x command palette
enum class InputMode {
  NORMAL, // Process hotkeys normally
  COMMAND // M-x mode: capture text input
};

class GEODRAW_API App {
public:
  Camera camera;

  using DrawCallback = std::function<void()>;
  using UpdateCallback = std::function<void(float dt)>;
  using KeyCallback = std::function<void(int key, int action, int mods)>;
  using InputFocusCallback = std::function<bool()>;  // Returns true if UI wants input capture

  App(int width, int height, const char *title);
  ~App();

  // Set user callbacks (single-slot, priority 0 — backward compat)
  // @deprecated Use addDrawCallback(cb, priority) / addUpdateCallback(cb, priority) instead.
  // set* replaces ALL existing priority-0 entries; a plugin that calls set* after add*
  // will silently remove other plugins' callbacks.
  [[deprecated("Use addDrawCallback(cb, priority) instead; set* replaces ALL priority-0 entries")]]
  void setDrawCallback(DrawCallback cb);
  [[deprecated("Use addUpdateCallback(cb, priority) instead; set* replaces ALL priority-0 entries")]]
  void setUpdateCallback(UpdateCallback cb);
  void setInputFocusCallback(InputFocusCallback cb);

  // Multi-callback support: add additional callbacks at an explicit priority.
  // Lower priority values execute first (Priority::BASE=0, OVERLAY=10, UI=20).
  // Multiple callbacks at the same priority execute in insertion order.
  void addUpdateCallback(UpdateCallback cb, int priority = Priority::BASE);
  void addDrawCallback(DrawCallback cb, int priority = Priority::BASE);

  // Convenience: call module.attach(*this)
  void addModule(IAppModule& module);

  // Set application docstring (displayed in help)
  void setDocstring(const std::string &doc);

  // Register keyboard and input callbacks (global commands)
  void addCmd(const char *commandName, std::function<void()> command,
              const char *docstring, int key = -1, int mods = 0, bool forceUpdate = true);
  void addToggle(const char *commandName, bool &variable, const char *docstring, int key = -1, int mods = 0, bool forceUpdate = true);

  // Register commands to a minor mode
  void addCmd(const char *commandName, std::function<void()> command,
              const char *docstring, int key, int mods, bool forceUpdate,
              MinorMode& mode);
  void addToggle(const char *commandName, bool &variable, const char *docstring,
                 int key, int mods, bool forceUpdate, MinorMode& mode);

  //==========================================================================
  // Minor Mode Management
  //==========================================================================

  /**
   * Create a named minor mode. Minor modes provide keybindings that take
   * precedence over global bindings when active.
   *
   * @param name The name of the minor mode (e.g., "shape-edit")
   * @return Reference to the created minor mode
   */
  MinorMode& createMinorMode(const std::string& name);

  /**
   * Activate a minor mode. Its keybindings will take precedence over
   * previously activated modes and global bindings.
   */
  void activateMinorMode(MinorMode& mode);

  /**
   * Deactivate a minor mode. Its keybindings will no longer be checked.
   */
  void deactivateMinorMode(MinorMode& mode);

  /**
   * Toggle a minor mode.
   */
  void toggleMinorMode(MinorMode& mode);

  /**
   * Check if a minor mode is currently active.
   */
  bool isMinorModeActive(const MinorMode& mode) const;

  /**
   * Get list of currently active minor modes (in activation order).
   */
  const std::vector<MinorMode*>& getActiveMinorModes() const;

  /**
   * Get all registered global commands.
   * Used by ImGui and help system to display available commands.
   */
  std::vector<CommandInfo> getCommands() const;

  /**
   * Get all registered minor modes (active or not).
   */
  const std::vector<std::unique_ptr<MinorMode>>& getMinorModes() const;

  /**
   * Register a plugin panel for automated show/hide management.
   * Called from plugin attach() to opt into ImGuiPlugin::drawPluginsPanel().
   */
  void registerPluginPanel(PluginPanelInfo info);

  /**
   * Get all registered plugin panels (managed by ImGuiPlugin::drawPluginsPanel).
   */
  std::vector<PluginPanelInfo>& getPluginPanels();

  void renderRuler(Scene &rulerScene);

  // Run the main loop
  void run();

  // Quit the application
  void quit();
  void showHelp();

  // Get window dimensions
  int getWidth() const { return width; }
  int getHeight() const { return height; }

  // Accessor methods for plugin pattern
  GLFWwindow *getWindow() const;
  DrawCallback getDrawCallback() const;

  // HiDPI-aware framebuffer size in physical pixels
  glm::ivec2 getFramebufferSize() const;
  // Cursor position in logical window coordinates
  glm::dvec2 getCursorPos() const;
  // True if mouse button is currently pressed (0=left, 1=right, 2=middle)
  bool isMouseButtonPressed(int button) const;
  Renderer &getRenderer() { return *renderer; }

  // Ruler control
  const RulerState &getRulerState() const { return rulerState; }

  // Request a forced update on next frame (for ImGui interactions)
  void requestUpdate() { forceUpdate = true; }

  // Request continuous updates until explicitly cancelled.
  // The application redraws every frame while any reason is active.
  // Multiple independent callers can request/cancel without interfering —
  // the app keeps running as long as at least one reason is registered.
  //
  // Suggested reason strings (dot-separated namespacing):
  //   "scenario.playback", "scenario.edit_mode", "scenario.proxy_hover"
  //   "camera_trajectory.playback", "video_capture"
  void requestContinuousUpdate(const char* reason);

  // Cancel a previously registered continuous-update reason.
  // No-op if the reason was not registered.
  void cancelContinuousUpdate(const char* reason);

  //==========================================================================
  // Scene Pair Management
  //==========================================================================

  /**
   * Add a scene pair (normal scene + editable scene).
   * Most applications only need one scene pair.
   *
   * @param name Optional name for the scene pair
   * @return Index of the added scene pair
   */
  size_t addScene(const std::string& name = "default");

  /**
   * Get the normal scene for a scene pair.
   * Users populate this scene via Add() calls.
   * When editing is enabled, Shape3 objects are auto-registered as proxies.
   *
   * @param index Scene pair index (default: 0)
   * @return Reference to the normal scene
   */
  Scene& scene(size_t index = 0);

  /**
   * Get a scene pair by index.
   *
   * @param index Scene pair index
   * @return Reference to the scene pair
   */
  ScenePair& getScenePair(size_t index = 0);

  /**
   * Get number of scene pairs.
   */
  size_t sceneCount() const { return scenePairs_.size(); }

  // Per-scene viewport region (framebuffer coordinates, Y from bottom).
  struct SceneViewport {
    int fbX, fbY;  // OpenGL origin (Y from bottom-left of framebuffer)
    int fbW, fbH;
  };

  /// Framebuffer viewports for all scene pairs, as populated by the last
  /// renderScenePairs() call. Empty when not in grid mode or before first frame.
  const std::vector<SceneViewport>& getSceneViewports() const {
    return sceneViewports_;
  }

  /**
   * Activate N×M grid layout. When rows > 0 and cols > 0, scenePairs are
   * rendered into a grid of sub-viewports instead of the full window.
   */
  void setSceneGrid(int rows, int cols);

  /**
   * Enable and return the per-scene camera for scene pair at index.
   * Once called, renderScenePair uses this camera instead of App::camera.
   */
  Camera& sceneCamera(size_t index = 0);

  //==========================================================================
  // Editing Support
  //==========================================================================

  /**
   * Enable editing support for a scene pair.
   * When enabled:
   * - Registers TAB (toggle edit mode), G (cycle gizmo), ENTER (commit), ESC (cancel)
   * - Handles mouse input for object selection and gizmo manipulation
   * - In edit mode, the editable scene (proxies + gizmos) is rendered instead of normal
   *
   * @param sceneIndex Scene pair index (default: 0)
   */
  void enableEditing(size_t sceneIndex = 0);

  /**
   * Check if editing support is enabled for any scene pair
   */
  bool isEditingEnabled() const { return editingEnabled_; }

  /**
   * Check if editing is enabled for a specific scene pair
   */
  bool isEditingEnabled(size_t sceneIndex) const;

  /**
   * Get the editing context for a scene pair
   *
   * @param sceneIndex Scene pair index (default: 0)
   */
  EditingContext& editingContext(size_t sceneIndex = 0);

  //==========================================================================
  // Tooltip Support
  //==========================================================================

  /**
   * Get the application's tooltip system for registering providers.
   * Lazy-creates the TooltipSystem on first access.
   *
   * Usage:
   *   app.tooltips().registerProvider("Vehicle", [](const TooltipContext& ctx) {
   *       return TooltipContent::makeText("Vehicle " + std::to_string(ctx.objectId));
   *   });
   */
  TooltipSystem& tooltips();

  bool verbose = false;   // Set to true to get verbose printouts.
  bool queryQuit = false; // Set to true to avoid quitting Gui by mistake.

  // Perform depth-based picking at screen position
  // Returns world position if valid geometry hit, nullopt otherwise
  std::optional<glm::vec3> depthBasedPicking(float fbMouseX, float fbMouseY,
                                             int fbWidth, int fbHeight,
                                             bool verbose = true);

private:
  bool initialize();
  void mainLoop();
  void processInput();

  // Per-scene viewport region (framebuffer coordinates, Y from bottom).
  // Rebuilt each frame in renderScenePairs().
  std::vector<SceneViewport> sceneViewports_;

  // Active camera context: which camera + viewport to use for input/picking.
  struct ActiveCameraCtx {
    Camera* cam;        // Camera to drive (per-scene or App::camera)
    int vpX, vpY;       // Viewport origin in framebuffer OpenGL coords (Y from bottom)
    int vpW, vpH;       // Viewport dimensions in framebuffer pixels
    size_t sceneIndex;  // Scene index (SIZE_MAX = using global camera)
  };

  // Returns the viewport-relative camera context for a given scene index.
  ActiveCameraCtx buildActiveCameraCtx(size_t sceneIndex, int fbW, int fbH) const;

  // Returns scene index whose viewport contains (fbX, fbY) in top-origin fb coords.
  // fbH is the total framebuffer height for Y-flip. Returns nullopt if outside all viewports.
  std::optional<size_t> getSceneIndexAtCursor(double fbX, double fbY, int fbW, int fbH) const;

  void handleDoubleClick(double mouseX, double mouseY, const ActiveCameraCtx& ctx);

  void registerKeyCallback(KeyCallback cb);

  // Read depth value from framebuffer at specified position
  // Returns std::nullopt if depth read fails or indicates background
  std::optional<float> readDepthAtPosition(float fbX, float fbY);

  // Viewport-aware picking: uses per-scene camera and viewport-local coordinates.
  std::optional<glm::vec3> depthBasedPickingInViewport(
      float fbMouseX, float fbMouseY,
      const ActiveCameraCtx& ctx,
      int fbWidth, int fbHeight,
      bool verbose = false);

  GLFWwindow *window = nullptr;
  int width, height;
  std::string title;  // Displayed in app window frame

  // Window state persistence (logical/screen-coordinate size+pos, saved on quit)
  std::filesystem::path windowStatePath_;
  int winLogicalW_ = 0, winLogicalH_ = 0;
  int winPosX_ = INT_MIN, winPosY_ = INT_MIN; // INT_MIN = no saved position
  void loadWindowState();
  void saveWindowState();

  std::unique_ptr<Renderer> renderer;

  // Callback storage (sorted by priority, ascending)
  struct UpdateCallbackEntry { int priority; UpdateCallback cb; };
  struct DrawCallbackEntry   { int priority; DrawCallback cb; };
  std::vector<UpdateCallbackEntry> updateCallbacks_;
  std::vector<DrawCallbackEntry>   drawCallbacks_;

  // Keyboard and input callbacks
  std::vector<KeyCallback> keyCallbacks;
  InputFocusCallback inputFocusCallback_ = nullptr;  // Set by ImGuiPlugin
  float scrollDelta = 0.0f;
  float scrollDeltaX = 0.0f;  // Horizontal scroll for two-finger swipe


  // Command tracking
  std::set<int> registeredKeys;
  std::vector<CommandInfo> commands;
  std::unordered_map<std::string, size_t> commandNameIndex; // M-x lookup
  std::unordered_map<std::string, bool*> togglePointers_;   // Toggle variable pointers
  std::string docstring; // Application description (displayed in help)

  // Minor mode storage
  std::vector<std::unique_ptr<MinorMode>> minorModes_;
  std::vector<MinorMode*> activeMinorModes_;  // Activation order (last = highest priority)
  std::set<std::string> minorModeCommandNames_;  // Names of all commands registered to any minor mode

  // Plugin panel registry (populated by registerPluginPanel, read by ImGuiPlugin)
  std::vector<PluginPanelInfo> pluginPanels_;

  // Dispatch a key event through minor modes and global commands
  // Returns true if the event was consumed
  bool dispatchKeyEvent(int key, int action, int mods);

  // M-x command palette state
  InputMode inputMode = InputMode::NORMAL;
  std::string commandBuffer;
  bool escPressed = false; // Track ESC press for ESC-x sequence (GUI path)
  bool stdinEscArmed_ = false; // Track ESC for terminal path
#ifndef _WIN32
  struct termios originalTermios_{};
#endif

  // Mouse state tracking for input handling
  struct MouseState {
    double lastX = 0.0, lastY = 0.0;
    double currentX = 0.0, currentY = 0.0;
    double dragStartX = 0.0, dragStartY = 0.0;      // For threshold detection
    double accumulatedDragX = 0.0;                   // Accumulated horizontal drag
    double accumulatedDragY = 0.0;                   // Accumulated vertical drag
    bool leftButtonPressed = false;
    bool rightButtonPressed = false;
    double lastClickTime = 0.0;

    // Pan anchor tracking
    bool hasAnchorPoint = false;
    glm::vec3 anchorWorldPoint{0.0f};    // ORBIT mode anchor
    glm::dvec3 anchorECEF{0.0};          // GLOBE mode anchor

    // Velocity tracking for pan momentum
    glm::vec2 smoothedVelocity{0.0f};    // Smoothed velocity (pixels/second)
    double lastDragTime = 0.0;            // Time of last drag update
    bool wasDraggingInPanMode = false;    // Track if we were dragging in PAN mode

    std::optional<size_t> dragSceneIndex;  // Locked scene during active drag
  } mouseState;

  // Ruler state for distance measurement
  RulerState rulerState;
  Scene rulerScene_;  // Persistent ruler overlay scene (member to avoid dangling pointer in renderer)

  void updateRulerTip(); // Update ruler tip with pixel-space snapping

  // M-x command palette methods
  void enterCommandMode();
  void cancelCommandMode();
  void executeCommand(const std::string &cmd);
  void enableRawTerminal();
  void disableRawTerminal();
  void pollStdin();
  void handleStdinChar(char c);
  void dispatchStdinChar(char c);
  void searchCommands(const std::string &query); // Search and list commands
  std::string keyToString(int key);              // Convert GLFW key to string
  std::string generateCommandName(const std::string &docstring);

  double lastTime = 0.0;
  bool forceUpdate = true;
  bool autoFramingDone_ = false;  // Track if initial auto-frame has been done

  // Active continuous-update reasons (see requestContinuousUpdate/cancelContinuousUpdate).
  // The app redraws every frame while this set is non-empty.
  std::set<std::string> continuousUpdateReasons_;

  // Depth values <= this threshold are considered background (Reverse-Z: 0 = far/background)
  static constexpr float BACKGROUND_DEPTH_THRESHOLD = 0.0001f;

  // Ruler configuration
  static constexpr float RULER_SNAP_THRESHOLD_PIXELS = 3.0f;

  // PAN mode switch threshold
  static constexpr float PAN_SWITCH_THRESHOLD_PIXELS = 30.0f;

  //==========================================================================
  // Scene Pair Management
  //==========================================================================

  std::vector<ScenePair> scenePairs_;
  size_t activeSceneIndex_ = 0;  // Currently active scene for editing
  int gridRows_ = 0;  // 0 = no grid (full-window mode)
  int gridCols_ = 0;

  // Render all scene pairs (called from run())
  void renderScenePairs();

  // Render a single scene pair
  void renderScenePair(size_t index);

  //==========================================================================
  // Editing State
  //==========================================================================

  bool editingEnabled_ = false;  // True if any scene has editing enabled

  // Track mouse state for edit mode
  bool editMouseWasPressed_ = false;

  // Internal methods for editing
  void registerEditingCommands();
  void handleEditModeInput(ScenePair& pair);
  void registerProxiesFromScene(ScenePair& pair);

  //==========================================================================
  // Tooltip State
  //==========================================================================

  std::unique_ptr<TooltipSystem> tooltipSystem_;  // Lazy-created tooltip system
};

} // namespace geodraw
