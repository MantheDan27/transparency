#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <uxtheme.h>
#include <dwmapi.h>
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

// ─── Color Palette ───────────────────────────────────────────────────────────
namespace Theme {

constexpr COLORREF BG_APP       = RGB(18,  18,  30);
constexpr COLORREF BG_SIDEBAR   = RGB(10,  10,  22);
constexpr COLORREF BG_CARD      = RGB(28,  28,  46);
constexpr COLORREF BG_ROW_ALT   = RGB(14,  14,  28);
constexpr COLORREF BG_ROW_HOV   = RGB(24,  24,  48);
constexpr COLORREF BG_ROW_SEL   = RGB(24,  42,  68);
constexpr COLORREF ACCENT       = RGB(74,  158, 255);
constexpr COLORREF TEXT_PRIMARY = RGB(232, 232, 248);
constexpr COLORREF TEXT_SECONDARY = RGB(170,170, 192);
constexpr COLORREF TEXT_MUTED   = RGB(68,  68,  102);
constexpr COLORREF BORDER       = RGB(37,  37,  64);
constexpr COLORREF SUCCESS      = RGB(46,  213, 115);
constexpr COLORREF WARNING      = RGB(255, 165, 2);
constexpr COLORREF DANGER       = RGB(255, 71,  87);
constexpr COLORREF SIDEBAR_BORDER = RGB(24, 24, 46);
constexpr COLORREF BG_INPUT     = RGB(22,  22,  38);

// ─── Brush Cache ─────────────────────────────────────────────────────────────
inline HBRUSH BrushApp()      { static HBRUSH b = CreateSolidBrush(BG_APP);       return b; }
inline HBRUSH BrushSidebar()  { static HBRUSH b = CreateSolidBrush(BG_SIDEBAR);   return b; }
inline HBRUSH BrushCard()     { static HBRUSH b = CreateSolidBrush(BG_CARD);      return b; }
inline HBRUSH BrushRowAlt()   { static HBRUSH b = CreateSolidBrush(BG_ROW_ALT);   return b; }
inline HBRUSH BrushRowHov()   { static HBRUSH b = CreateSolidBrush(BG_ROW_HOV);   return b; }
inline HBRUSH BrushRowSel()   { static HBRUSH b = CreateSolidBrush(BG_ROW_SEL);   return b; }
inline HBRUSH BrushAccent()   { static HBRUSH b = CreateSolidBrush(ACCENT);       return b; }
inline HBRUSH BrushBorder()   { static HBRUSH b = CreateSolidBrush(BORDER);       return b; }
inline HBRUSH BrushSuccess()  { static HBRUSH b = CreateSolidBrush(SUCCESS);      return b; }
inline HBRUSH BrushWarning()  { static HBRUSH b = CreateSolidBrush(WARNING);      return b; }
inline HBRUSH BrushDanger()   { static HBRUSH b = CreateSolidBrush(DANGER);       return b; }
inline HBRUSH BrushInput()    { static HBRUSH b = CreateSolidBrush(BG_INPUT);     return b; }
inline HBRUSH BrushNull()     { static HBRUSH b = (HBRUSH)GetStockObject(NULL_BRUSH); return b; }

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
