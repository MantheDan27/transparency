#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <uxtheme.h>
#include <dwmapi.h>
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

// ─── Color Palette (industrial dark — matches design spec exactly) ────────────
namespace Theme {

// Backgrounds
constexpr COLORREF BG_APP       = RGB(11,  14,  20);   // #0b0e14 near-black
constexpr COLORREF BG_SIDEBAR   = RGB(17,  21,  32);   // #111520 dark navy
constexpr COLORREF BG_CARD      = RGB(24,  29,  46);   // #181d2e mid-navy
constexpr COLORREF BG_ELEVATED  = RGB(24,  29,  46);   // same as card
constexpr COLORREF BG_INPUT     = RGB(24,  29,  46);   // inputs / hovers
constexpr COLORREF BG_ROW_ALT   = RGB(17,  21,  32);   // alternating row
constexpr COLORREF BG_ROW_HOV   = RGB(24,  29,  46);   // hover
constexpr COLORREF BG_ROW_SEL   = RGB(26,  50,  90);   // selected

// Borders
constexpr COLORREF BORDER         = RGB(31,  39,  64);  // #1f2740
constexpr COLORREF SIDEBAR_BORDER = RGB(31,  39,  64);

// Text
constexpr COLORREF TEXT_PRIMARY   = RGB(200, 212, 240); // #c8d4f0 cool white
constexpr COLORREF TEXT_SECONDARY = RGB(150, 168, 210); // mid value
constexpr COLORREF TEXT_MUTED     = RGB(78,  95,  133); // #4e5f85

// Accent colors
constexpr COLORREF ACCENT         = RGB(61,  127, 255); // #3d7fff electric blue
constexpr COLORREF ACCENT_GLOW    = RGB(0,   229, 255); // #00e5ff cyan — active/live
constexpr COLORREF SUCCESS        = RGB(0,   229, 122); // #00e57a online/success
constexpr COLORREF DANGER         = RGB(255, 64,  96);  // #ff4060 offline/alert
constexpr COLORREF WARNING        = RGB(255, 200, 50);  // #ffc832 unknown/caution
constexpr COLORREF WATCHLIST      = RGB(168, 85,  247); // #a855f7 watchlist purple

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
inline HFONT FontBody() {
    static HFONT f = CreateFont(
        -MulDiv(11, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    return f;
}

inline HFONT FontBold() {
    static HFONT f = CreateFont(
        -MulDiv(11, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    return f;
}

inline HFONT FontHeader() {
    static HFONT f = CreateFont(
        -MulDiv(18, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    return f;
}

inline HFONT FontSmall() {
    static HFONT f = CreateFont(
        -MulDiv(9, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    return f;
}

inline HFONT FontMono() {
    static HFONT f = CreateFont(
        -MulDiv(10, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    return f;
}

inline HFONT FontBrand() {
    static HFONT f = CreateFont(
        -MulDiv(14, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
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
