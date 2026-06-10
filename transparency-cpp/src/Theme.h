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

// ─── Design System: Transparency — Dark Cyan Edition ────────────────────────
// Pure black backgrounds, double-outline cyan borders, light cyan-white text.
// Semantic accent colors (green/amber/red) brightened for dark background.

namespace Theme {

// ── Backgrounds (4-layer depth system — pure black) ─────────────────────────
constexpr COLORREF BG_ROOT      = RGB(0,   0,   0);    // #000000 — pure black
constexpr COLORREF BG_SURFACE   = RGB(5,   8,  12);    // near-black surface
constexpr COLORREF BG_ELEVATED  = RGB(10,  15,  22);   // cards, hover states
constexpr COLORREF BG_OVERLAY   = RGB(15,  20,  30);   // modals, dropdowns

// Derived background states
constexpr COLORREF BG_INPUT     = RGB(5,   8,  12);    // inputs
constexpr COLORREF BG_ROW_ALT   = RGB(7,   10,  16);   // subtle row alternation
constexpr COLORREF BG_ROW_HOV   = RGB(12,  18,  26);   // hover = elevated
constexpr COLORREF BG_ROW_SEL   = RGB(0,   30,  55);   // cyan-tinted selection
constexpr COLORREF BG_NAV_ACTIVE= RGB(0,   22,  42);   // nav active background

// Backward compatibility aliases
constexpr COLORREF BG_APP     = BG_ROOT;
constexpr COLORREF BG_SIDEBAR = BG_ROOT;
constexpr COLORREF BG_CARD    = BG_ELEVATED;

// ── Borders — cyan ───────────────────────────────────────────────────────────
constexpr COLORREF BORDER_DEFAULT = RGB(0,  200, 220);  // bright cyan border
constexpr COLORREF BORDER_SUBTLE  = RGB(0,   90, 110);  // dim cyan separator
constexpr COLORREF BORDER_FOCUS   = RGB(0,  240, 255);  // bright cyan focus ring

// Backward compatibility
constexpr COLORREF BORDER         = BORDER_DEFAULT;
constexpr COLORREF SIDEBAR_BORDER = BORDER_SUBTLE;

// ── Text — readable on black ─────────────────────────────────────────────────
constexpr COLORREF TEXT_PRIMARY   = RGB(210, 240, 255); // near-white cyan — headings
constexpr COLORREF TEXT_SECONDARY = RGB(130, 190, 215); // medium cyan-blue — body
constexpr COLORREF TEXT_TERTIARY  = RGB(65,  120, 150); // muted cyan — hints, timestamps

// Backward compatibility
constexpr COLORREF TEXT_MUTED = TEXT_TERTIARY;

// ── Accents (brightened for dark background) ─────────────────────────────────
constexpr COLORREF ACCENT_BLUE   = RGB(30,  140, 255);  // vivid blue — primary actions
constexpr COLORREF ACCENT_CYAN   = RGB(0,   220, 255);  // vivid cyan — confidence, power user
constexpr COLORREF ACCENT_GREEN  = RGB(0,   195,  80);  // vivid green — trusted, healthy
constexpr COLORREF ACCENT_AMBER  = RGB(255, 165,   0);  // vivid amber — warning, caution
constexpr COLORREF ACCENT_RED    = RGB(255,  55,  75);  // vivid red — critical, blocked
constexpr COLORREF ACCENT_PURPLE = RGB(165,  75, 255);  // vivid violet — watchlist

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
    SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
}

inline void ApplyDarkEdit(HWND hwnd) {
    (void)hwnd;
}

inline void SetDarkTitlebar(HWND hwnd) {
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
}

// ── GDI+ helpers ─────────────────────────────────────────────────────────────

inline Gdiplus::Color GdipColor(COLORREF cr, BYTE alpha = 255) {
    return Gdiplus::Color(alpha, GetRValue(cr), GetGValue(cr), GetBValue(cr));
}

// Draws a double cyan outline on a GDI+ path:
//   outer ring: wider pen, 40% alpha (halo)
//   inner ring: 1px pen, full opacity (sharp edge)
inline void DrawDoubleOutline(Gdiplus::Graphics& g, Gdiplus::GraphicsPath& path,
                              COLORREF color, float innerWidth = 1.0f) {
    Gdiplus::Pen outerPen(GdipColor(color, 80), innerWidth + 2.5f);
    g.DrawPath(&outerPen, &path);
    Gdiplus::Pen innerPen(GdipColor(color, 220), innerWidth);
    g.DrawPath(&innerPen, &path);
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
        DrawDoubleOutline(g, path, borderColor, (float)borderWidth);
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

    DrawDoubleOutline(g, path, borderColor);
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
        DrawDoubleOutline(g, path, borderColor, (float)borderWidth);
    }
}

// ── Button drawing (dark cyan theme) ─────────────────────────────────────────
// variant: 0 = primary (dark blue fill, cyan double border)
//          1 = secondary (dark elevated, cyan double border)
//          2 = destructive (dark red fill, red double border)
// selected: cyan focus ring
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

    // Cyan focus ring — drawn outside the button shape
    if (selected) {
        Gdiplus::GraphicsPath focusPath;
        focusPath.AddArc(x - 3, y - 3, d, d, 180, 90);
        focusPath.AddArc(x + w - d - 1 + 3, y - 3, d, d, 270, 90);
        focusPath.AddArc(x + w - d - 1 + 3, y + h - d - 1 + 3, d, d, 0, 90);
        focusPath.AddArc(x - 3, y + h - d - 1 + 3, d, d, 90, 90);
        focusPath.CloseFigure();
        Gdiplus::Pen focusPen(GdipColor(BORDER_FOCUS, 200), 2.0f);
        g.DrawPath(&focusPen, &focusPath);
    }

    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d - 1, y, d, d, 270, 90);
    path.AddArc(x + w - d - 1, y + h - d - 1, d, d, 0, 90);
    path.AddArc(x, y + h - d - 1, d, d, 90, 90);
    path.CloseFigure();

    if (variant == 0) {
        // Primary: dark blue fill + cyan double border
        COLORREF topC = pressed ? RGB(0, 25, 75) : RGB(0, 35, 100);
        COLORREF botC = pressed ? RGB(0, 18, 58) : RGB(0, 25, 78);
        Gdiplus::LinearGradientBrush grad(
            Gdiplus::Point(x, y), Gdiplus::Point(x, y + h),
            GdipColor(topC), GdipColor(botC));
        g.FillPath(&grad, &path);
        // Subtle cyan shimmer on top
        {
            Gdiplus::Region clip; g.GetClip(&clip);
            g.SetClip(&path);
            Gdiplus::LinearGradientBrush shimmer(
                Gdiplus::Point(x, y), Gdiplus::Point(x, y + h / 2),
                Gdiplus::Color(25, 0, 220, 255),
                Gdiplus::Color(0,  0, 220, 255));
            g.FillRectangle(&shimmer, x, y, w, h / 2);
            g.SetClip(&clip);
        }
        DrawDoubleOutline(g, path, BORDER_DEFAULT);

    } else if (variant == 2) {
        // Destructive: dark red fill + red double border
        COLORREF topC = pressed ? RGB(100, 10, 22) : RGB(130, 15, 30);
        COLORREF botC = pressed ? RGB(80,  8,  18) : RGB(105, 10, 24);
        Gdiplus::LinearGradientBrush grad(
            Gdiplus::Point(x, y), Gdiplus::Point(x, y + h),
            GdipColor(topC), GdipColor(botC));
        g.FillPath(&grad, &path);
        {
            Gdiplus::Region clip; g.GetClip(&clip);
            g.SetClip(&path);
            Gdiplus::LinearGradientBrush shimmer(
                Gdiplus::Point(x, y), Gdiplus::Point(x, y + h / 2),
                Gdiplus::Color(20, 255, 55, 75),
                Gdiplus::Color(0,  255, 55, 75));
            g.FillRectangle(&shimmer, x, y, w, h / 2);
            g.SetClip(&clip);
        }
        DrawDoubleOutline(g, path, ACCENT_RED);

    } else {
        // Secondary: dark elevated fill + cyan double border
        Gdiplus::LinearGradientBrush grad(
            Gdiplus::Point(x, y), Gdiplus::Point(x, y + h),
            GdipColor(pressed ? BG_OVERLAY : BG_ELEVATED),
            GdipColor(BG_SURFACE));
        g.FillPath(&grad, &path);
        DrawDoubleOutline(g, path, BORDER_DEFAULT);
    }
}

// ── Filter pill (dark cyan theme) ────────────────────────────────────────────
// Active: cyan-tinted dark background + cyan double border
// Inactive: nearly invisible on dark bg
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
        // Active: dark cyan tint fill + cyan double border
        Gdiplus::LinearGradientBrush fill(
            Gdiplus::Point(x, y), Gdiplus::Point(x, y + h),
            Gdiplus::Color(55, 0, 220, 255),
            Gdiplus::Color(35, 0, 180, 210));
        g.FillPath(&fill, &path);
        DrawDoubleOutline(g, path, BORDER_DEFAULT);
    } else if (pressed) {
        Gdiplus::SolidBrush fill(Gdiplus::Color(40, 0, 200, 220));
        g.FillPath(&fill, &path);
        Gdiplus::Pen pen(GdipColor(BORDER_SUBTLE, 120), 1.0f);
        g.DrawPath(&pen, &path);
    } else {
        // Inactive: very subtle dark fill + dim cyan outline
        Gdiplus::SolidBrush fill(Gdiplus::Color(18, 0, 200, 220));
        g.FillPath(&fill, &path);
        Gdiplus::Pen pen(GdipColor(BORDER_SUBTLE, 80), 0.8f);
        g.DrawPath(&pen, &path);
    }
}

inline void DrawConfidenceBar(HDC hdc, int x, int y, int w, int h, int pct) {
    Gdiplus::Graphics g(hdc);
    // Track uses BG_ELEVATED on dark background
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

    Gdiplus::SolidBrush fill(GdipColor(BG_ELEVATED, alpha));
    g.FillPath(&fill, &path);

    DrawDoubleOutline(g, path, BORDER_DEFAULT);
}

inline void DrawAlertBanner(HDC hdc, const RECT& rc, COLORREF accentColor) {
    Gdiplus::Graphics g(hdc);
    int x = rc.left, y = rc.top;
    int w = rc.right - rc.left, h = rc.bottom - rc.top;

    Gdiplus::SolidBrush fill(GdipColor(accentColor, 28));
    g.FillRectangle(&fill, x, y, w, h);

    Gdiplus::Pen pen(GdipColor(accentColor, 140), 1.0f);
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

    Gdiplus::SolidBrush fill(GdipColor(accentColor, 35));
    g.FillPath(&fill, &path);

    DrawDoubleOutline(g, path, accentColor);

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

    // On pure black, shadows are subtle cyan glow instead of dark drop shadow
    for (int i = blur; i > 0; i -= 2) {
        Gdiplus::GraphicsPath path;
        path.AddArc(x - i, y + offsetY - i, d, d, 180, 90);
        path.AddArc(x + w - d - 1 + i, y + offsetY - i, d, d, 270, 90);
        path.AddArc(x + w - d - 1 + i, y + h - d - 1 + offsetY + i, d, d, 0, 90);
        path.AddArc(x - i, y + h - d - 1 + offsetY + i, d, d, 90, 90);
        path.CloseFigure();
        BYTE alpha = (BYTE)(10 - i);
        if ((int)alpha > 10) alpha = 0;
        Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(alpha, 0, 200, 220));
        g.FillPath(&shadowBrush, &path);
    }
}

} // namespace Theme
