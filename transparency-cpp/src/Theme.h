#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <algorithm>
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

// ═══════════════════════════════════════════════════════════════════════════════
// TRANSPARENCY v4.0.0 — Premium Glassmorphism Theme
// ═══════════════════════════════════════════════════════════════════════════════
// A polished dark theme with high-contrast text, subtle glass effects,
// and professional gradients for a network intelligence dashboard.
// ═══════════════════════════════════════════════════════════════════════════════

namespace Theme {

// ─── Core Background Palette ──────────────────────────────────────────────────
// Deep obsidian blacks with subtle blue undertones for depth
constexpr COLORREF BG_APP         = RGB(8,   10,   14);   // #080a0e - deepest base
constexpr COLORREF BG_SIDEBAR     = RGB(12,  16,   24);   // #0c1018 - sidebar panel
constexpr COLORREF BG_TOPBAR      = RGB(10,  14,   20);   // #0a0e14 - topbar strip
constexpr COLORREF BG_CARD        = RGB(18,  24,   38);   // #121826 - card surface
constexpr COLORREF BG_CARD_HEADER = RGB(24,  32,   50);   // #182032 - card header
constexpr COLORREF BG_ELEVATED    = RGB(28,  36,   56);   // #1c2438 - elevated panels
constexpr COLORREF BG_INPUT       = RGB(16,  22,   34);   // #101622 - input fields
constexpr COLORREF BG_ROW_ALT     = RGB(14,  18,   28);   // #0e121c - alternating rows
constexpr COLORREF BG_ROW_HOV     = RGB(32,  44,   72);   // #202c48 - hover state
constexpr COLORREF BG_ROW_SEL     = RGB(40,  60,  100);   // #283c64 - selected state

// ─── Glass Effect Colors ──────────────────────────────────────────────────────
// Used for creating frosted glass highlights and depth
constexpr COLORREF GLASS_HIGHLIGHT = RGB(60,  80,  120);  // #3c5078 - top edge glow
constexpr COLORREF GLASS_SHADOW    = RGB(4,    6,   10);  // #04060a - bottom shadow

// ─── Border Palette ───────────────────────────────────────────────────────────
constexpr COLORREF BORDER         = RGB(45,  58,   82);   // #2d3a52 - standard border
constexpr COLORREF BORDER_LIGHT   = RGB(56,  72,  100);   // #384864 - highlighted border
constexpr COLORREF BORDER_SUBTLE  = RGB(32,  42,   60);   // #202a3c - subtle separator
constexpr COLORREF SIDEBAR_BORDER = RGB(48,  62,   88);   // #303e58 - sidebar edge

// ─── Text Palette — HIGH CONTRAST ─────────────────────────────────────────────
// WCAG AAA compliant contrast ratios against dark backgrounds
constexpr COLORREF TEXT_PRIMARY   = RGB(248, 250, 252);   // #f8fafc - pure white-blue
constexpr COLORREF TEXT_SECONDARY = RGB(200, 212, 228);   // #c8d4e4 - bright secondary
constexpr COLORREF TEXT_MUTED     = RGB(160, 175, 200);   // #a0afc8 - muted but readable
constexpr COLORREF TEXT_DIM       = RGB(120, 135, 160);   // #7887a0 - dim labels
constexpr COLORREF TEXT_ON_ACCENT = RGB(255, 255, 255);   // #ffffff - text on accent bg

// ─── Accent Colors ────────────────────────────────────────────────────────────
constexpr COLORREF ACCENT         = RGB(66,  135, 245);   // #4287f5 - primary blue
constexpr COLORREF ACCENT_BRIGHT  = RGB(100, 160, 255);   // #64a0ff - hover/glow
constexpr COLORREF ACCENT_DIM     = RGB(35,   65, 120);   // #234178 - subtle highlights
constexpr COLORREF ACCENT_BG      = RGB(25,   50,  95);   // #19325f - accent backgrounds

// ─── Semantic Status Colors — VIBRANT ─────────────────────────────────────────
constexpr COLORREF SUCCESS        = RGB(52,  211, 153);   // #34d399 - emerald green
constexpr COLORREF SUCCESS_DIM    = RGB(20,   80,  60);   // #14503c - success background
constexpr COLORREF DANGER         = RGB(251, 113, 133);   // #fb7185 - rose red
constexpr COLORREF DANGER_DIM     = RGB(100,  35,  45);   // #64232d - danger background
constexpr COLORREF WARNING        = RGB(251, 191,  36);   // #fbbf24 - amber
constexpr COLORREF WARNING_DIM    = RGB(100,  75,  15);   // #644b0f - warning background
constexpr COLORREF INFO           = RGB(96,  165, 250);   // #60a5fa - sky blue
constexpr COLORREF INFO_DIM       = RGB(30,   60, 100);   // #1e3c64 - info background

// ─── Trust State Colors ───────────────────────────────────────────────────────
constexpr COLORREF TRUST_OWNED    = RGB(52,  211, 153);   // emerald - owned devices
constexpr COLORREF TRUST_GUEST    = RGB(251, 191,  36);   // amber - guest devices
constexpr COLORREF TRUST_WATCHLIST= RGB(192, 132, 252);   // #c084fc - purple watchlist
constexpr COLORREF TRUST_BLOCKED  = RGB(251, 113, 133);   // rose - blocked
constexpr COLORREF TRUST_UNKNOWN  = RGB(148, 163, 184);   // #94a3b8 - slate gray

// ─── Network Status Indicators ────────────────────────────────────────────────
constexpr COLORREF ONLINE_GREEN   = RGB(34,  197, 94);    // #22c55e - online dot
constexpr COLORREF OFFLINE_RED    = RGB(239,  68,  68);   // #ef4444 - offline dot
constexpr COLORREF LATENCY_GOOD   = RGB(34,  197,  94);   // green - <50ms
constexpr COLORREF LATENCY_MED    = RGB(251, 191,  36);   // amber - 50-150ms
constexpr COLORREF LATENCY_BAD    = RGB(239,  68,  68);   // red - >150ms

// ─── Port Security Colors ─────────────────────────────────────────────────────
constexpr COLORREF PORT_SAFE      = RGB(100, 116, 139);   // #64748b - normal port
constexpr COLORREF PORT_RISKY     = RGB(251, 113, 133);   // rose - risky port (23,445,etc)
constexpr COLORREF PORT_SECURE    = RGB(52,  211, 153);   // emerald - secure (443,22)

// ═══════════════════════════════════════════════════════════════════════════════
// BRUSH CACHE — Static brushes for performance
// ═══════════════════════════════════════════════════════════════════════════════

inline HBRUSH BrushApp()          { static HBRUSH b = CreateSolidBrush(BG_APP);          return b; }
inline HBRUSH BrushSidebar()      { static HBRUSH b = CreateSolidBrush(BG_SIDEBAR);      return b; }
inline HBRUSH BrushTopbar()       { static HBRUSH b = CreateSolidBrush(BG_TOPBAR);       return b; }
inline HBRUSH BrushCard()         { static HBRUSH b = CreateSolidBrush(BG_CARD);         return b; }
inline HBRUSH BrushCardHeader()   { static HBRUSH b = CreateSolidBrush(BG_CARD_HEADER);  return b; }
inline HBRUSH BrushElevated()     { static HBRUSH b = CreateSolidBrush(BG_ELEVATED);     return b; }
inline HBRUSH BrushInput()        { static HBRUSH b = CreateSolidBrush(BG_INPUT);        return b; }
inline HBRUSH BrushRowAlt()       { static HBRUSH b = CreateSolidBrush(BG_ROW_ALT);      return b; }
inline HBRUSH BrushRowHov()       { static HBRUSH b = CreateSolidBrush(BG_ROW_HOV);      return b; }
inline HBRUSH BrushRowSel()       { static HBRUSH b = CreateSolidBrush(BG_ROW_SEL);      return b; }
inline HBRUSH BrushBorder()       { static HBRUSH b = CreateSolidBrush(BORDER);          return b; }
inline HBRUSH BrushBorderLight()  { static HBRUSH b = CreateSolidBrush(BORDER_LIGHT);    return b; }
inline HBRUSH BrushAccent()       { static HBRUSH b = CreateSolidBrush(ACCENT);          return b; }
inline HBRUSH BrushAccentBright() { static HBRUSH b = CreateSolidBrush(ACCENT_BRIGHT);   return b; }
inline HBRUSH BrushAccentDim()    { static HBRUSH b = CreateSolidBrush(ACCENT_DIM);      return b; }
inline HBRUSH BrushAccentBg()     { static HBRUSH b = CreateSolidBrush(ACCENT_BG);       return b; }
inline HBRUSH BrushSuccess()      { static HBRUSH b = CreateSolidBrush(SUCCESS);         return b; }
inline HBRUSH BrushSuccessDim()   { static HBRUSH b = CreateSolidBrush(SUCCESS_DIM);     return b; }
inline HBRUSH BrushDanger()       { static HBRUSH b = CreateSolidBrush(DANGER);          return b; }
inline HBRUSH BrushDangerDim()    { static HBRUSH b = CreateSolidBrush(DANGER_DIM);      return b; }
inline HBRUSH BrushWarning()      { static HBRUSH b = CreateSolidBrush(WARNING);         return b; }
inline HBRUSH BrushWarningDim()   { static HBRUSH b = CreateSolidBrush(WARNING_DIM);     return b; }
inline HBRUSH BrushInfo()         { static HBRUSH b = CreateSolidBrush(INFO);            return b; }
inline HBRUSH BrushInfoDim()      { static HBRUSH b = CreateSolidBrush(INFO_DIM);        return b; }
inline HBRUSH BrushWatchlist()    { static HBRUSH b = CreateSolidBrush(TRUST_WATCHLIST); return b; }
inline HBRUSH BrushOnline()       { static HBRUSH b = CreateSolidBrush(ONLINE_GREEN);    return b; }
inline HBRUSH BrushOffline()      { static HBRUSH b = CreateSolidBrush(OFFLINE_RED);     return b; }
inline HBRUSH BrushNull()         { static HBRUSH b = (HBRUSH)GetStockObject(NULL_BRUSH);return b; }

// ═══════════════════════════════════════════════════════════════════════════════
// FONT CACHE — DPI-aware fonts
// ═══════════════════════════════════════════════════════════════════════════════

inline int DpiY() {
    HDC hdc = GetDC(nullptr);
    if (!hdc) return 96;
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);
    return dpi > 0 ? dpi : 96;
}

inline HFONT MakeUiFont(int pointSize, int weight, bool italic = false) {
    return CreateFontW(
        -MulDiv(pointSize, DpiY(), 72),
        0, 0, 0, weight, italic ? TRUE : FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI Variable");
}

inline HFONT MakeIconFont(int pointSize) {
    return CreateFontW(
        -MulDiv(pointSize, DpiY(), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
}

// Standard UI fonts
inline HFONT FontBody()     { static HFONT f = MakeUiFont(12, FW_NORMAL);   return f; }
inline HFONT FontBold()     { static HFONT f = MakeUiFont(12, FW_SEMIBOLD); return f; }
inline HFONT FontSmall()    { static HFONT f = MakeUiFont(10, FW_NORMAL);   return f; }
inline HFONT FontSmallBold(){ static HFONT f = MakeUiFont(10, FW_SEMIBOLD); return f; }
inline HFONT FontTiny()     { static HFONT f = MakeUiFont(9,  FW_NORMAL);   return f; }

// Heading fonts
inline HFONT FontHeader()   { static HFONT f = MakeUiFont(18, FW_SEMIBOLD); return f; }
inline HFONT FontSection()  { static HFONT f = MakeUiFont(13, FW_SEMIBOLD); return f; }
inline HFONT FontBrand()    { static HFONT f = MakeUiFont(15, FW_BOLD);     return f; }
inline HFONT FontSubtitle() { static HFONT f = MakeUiFont(9,  FW_MEDIUM);   return f; }

// KPI / Dashboard fonts
inline HFONT FontKPI()      { static HFONT f = MakeUiFont(32, FW_LIGHT);    return f; }
inline HFONT FontKPISmall() { static HFONT f = MakeUiFont(24, FW_LIGHT);    return f; }

// Monospace for technical data
inline HFONT FontMono() {
    static HFONT f = CreateFontW(
        -MulDiv(11, DpiY(), 72),
        0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Code");
    return f;
}

inline HFONT FontMonoBold() {
    static HFONT f = CreateFontW(
        -MulDiv(11, DpiY(), 72),
        0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Cascadia Code");
    return f;
}

// Icon font for glyphs
inline HFONT FontIcon()     { static HFONT f = MakeIconFont(14); return f; }
inline HFONT FontIconLarge(){ static HFONT f = MakeIconFont(20); return f; }

// ═══════════════════════════════════════════════════════════════════════════════
// DRAWING HELPERS — Glassmorphism effects
// ═══════════════════════════════════════════════════════════════════════════════

// Blend two colors with alpha (0-255)
inline COLORREF BlendColors(COLORREF c1, COLORREF c2, int alpha) {
    int r = (GetRValue(c1) * (255 - alpha) + GetRValue(c2) * alpha) / 255;
    int g = (GetGValue(c1) * (255 - alpha) + GetGValue(c2) * alpha) / 255;
    int b = (GetBValue(c1) * (255 - alpha) + GetBValue(c2) * alpha) / 255;
    return RGB(r, g, b);
}

// Lighten a color by amount (0-255)
inline COLORREF Lighten(COLORREF c, int amount) {
    return RGB(
        (std::min)(255, GetRValue(c) + amount),
        (std::min)(255, GetGValue(c) + amount),
        (std::min)(255, GetBValue(c) + amount));
}

// Darken a color by amount (0-255)
inline COLORREF Darken(COLORREF c, int amount) {
    return RGB(
        (std::max)(0, GetRValue(c) - amount),
        (std::max)(0, GetGValue(c) - amount),
        (std::max)(0, GetBValue(c) - amount));
}

// Fill a rounded rectangle
inline void FillRoundRect(HDC hdc, const RECT& rc, int radius, COLORREF color) {
    HBRUSH br = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 0, color);
    HBRUSH oldB = (HBRUSH)SelectObject(hdc, br);
    HPEN oldP = (HPEN)SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(br);
    DeleteObject(pen);
}

// Draw rounded rectangle with border
inline void DrawRoundRect(HDC hdc, const RECT& rc, int radius,
                          COLORREF fill, COLORREF border, int borderWidth = 1) {
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, borderWidth, border);
    HBRUSH oldB = (HBRUSH)SelectObject(hdc, br);
    HPEN oldP = (HPEN)SelectObject(hdc, pen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(br);
    DeleteObject(pen);
}

// ─── Premium Glass Card ───────────────────────────────────────────────────────
// Draws a card with glass-like top highlight, subtle inner glow, and border
inline void DrawGlassCard(HDC hdc, const RECT& rc, COLORREF bg = BG_CARD, int radius = 8) {
    // 1. Main fill with rounded corners
    FillRoundRect(hdc, rc, radius, bg);

    // 2. Top highlight line (glass reflection)
    COLORREF highlight = Lighten(bg, 20);
    HPEN hlPen = CreatePen(PS_SOLID, 1, highlight);
    HPEN oldPen = (HPEN)SelectObject(hdc, hlPen);
    MoveToEx(hdc, rc.left + radius, rc.top + 1, nullptr);
    LineTo(hdc, rc.right - radius, rc.top + 1);
    SelectObject(hdc, oldPen);
    DeleteObject(hlPen);

    // 3. Border outline
    HPEN borderPen = CreatePen(PS_SOLID, 1, BORDER);
    HBRUSH nullBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
    oldPen = (HPEN)SelectObject(hdc, borderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, nullBrush);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(borderPen);
}

// ─── Glass Card with Accent Edge ──────────────────────────────────────────────
inline void DrawGlassCardAccent(HDC hdc, const RECT& rc, COLORREF accent,
                                 COLORREF bg = BG_CARD, int radius = 8) {
    DrawGlassCard(hdc, rc, bg, radius);

    // Left accent bar
    RECT accentRc = { rc.left + 2, rc.top + radius, rc.left + 5, rc.bottom - radius };
    HBRUSH accentBr = CreateSolidBrush(accent);
    FillRect(hdc, &accentRc, accentBr);
    DeleteObject(accentBr);
}

// ─── Draw Pill Badge ──────────────────────────────────────────────────────────
inline void DrawPill(HDC hdc, const RECT& rc, const wchar_t* text,
                     COLORREF bg, COLORREF fg, HFONT font = nullptr) {
    if (!font) font = FontSmall();

    int h = rc.bottom - rc.top;
    FillRoundRect(hdc, rc, h, bg);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, fg);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    DrawText(hdc, text, -1, (LPRECT)&rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

// ─── Draw Status Dot ──────────────────────────────────────────────────────────
inline void DrawStatusDot(HDC hdc, int cx, int cy, int radius, COLORREF color, bool glow = true) {
    if (glow) {
        // Outer glow
        COLORREF glowColor = BlendColors(color, BG_APP, 180);
        HBRUSH glowBr = CreateSolidBrush(glowColor);
        HPEN glowPen = CreatePen(PS_SOLID, 0, glowColor);
        SelectObject(hdc, glowBr);
        SelectObject(hdc, glowPen);
        Ellipse(hdc, cx - radius - 2, cy - radius - 2, cx + radius + 3, cy + radius + 3);
        DeleteObject(glowBr);
        DeleteObject(glowPen);
    }

    // Main dot
    HBRUSH br = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 0, color);
    HBRUSH oldB = (HBRUSH)SelectObject(hdc, br);
    HPEN oldP = (HPEN)SelectObject(hdc, pen);
    Ellipse(hdc, cx - radius, cy - radius, cx + radius + 1, cy + radius + 1);
    SelectObject(hdc, oldB);
    SelectObject(hdc, oldP);
    DeleteObject(br);
    DeleteObject(pen);
}

// ─── Draw Progress Bar ────────────────────────────────────────────────────────
inline void DrawProgressBar(HDC hdc, const RECT& rc, int percent,
                            COLORREF bg = BG_INPUT, COLORREF fg = ACCENT) {
    int h = rc.bottom - rc.top;

    // Track background
    FillRoundRect(hdc, rc, h, bg);

    // Fill bar
    if (percent > 0) {
        int fillW = (rc.right - rc.left) * (std::min)(100, percent) / 100;
        if (fillW > 0) {
            RECT fillRc = { rc.left, rc.top, rc.left + fillW, rc.bottom };
            FillRoundRect(hdc, fillRc, h, fg);
        }
    }
}

// ─── Draw Separator Line ──────────────────────────────────────────────────────
inline void DrawSeparator(HDC hdc, int x1, int y, int x2, COLORREF color = BORDER_SUBTLE) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    MoveToEx(hdc, x1, y, nullptr);
    LineTo(hdc, x2, y);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

// ─── Draw Section Header ──────────────────────────────────────────────────────
inline void DrawSectionHeader(HDC hdc, const RECT& rc, const wchar_t* text,
                               COLORREF color = TEXT_DIM) {
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    HFONT oldFont = (HFONT)SelectObject(hdc, FontSmallBold());
    DrawText(hdc, text, -1, (LPRECT)&rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    // Line after text
    SIZE sz;
    GetTextExtentPoint32(hdc, text, (int)wcslen(text), &sz);
    DrawSeparator(hdc, rc.left + sz.cx + 10, (rc.top + rc.bottom) / 2, rc.right);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SYSTEM THEME HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

inline void ApplyDarkScrollbar(HWND hwnd) {
    SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
}

inline void ApplyDarkEdit(HWND hwnd) {
    SetWindowTheme(hwnd, L"DarkMode_CFD", nullptr);
}

inline void ApplyDarkComboBox(HWND hwnd) {
    SetWindowTheme(hwnd, L"DarkMode_CFD", nullptr);
}

inline void ApplyDarkListView(HWND hwnd) {
    SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
    ListView_SetBkColor(hwnd, BG_CARD);
    ListView_SetTextBkColor(hwnd, BG_CARD);
    ListView_SetTextColor(hwnd, TEXT_PRIMARY);
}

inline void SetDarkTitlebar(HWND hwnd) {
    BOOL dark = TRUE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
}

// ─── Get Latency Color ────────────────────────────────────────────────────────
inline COLORREF GetLatencyColor(int ms) {
    if (ms < 0)   return TEXT_MUTED;
    if (ms < 50)  return LATENCY_GOOD;
    if (ms < 150) return LATENCY_MED;
    return LATENCY_BAD;
}

// ─── Get Trust Color ──────────────────────────────────────────────────────────
inline COLORREF GetTrustColor(const wchar_t* trust) {
    if (wcscmp(trust, L"owned") == 0)     return TRUST_OWNED;
    if (wcscmp(trust, L"guest") == 0)     return TRUST_GUEST;
    if (wcscmp(trust, L"watchlist") == 0) return TRUST_WATCHLIST;
    if (wcscmp(trust, L"blocked") == 0)   return TRUST_BLOCKED;
    return TRUST_UNKNOWN;
}

// ─── App Version ──────────────────────────────────────────────────────────────
constexpr const wchar_t* VERSION = L"v4.0.0";
constexpr const wchar_t* VERSION_FULL = L"Transparency v4.0.0";

} // namespace Theme
