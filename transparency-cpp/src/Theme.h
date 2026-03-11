#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <uxtheme.h>
#include <dwmapi.h>
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

// ─── Color Palette (Premium Obsidian) ───────────────────────────────────────
namespace Theme {

// Backgrounds
constexpr COLORREF BG_APP       = RGB(8,   10,  14);   // #080a0e
constexpr COLORREF BG_SIDEBAR   = RGB(12,  16,  24);   // #0c1018
constexpr COLORREF BG_CARD      = RGB(20,  26,  38);   // #141a26
constexpr COLORREF BG_ELEVATED  = RGB(30,  38,  56);   // #1e2638
constexpr COLORREF BG_INPUT     = RGB(18,  24,  36);   // #121824
constexpr COLORREF BG_ROW_ALT   = RGB(13,  17,  25);   // #0d1119
constexpr COLORREF BG_ROW_HOV   = RGB(36,  45,  67);   // #242d43
constexpr COLORREF BG_ROW_SEL   = RGB(44,  61,  94);   // #2c3d5e

// Borders / separators
constexpr COLORREF BORDER         = RGB(42,  51,  72);  // #2a3348
constexpr COLORREF SIDEBAR_BORDER = RGB(52,  63,  86);  // #343f56

// Text
constexpr COLORREF TEXT_PRIMARY   = RGB(236, 242, 250); // #ecf2fa
constexpr COLORREF TEXT_SECONDARY = RGB(178, 190, 210); // #b2bed2
constexpr COLORREF TEXT_MUTED     = RGB(126, 140, 165); // #7e8ca5

// Semantic / accent colors
constexpr COLORREF ACCENT         = RGB(86,  142, 255); // #568eff
constexpr COLORREF ACCENT_GLOW    = RGB(130, 176, 255); // #82b0ff
constexpr COLORREF SUCCESS        = RGB(63,  204, 145); // #3fcc91
constexpr COLORREF DANGER         = RGB(255, 96,  122); // #ff607a
constexpr COLORREF WARNING        = RGB(232, 185, 75);  // #e8b94b
constexpr COLORREF WATCHLIST      = RGB(170, 129, 255); // #aa81ff

// ─── Brush Cache ─────────────────────────────────────────────────────────────
inline HBRUSH BrushApp()       { static HBRUSH b = CreateSolidBrush(BG_APP);       return b; }
inline HBRUSH BrushSidebar()   { static HBRUSH b = CreateSolidBrush(BG_SIDEBAR);   return b; }
inline HBRUSH BrushCard()      { static HBRUSH b = CreateSolidBrush(BG_CARD);      return b; }
inline HBRUSH BrushRowAlt()    { static HBRUSH b = CreateSolidBrush(BG_ROW_ALT);   return b; }
inline HBRUSH BrushRowHov()    { static HBRUSH b = CreateSolidBrush(BG_ROW_HOV);   return b; }
inline HBRUSH BrushRowSel()    { static HBRUSH b = CreateSolidBrush(BG_ROW_SEL);   return b; }
inline HBRUSH BrushAccent()    { static HBRUSH b = CreateSolidBrush(ACCENT);       return b; }
inline HBRUSH BrushAccentGlow(){ static HBRUSH b = CreateSolidBrush(ACCENT_GLOW);  return b; }
inline HBRUSH BrushBorder()    { static HBRUSH b = CreateSolidBrush(BORDER);       return b; }
inline HBRUSH BrushSuccess()   { static HBRUSH b = CreateSolidBrush(SUCCESS);      return b; }
inline HBRUSH BrushWarning()   { static HBRUSH b = CreateSolidBrush(WARNING);      return b; }
inline HBRUSH BrushDanger()    { static HBRUSH b = CreateSolidBrush(DANGER);       return b; }
inline HBRUSH BrushWatchlist() { static HBRUSH b = CreateSolidBrush(WATCHLIST);    return b; }
inline HBRUSH BrushInput()     { static HBRUSH b = CreateSolidBrush(BG_INPUT);     return b; }
inline HBRUSH BrushNull()      { static HBRUSH b = (HBRUSH)GetStockObject(NULL_BRUSH); return b; }

// ─── Font Helpers ─────────────────────────────────────────────────────────────
inline int DpiY() {
    HDC hdc = GetDC(nullptr);
    if (!hdc) return 96;
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);
    return dpi > 0 ? dpi : 96;
}

inline HFONT MakeUiFont(int pointSize, int weight) {
    return CreateFontW(
        -MulDiv(pointSize, DpiY(), 72),
        0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Variable");
}

inline HFONT FontBody() {
    static HFONT f = MakeUiFont(13, FW_NORMAL);
    return f;
}

inline HFONT FontBold() {
    static HFONT f = MakeUiFont(13, FW_SEMIBOLD);
    return f;
}

inline HFONT FontHeader() {
    static HFONT f = MakeUiFont(21, FW_SEMIBOLD);
    return f;
}

inline HFONT FontSmall() {
    static HFONT f = MakeUiFont(11, FW_NORMAL);
    return f;
}

inline HFONT FontMono() {
    static HFONT f = CreateFontW(
        -MulDiv(11, DpiY(), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Code");
    return f;
}

inline HFONT FontBrand() {
    static HFONT f = MakeUiFont(16, FW_BOLD);
    return f;
}

// ─── Apply dark theme to a control ────────────────────────────────────────────
inline void ApplyDarkScrollbar(HWND hwnd) {
    SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
}

inline void ApplyDarkEdit(HWND hwnd) {
    SetWindowTheme(hwnd, L"DarkMode_CFD", nullptr);
}

inline void SetDarkTitlebar(HWND hwnd) {
    BOOL dark = TRUE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
}

} // namespace Theme
