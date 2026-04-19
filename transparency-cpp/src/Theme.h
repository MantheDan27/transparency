#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <uxtheme.h>
#include <dwmapi.h>
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#include <objbase.h>
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// ─── Design System: Transparency v4.21 ──────────────────────────────────────
// Warm beige + phthalocyanine blue — high readability, light theme.
// Semantic accent colors (green/amber/red) are preserved for trust, alerts,
// and risk — only backgrounds and primary text have changed.

namespace Theme {

// ── Backgrounds (4-layer depth system — warm beige) ─────────────────────────
constexpr COLORREF BG_ROOT      = RGB(252, 249, 238);  // #FCF9EE — warmest cream (sidebar, root)
constexpr COLORREF BG_SURFACE   = RGB(246, 242, 226);  // #F6F2E2 — main content area
constexpr COLORREF BG_ELEVATED  = RGB(238, 233, 210);  // #EEE9D2 — cards, hover states
constexpr COLORREF BG_OVERLAY   = RGB(228, 222, 196);  // #E4DEC4 — modals, dropdowns

// Derived background states
constexpr COLORREF BG_INPUT     = RGB(252, 249, 238);  // inputs use root layer
constexpr COLORREF BG_ROW_ALT   = RGB(249, 246, 232);  // subtle row alternation
constexpr COLORREF BG_ROW_HOV   = RGB(238, 233, 210);  // hover = elevated
constexpr COLORREF BG_ROW_SEL   = RGB(210, 222, 242);  // phthalo blue tint for selection
constexpr COLORREF BG_NAV_ACTIVE= RGB(218, 228, 248);  // nav active background

// Backward compatibility aliases
constexpr COLORREF BG_APP     = BG_ROOT;
constexpr COLORREF BG_SIDEBAR = BG_ROOT;
constexpr COLORREF BG_CARD    = BG_ELEVATED;

// ── Borders ──────────────────────────────────────────────────────────────────
constexpr COLORREF BORDER_DEFAULT = RGB(198, 190, 168);  // warm tan border
constexpr COLORREF BORDER_SUBTLE  = RGB(215, 208, 188);  // subtle separator
constexpr COLORREF BORDER_FOCUS   = RGB(0,   80,  195);  // phthalo blue focus ring

// Backward compatibility
constexpr COLORREF BORDER         = BORDER_DEFAULT;
constexpr COLORREF SIDEBAR_BORDER = BORDER_SUBTLE;

// ── Text ─────────────────────────────────────────────────────────────────────
constexpr COLORREF TEXT_PRIMARY   = RGB(0,   42,  108);  // phthalocyanine blue — headings, labels
constexpr COLORREF TEXT_SECONDARY = RGB(45,  85,  145);  // medium phthalo — body, descriptions
constexpr COLORREF TEXT_TERTIARY  = RGB(105, 135, 175);  // light phthalo — hints, timestamps

// Backward compatibility
constexpr COLORREF TEXT_MUTED = TEXT_TERTIARY;

// ── Accents (semantic roles preserved — darkened for light background) ────────
// Green = trusted / healthy / success
// Amber = warning / caution / unknown
// Red   = critical / blocked / destructive
constexpr COLORREF ACCENT_BLUE   = RGB(0,   80,  195);  // phthalocyanine blue — primary actions
constexpr COLORREF ACCENT_CYAN   = RGB(0,   120, 185);  // teal — confidence scores, power user
constexpr COLORREF ACCENT_GREEN  = RGB(0,   130, 55);   // forest green — trusted, healthy, success
constexpr COLORREF ACCENT_AMBER  = RGB(175, 95,  0);    // dark amber — warning, caution, unknown
constexpr COLORREF ACCENT_RED    = RGB(185, 25,  45);   // dark crimson — critical, blocked, destructive
constexpr COLORREF ACCENT_PURPLE = RGB(110, 25,  175);  // deep violet — watchlist, rare

// Backward compatibility
constexpr COLORREF ACCENT      = ACCENT_BLUE;
constexpr COLORREF ACCENT_GLOW = ACCENT_CYAN;
constexpr COLORREF SUCCESS     = ACCENT_GREEN;
constexpr COLORREF DANGER      = ACCENT_RED;
constexpr COLORREF WARNING     = ACCENT_AMBER;
constexpr COLORREF WATCHLIST   = ACCENT_PURPLE;

// ── Spacing (base-4 system) ───────────────────────────────────────────────────
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

// ── Border Radii ─────────────────────────────────────────────────────────────
constexpr int RADIUS_SM   = 6;
constexpr int RADIUS_MD   = 10;
constexpr int RADIUS_LG   = 14;
constexpr int RADIUS_XL   = 20;

// ── Layout Constants ─────────────────────────────────────────────────────────
constexpr int SIDEBAR_W       = 260;
constexpr int CONTENT_MAX_W   = 1200;
constexpr int CARD_PADDING    = 20;
constexpr int GRID_GAP        = 16;
constexpr int PAGE_PADDING    = 32;
constexpr int MODAL_MAX_W     = 520;
constexpr int MIN_TARGET      = 44;

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

// Backward compatibility aliases
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

// ── Font face detection ───────────────────────────────────────────────────────
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

// ── Font creation helper ──────────────────────────────────────────────────────
inline HFONT MakeFont(int pxSize, int weight, bool mono = false) {
    return CreateFont(
        -MulDiv(pxSize, GetDeviceCaps(GetDC(NULL), LOGPIXELSY), 96),
        0, 0, 0, weight, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        mono ? (FIXED_PITCH | FF_MODERN) : (DEFAULT_PITCH | FF_SWISS),
        mono ? FontFaceMono() : FontFaceSans());
}

// ── Type Scale ────────────────────────────────────────────────────────────────
inline HFONT FontDisplay() { static HFONT f = MakeFont(48, FW_BOLD);     return f; }
inline HFONT FontH1()      { static HFONT f = MakeFont(32, FW_BOLD);     return f; }
inline HFONT FontH2()      { static HFONT f = MakeFont(24, FW_SEMIBOLD); return f; }
inline HFONT FontH3()      { static HFONT f = MakeFont(18, FW_SEMIBOLD); return f; }
inline HFONT FontBody()    { static HFONT f = MakeFont(16, FW_NORMAL);   return f; }
inline HFONT FontBodySm()  { static HFONT f = MakeFont(14, FW_NORMAL);   return f; }
inline HFONT FontCaption() { static HFONT f = MakeFont(12, FW_MEDIUM);   return f; }
inline HFONT FontMono()    { static HFONT f = MakeFont(14, FW_NORMAL, true); return f; }
inline HFONT FontNavActive()   { static HFONT f = MakeFont(13, FW_SEMIBOLD); return f; }
inline HFONT FontNavInactive() { return FontBodySm(); }
inline HFONT FontBold()    { static HFONT f = MakeFont(15, FW_SEMIBOLD); return f; }
inline HFONT FontHeader()  { return FontH1(); }
inline HFONT FontSmall()   { return FontBodySm(); }
inline HFONT FontBrand()   { return FontH2(); }

// ── Apply theme to controls ───────────────────────────────────────────────────
inline void ApplyDarkScrollbar(HWND hwnd) {
    // Light theme: standard Explorer scrollbar (no dark mode override)
    SetWindowTheme(hwnd, L"Explorer", nullptr);
}

inline void ApplyDarkEdit(HWND hwnd) {
    // Light theme: no special theming — WM_CTLCOLOREDIT sets colors
    (void)hwnd;
}

inline void SetDarkTitlebar(HWND hwnd) {
    // Light theme: light titlebar
    BOOL dark = FALSE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
}

// ── GDI+ helpers ─────────────────────────────────────────────────────────────

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
    int d = radius * 2; if (d > w) d = w; if (d > h) d = h;

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
    int d = radius * 2; if (d > w) d = w; if (d > h) d = h;

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d - 1, y, d, d, 270, 90);
    path.AddArc(x + w - d - 1, y + h - d - 1, d, d, 0, 90);
    path.AddArc(x, y + h - d - 1, d, d, 90, 90);
    path.CloseFigure();

    Gdiplus::SolidBrush fill(GdipColor(fillColor));
    g.FillPath(&fill, &path);

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
    int d = radius * 2; if (d > w) d = w; if (d > h) d = h;

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

// ── Button drawing (light beige theme) ───────────────────────────────────────
// variant: 0 = primary (phthalo blue fill, white text)
//          1 = secondary (elevated beige, phthalo border)
//          2 = destructive (dark red fill, red border)
// selected: focus ring indicator
inline void DrawGlassButton(HDC hdc, const RECT& rc, int radius,
                            bool pressed, int variant = 0,
                            bool selected = false) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    int x = rc.left, y = rc.top;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    int d = radius * 2; if (d > w) d = w; if (d > h) d = h;

    // Focus ring — drawn outside the button shape before filling
    if (selected) {
        Gdiplus::GraphicsPath focusPath;
        focusPath.AddArc(x - 2, y - 2, d, d, 180, 90);
        focusPath.AddArc(x + w - d - 1 + 2, y - 2, d, d, 270, 90);
        focusPath.AddArc(x + w - d - 1 + 2, y + h - d - 1 + 2, d, d, 0, 90);
        focusPath.AddArc(x - 2, y + h - d - 1 + 2, d, d, 90, 90);
        focusPath.CloseFigure();
        Gdiplus::Pen focusPen(GdipColor(ACCENT_BLUE, 180), 2.0f);
        g.DrawPath(&focusPen, &focusPath);
    }

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d - 1, y, d, d, 270, 90);
    path.AddArc(x + w - d - 1, y + h - d - 1, d, d, 0, 90);
    path.AddArc(x, y + h - d - 1, d, d, 90, 90);
    path.CloseFigure();

    if (variant == 0) {
        // Primary: solid phthalo blue gradient
        COLORREF topC = pressed ? RGB(0, 62, 162) : RGB(0, 80, 195);
        COLORREF botC = pressed ? RGB(0, 48, 136) : RGB(0, 62, 168);
        Gdiplus::LinearGradientBrush grad(
            Gdiplus::Point(x, y), Gdiplus::Point(x, y + h),
            GdipColor(topC), GdipColor(botC));
        g.FillPath(&grad, &path);
        // White shimmer on top portion
        {
            Gdiplus::Region clip; g.GetClip(&clip);
            g.SetClip(&path);
            Gdiplus::LinearGradientBrush shimmer(
                Gdiplus::Point(x, y), Gdiplus::Point(x, y + h / 2),
                Gdiplus::Color(35, 255, 255, 255),
                Gdiplus::Color(0,  255, 255, 255));
            g.FillRectangle(&shimmer, x, y, w, h / 2);
            g.SetClip(&clip);
        }
        Gdiplus::Pen pen(GdipColor(pressed ? RGB(0,48,136) : RGB(0,55,158), 220), 1.0f);
        g.DrawPath(&pen, &path);

    } else if (variant == 2) {
        // Destructive: dark crimson gradient
        COLORREF topC = pressed ? RGB(155, 20, 38) : RGB(185, 25, 45);
        COLORREF botC = pressed ? RGB(130, 15, 30) : RGB(160, 20, 40);
        Gdiplus::LinearGradientBrush grad(
            Gdiplus::Point(x, y), Gdiplus::Point(x, y + h),
            GdipColor(topC), GdipColor(botC));
        g.FillPath(&grad, &path);
        {
            Gdiplus::Region clip; g.GetClip(&clip);
            g.SetClip(&path);
            Gdiplus::LinearGradientBrush shimmer(
                Gdiplus::Point(x, y), Gdiplus::Point(x, y + h / 2),
                Gdiplus::Color(30, 255, 255, 255),
                Gdiplus::Color(0,  255, 255, 255));
            g.FillRectangle(&shimmer, x, y, w, h / 2);
            g.SetClip(&clip);
        }
        Gdiplus::Pen pen(GdipColor(pressed ? RGB(130,15,30) : RGB(185,25,45), 220), 1.0f);
        g.DrawPath(&pen, &path);

    } else {
        // Secondary: elevated beige — fills track the palette automatically
        Gdiplus::LinearGradientBrush grad(
            Gdiplus::Point(x, y), Gdiplus::Point(x, y + h),
            GdipColor(pressed ? BG_OVERLAY : BG_ELEVATED),
            GdipColor(BG_SURFACE));
        g.FillPath(&grad, &path);
        // Subtle dark edge at top (gives raised appearance on light bg)
        {
            Gdiplus::Region clip; g.GetClip(&clip);
            g.SetClip(&path);
            Gdiplus::LinearGradientBrush edge(
                Gdiplus::Point(x, y), Gdiplus::Point(x, y + 4),
                Gdiplus::Color(16, 0, 42, 108),
                Gdiplus::Color(0,  0, 42, 108));
            g.FillRectangle(&edge, x, y, w, 4);
            g.SetClip(&clip);
        }
        Gdiplus::Pen pen(GdipColor(BORDER_DEFAULT, 200), 1.0f);
        g.DrawPath(&pen, &path);
    }
}

// ── Filter pill (light beige theme) ─────────────────────────────────────────
// Active: phthalo blue tinted background + border
// Inactive: barely-there warm tint
inline void DrawGlassPill(HDC hdc, const RECT& rc, int radius,
                          bool active, bool pressed) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    int x = rc.left, y = rc.top;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    int d = radius * 2; if (d > w) d = w; if (d > h) d = h;

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d - 1, y, d, d, 270, 90);
    path.AddArc(x + w - d - 1, y + h - d - 1, d, d, 0, 90);
    path.AddArc(x, y + h - d - 1, d, d, 90, 90);
    path.CloseFigure();

    if (active) {
        // Active: phthalo blue tinted fill + solid border
        Gdiplus::LinearGradientBrush fill(
            Gdiplus::Point(x, y), Gdiplus::Point(x, y + h),
            Gdiplus::Color(58, 0, 80, 195),
            Gdiplus::Color(40, 0, 60, 165));
        g.FillPath(&fill, &path);
        Gdiplus::Pen pen(Gdiplus::Color(90, 0, 80, 195), 1.2f);
        g.DrawPath(&pen, &path);
    } else if (pressed) {
        Gdiplus::SolidBrush fill(Gdiplus::Color(35, 0, 80, 195));
        g.FillPath(&fill, &path);
        Gdiplus::Pen pen(Gdiplus::Color(50, 0, 80, 195), 1.0f);
        g.DrawPath(&pen, &path);
    } else {
        // Inactive: barely-there warm tint
        Gdiplus::SolidBrush fill(Gdiplus::Color(20, 120, 100, 60));
        g.FillPath(&fill, &path);
        Gdiplus::Pen pen(Gdiplus::Color(35, 100, 80, 50), 0.8f);
        g.DrawPath(&pen, &path);
    }
}

inline void DrawConfidenceBar(HDC hdc, int x, int y, int w, int h, int pct) {
    Gdiplus::Graphics g(hdc);
    // Track uses BG_ELEVATED (slightly darker than surface) for visibility on light bg
    Gdiplus::SolidBrush trackBrush(GdipColor(BG_ELEVATED));
    g.FillRectangle(&trackBrush, x, y, w, h);
    int fillW = (w * pct) / 100;
    if (fillW > 0) {
        Gdiplus::LinearGradientBrush grad(
            Gdiplus::Point(x, y), Gdiplus::Point(x + w, y),
            GdipColor(ACCENT_BLUE), GdipColor(ACCENT_CYAN));
        g.FillRectangle(&grad, x, y, fillW, h);
    }
}

inline void DrawGlassPanel(HDC hdc, const RECT& rc, int radius, BYTE alpha = 100) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    int x = rc.left, y = rc.top;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;
    int d = radius * 2; if (d > w) d = w; if (d > h) d = h;

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d - 1, y, d, d, 270, 90);
    path.AddArc(x + w - d - 1, y + h - d - 1, d, d, 0, 90);
    path.AddArc(x, y + h - d - 1, d, d, 90, 90);
    path.CloseFigure();

    Gdiplus::SolidBrush fill(GdipColor(BG_SURFACE, alpha));
    g.FillPath(&fill, &path);

    // Phthalo blue panel border (darker than bg — visible on light surface)
    Gdiplus::Pen pen(Gdiplus::Color(40, 0, 42, 108), 1.0f);
    g.DrawPath(&pen, &path);
}

inline void DrawAlertBanner(HDC hdc, const RECT& rc, COLORREF accentColor) {
    Gdiplus::Graphics g(hdc);
    int x = rc.left, y = rc.top;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;

    Gdiplus::SolidBrush fill(GdipColor(accentColor, 22));
    g.FillRectangle(&fill, x, y, w, h);

    Gdiplus::Pen pen(GdipColor(accentColor, 72), 1.0f);
    g.DrawRectangle(&pen, x, y, w - 1, h - 1);

    Gdiplus::SolidBrush bar(GdipColor(accentColor));
    g.FillRectangle(&bar, x, y, 3, h);
}

inline void DrawPillBadge(HDC hdc, int x, int y, int w, int h,
                          COLORREF accentColor, const wchar_t* text, HFONT font) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    int diam = h;

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, diam, diam, 90, 180);
    path.AddArc(x + w - diam, y, diam, diam, 270, 180);
    path.CloseFigure();

    Gdiplus::SolidBrush fill(GdipColor(accentColor, 28));
    g.FillPath(&fill, &path);

    Gdiplus::Pen pen(GdipColor(accentColor, 72), 1.0f);
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
    int d = radius * 2; if (d > w) d = w; if (d > h) d = h;

    for (int i = blur; i > 0; i -= 2) {
        Gdiplus::GraphicsPath path;
        path.AddArc(x - i, y + offsetY - i, d, d, 180, 90);
        path.AddArc(x + w - d - 1 + i, y + offsetY - i, d, d, 270, 90);
        path.AddArc(x + w - d - 1 + i, y + h - d - 1 + offsetY + i, d, d, 0, 90);
        path.AddArc(x - i, y + h - d - 1 + offsetY + i, d, d, 90, 90);
        path.CloseFigure();
        BYTE alpha = (BYTE)(14 - i * 2);
        if ((int)alpha > 14) alpha = 0;
        Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(alpha, 0, 42, 108));
        g.FillPath(&shadowBrush, &path);
    }
}

} // namespace Theme
