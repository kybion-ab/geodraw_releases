/*******************************************************************************
 * File: video_capture_plugin.hpp
 *
 * Description: IAppModule plugin that iterates through a
 * CameraTrajectoryPlugin's baked poses, captures each rendered frame with
 * glReadPixels, and saves PNG frames to disk.  A separate FFmpeg invocation
 * (shown in the ImGui panel) combines them into a video.
 *
 * Usage:
 *   CameraTrajectoryPlugin camTraj;
 *   VideoCapturePlugin videoCap(camTraj);
 *   app.addModule(camTraj);
 *   app.addModule(videoCap);
 *
 *   // In ImGui callback:
 *   videoCap.drawImGuiPanel(app, ImGui::GetCurrentContext());
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2026-02-20
 *
 *******************************************************************************/

#pragma once

#include "geodraw/app/app_module.hpp"
#include "geodraw/export/export.hpp"
#include "geodraw/modules/camera_trajectory/camera_trajectory_plugin.hpp"

#include <future>
#include <string>
#include <vector>

namespace geodraw {

class App;
class ScenarioPlugin;
struct MinorMode;

/**
 * Plugin that captures video frames by stepping through a
 * CameraTrajectoryPlugin's baked poses and writing PNG files with glReadPixels.
 */
class GEODRAW_API VideoCapturePlugin : public IAppModule {
public:
    explicit VideoCapturePlugin(CameraTrajectoryPlugin& camTraj);

    void attach(App& app) override;
    void detach(App& app) override;

    /// Link a ScenarioPlugin for timestep synchronization during capture.
    void setScenarioSync(ScenarioPlugin& scenario);

#ifdef GEODRAW_HAS_IMGUI
    /**
     * Draw the video capture ImGui control panel.
     * Call inside an imgui.setImGuiCallback() lambda.
     * Pass ImGui::GetCurrentContext() as imguiCtx.
     */
    void drawImGuiPanel(App& app, void* imguiCtx = nullptr);
#endif

    MinorMode* getMinorMode() { return minorMode_; }

private:
    void onUpdate(App& app, float dt);
    void onDraw(App& app, bool afterImGui);
    void startCapture(App& app);
    void stopCapture(App& app);
    void captureFrame(App& app);

    CameraTrajectoryPlugin& camTraj_;
    MinorMode* minorMode_ = nullptr;

    ScenarioPlugin* scenario_              = nullptr;
    bool            syncScenario_          = true;
    bool            savedScenarioPlaying_  = false;
    int             savedScenarioTimestep_ = 0;

    bool isCapturing_                 = false;
    int  currentPoseIndex_            = 0;
    int  stabilizationFramesRemaining_ = 0;
    std::vector<CameraKeyframe> capturePoses_;

    // Settings (editable in ImGui)
    char  outputDir_[256]      = "./frames";
    char  filenamePrefix_[64]  = "frame";
    float fps_                 = 30.0f;
    int   stabilizationFrames_ = 2;
    bool  captureWithImGui_    = false;  // true: capture after ImGui renders
    char  outputMp4_[512]      = "output.mp4";

    // Async PNG write state
    std::vector<std::future<void>> pendingWrites_;

    // Video creation state
    std::future<int> ffmpegFuture_;
    std::string      ffmpegStatusMsg_;

    // Status
    enum class Status { Idle, Capturing, Done, Error };
    Status      status_        = Status::Idle;
    std::string statusMessage_;
    int         totalFramesSaved_ = 0;

    // Standalone panel open/expand state
    bool panelOpen_   = true;
    bool wasExpanded_ = true;

#ifdef GEODRAW_HAS_IMGUI
    void drawImGuiPanelImpl(App& app);
    /// Draw panel contents only (no Begin/End). Used by both standalone and registry paths.
    void drawPanelContents(App& app);
#endif
};

} // namespace geodraw
