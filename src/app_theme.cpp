#include "app_theme.hpp"
#include <dwmapi.h>
#include <uxtheme.h>

namespace Theme {

int Scale(int val) {
    return MulDiv(val, g_app.dpi, 96);
}

void UpdateDpi(HWND hwnd) {
    typedef UINT(WINAPI* PFN_GetDpiForWindow)(HWND);
    static auto pfnGetDpi = (PFN_GetDpiForWindow)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    if (pfnGetDpi && hwnd) {
        g_app.dpi = (int)pfnGetDpi(hwnd);
    } else {
        HDC hdc = GetDC(hwnd);
        if (hdc) {
            g_app.dpi = GetDeviceCaps(hdc, LOGPIXELSY);
            ReleaseDC(hwnd, hdc);
        } else {
            g_app.dpi = 96;
        }
    }
}

static HFONT CreateAppFont(int pointSize, int weight, const wchar_t* primaryFace) {
    int height = -MulDiv(pointSize, g_app.dpi, 72);
    HFONT hFont = CreateFontW(
        height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, VARIABLE_PITCH | FF_SWISS, primaryFace
    );
    return hFont;
}

void Init(HWND hwnd) {
    UpdateDpi(hwnd);

    if (g_app.brushBackground) DeleteObject(g_app.brushBackground);
    if (g_app.brushPanel) DeleteObject(g_app.brushPanel);
    if (g_app.brushField) DeleteObject(g_app.brushField);

    g_app.brushBackground = CreateSolidBrush(COLOR_BG);
    g_app.brushPanel = CreateSolidBrush(COLOR_PANEL);
    g_app.brushField = CreateSolidBrush(COLOR_FIELD);

    if (g_app.fontNormal) DeleteObject(g_app.fontNormal);
    if (g_app.fontTitle) DeleteObject(g_app.fontTitle);
    if (g_app.fontStatus) DeleteObject(g_app.fontStatus);

    g_app.fontNormal = CreateAppFont(10, FW_NORMAL, L"Segoe UI Variable Text");
    g_app.fontTitle = CreateAppFont(12, FW_SEMIBOLD, L"Segoe UI Variable Display");
    g_app.fontStatus = CreateAppFont(9, FW_NORMAL, L"Segoe UI");
}

void Cleanup() {
    if (g_app.brushBackground) { DeleteObject(g_app.brushBackground); g_app.brushBackground = nullptr; }
    if (g_app.brushPanel) { DeleteObject(g_app.brushPanel); g_app.brushPanel = nullptr; }
    if (g_app.brushField) { DeleteObject(g_app.brushField); g_app.brushField = nullptr; }

    if (g_app.fontNormal) { DeleteObject(g_app.fontNormal); g_app.fontNormal = nullptr; }
    if (g_app.fontTitle) { DeleteObject(g_app.fontTitle); g_app.fontTitle = nullptr; }
    if (g_app.fontStatus) { DeleteObject(g_app.fontStatus); g_app.fontStatus = nullptr; }
}

void ApplyControlStyle(HWND hControl) {
    if (!hControl) return;
    SetWindowTheme(hControl, L"DarkMode_Explorer", nullptr);
    if (g_app.fontNormal) {
        SendMessageW(hControl, WM_SETFONT, (WPARAM)g_app.fontNormal, TRUE);
    }
}

void SetControlFont(HWND hControl, bool title) {
    if (!hControl) return;
    HFONT f = title ? g_app.fontTitle : g_app.fontNormal;
    if (f) SendMessageW(hControl, WM_SETFONT, (WPARAM)f, TRUE);
}

void EnableDarkTitleBar(HWND hwnd) {
    if (!hwnd) return;
    BOOL dark = TRUE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 (Windows 11 / Windows 10 20H1+), 19 on 1809
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark)))) {
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    }
}

} // namespace Theme
