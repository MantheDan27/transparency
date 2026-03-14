#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <uxtheme.h>
#include <dwmapi.h>
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// ─── Design System: Transparency v4.1 ───────────────────────────────────────
// Single source of truth for all visual tokens. See design-system.md for spec.
// Rule: NEVER hardcode colors, fonts, sizes, or spacing inline.

namespace Theme {

// ── Backgrounds (4-layer depth system — never skip layers) ──────────────────
constexpr COLORREF BG_ROOT      = RGB(10,  12,  16);   // #0A0C10 — Layer 1: app background, sidebar
constexpr COLORREF BG_SURFACE   = RGB(18,  21,  28);   // #12151C — Layer 2: content area, panels
constexpr COLORREF BG_ELEVATED  = RGB(26,  30,  40);   // #1A1E28 — Layer 3: cards, hover states
constexpr COLORREF BG_OVERLAY   = RGB(34,  40,  56);   // #222838 — Layer 4: modals, dropdowns, popovers

// Derived background states
constexpr COLORREF BG_INPUT     = RGB(10,  12,  16);   // inputs use root layer
constexpr COLORREF BG_ROW_ALT   = RGB(14,  17,  22);   // subtle alternation on root
constexpr COLORREF BG_ROW_HOV   = RGB(26,  30,  40);   // hover = elevated layer
constexpr COLORREF BG_ROW_SEL   = RGB(24,  37,  62);   // accent_blue @ 15% on surface
constexpr COLORREF BG_NAV_ACTIVE= RGB(18,  29,  52);   // accent_blue @ 15% on root

// Backward compatibility aliases
constexpr COLORREF BG_APP     = BG_ROOT;
constexpr COLORREF BG_SIDEBAR = BG_ROOT;
constexpr COLORREF BG_CARD    = BG_ELEVATED;

// ── Borders ──────────────────────────────────────────────────────────────────
constexpr COLORREF BORDER_DEFAULT = RGB(42,  48,  64);  // #2A3040 — card borders, dividers
constexpr COLORREF BORDER_SUBTLE  = RGB(30,  35,  48);  // #1E2330 — subtle separators
constexpr COLORREF BORDER_FOCUS   = RGB(61,  127, 255); // #3D7FFF — focus rings, active borders

// Backward compatibility
constexpr COLORREF BORDER         = BORDER_DEFAULT;
constexpr COLORREF SIDEBAR_BORDER = BORDER_SUBTLE;

// ── Text ─────────────────────────────────────────────────────────────────────
constexpr COLORREF TEXT_PRIMARY   = RGB(232, 236, 244); // #E8ECF4 — headings, primary content
constexpr COLORREF TEXT_SECONDARY = RGB(136, 146, 168); // #8892A8 — body text, descriptions
constexpr COLORREF TEXT_TERTIARY  = RGB(85,  94,  114); // #555E72 — disabled, hints, timestamps

// Backward compatibility
constexpr COLORREF TEXT_MUTED = TEXT_TERTIARY;

// ── Accents (each has ONE semantic role — do not mix) ────────────────────────
constexpr COLORREF ACCENT_BLUE   = RGB(61,  127, 255); // #3D7FFF — primary actions, links, focus
constexpr COLORREF ACCENT_CYAN   = RGB(0,   229, 255); // #00E5FF — confidence scores, power user
constexpr COLORREF ACCENT_GREEN  = RGB(0,   229, 122); // #00E57A — trusted, healthy, success
constexpr COLORREF ACCENT_AMBER  = RGB(255, 200, 50);  // #FFC832 — warning, caution, unknown
constexpr COLORREF ACCENT_RED    = RGB(255, 64,  96);  // #FF4060 — critical, blocked, destructive
constexpr COLORREF ACCENT_PURPLE = RGB(168, 85,  247); // #A855F7 — premium, rare, watchlist

// Backward compatibility
constexpr COLORREF ACCENT      = ACCENT_BLUE;
constexpr COLORREF ACCENT_GLOW = ACCENT_CYAN;
constexpr COLORREF SUCCESS     = ACCENT_GREEN;
constexpr COLORREF DANGER      = ACCENT_RED;
constexpr COLORREF WARNING     = ACCENT_AMBER;
constexpr COLORREF WATCHLIST   = ACCENT_PURPLE;

// ── Spacing (base-4 system — all multiples of 4) ────────────────────────────
constexpr int SP1  = 4;
constexpr int SP2  = 8;
constexpr int SP3  = 12;
constexpr int SP4  = 16;
constexpr int SP5  = 20;
constexpr int SP6  = 24;
constexpr int SP8  = 32;
constexpr int SP10 = 40;
constexpr int SP12 = 48;
constexpr int SP16 = 64;

// ── Border Radii (in pixels) ─────────────────────────────────────────────────
constexpr int RADIUS_SM   = 6;    // badges, chips, small buttons
constexpr int RADIUS_MD   = 10;   // cards, inputs, dropdowns
constexpr int RADIUS_LG   = 14;   // modals, panels
constexpr int RADIUS_XL   = 20;   // feature sections

// ── Layout Constants ─────────────────────────────────────────────────────────
constexpr int SIDEBAR_W       = 260;  // sidebar fixed width
constexpr int CONTENT_MAX_W   = 1200; // content area max width
constexpr int CARD_PADDING    = 20;   // card inner padding
constexpr int GRID_GAP        = 16;   // grid gap between cards
constexpr int PAGE_PADDING    = 32;   // page-level padding
constexpr int MODAL_MAX_W     = 520;  // modal max width
constexpr int MIN_TARGET      = 44;   // minimum interactive target size

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

// ── GDI+ Drawing Helpers ────────────────────────────────────────────────────

inline Gdiplus::Color GdipColor(COLORREF cr, BYTE alpha = 255) {
    return Gdiplus::Color(alpha, GetRValue(cr), GetGValue(cr), GetBValue(cr));
}

inline void DrawRoundedCard(HDC hdc, const RECT& rc, int radius,
                            COLORREF fillColor, COLORREF borderColor,
                            int borderWidth = 1) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    int x = rc.left, y = rc.top;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    int d = radius * 2;
    if (d > w) d = w; if (d > h) d = h;

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d - 1, y, d, d, 270, 90);
    path.AddArc(x + w - d - 1, y + h - d - 1, d, d, 0, 90);
    path.AddArc(x, y + h - d - 1, d, d, 90, 90);
    path.CloseFigure();

    Gdiplus::SolidBrush fill(GdipColor(fillColor));
    g.FillPath(&fill, &path);

    if (borderWidth > 0) {
        Gdiplus::Pen pen(GdipColor(borderColor), (Gdiplus::REAL)borderWidth);
        g.DrawPath(&pen, &path);
    }
}

inline void DrawAccentCard(HDC hdc, const RECT& rc, int radius,
                           COLORREF fillColor, COLORREF borderColor,
                           COLORREF accentColor) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    int x = rc.left, y = rc.top;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    int d = radius * 2;
    if (d > w) d = w; if (d > h) d = h;

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d - 1, y, d, d, 270, 90);
    path.AddArc(x + w - d - 1, y + h - d - 1, d, d, 0, 90);
    path.AddArc(x, y + h - d - 1, d, d, 90, 90);
    path.CloseFigure();

    Gdiplus::SolidBrush fill(GdipColor(fillColor));
    g.FillPath(&fill, &path);

    // Top accent bar (3px, clipped to card)
    Gdiplus::Region oldClip;
    g.GetClip(&oldClip);
    g.SetClip(&path);
    Gdiplus::SolidBrush accentBrush(GdipColor(accentColor));
    g.FillRectangle(&accentBrush, x, y, w, 3);
    g.SetClip(&oldClip);

    Gdiplus::Pen pen(GdipColor(borderColor), 1.0f);
    g.DrawPath(&pen, &path);
}

inline void DrawGradientButton(HDC hdc, const RECT& rc, int radius,
                               COLORREF topColor, COLORREF bottomColor,
                               COLORREF borderColor = 0, int borderWidth = 0) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    int x = rc.left, y = rc.top;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    int d = radius * 2;
    if (d > w) d = w; if (d > h) d = h;

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d - 1, y, d, d, 270, 90);
    path.AddArc(x + w - d - 1, y + h - d - 1, d, d, 0, 90);
    path.AddArc(x, y + h - d - 1, d, d, 90, 90);
    path.CloseFigure();

    Gdiplus::LinearGradientBrush grad(
        Gdiplus::Point(x, y), Gdiplus::Point(x + w, y + h),
        GdipColor(topColor), GdipColor(bottomColor));
    g.FillPath(&grad, &path);

    if (borderWidth > 0) {
        Gdiplus::Pen pen(GdipColor(borderColor), (Gdiplus::REAL)borderWidth);
        g.DrawPath(&pen, &path);
    }
}

inline void DrawConfidenceBar(HDC hdc, int x, int y, int w, int h, int pct) {
    Gdiplus::Graphics g(hdc);
    // Track
    Gdiplus::SolidBrush trackBrush(GdipColor(BG_ROOT));
    g.FillRectangle(&trackBrush, x, y, w, h);
    // Fill with gradient
    int fillW = (w * pct) / 100;
    if (fillW > 0) {
        Gdiplus::LinearGradientBrush grad(
            Gdiplus::Point(x, y), Gdiplus::Point(x + w, y),
            GdipColor(ACCENT_BLUE), GdipColor(ACCENT_CYAN));
        g.FillRectangle(&grad, x, y, fillW, h);
    }
}

inline void DrawGlassPanel(HDC hdc, const RECT& rc, int radius,
                           BYTE alpha = 153) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    int x = rc.left, y = rc.top;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    int d = radius * 2;
    if (d > w) d = w; if (d > h) d = h;

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d - 1, y, d, d, 270, 90);
    path.AddArc(x + w - d - 1, y + h - d - 1, d, d, 0, 90);
    path.AddArc(x, y + h - d - 1, d, d, 90, 90);
    path.CloseFigure();

    Gdiplus::SolidBrush fill(GdipColor(BG_SURFACE, alpha));
    g.FillPath(&fill, &path);

    Gdiplus::Pen pen(Gdiplus::Color(20, 255, 255, 255), 1.0f);
    g.DrawPath(&pen, &path);
}

inline void DrawAlertBanner(HDC hdc, const RECT& rc, COLORREF accentColor) {
    Gdiplus::Graphics g(hdc);
    int x = rc.left, y = rc.top;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;

    Gdiplus::SolidBrush fill(GdipColor(accentColor, 20));
    g.FillRectangle(&fill, x, y, w, h);

    Gdiplus::Pen pen(GdipColor(accentColor, 64), 1.0f);
    g.DrawRectangle(&pen, x, y, w - 1, h - 1);

    Gdiplus::SolidBrush bar(GdipColor(accentColor));
    g.FillRectangle(&bar, x, y, 3, h);
}

inline void DrawPillBadge(HDC hdc, int x, int y, int w, int h,
                          COLORREF accentColor, const wchar_t* text,
                          HFONT font) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    int d = h;

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 90, 180);
    path.AddArc(x + w - d, y, d, d, 270, 180);
    path.CloseFigure();

    Gdiplus::SolidBrush fill(GdipColor(accentColor, 31));
    g.FillPath(&fill, &path);

    Gdiplus::Pen pen(GdipColor(accentColor, 64), 1.0f);
    g.DrawPath(&pen, &path);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, accentColor);
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    RECT textRc = { x + 4, y, x + w - 4, y + h };
    DrawText(hdc, text, -1, &textRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);
}

inline void DrawCardShadow(HDC hdc, const RECT& rc, int radius,
                           int offsetY = 2, int blur = 6) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    int x = rc.left, y = rc.top;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int d = radius * 2;
    if (d > w) d = w; if (d > h) d = h;

    for (int i = blur; i > 0; i -= 2) {
        Gdiplus::GraphicsPath path;
        path.AddArc(x - i, y + offsetY - i, d, d, 180, 90);
        path.AddArc(x + w - d - 1 + i, y + offsetY - i, d, d, 270, 90);
        path.AddArc(x + w - d - 1 + i, y + h - d - 1 + offsetY + i, d, d, 0, 90);
        path.AddArc(x - i, y + h - d - 1 + offsetY + i, d, d, 90, 90);
        path.CloseFigure();
        BYTE alpha = (BYTE)(20 - i * 3);
        if (alpha > 20) alpha = 0;
        Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(alpha, 0, 0, 0));
        g.FillPath(&shadowBrush, &path);
    }
}

} // namespace Theme
