#pragma once
#include "app_state.hpp"

namespace Theme {

constexpr COLORREF COLOR_BG      = RGB(28, 28, 30);
constexpr COLORREF COLOR_PANEL   = RGB(38, 38, 42);
constexpr COLORREF COLOR_FIELD   = RGB(50, 50, 56);
constexpr COLORREF COLOR_FG      = RGB(242, 242, 245);
constexpr COLORREF COLOR_MUTED   = RGB(160, 160, 170);
constexpr COLORREF COLOR_ACCENT  = RGB(0, 153, 220);
constexpr COLORREF COLOR_BORDER  = RGB(70, 70, 78);
constexpr COLORREF COLOR_SUCCESS = RGB(52, 199, 89);
constexpr COLORREF COLOR_DANGER  = RGB(255, 69, 58);

int Scale(int val);
void UpdateDpi(HWND hwnd);
void Init(HWND hwnd);
void Cleanup();
void ApplyControlStyle(HWND hControl);
void SetControlFont(HWND hControl, bool title = false);
void EnableDarkTitleBar(HWND hwnd);

} // namespace Theme
