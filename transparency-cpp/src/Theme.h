#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <uxtheme.h>
#include <dwmapi.h>
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

// ─── Color Palette (refined professional dark) ───────────────────────────────
namespace Theme {

// Backgrounds — warmer, softer dark tones
constexpr COLORREF BG_APP       = RGB(16,  18,  24);   // #101218 soft charcoal
constexpr COLORREF BG_SIDEBAR   = RGB(20,  22,  30);   // #14161e dark slate
constexpr COLORREF BG_CARD      = RGB(26,  29,  39);   // #1a1d27 card surface
constexpr COLORREF BG_ELEVATED  = RGB(30,  33,  43);   // #1e212b elevated surface
constexpr COLORREF BG_INPUT     = RGB(30,  33,  43);   // #1e212b inputs
constexpr COLORREF BG_ROW_ALT   = RGB(22,  24,  33);   // #161821 alternating row
constexpr COLORREF BG_ROW_HOV   = RGB(30,  33,  43);   // #1e212b hover
constexpr COLORREF BG_ROW_SEL   = RGB(35,  55,  85);   // #233755 selected

// Borders — subtle, warm gray
constexpr COLORREF BORDER         = RGB(37,  40,  48);  // #252830
constexpr COLORREF SIDEBAR_BORDER = RGB(37,  40,  48);

// Text — warmer, easier on the eyes
constexpr COLORREF TEXT_PRIMARY   = RGB(226, 228, 234); // #e2e4ea warm white
constexpr COLORREF TEXT_SECONDARY = RGB(139, 144, 160); // #8b90a0 warm mid
constexpr COLORREF TEXT_MUTED     = RGB(86,  91,  110); // #565b6e muted

// Accent colors — refined, less neon
constexpr COLORREF ACCENT         = RGB(91,  141, 239); // #5b8def refined blue
constexpr COLORREF ACCENT_GLOW    = RGB(56,  189, 248); // #38bdf8 sky blue
constexpr COLORREF SUCCESS        = RGB(52,  211, 153); // #34d399 emerald
constexpr COLORREF DANGER         = RGB(248, 113, 113); // #f87171 soft red
constexpr COLORREF WARNING        = RGB(251, 191, 36);  // #fbbf24 amber
constexpr COLORREF WATCHLIST      = RGB(167, 139, 250); // #a78bfa violet

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
        -MulDiv(12, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Variable");
    return f;
}

inline HFONT FontBold() {
    static HFONT f = CreateFont(
        -MulDiv(12, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Variable");
    return f;
}

inline HFONT FontHeader() {
    static HFONT f = CreateFont(
        -MulDiv(20, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Variable");
    return f;
}

inline HFONT FontSmall() {
    static HFONT f = CreateFont(
        -MulDiv(10, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Variable");
    return f;
}

inline HFONT FontMono() {
    static HFONT f = CreateFont(
        -MulDiv(11, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Code");
    return f;
}

inline HFONT FontBrand() {
    static HFONT f = CreateFont(
        -MulDiv(15, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 72),
        0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Variable");
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
