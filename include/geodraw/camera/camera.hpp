/*******************************************************************************
 * File: camera.hpp
 *
 * Description: See camera.cpp.
 *
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include "geodraw/geometry/ray.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>

namespace geodraw {

struct GEODRAW_API CameraNearFrustum {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 front = {1.0f, 0.0f, 0.0f};      // normalized look direction
    glm::vec3 worldUp = {0.0, 0.0, 1.0};       // normalized world up vector
    glm::vec3 right = glm::cross(front, worldUp); // normalized up vector
    glm::vec3 up = glm::cross(right, front); // normalized up vector
    float fov = glm::radians(45.0f);  // in radians
    float focal_length = 1.0f;
};

// Note: The Camera class is dedicated solely for the viewer camera used to
// render the scene. To visualize "cameras" in the scene, use the CameraNearFrustum
// struct.
class GEODRAW_API Camera {
public:
  enum class CameraMode { ORBIT, GLOBE };
  enum class InteractionState { PIVOT, PAN };

  // Ray is defined in geodraw/geometry/ray.hpp; alias kept for backward compat.
  using Ray = geodraw::Ray;

  Camera(glm::vec3 position = {0.0f, 0.0f, 3.0f},
         glm::vec3 up = {0.0f, 0.0f, 1.0f}, float yaw = -90.0f,
         float pitch = 0.0f);

  glm::mat4 getViewMatrix() const;
  glm::mat4 getProjectionMatrix(float aspectRatio) const;

  // Mode switching
  void setMode(CameraMode newMode);
  CameraMode getMode() const { return mode; }

  // Orbit camera controls
  void orbitRotate(float deltaX, float deltaY);
  void orbitPan(float deltaX, float deltaY, float windowHeight);
  void orbitZoom(float deltaY);

  // Target/pivot point management
  void setTarget(const glm::vec3 &newTarget);
  glm::vec3 getTarget() const { return target; }

  // Auto-frame camera to fit bounding box in view
  void autoFrame(const glm::vec3 &bboxMin, const glm::vec3 &bboxMax,
                 float targetYaw = glm::radians(30.0f), float targetPitch = glm::radians(30.0f));

  // Ray casting for picking
  Ray getRayFromScreenPos(float screenX, float screenY, float screenWidth,
                          float screenHeight) const;

  // Unproject screen coordinates + depth value to world space
  // screenX, screenY: screen coordinates
  // depth: normalized depth [0,1] from depth buffer
  // screenWidth, screenHeight: screen dimensions
  glm::vec3 unprojectScreenPoint(float screenX, float screenY, float depth,
                                 float screenWidth, float screenHeight) const;

  // Project world coordinates to screen space
  // worldPos: position in world/camera-relative coordinates
  // screenWidth, screenHeight: screen dimensions
  // Returns: (screenX, screenY, depth) where depth is in [0,1] range
  //          Returns std::nullopt if point is behind camera or outside frustum
  std::optional<glm::vec3> projectToScreen(const glm::vec3& worldPos,
                                            float screenWidth, float screenHeight) const;

  // Camera mode
  CameraMode mode = CameraMode::ORBIT;

  // Interaction state (PIVOT vs PAN)
  InteractionState interactionState = InteractionState::PIVOT;

  void setInteractionState(InteractionState state) { interactionState = state; }
  InteractionState getInteractionState() const { return interactionState; }

  // Camera vectors (used by orbit and globe modes)
  glm::vec3 position;
  glm::vec3 front;
  glm::vec3 up;
  glm::vec3 right;
  glm::vec3 worldUp;

  // Orbit mode parameters
  glm::vec3 target = {0.0f, 0.0f, 0.0f};
  float distance = 0.0f; // 0 = auto-frame from geometry
  float orbitYaw = glm::radians(30.0f);    // radians
  float orbitPitch = glm::radians(30.0f);  // radians

  // Globe mode parameters
  // Target point on Earth's surface (latitude, longitude in degrees)
  double globeTargetLat = 0.0;   // Latitude in degrees (-90 to 90)
  double globeTargetLon = 0.0;   // Longitude in degrees (-180 to 180)
  double globeAltitude = 10000000.0;  // Camera altitude above target in meters
  float globeYaw = 0.0f;         // Horizontal orbit angle around target (radians)
  float globePitch = glm::radians(45.0f);  // Vertical angle in radians (0 = horizon, π/2 = directly above)

  // Globe mode controls
  void globeRotate(float deltaX, float deltaY);
  void globeZoom(float deltaY);
  void globePan(float deltaX, float deltaY, float screenHeight);
  void setGlobeTarget(double latitude, double longitude);
  void setGlobeAltitude(double altitude);

  // Anchor-based panning (point-under-cursor tracking)
  void orbitPanWithAnchor(const glm::vec3& anchorWorld,
                          const glm::vec3& currentScreenPos,
                          float screenWidth, float screenHeight);
  void globePanWithAnchor(const glm::dvec3& anchorECEF,
                          float deltaX, float deltaY,
                          float screenHeight);

  // Get view matrix for globe mode (returns camera-relative positions)
  glm::mat4 getGlobeViewMatrix() const;

  // Get camera ECEF position for globe mode
  glm::dvec3 getGlobeCameraECEF() const;

  // Get target ECEF position for globe mode
  glm::dvec3 getGlobeTargetECEF() const;

  // Input sensitivity
  float rotationSpeed = 0.008f;
#if defined(__APPLE__) && defined(__MACH__)
  float panSpeed = 0.9f;
#elif defined(__linux__)
  float panSpeed = 0.9f;
#else
    std::cerr << "Unsupported operating system" << std::endl;
    exit(1);
#endif
  float zoomSpeed = 0.0005f;

  // Zoom parameters
  float minZoomDistance = 0.1f;
  float maxZoomDistance = 1000.0f;

  // Globe zoom parameters (in meters)
  double minGlobeAltitude = 5.0;         // 5 meters minimum
  double maxGlobeAltitude = 50000000.0;    // 50,000 km maximum (well beyond Earth)

  // Shared parameters
  float fov = glm::radians(45.0f);  // radians
  float focal_length = 1.0f;
  float nearPlane = 0.5 * minZoomDistance;
  float farPlane = 2 * maxZoomDistance;

  //==========================================================================
  // Pan Momentum (critical damping for smooth deceleration)
  //==========================================================================

  // Time constant for exponential decay (seconds)
  static constexpr float PAN_MOMENTUM_TIME_CONSTANT = 0.5f;

  // Minimum velocity threshold to stop momentum (pixels/second)
  static constexpr float PAN_MOMENTUM_MIN_VELOCITY = 5.0f;

  // Pan velocity in screen-space (pixels/second)
  glm::vec2 panVelocity{0.0f, 0.0f};

  // Whether momentum is currently active
  bool panMomentumActive = false;

  // Apply pan momentum for one frame, decaying velocity
  // Returns true if momentum is still active
  bool applyPanMomentum(float dt, float screenHeight);

  // Set pan velocity and activate momentum
  void setPanVelocity(const glm::vec2& velocity);

  // Stop momentum immediately
  void stopPanMomentum();

  // Check if momentum is active and significant
  bool hasPanMomentum() const;

  //==========================================================================
  // High-Altitude Auto-Orientation (GLOBE mode PAN state)
  //==========================================================================

  // Altitude thresholds (meters) with hysteresis
  static constexpr double HIGH_ALT_ACTIVATION_THRESHOLD = 500000.0;   // 500 km
  static constexpr double HIGH_ALT_DEACTIVATION_THRESHOLD = 400000.0; // 400 km

  // Time constant for orientation convergence (seconds)
  static constexpr float HIGH_ALT_ORIENTATION_TIME_CONSTANT = 0.5f;

  // Hysteresis state flag
  bool highAltitudeOrientationActive = false;

  // Apply high-altitude auto-orientation for one frame
  // Returns true if still converging (needs continued updates)
  bool applyHighAltitudeOrientation(float dt);

  //==========================================================================
  // ECEF Position Support (for camera-relative rendering)
  //==========================================================================

  /**
   * ECEF position of the camera in meters.
   * Used for camera-relative rendering with Scene::setOrigin().
   * Update this when the camera moves in ECEF space.
   */
  glm::dvec3 ecefPosition{0.0, 0.0, 0.0};

  /**
   * Get ECEF position for use with Scene::setOrigin()
   */
  glm::dvec3 getECEFPosition() const { return ecefPosition; }

  /**
   * Set ECEF position directly
   */
  void setECEFPosition(const glm::dvec3& ecef) { ecefPosition = ecef; }

private:
  void updateCameraVectors();
};

} // namespace geodraw
