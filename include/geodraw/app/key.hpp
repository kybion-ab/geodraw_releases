/*******************************************************************************
 * File : key.hpp
 *
 * Description: GLFW-independent key and modifier constants for geodraw.
 * Values match GLFW_KEY_* and GLFW_MOD_* so casts to int are transparent.
 *
 * Author  : Magnus Nilsson, Kybion AB
 * Date    : 2026-03-05
 *
 *******************************************************************************/

#pragma once

namespace geodraw {

/// Keyboard key constants. Values match GLFW_KEY_* integers.
enum class Key : int {
    None      = -1,

    // Letters
    A = 65,  B = 66,  C = 67,  D = 68,  E = 69,  F = 70,
    G = 71,  H = 72,  I = 73,  J = 74,  K = 75,  L = 76,
    M = 77,  N = 78,  O = 79,  P = 80,  Q = 81,  R = 82,
    S = 83,  T = 84,  U = 85,  V = 86,  W = 87,  X = 88,
    Y = 89,  Z = 90,

    // Digits
    D0 = 48, D1 = 49, D2 = 50, D3 = 51, D4 = 52,
    D5 = 53, D6 = 54, D7 = 55, D8 = 56, D9 = 57,

    // Function keys
    F1  = 290, F2  = 291, F3  = 292, F4  = 293,
    F5  = 294, F6  = 295, F7  = 296, F8  = 297,
    F9  = 298, F10 = 299, F11 = 300, F12 = 301,

    // Navigation / editing
    Space     = 32,
    Escape    = 256,
    Enter     = 257,
    Tab       = 258,
    Backspace = 259,
    Insert    = 260,
    Delete    = 261,
    Right     = 262,
    Left      = 263,
    Down      = 264,
    Up        = 265,
    PageUp    = 266,
    PageDown  = 267,
    Home      = 268,
    End       = 269,

    // Punctuation
    Equal = 61,
    Minus = 45,
};

/// Modifier key bit-flags. Values match GLFW_MOD_* integers.
namespace Mod {
    inline constexpr int None  = 0;
    inline constexpr int Shift = 0x0001;
    inline constexpr int Ctrl  = 0x0002;
    inline constexpr int Alt   = 0x0004;
    inline constexpr int Super = 0x0008;
}

/// Convert Key enum to int for use with addCmd/addToggle key parameters.
inline constexpr int key(Key k) { return static_cast<int>(k); }

} // namespace geodraw
