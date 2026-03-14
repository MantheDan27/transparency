#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <uxtheme.h>
#include <dwmapi.h>
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

#include "ui/ui_theme.h"

// ─── Win32 Design System: Transparency ──────────────────────────────────────
// Wraps platform-agnostic tokens from ui_theme.h for Win32 GDI usage.
// All COLORREF values are derived from theme:: ARGB constants.
// Rule: NEVER hardcode colors, fonts, sizes, or spacing inline.

namespace Theme {

// ── ARGB → COLORREF conversion ──────────────────────────────────────────────
inline constexpr COLORREF to_colorref(uint32_t argb) {
    return RGB((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF);
}

// ── Backgrounds (4-layer depth system — never skip layers) ──────────────────
constexpr COLORREF BG_ROOT      = to_colorref(theme::bg_root);
constexpr COLORREF BG_SURFACE   = to_colorref(theme::bg_surface);
constexpr COLORREF BG_ELEVATED  = to_colorref(theme::bg_elevated);
constexpr COLORREF BG_OVERLAY   = to_colorref(theme::bg_overlay);

// Derived background states
constexpr COLORREF BG_INPUT     = to_colorref(theme::bg_input);
constexpr COLORREF BG_ROW_ALT   = to_colorref(theme::bg_row_alt);
constexpr COLORREF BG_ROW_HOV   = to_colorref(theme::bg_row_hov);
constexpr COLORREF BG_ROW_SEL   = to_colorref(theme::bg_row_sel);
constexpr COLORREF BG_NAV_ACTIVE= to_colorref(theme::bg_nav_active);

// Backward compatibility aliases
constexpr COLORREF BG_APP     = BG_ROOT;
constexpr COLORREF BG_SIDEBAR = BG_ROOT;
constexpr COLORREF BG_CARD    = BG_ELEVATED;

// ── Borders ──────────────────────────────────────────────────────────────────
constexpr COLORREF BORDER_DEFAULT = to_colorref(theme::border_default);
constexpr COLORREF BORDER_SUBTLE  = to_colorref(theme::border_subtle);
constexpr COLORREF BORDER_FOCUS   = to_colorref(theme::border_focus);

// Backward compatibility
constexpr COLORREF BORDER         = BORDER_DEFAULT;
constexpr COLORREF SIDEBAR_BORDER = BORDER_SUBTLE;

// ── Text ─────────────────────────────────────────────────────────────────────
constexpr COLORREF TEXT_PRIMARY   = to_colorref(theme::text_primary);
constexpr COLORREF TEXT_SECONDARY = to_colorref(theme::text_secondary);
constexpr COLORREF TEXT_TERTIARY  = to_colorref(theme::text_tertiary);

// Backward compatibility
constexpr COLORREF TEXT_MUTED = TEXT_TERTIARY;

// ── Accents (each has ONE semantic role — do not mix) ────────────────────────
constexpr COLORREF ACCENT_BLUE   = to_colorref(theme::accent_blue);
constexpr COLORREF ACCENT_CYAN   = to_colorref(theme::accent_cyan);
constexpr COLORREF ACCENT_GREEN  = to_colorref(theme::accent_green);
constexpr COLORREF ACCENT_AMBER  = to_colorref(theme::accent_amber);
constexpr COLORREF ACCENT_RED    = to_colorref(theme::accent_red);
constexpr COLORREF ACCENT_PURPLE = to_colorref(theme::accent_purple);

// Backward compatibility
constexpr COLORREF ACCENT      = ACCENT_BLUE;
constexpr COLORREF ACCENT_GLOW = ACCENT_CYAN;
constexpr COLORREF SUCCESS     = ACCENT_GREEN;
constexpr COLORREF DANGER      = ACCENT_RED;
constexpr COLORREF WARNING     = ACCENT_AMBER;
constexpr COLORREF WATCHLIST   = ACCENT_PURPLE;

// ── Spacing (base-4 system — all multiples of 4) ────────────────────────────
constexpr int SP1  = theme::sp1;
constexpr int SP2  = theme::sp2;
constexpr int SP3  = theme::sp3;
constexpr int SP4  = theme::sp4;
constexpr int SP5  = theme::sp5;
constexpr int SP6  = theme::sp6;
constexpr int SP8  = theme::sp8;
constexpr int SP10 = theme::sp10;
constexpr int SP12 = theme::sp12;
constexpr int SP16 = theme::sp16;

// ── Border Radii (in pixels) ─────────────────────────────────────────────────
constexpr int RADIUS_SM   = theme::radius_sm;
constexpr int RADIUS_MD   = theme::radius_md;
constexpr int RADIUS_LG   = theme::radius_lg;
constexpr int RADIUS_XL   = theme::radius_xl;
constexpr int RADIUS_FULL = theme::radius_full;

// ── Layout Constants ─────────────────────────────────────────────────────────
constexpr int SIDEBAR_W       = theme::sidebar_w;
constexpr int CONTENT_MAX_W   = theme::content_max_w;
constexpr int CARD_PADDING    = theme::card_padding;
constexpr int GRID_GAP        = theme::grid_gap;
constexpr int PAGE_PADDING    = theme::page_padding;
constexpr int MODAL_MAX_W     = theme::modal_max_w;
constexpr int MIN_TARGET      = theme::min_target;

// ── Alpha blend helper ───────────────────────────────────────────────────────
inline COLORREF AlphaBlend(COLORREF fg, COLORREF bg, int alphaPct) {
    int r = (GetRValue(fg) * alphaPct + GetRValue(bg) * (100 - alphaPct)) / 100;
    int g = (GetGValue(fg) * alphaPct + GetGValue(bg) * (100 - alphaPct)) / 100;
    int b = (GetBValue(fg) * alphaPct + GetBValue(bg) * (100 - alphaPct)) / 100;
    return RGB(r, g, b);
}

// ── Brush Cache ──────────────────────────────────────────────────────────────
inline HBRUSH BrushRoot()         { static HBRUSH b = CreateSolidBrush(BG_ROOT);         return b; }
inline HBRUSH BrushSurface()      { static HBRUSH b = CreateSolidBrush(BG_SURFACE);      return b; }
inline HBRUSH BrushElevated()     { static HBRUSH b = CreateSolidBrush(BG_ELEVATED);     return b; }
inline HBRUSH BrushOverlay()      { static HBRUSH b = CreateSolidBrush(BG_OVERLAY);      return b; }
inline HBRUSH BrushRowAlt()       { static HBRUSH b = CreateSolidBrush(BG_ROW_ALT);      return b; }
inline HBRUSH BrushRowSel()       { static HBRUSH b = CreateSolidBrush(BG_ROW_SEL);      return b; }
inline HBRUSH BrushNavActive()    { static HBRUSH b = CreateSolidBrush(BG_NAV_ACTIVE);   return b; }
inline HBRUSH BrushBorderDefault(){ static HBRUSH b = CreateSolidBrush(BORDER_DEFAULT);  return b; }
inline HBRUSH BrushBorderSubtle() { static HBRUSH b = CreateSolidBrush(BORDER_SUBTLE);   return b; }
inline HBRUSH BrushAccentBlue()   { static HBRUSH b = CreateSolidBrush(ACCENT_BLUE);     return b; }
inline HBRUSH BrushAccentCyan()   { static HBRUSH b = CreateSolidBrush(ACCENT_CYAN);     return b; }
inline HBRUSH BrushAccentGreen()  { static HBRUSH b = CreateSolidBrush(ACCENT_GREEN);    return b; }
inline HBRUSH BrushAccentAmber()  { static HBRUSH b = CreateSolidBrush(ACCENT_AMBER);    return b; }
inline HBRUSH BrushAccentRed()    { static HBRUSH b = CreateSolidBrush(ACCENT_RED);      return b; }
inline HBRUSH BrushAccentPurple() { static HBRUSH b = CreateSolidBrush(ACCENT_PURPLE);   return b; }
inline HBRUSH BrushNull()         { static HBRUSH b = (HBRUSH)GetStockObject(NULL_BRUSH); return b; }

// Backward compatibility brush aliases
inline HBRUSH BrushApp()       { return BrushRoot(); }
inline HBRUSH BrushSidebar()   { return BrushRoot(); }
inline HBRUSH BrushCard()      { return BrushElevated(); }
inline HBRUSH BrushRowHov()    { return BrushElevated(); }
inline HBRUSH BrushInput()     { return BrushRoot(); }
inline HBRUSH BrushAccent()    { return BrushAccentBlue(); }
inline HBRUSH BrushAccentGlow(){ return BrushAccentCyan(); }
inline HBRUSH BrushBorder()    { return BrushBorderDefault(); }
inline HBRUSH BrushSuccess()   { return BrushAccentGreen(); }
inline HBRUSH BrushWarning()   { return BrushAccentAmber(); }
inline HBRUSH BrushDanger()    { return BrushAccentRed(); }
inline HBRUSH BrushWatchlist() { return BrushAccentPurple(); }

// ── Font face detection (Geist with system fallbacks) ────────────────────────
inline const wchar_t* FontFaceSans() {
    static const wchar_t* face = []() -> const wchar_t* {
        HDC hdc = GetDC(NULL);
        LOGFONT lf = {}; lf.lfCharSet = DEFAULT_CHARSET;
        wcscpy_s(lf.lfFaceName, L"Geist");
        bool found = false;
        EnumFontFamiliesEx(hdc, &lf,
            [](const LOGFONT*, const TEXTMETRIC*, DWORD, LPARAM p) -> int {
                *reinterpret_cast<bool*>(p) = true; return 0;
            }, (LPARAM)&found, 0);
        ReleaseDC(NULL, hdc);
        return found ? L"Geist" : L"Segoe UI";
    }();
    return face;
}

inline const wchar_t* FontFaceMono() {
    static const wchar_t* face = []() -> const wchar_t* {
        HDC hdc = GetDC(NULL);
        LOGFONT lf = {}; lf.lfCharSet = DEFAULT_CHARSET;
        wcscpy_s(lf.lfFaceName, L"Geist Mono");
        bool found = false;
        EnumFontFamiliesEx(hdc, &lf,
            [](const LOGFONT*, const TEXTMETRIC*, DWORD, LPARAM p) -> int {
                *reinterpret_cast<bool*>(p) = true; return 0;
            }, (LPARAM)&found, 0);
        ReleaseDC(NULL, hdc);
        return found ? L"Geist Mono" : L"Consolas";
    }();
    return face;
}

// ── Font creation helper (pixel size, DPI-aware) ─────────────────────────────
inline HFONT MakeFont(int pxSize, int weight, bool mono = false) {
    return CreateFont(
        -MulDiv(pxSize, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 96),
        0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        mono ? (FIXED_PITCH | FF_MODERN) : (DEFAULT_PITCH | FF_SWISS),
        mono ? FontFaceMono() : FontFaceSans());
}

// ── Type Scale ───────────────────────────────────────────────────────────────
// Display  48px Bold(700)     — Hero stats
// H1       32px Bold(700)     — Page titles
// H2       24px SemiBold(600) — Section headers
// H3       18px SemiBold(600) — Card titles
// Body     15px Regular(400)  — Default body (minimum for readable content)
// BodySm   13px Regular(400)  — Descriptions, body small
// Caption  11px Medium(500)   — Labels, badges (UPPERCASE, +0.04em tracking)
// Mono     13px Regular(400)  — IPs, MACs, ports, hashes, scan output

inline HFONT FontDisplay() { static HFONT f = MakeFont(48, FW_BOLD);     return f; }
inline HFONT FontH1()      { static HFONT f = MakeFont(32, FW_BOLD);     return f; }
inline HFONT FontH2()      { static HFONT f = MakeFont(24, FW_SEMIBOLD); return f; }
inline HFONT FontH3()      { static HFONT f = MakeFont(18, FW_SEMIBOLD); return f; }
inline HFONT FontBody()    { static HFONT f = MakeFont(15, FW_NORMAL);   return f; }
inline HFONT FontBodySm()  { static HFONT f = MakeFont(13, FW_NORMAL);   return f; }
inline HFONT FontCaption() { static HFONT f = MakeFont(11, FW_MEDIUM);   return f; }
inline HFONT FontMono()    { static HFONT f = MakeFont(13, FW_NORMAL, true); return f; }

// Nav-specific fonts (13px, weight varies by state)
inline HFONT FontNavActive()   { static HFONT f = MakeFont(13, FW_SEMIBOLD); return f; }
inline HFONT FontNavInactive() { return FontBodySm(); }

// Backward compatibility font aliases
inline HFONT FontBold()    { static HFONT f = MakeFont(15, FW_SEMIBOLD); return f; }
inline HFONT FontHeader()  { return FontH1(); }
inline HFONT FontSmall()   { return FontBodySm(); }
inline HFONT FontBrand()   { return FontH2(); }

// ── Apply dark theme to controls ─────────────────────────────────────────────
inline void ApplyDarkScrollbar(HWND hwnd) {
    SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
}

inline void ApplyDarkEdit(HWND hwnd) {
    SetWindowTheme(hwnd, L"DarkMode_CFD", nullptr);
}

inline void SetDarkTitlebar(HWND hwnd) {
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
}

} // namespace Theme
