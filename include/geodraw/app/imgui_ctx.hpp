/*******************************************************************************
 * File: imgui_ctx.hpp
 *
 * Description: ImGuiCtx — typed carrier for an ImGui context pointer across
 * the shared-library boundary.
 *
 * Keeping this in its own header lets plugin headers include only this small
 * file instead of all of app.hpp.
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2026-03-10
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"

namespace geodraw {

/// Carries an ImGui context pointer across the shared-library boundary.
/// Construct with ImGui::GetCurrentContext() at the call site in the main
/// binary; plugin draw methods call ImGui::SetCurrentContext(ctx.ptr) before
/// any ImGui API calls.  Using a named type (rather than void*) prevents
/// accidentally passing an unrelated pointer.
struct GEODRAW_API ImGuiCtx {
  void* ptr = nullptr;
  ImGuiCtx() = default;
  explicit ImGuiCtx(void* p) : ptr(p) {}
};

} // namespace geodraw
