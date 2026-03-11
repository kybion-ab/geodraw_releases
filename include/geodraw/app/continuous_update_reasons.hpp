/*******************************************************************************
 * File: continuous_update_reasons.hpp
 *
 * Description: Named constants for continuous-update reason strings.
 *
 * Use these constants with App::requestContinuousUpdate() /
 * App::cancelContinuousUpdate() to avoid silent typo bugs.
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2026-03-10
 *
 *******************************************************************************/

#pragma once

namespace geodraw {
namespace ContinuousUpdateReason {

inline constexpr const char* ScenarioPlayback      = "scenario.playback";
inline constexpr const char* ScenarioEditMode      = "scenario.edit_mode";
inline constexpr const char* ScenarioProxyHover    = "scenario.proxy_hover";
inline constexpr const char* CameraTrajectory      = "camera_trajectory.playback";
inline constexpr const char* VideoCapture          = "video_capture";
inline constexpr const char* AppEditMode           = "app.edit_mode";
inline constexpr const char* Ruler                 = "ruler";

} // namespace ContinuousUpdateReason
} // namespace geodraw
