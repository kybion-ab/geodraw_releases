/*******************************************************************************
 * File: camera_trajectory_plugin.hpp
 *
 * Description: IAppModule plugin for recording and playing back camera
 * trajectories with smooth SLERP interpolation between keyframes.
 *
 * Usage (embedded in any app):
 *   CameraTrajectoryPlugin camTraj;
 *   app.addModule(camTraj);
 *
 *   // In ImGui callback:
 *   if (camTraj.getMinorMode() && app.isMinorModeActive(*camTraj.getMinorMode()))
 *       camTraj.drawImGuiPanel(app.camera, app, ImGui::GetCurrentContext());
 *
 *   // Code-defined trajectory (optional):
 *   auto& kfs = camTraj.keyframes();
 *   kfs.push_back({0.0, {0,0,0}, 20.f, 0.f,               glm::radians(45.f), glm::radians(45.f)});
 *   kfs.push_back({3.0, {0,0,0}, 20.f, glm::radians(90.f), glm::radians(30.f), glm::radians(45.f)});
 *   camTraj.sortKeyframes();
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2026-02-20
 *
 *******************************************************************************/

#pragma once

#include "geodraw/app/app_module.hpp"
#include "geodraw/camera/camera.hpp"
#include "geodraw/export/export.hpp"
#include "geodraw/scene/scene.hpp"

#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

namespace geodraw {

class App;
struct MinorMode;

/**
 * A single camera pose keyframe in a trajectory.
 * Supports both ORBIT and GLOBE camera modes; the `mode` field acts as a
 * discriminant. ORBIT fields (target, distance, orbitYaw, orbitPitch) and
 * GLOBE fields (globeTargetLat/Lon, globeAltitude, globeYaw, globePitch) are
 * stored together. Only the fields matching `mode` are meaningful.
 */
struct GEODRAW_API CameraKeyframe {
    double    time        = 0.0;
    float     fov         = glm::radians(45.0f);

    // Mode discriminant
    Camera::CameraMode mode = Camera::CameraMode::ORBIT;

    // ORBIT mode fields
    glm::vec3 target      = {0.f, 0.f, 0.f};
    float     distance    = 10.0f;
    float     orbitYaw    = glm::radians(30.0f);
    float     orbitPitch  = glm::radians(30.0f);

    // GLOBE mode fields
    double globeTargetLat = 0.0;           // degrees
    double globeTargetLon = 0.0;           // degrees
    double globeAltitude  = 10000000.0;    // meters
    float  globeYaw       = 0.0f;          // radians
    float  globePitch     = glm::radians(45.0f); // radians

    /// Capture the current camera state as a keyframe at the given time.
    static CameraKeyframe fromCamera(double time, const Camera& cam);

    /// Apply this keyframe to a camera, triggering updateCameraVectors().
    void applyToCamera(Camera& camera) const;
};

/**
 * Interpolate between two keyframes at normalized t∈[0,1].
 * Orientation (yaw/pitch) is smoothed using SLERP via quaternions.
 * Target, distance, and FOV use plain LERP.
 */
GEODRAW_API CameraKeyframe interpolateKeyframes(const CameraKeyframe& a,
                                                const CameraKeyframe& b,
                                                double t);

/**
 * Catmull-Rom spline interpolation across four keyframes.
 * Gives C¹-continuous (smooth velocity) paths through all control points.
 * prev/next are the neighbours outside the [a,b] segment; pass a/b
 * themselves at the trajectory boundaries for clamped behaviour.
 */
GEODRAW_API CameraKeyframe interpolateKeyframesSpline(const CameraKeyframe& prev,
                                                       const CameraKeyframe& a,
                                                       const CameraKeyframe& b,
                                                       const CameraKeyframe& next,
                                                       double t);

/**
 * Plugin for recording and playing back camera trajectories.
 *
 * Attach to an App via app.addModule() to register callbacks and keybindings.
 * The minor mode "CameraTrajectory" is created during attach() and can be
 * activated/deactivated independently from other modes.
 */
class GEODRAW_API CameraTrajectoryPlugin : public IAppModule {
public:
    CameraTrajectoryPlugin();
    ~CameraTrajectoryPlugin() override;

    CameraTrajectoryPlugin(const CameraTrajectoryPlugin&) = delete;
    CameraTrajectoryPlugin& operator=(const CameraTrajectoryPlugin&) = delete;

    //=========================================================================
    // IAppModule interface
    //=========================================================================

    void attach(App& app) override;
    void detach(App& app) override;

    //=========================================================================
    // ImGui panel
    //=========================================================================

#ifdef GEODRAW_HAS_IMGUI
    /**
     * Draw the camera trajectory ImGui control panel.
     * Call this inside an imgui.setImGuiCallback() lambda.
     * Pass ImGui::GetCurrentContext() as imguiCtx so the shared library
     * can call ImGui::SetCurrentContext() before making any ImGui calls.
     */
    void drawImGuiPanel(Camera& camera, App& app, void* imguiCtx = nullptr);
#endif

    //=========================================================================
    // Minor mode access
    //=========================================================================

    /// Minor mode created during attach() for keybindings.
    /// Returns nullptr before attach() is called.
    MinorMode* getMinorMode() { return minorMode_; }
    const MinorMode* getMinorMode() const { return minorMode_; }

    //=========================================================================
    // Keyframe management
    //=========================================================================

    std::vector<CameraKeyframe>& keyframes() { return keyframes_; }
    const std::vector<CameraKeyframe>& keyframes() const { return keyframes_; }

    /// Sort keyframes by time. Call after bulk insertion.
    void sortKeyframes();

    /// Total duration of the trajectory (time of last keyframe).
    double duration() const;

    /// Densify the trajectory by sampling at 1/fps intervals using SLERP.
    /// The resulting bakedPoses() are used for frustum visualization.
    /// Call after editing keyframes or changing FPS.
    void bake();

    const std::vector<CameraKeyframe>& bakedPoses() const { return bakedPoses_; }

    //=========================================================================
    // FPS
    //=========================================================================

    float fps() const { return fps_; }
    void  setFps(float fps) { fps_ = fps; bakedStale_ = true; }

    //=========================================================================
    // CSV persistence
    //=========================================================================

    /// Save keyframes to a CSV file.
    /// Format: time,target_x,target_y,target_z,distance,orbit_yaw,orbit_pitch,fov
    bool saveToCSV(const std::string& path) const;

    /// Load keyframes from a CSV file. Clears existing keyframes.
    bool loadFromCSV(const std::string& path);

    //=========================================================================
    // Playback state
    //=========================================================================

    bool  isPlaying()     const { return isPlaying_; }
    double currentTime()  const { return currentTime_; }

    void setPlaying(bool playing) { isPlaying_ = playing; }
    void setCurrentTime(double t);
    void setCapturing(bool v) { captureInProgress_ = v; }

private:
    /// View mode for the camera frustum visualization.
    enum class FrustumViewMode {
        FollowCamera, ///< Viewer camera follows the trajectory (no frustum drawn).
        Bystander,    ///< Viewer camera is free; all baked poses shown, active highlighted.
    };

    std::vector<CameraKeyframe> keyframes_;
    std::vector<CameraKeyframe> bakedPoses_;  ///< Dense poses produced by bake().
    MinorMode* minorMode_ = nullptr;

    // Playback state
    bool   isPlaying_         = false;
    double currentTime_       = 0.0;
    float  playbackSpeed_     = 1.0f;
    bool   loop_              = true;
    bool   showVisualization_ = true;
    bool   captureInProgress_ = false;

    // Recording state
    float fps_             = 60.0f;
    float recordTime_      = 0.0f;
    bool  autoAdvanceTime_ = true;
    float autoAdvanceStep_ = 1.0f;
    int   selectedKeyframe_ = -1;

    // Display state
    FrustumViewMode frustumViewMode_ = FrustumViewMode::FollowCamera;
    bool            bakedStale_      = false; ///< True when keyframes/FPS changed since last bake.

    // File I/O state (ImGui)
    char csvPath_[256] = "trajectory.csv";

    Scene scene_;  ///< Private scene rebuilt in onUpdate; rendered in onDraw.

    // Standalone panel open/expand state (used by drawImGuiPanelImpl for direct use)
    bool panelOpen_   = true;
    bool wasExpanded_ = true;

    void onUpdate(App& app, float dt);
    void onDraw(App& app);
    void rebuildScene(const Camera& cam);

#ifdef GEODRAW_HAS_IMGUI
    void drawImGuiPanelImpl(Camera& camera, App& app);
    /// Draw panel contents only (no Begin/End). Used by both standalone and registry paths.
    void drawPanelContents(Camera& camera, App& app);
#endif

    /// Sample the trajectory at time t (clamps outside range).
    CameraKeyframe sampleAt(double t) const;
};

} // namespace geodraw
