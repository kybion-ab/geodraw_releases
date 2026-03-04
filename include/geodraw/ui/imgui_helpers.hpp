/*******************************************************************************
 * File: imgui_helpers.hpp
 *
 * Description: Thin C++ wrappers around ImGui calls, compiled into libgeodraw.
 *
 * Purpose: Allows Python bindings (_geodraw.so) to call ImGui without
 * including ImGui headers or compiling imgui.cpp. This avoids the dual-GImGui
 * crash (see MEMORY.md: "ImGui Multi-Context Problem").
 *
 * All functions use plain C++ types only (no ImGui types in the interface).
 *
 *******************************************************************************/

#pragma once

#include "geodraw/export/export.hpp"
#include <string>
#include <utility>

namespace geodraw::imgui {

/// Begin a new ImGui window. Returns false if the window is collapsed/clipped.
GEODRAW_API bool begin(const char* title, bool* p_open = nullptr);

/// End an ImGui window. Always call after begin().
GEODRAW_API void end();

/// Display plain text (no format string processing).
GEODRAW_API void text(const char* s);

/// Display a button. Returns true when clicked.
GEODRAW_API bool button(const char* label);

/// Checkbox widget. Returns {changed, new_value}.
GEODRAW_API std::pair<bool, bool> checkbox(const char* label, bool v);

/// Horizontal float slider. Returns {changed, new_value}.
GEODRAW_API std::pair<bool, float> slider_float(const char* label, float v,
                                                 float v_min, float v_max);

/// Single-line text input (max 255 chars). Returns {changed, new_text}.
GEODRAW_API std::pair<bool, std::string> input_text(const char* label,
                                                      const std::string& s);

/// Horizontal separator line.
GEODRAW_API void separator();

/// Place next item on the same line as the previous item.
GEODRAW_API void same_line();

/// Set position of next window on first use.
GEODRAW_API void set_next_window_pos(float x, float y);

/// Set size of next window on first use.
GEODRAW_API void set_next_window_size(float w, float h);

/// Begin a tree node. Returns true if open; call tree_pop() if true.
GEODRAW_API bool tree_node(const char* label);

/// End a tree node opened by tree_node().
GEODRAW_API void tree_pop();

/// Collapsing section header. Returns true if open.
GEODRAW_API bool collapsing_header(const char* label);

/// Add vertical spacing.
GEODRAW_API void spacing();

/// Horizontal integer slider. Returns {changed, new_value}.
GEODRAW_API std::pair<bool, int> slider_int(const char* label, int v,
                                              int v_min, int v_max);

/// Integer input field. Returns {changed, new_value}.
GEODRAW_API std::pair<bool, int> input_int(const char* label, int v);

/// Begin a disabled (greyed-out) section. Nest with end_disabled().
GEODRAW_API void begin_disabled();

/// End a disabled section opened by begin_disabled().
GEODRAW_API void end_disabled();

/// Display colored text. r/g/b in [0, 1].
GEODRAW_API void text_colored(const char* s, float r, float g, float b);

/// Display greyed-out (disabled) text.
GEODRAW_API void text_disabled(const char* s);

} // namespace geodraw::imgui
