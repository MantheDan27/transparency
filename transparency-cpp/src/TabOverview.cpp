#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <iphlpapi.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <map>

#pragma comment(lib, "iphlpapi.lib")

#include "TabOverview.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"
#include "Scanner.h"
#include "GlossaryPopup.h"

using std::wstring;

const wchar_t* TabOverview::s_className = L"TransparencyTabOverview";

static wstring ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return L"";
    wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

static const wchar_t* KPI_LABELS[] = {
    L"Devices Online",
    L"Unknown Devices",
    L"Active Alerts",
    L"Gateway Latency"
};

// Accent colors for each KPI tile (top border + sparkline) — from design system
static const COLORREF KPI_ACCENTS[] = {
    Theme::ACCENT_GREEN,   // Devices Online  — success/healthy
    Theme::ACCENT_AMBER,   // Unknown Devices — warning/caution
    Theme::ACCENT_RED,     // Active Alerts   — critical
    Theme::ACCENT_BLUE,    // Gateway Latency — primary/info
};

bool TabOverview::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
    _mainWnd = mainWnd;

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.hbrBackground = Theme::BrushSurface();
    wc.lpszClassName = s_className;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    RegisterClassEx(&wc);

    _hwnd = CreateWindowEx(0, s_className, nullptr,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VSCROLL,
        x, y, w, h, parent, nullptr, GetModuleHandle(nullptr), this);

    return _hwnd != nullptr;
}

LRESULT CALLBACK TabOverview::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TabOverview* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<TabOverview*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TabOverview*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:
        return self->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lp));
    case WM_SIZE:
        self->OnSize(hwnd, LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_PAINT:
        return self->OnPaint(hwnd);
    case WM_ERASEBKGND:
        return 1;  // Suppress — OnPaint handles all drawing via double buffer
    case WM_COMMAND:
        return self->OnCommand(hwnd, wp, lp);
    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<NMHDR*>(lp);
        if (hdr && (hdr->code == NM_CLICK || hdr->code == NM_RETURN) &&
            hdr->idFrom == IDC_LINK_GENTLE_INFO) {
            auto* nmLink = reinterpret_cast<NMLINK*>(hdr);
            wstring href(nmLink->item.szUrl);
            HWND rootWnd = GetAncestor(hwnd, GA_ROOT);
            const wchar_t* exp = LookupAcronym(href);
            if (exp) ShowGlossaryPopup(rootWnd, href, exp);
        }
        return 0;
    }
    case WM_DRAWITEM:
        return self->OnDrawItem(hwnd, reinterpret_cast<DRAWITEMSTRUCT*>(lp));
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SetBkColor(hdc, Theme::BG_SURFACE);
        return (LRESULT)Theme::BrushSurface();
    }
    case WM_VSCROLL:
        return self->OnVScroll(hwnd, wp);
    case WM_MOUSEWHEEL:
        return self->OnMouseWheel(hwnd, GET_WHEEL_DELTA_WPARAM(wp));
    case WM_SCAN_COMPLETE:
        return self->OnScanComplete(hwnd);
    case WM_SCAN_PROGRESS:
        return self->OnScanProgress(hwnd, wp, lp);
    case WM_MONITOR_TICK:
        return self->OnMonitorTick(hwnd);
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
}

LRESULT TabOverview::OnCreate(HWND hwnd, LPCREATESTRUCT cs) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    CreateControls(hwnd, rc.right, rc.bottom);
    RefreshNetworkInfo();
    return 0;
}

// ── Layout constants ──────────────────────────────────────────────────────────
static const int TILE_Y     = 120;  // pushed down for NIC selector row
static const int TILE_H     = 110;  // taller for Display-size numbers + sparkline
static const int PILL_Y_OFF = 36;
static const int BTN_H      = 44;   // min interactive target per design system
static const int DASH_MIN_H = 380;  // minimum security dashboard height

static void GetLayoutMetrics(int cx, int /*cy*/,
    int& tileW, int& pillY, int& btnY, int& listY) {
    tileW = (cx - 40) / 4;
    pillY = TILE_Y + TILE_H + PILL_Y_OFF;
    btnY  = pillY + 34;
    listY = btnY + BTN_H + 52;
}

// Compute total content height (may exceed viewport → scrollable)
static int ComputeContentHeight(int cx, int cy) {
    int tileW, pillY, btnY, listY;
    GetLayoutMetrics(cx, cy, tileW, pillY, btnY, listY);
    int mapH = std::max(cy - listY - 16, DASH_MIN_H);
    return listY + mapH + 40;
}

void TabOverview::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    int tileW, pillY, btnY, listY;
    GetLayoutMetrics(cx, cy, tileW, pillY, btnY, listY);

    // KPI tiles — owner-drawn buttons so we can paint number + sparkline
    for (int i = 0; i < 4; i++) {
        int x = 16 + i * (tileW + 8);
        _hKpi[i] = CreateWindowEx(0, L"BUTTON", nullptr,
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            x, TILE_Y, tileW, TILE_H,
            hwnd, (HMENU)(INT_PTR)(IDC_STATIC_KPI1 + i), hInst, nullptr);
    }

    // Scan mode pills
    _hModeQuick = CreateWindowEx(0, L"BUTTON", L"Quick",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        16, pillY, 72, 24, hwnd, (HMENU)9200, hInst, nullptr);
    SendMessage(_hModeQuick, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    // Standard is the recommended default — marked with a star
    _hModeStandard = CreateWindowEx(0, L"BUTTON", L"Standard ★",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        94, pillY, 110, 24, hwnd, (HMENU)9201, hInst, nullptr);
    SendMessage(_hModeStandard, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hModeStandard, BM_SETCHECK, BST_CHECKED, 0);  // recommended default

    _hModeDeep = CreateWindowEx(0, L"BUTTON", L"Deep",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        210, pillY, 68, 24, hwnd, (HMENU)9202, hInst, nullptr);
    SendMessage(_hModeDeep, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hCheckGentle = CreateWindowEx(0, L"BUTTON", L"Gentle Mode",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        290, pillY, 108, 24, hwnd, (HMENU)IDC_CHECK_GENTLE, hInst, nullptr);
    SendMessage(_hCheckGentle, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    // [?] info link next to Gentle Mode
    _hLinkGentleInfo = CreateWindowEx(0, WC_LINK, L"<a href=\"GENTLE\">[?]</a>",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        402, pillY + 2, 24, 20, hwnd, (HMENU)IDC_LINK_GENTLE_INFO, hInst, nullptr);
    SendMessage(_hLinkGentleInfo, WM_SETFONT, (WPARAM)Theme::FontBodySm(), TRUE);
    SubclassLinkCtrl(_hLinkGentleInfo);

    // Mode description label — shows a one-liner about the selected scan mode
    _hModeDesc = CreateWindowEx(0, L"STATIC",
        L"Recommended ★ — balanced discovery, finishes in about 60 seconds",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
        16, pillY + 28, 500, 18, hwnd, (HMENU)IDC_STATIC_MODE_DESC, hInst, nullptr);
    SendMessage(_hModeDesc, WM_SETFONT, (WPARAM)Theme::FontBodySm(), TRUE);

    // Action buttons — owner-drawn for design system styling
    _hBtnQuickScan = CreateWindowEx(0, L"BUTTON", L"Quick Scan",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        Theme::SP4, btnY, 120, BTN_H, hwnd, (HMENU)IDC_BTN_SCAN_QUICK, hInst, nullptr);

    _hBtnDeepScan = CreateWindowEx(0, L"BUTTON", L"Deep Scan",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        Theme::SP4 + 128, btnY, 120, BTN_H, hwnd, (HMENU)IDC_BTN_SCAN_DEEP, hInst, nullptr);

    _hBtnMonStart = CreateWindowEx(0, L"BUTTON", L"Start Monitor",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        Theme::SP4 + 256, btnY, 130, BTN_H, hwnd, (HMENU)IDC_BTN_MONITOR_START, hInst, nullptr);

    _hBtnMonStop = CreateWindowEx(0, L"BUTTON", L"Stop Monitor",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        Theme::SP4 + 394, btnY, 130, BTN_H, hwnd, (HMENU)IDC_BTN_MONITOR_STOP, hInst, nullptr);
    EnableWindow(_hBtnMonStop, FALSE);

    _hBtnExport = CreateWindowEx(0, L"BUTTON", L"Export Report",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        Theme::SP4 + 532, btnY, 130, BTN_H, hwnd, (HMENU)IDC_BTN_EXPORT, hInst, nullptr);

    _hBtnNotif = CreateWindowEx(0, L"BUTTON", L"Notifications",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        Theme::SP4 + 670, btnY, 130, BTN_H, hwnd, (HMENU)IDC_BTN_NOTIFICATIONS, hInst, nullptr);

    // Progress / status
    _hStatusText = CreateWindowEx(0, L"STATIC", L"Ready. Run a scan to discover devices.",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, btnY + BTN_H + 8, cx - 32, 20,
        hwnd, (HMENU)IDC_STATIC_STATUS, hInst, nullptr);
    SendMessage(_hStatusText, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        16, btnY + BTN_H + 32, cx - 32, 8,
        hwnd, nullptr, hInst, nullptr);
    SendMessage(_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

    // Network info — NIC selection row
    _hNetworkInfo = CreateWindowEx(0, L"STATIC", L"Detecting network...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 16, cx - 400, 40,
        hwnd, (HMENU)IDC_STATIC_NET_INFO, hInst, nullptr);
    SendMessage(_hNetworkInfo, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    // NIC selector combo
    _hNicCombo = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        16, 44, 380, 160, hwnd, (HMENU)IDC_COMBO_NIC_SELECT, hInst, nullptr);
    SendMessage(_hNicCombo, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hNicPin = CreateWindowEx(0, L"BUTTON", L"Pin",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        402, 44, 50, 24, hwnd, (HMENU)IDC_BTN_NIC_PIN, hInst, nullptr);
    SendMessage(_hNicPin, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);

    // NIC reason text
    _hNicReason = CreateWindowEx(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 72, cx - 32, 30,
        hwnd, nullptr, hInst, nullptr);
    SendMessage(_hNicReason, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);

    // Right-side changes list  (40% of bottom width)
    int mapW = (cx - 40) * 6 / 10;
    int listX = 16 + mapW + 8;
    int listW = cx - listX - 16;
    int listH = cy - listY - 16;

    _hChangesList = CreateWindowEx(
        WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | WS_VSCROLL,
        listX, listY, std::max(listW, 100), std::max(listH, 50),
        hwnd, (HMENU)IDC_LIST_CHANGES, hInst, nullptr);

    SendMessage(_hChangesList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hChangesList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    Theme::ApplyDarkScrollbar(_hChangesList);

    LVCOLUMN col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt  = LVCFMT_LEFT;

    col.cx = 85; col.pszText = (LPWSTR)L"Time";
    ListView_InsertColumn(_hChangesList, 0, &col);

    col.cx = 110; col.pszText = (LPWSTR)L"Change";
    ListView_InsertColumn(_hChangesList, 1, &col);

    col.cx = 280; col.pszText = (LPWSTR)L"Details";
    ListView_InsertColumn(_hChangesList, 2, &col);
}

void TabOverview::LayoutControls(int cx, int cy) {
    if (!_hwnd) return;

    _viewHeight = cy;
    _contentHeight = ComputeContentHeight(cx, cy);

    int sOff = -_scrollY;  // scroll offset applied to all positions

    int tileW, pillY, btnY, listY;
    GetLayoutMetrics(cx, cy, tileW, pillY, btnY, listY);

    // Batch all repositioning to reduce flicker
    HDWP hdwp = BeginDeferWindowPos(20);

    auto deferMove = [&](HWND hw, int x, int y, int w, int h) {
        if (hw && hdwp)
            hdwp = DeferWindowPos(hdwp, hw, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    };

    for (int i = 0; i < 4; i++) {
        int x = 16 + i * (tileW + 8);
        deferMove(_hKpi[i], x, TILE_Y + sOff, tileW, TILE_H);
    }

    deferMove(_hModeQuick,      16,  pillY + sOff, 72,  24);
    deferMove(_hModeStandard,   94,  pillY + sOff, 110, 24);
    deferMove(_hModeDeep,       210, pillY + sOff, 68,  24);
    deferMove(_hCheckGentle,    290, pillY + sOff, 108, 24);
    deferMove(_hLinkGentleInfo, 402, pillY + sOff + 2, 24, 20);
    deferMove(_hModeDesc,       16,  pillY + sOff + 28, std::max(cx - 32, 300), 18);

    deferMove(_hBtnQuickScan, Theme::SP4, btnY + sOff, 120, BTN_H);
    deferMove(_hBtnDeepScan, Theme::SP4 + 128, btnY + sOff, 120, BTN_H);
    deferMove(_hBtnMonStart, Theme::SP4 + 256, btnY + sOff, 130, BTN_H);
    deferMove(_hBtnMonStop, Theme::SP4 + 394, btnY + sOff, 130, BTN_H);
    deferMove(_hBtnExport, Theme::SP4 + 532, btnY + sOff, 130, BTN_H);
    deferMove(_hBtnNotif,  Theme::SP4 + 670, btnY + sOff, 130, BTN_H);

    deferMove(_hStatusText, 16, btnY + BTN_H + 8 + sOff, cx - 32, 20);
    deferMove(_hProgressBar, 16, btnY + BTN_H + 32 + sOff, cx - 32, 8);
    deferMove(_hNetworkInfo, 16, 16 + sOff, cx - 400, 40);
    deferMove(_hNicCombo, 16, 44 + sOff, 380, 160);
    deferMove(_hNicPin, 402, 44 + sOff, 50, 24);
    deferMove(_hNicReason, 16, 72 + sOff, cx - 32, 30);

    // Security dashboard rect (left 60%) — enforce minimum height
    int mapW = (cx - 40) * 6 / 10;
    int mapH = std::max(cy - listY - 16, DASH_MIN_H);
    _mapRect = { 16, listY + sOff, 16 + mapW, listY + mapH + sOff };

    // Changes list (right 40%)
    int listX = 16 + mapW + 8;
    int listW = cx - listX - 16;
    deferMove(_hChangesList, listX, listY + sOff, std::max(listW, 100), std::max(mapH, 50));

    if (hdwp) EndDeferWindowPos(hdwp);

    UpdateScrollBar(_hwnd);
}

LRESULT TabOverview::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

void TabOverview::UpdateScrollBar(HWND hwnd) {
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = _contentHeight;
    si.nPage  = _viewHeight;
    si.nPos   = _scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

// Incremental scroll: bitblts existing content, moves children and _mapRect by dy,
// and only repaints the newly-exposed strip — no full LayoutControls recalc needed.
void TabOverview::ApplyScrollDelta(HWND hwnd, int dy) {
    int maxScroll = std::max(0, _contentHeight - _viewHeight);
    int newScrollY = std::max(0, std::min(_scrollY + dy, maxScroll));
    int actualDy = _scrollY - newScrollY;
    if (actualDy == 0) return;

    _scrollY = newScrollY;

    // Keep _mapRect in sync so OnPaint draws the security dashboard at the right place.
    _mapRect.top    += actualDy;
    _mapRect.bottom += actualDy;

    RECT rc; GetClientRect(hwnd, &rc);
    ScrollWindowEx(hwnd, 0, actualDy, nullptr, &rc, nullptr, nullptr,
                   SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    UpdateScrollBar(hwnd);
}

// Absolute jump (thumb drag, page): full recalc so _mapRect is computed from scratch.
void TabOverview::ApplyScrollAbsolute(HWND hwnd, int newScrollY) {
    int maxScroll = std::max(0, _contentHeight - _viewHeight);
    newScrollY = std::max(0, std::min(newScrollY, maxScroll));
    if (newScrollY == _scrollY) return;

    _scrollY = newScrollY;
    RECT rc; GetClientRect(hwnd, &rc);
    LayoutControls(rc.right, rc.bottom);
    UpdateScrollBar(hwnd);
    InvalidateRect(hwnd, nullptr, FALSE);
}

LRESULT TabOverview::OnVScroll(HWND hwnd, WPARAM wp) {
    SCROLLINFO si = {};
    si.cbSize = sizeof(si); si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);

    switch (LOWORD(wp)) {
    case SB_LINEUP:        ApplyScrollDelta(hwnd, -20);             break;
    case SB_LINEDOWN:      ApplyScrollDelta(hwnd, +20);             break;
    case SB_PAGEUP:        ApplyScrollDelta(hwnd, -_viewHeight);    break;
    case SB_PAGEDOWN:      ApplyScrollDelta(hwnd, +_viewHeight);    break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: ApplyScrollAbsolute(hwnd, si.nTrackPos); break;
    }
    return 0;
}

LRESULT TabOverview::OnMouseWheel(HWND hwnd, int rawDelta) {
    UINT scrollLines = 3;
    SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0);
    if (scrollLines == WHEEL_PAGESCROLL)
        scrollLines = std::max(1u, (UINT)(_viewHeight / 20));

    int dy = -(rawDelta * (int)scrollLines * 20) / WHEEL_DELTA;
    ApplyScrollDelta(hwnd, dy);
    return 0;
}


// ── Owner-draw KPI tiles ──────────────────────────────────────────────────────

LRESULT TabOverview::OnDrawItem(HWND hwnd, DRAWITEMSTRUCT* dis) {
    if (!dis) return 0;

    // Owner-drawn action buttons
    if (dis->CtlID == IDC_BTN_SCAN_QUICK || dis->CtlID == IDC_BTN_SCAN_DEEP ||
        dis->CtlID == IDC_BTN_MONITOR_START || dis->CtlID == IDC_BTN_MONITOR_STOP ||
        dis->CtlID == IDC_BTN_EXPORT) {

        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        bool disabled = (dis->itemState & ODS_DISABLED) != 0;
        bool focused = (dis->itemState & ODS_FOCUS) != 0;

        // Primary buttons get accent glass, secondary get neutral glass
        bool isPrimary = (dis->CtlID == IDC_BTN_SCAN_QUICK);

        if (disabled) {
            Theme::DrawRoundedCard(hdc, rc, Theme::RADIUS_MD,
                Theme::BG_ELEVATED, Theme::BORDER_SUBTLE);
        } else {
            Theme::DrawGlassButton(hdc, rc, Theme::RADIUS_MD, pressed,
                                   isPrimary ? 0 : 1, focused);
        }

        // Button text
        wchar_t text[64] = {};
        GetWindowText(dis->hwndItem, text, 64);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, disabled ? Theme::TEXT_TERTIARY :
                          isPrimary ? RGB(255,255,255) : Theme::TEXT_PRIMARY);
        HFONT old = (HFONT)SelectObject(hdc, Theme::FontNavActive()); // 13px SemiBold
        DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);

        return TRUE;
    }

    // Notification button — amber tint when there are unread notifications
    if (dis->CtlID == IDC_BTN_NOTIFICATIONS) {
        HDC hdc = dis->hDC;
        RECT rc = dis->rcItem;
        bool pressed = (dis->itemState & ODS_SELECTED) != 0;
        bool focused = (dis->itemState & ODS_FOCUS) != 0;

        int count = _mainWnd ? (int)_mainWnd->_notifications.size() : 0;
        bool hasNotifs = count > 0;

        if (hasNotifs) {
            // Amber-tinted glass to signal pending notifications
            COLORREF amberBg = Theme::AlphaBlend(Theme::ACCENT_AMBER, Theme::BG_SURFACE, 18);
            Theme::DrawRoundedCard(hdc, rc, Theme::RADIUS_MD, amberBg, Theme::ACCENT_AMBER);
        } else {
            Theme::DrawGlassButton(hdc, rc, Theme::RADIUS_MD, pressed, 1, focused);
        }

        // Label: "Notifications" or "Notifications (N)"
        wchar_t label[48];
        if (hasNotifs)
            swprintf_s(label, L"Notifications (%d)", count);
        else
            wcscpy_s(label, L"Notifications");

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, hasNotifs ? Theme::ACCENT_AMBER : Theme::TEXT_PRIMARY);
        HFONT old = (HFONT)SelectObject(hdc, Theme::FontNavActive());
        DrawText(hdc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);

        return TRUE;
    }

    int idx = dis->CtlID - IDC_STATIC_KPI1;
    if (idx < 0 || idx > 3) return 0;

    HDC   hdc = dis->hDC;
    RECT  rc  = dis->rcItem;
    COLORREF accent = KPI_ACCENTS[idx];

    // Card shadow (subtle depth)
    Theme::DrawCardShadow(hdc, rc, Theme::RADIUS_MD);

    // Rounded card with top accent bar — design system radius_md
    Theme::DrawAccentCard(hdc, rc, Theme::RADIUS_MD, Theme::BG_ELEVATED,
                         Theme::BORDER_DEFAULT, accent);

    // Hero number — Display font (48px Bold) per design system
    wchar_t valStr[32];
    if (idx == 3) {
        if (_kpiVal[3] >= 0) swprintf_s(valStr, L"%dms", _kpiVal[3]);
        else                 wcscpy_s(valStr, L"--");
    } else {
        swprintf_s(valStr, L"%d", _kpiVal[idx]);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, accent);  // Number in accent color
    HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontDisplay());
    RECT numRc = { rc.left + Theme::SP4, rc.top + Theme::SP2, rc.right - Theme::SP4, rc.top + 58 };
    DrawText(hdc, valStr, -1, &numRc, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // Label — Caption style (11px, uppercase, wide tracking)
    SelectObject(hdc, Theme::FontCaption());
    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    RECT lblRc = { rc.left + Theme::SP4, rc.top + 60, rc.right - Theme::SP4, rc.top + 76 };
    DrawText(hdc, KPI_LABELS[idx], -1, &lblRc, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    SelectObject(hdc, oldFont);

    // Sparkline (7 bars) in the bottom area of the tile
    const std::vector<int>* hist = nullptr;
    switch (idx) {
    case 0: hist = &_devicesOnlineHistory; break;
    case 1: hist = &_unknownDevHistory;    break;
    case 2: hist = &_alertHistory;         break;
    case 3: hist = &_latencyHistory;       break;
    }
    if (hist && !hist->empty()) {
        RECT spRc = { rc.left + Theme::SP4, rc.top + 80, rc.right - Theme::SP4, rc.bottom - Theme::SP2 };
        DrawSparkline(hdc, spRc, *hist, accent);
    }

    return TRUE;
}

void TabOverview::DrawSparkline(HDC hdc, const RECT& rc,
                                const std::vector<int>& vals, COLORREF col) {
    if (vals.empty()) return;
    int n = (int)vals.size();
    int w = rc.right  - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    // Find max (at least 1)
    int maxVal = 1;
    for (int v : vals) if (v > maxVal) maxVal = v;

    // 40% opacity blend on card background (bg_elevated)
    COLORREF barCol = Theme::AlphaBlend(col, Theme::BG_ELEVATED, 40);
    HBRUSH barBrush = CreateSolidBrush(barCol);

    int barW = std::max(2, w / (n + 1));

    for (int i = 0; i < n; i++) {
        int barH = (vals[i] * h) / maxVal;
        if (barH < 1 && vals[i] > 0) barH = 1;
        int x = rc.left + i * (w / n);
        int bw = w / n - 1;
        if (bw < 1) bw = 1;
        RECT barRc = { x, rc.bottom - barH, x + bw, rc.bottom };
        if (barH > 4) {
            Theme::DrawRoundedCard(hdc, barRc, 2, barCol, barCol, 0);
        } else {
            FillRect(hdc, &barRc, barBrush);
        }
    }
    DeleteObject(barBrush);
}

// ── Security Dashboard ────────────────────────────────────────────────────────

void TabOverview::RebuildSecurityData(const ScanResult& r) {
    _trustCounts = {};
    _iotRiskCount = 0;
    for (auto& d : r.devices) {
        if      (d.trustState == L"owned")     _trustCounts.owned++;
        else if (d.trustState == L"known")     _trustCounts.known++;
        else if (d.trustState == L"guest")     _trustCounts.guest++;
        else if (d.trustState == L"unknown")   _trustCounts.unknown++;
        else if (d.trustState == L"blocked")   _trustCounts.blocked++;
        else if (d.trustState == L"watchlist") _trustCounts.watchlist++;
        if (d.iotRisk) _iotRiskCount++;
    }

    int score = 100;
    score -= std::min(_trustCounts.unknown * 8, 40);
    score -= std::min(_iotRiskCount * 12, 30);
    score -= std::min((int)r.anomalies.size() * 5, 20);
    if (score < 0) score = 0;
    _secScore = r.devices.empty() ? -1 : score;

    _topRisks.clear();
    if (_trustCounts.unknown > 0) {
        wchar_t buf[80];
        swprintf_s(buf, L"%d unknown device%ls on network",
                   _trustCounts.unknown, _trustCounts.unknown == 1 ? L"" : L"s");
        _topRisks.push_back(buf);
    }
    if (_iotRiskCount > 0) {
        wchar_t buf[80];
        swprintf_s(buf, L"%d IoT device%ls with risky port combinations",
                   _iotRiskCount, _iotRiskCount == 1 ? L"" : L"s");
        _topRisks.push_back(buf);
    }
    if (!r.anomalies.empty()) {
        wchar_t buf[80];
        swprintf_s(buf, L"%d active anomal%ls detected",
                   (int)r.anomalies.size(), r.anomalies.size() == 1 ? L"y" : L"ies");
        _topRisks.push_back(buf);
    }
    if (_trustCounts.blocked > 0) {
        wchar_t buf[80];
        swprintf_s(buf, L"%d blocked device%ls still on network",
                   _trustCounts.blocked, _trustCounts.blocked == 1 ? L"" : L"s");
        _topRisks.push_back(buf);
    }
    if (_trustCounts.watchlist > 0) {
        wchar_t buf[80];
        swprintf_s(buf, L"%d device%ls on watchlist",
                   _trustCounts.watchlist, _trustCounts.watchlist == 1 ? L"" : L"s");
        _topRisks.push_back(buf);
    }
    if (_topRisks.size() > 5) _topRisks.resize(5);
}

void TabOverview::DrawSecurityDashboard(HDC hdc, const RECT& rc) {
    Theme::DrawGlassPanel(hdc, rc, Theme::RADIUS_MD);
    SetBkMode(hdc, TRANSPARENT);

    HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontCaption());
    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    RECT hdrRc = { rc.left + 12, rc.top + 8, rc.right - 150, rc.top + 24 };
    DrawText(hdc, L"SECURITY OVERVIEW", -1, &hdrRc, DT_LEFT | DT_SINGLELINE);

    if (_secScore < 0) {
        SelectObject(hdc, Theme::FontBody());
        SetTextColor(hdc, Theme::TEXT_TERTIARY);
        RECT noRc = { rc.left, rc.top + 28, rc.right, rc.bottom };
        DrawText(hdc, L"Run a scan to see your network security assessment.",
                 -1, &noRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldFont);
        return;
    }

    COLORREF scoreCol = (_secScore >= 80) ? Theme::ACCENT_GREEN :
                        (_secScore >= 60) ? Theme::ACCENT_BLUE  :
                        (_secScore >= 40) ? Theme::ACCENT_AMBER : Theme::ACCENT_RED;
    const wchar_t* grade = (_secScore >= 80) ? L"SECURE"   :
                           (_secScore >= 60) ? L"FAIR"     :
                           (_secScore >= 40) ? L"AT RISK"  : L"CRITICAL";

    // Grade pill (top-right)
    Theme::DrawPillBadge(hdc,
        rc.right - 110, rc.top + 6, 98, 20,
        scoreCol, grade, Theme::FontCaption());

    // ── Score ring ────────────────────────────────────────────────────────────
    const int circleCx = rc.left + 90;
    const int circleCy = rc.top  + 118;
    const int circleR  = 54;
    const int innerR   = circleR - 12;

    {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        float ax = (float)(circleCx - circleR + 2);
        float ay = (float)(circleCy - circleR + 2);
        float aw = (float)((circleR - 2) * 2);

        Gdiplus::Pen trackPen(Theme::GdipColor(Theme::BORDER_DEFAULT, 200), 9.0f);
        trackPen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapFlat);
        g.DrawEllipse(&trackPen, ax, ay, aw, aw);

        float sweep = (_secScore / 100.0f) * 360.0f;
        if (sweep > 0.5f) {
            Gdiplus::Pen arcPen(Theme::GdipColor(scoreCol), 9.0f);
            arcPen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapFlat);
            g.DrawArc(&arcPen, ax, ay, aw, aw, -90.0f, sweep);
        }

        Gdiplus::SolidBrush innerBr(Theme::GdipColor(Theme::BG_ELEVATED));
        g.FillEllipse(&innerBr,
            (float)(circleCx - innerR), (float)(circleCy - innerR),
            (float)(innerR * 2), (float)(innerR * 2));
    }

    // Score number inside ring
    {
        wchar_t scoreStr[8];
        swprintf_s(scoreStr, L"%d", _secScore);
        SelectObject(hdc, Theme::FontH2());
        SetTextColor(hdc, scoreCol);
        RECT numRc = { circleCx - 30, circleCy - 16, circleCx + 30, circleCy + 16 };
        DrawText(hdc, scoreStr, -1, &numRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, Theme::FontSmall());
        SetTextColor(hdc, Theme::TEXT_TERTIARY);
        RECT subRc = { circleCx - 30, circleCy + 14, circleCx + 30, circleCy + 28 };
        DrawText(hdc, L"/ 100", -1, &subRc, DT_CENTER | DT_SINGLELINE);
    }

    // "SCORE" caption below ring
    {
        SelectObject(hdc, Theme::FontCaption());
        SetTextColor(hdc, Theme::TEXT_TERTIARY);
        RECT capRc = { circleCx - 50, circleCy + circleR + 6,
                       circleCx + 50, circleCy + circleR + 22 };
        DrawText(hdc, L"SCORE", -1, &capRc, DT_CENTER | DT_SINGLELINE);
    }

    // ── Risk factors (right of score circle) ──────────────────────────────────
    int riskX = circleCx + circleR + 20;
    int riskY = rc.top + 30;
    int riskW = rc.right - riskX - 16;

    SelectObject(hdc, Theme::FontBody());
    SetTextColor(hdc, Theme::TEXT_PRIMARY);
    RECT rHdrRc = { riskX, riskY, riskX + riskW, riskY + 20 };
    DrawText(hdc, L"Risk Factors", -1, &rHdrRc, DT_LEFT | DT_SINGLELINE);
    riskY += 26;

    if (_topRisks.empty()) {
        RECT bannerRc = { riskX, riskY, riskX + riskW, riskY + 28 };
        Theme::DrawAlertBanner(hdc, bannerRc, Theme::ACCENT_GREEN);
        SelectObject(hdc, Theme::FontBodySm());
        SetTextColor(hdc, Theme::ACCENT_GREEN);
        RECT okRc = { riskX + 12, riskY, riskX + riskW, riskY + 28 };
        DrawText(hdc, L"No active risks — network looks clean", -1, &okRc,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    } else {
        for (const auto& risk : _topRisks) {
            RECT bannerRc = { riskX, riskY, riskX + riskW, riskY + 26 };
            Theme::DrawAlertBanner(hdc, bannerRc, Theme::ACCENT_AMBER);
            SelectObject(hdc, Theme::FontBodySm());
            SetTextColor(hdc, Theme::TEXT_SECONDARY);
            RECT txtRc = { riskX + 12, riskY, riskX + riskW - 4, riskY + 26 };
            DrawText(hdc, risk.c_str(), -1, &txtRc,
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            riskY += 30;
        }
    }

    // ── Trust distribution bar ────────────────────────────────────────────────
    int barY = circleCy + circleR + 34;
    int barX = rc.left + 16;
    int barW = rc.right - rc.left - 32;
    const int barH = 18;

    SelectObject(hdc, Theme::FontCaption());
    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    RECT barHdrRc = { barX, barY, barX + barW, barY + 16 };
    DrawText(hdc, L"DEVICE TRUST DISTRIBUTION", -1, &barHdrRc, DT_LEFT | DT_SINGLELINE);
    barY += 20;

    int total = _trustCounts.total();
    struct TrustSeg { int count; COLORREF col; const wchar_t* label; };
    const TrustSeg segs[] = {
        { _trustCounts.owned,     Theme::ACCENT_GREEN,   L"Owned"     },
        { _trustCounts.known,     Theme::ACCENT_BLUE,    L"Known"     },
        { _trustCounts.guest,     Theme::ACCENT_AMBER,   L"Guest"     },
        { _trustCounts.unknown,   Theme::TEXT_SECONDARY, L"Unknown"   },
        { _trustCounts.blocked,   Theme::ACCENT_RED,     L"Blocked"   },
        { _trustCounts.watchlist, Theme::ACCENT_PURPLE,  L"Watchlist" },
    };

    // Track background
    RECT trackRc = { barX, barY, barX + barW, barY + barH };
    FillRect(hdc, &trackRc, Theme::BrushElevated());

    if (total > 0) {
        int x = barX;
        for (const auto& seg : segs) {
            if (seg.count <= 0) continue;
            int segW = (seg.count * barW) / total;
            if (segW < 2) segW = 2;
            HBRUSH br = CreateSolidBrush(seg.col);
            RECT sRc = { x, barY + 1, std::min(x + segW, barX + barW) - 1, barY + barH - 1 };
            FillRect(hdc, &sRc, br);
            DeleteObject(br);
            x += segW;
        }
    } else {
        SelectObject(hdc, Theme::FontSmall());
        SetTextColor(hdc, Theme::TEXT_TERTIARY);
        DrawText(hdc, L"No device data", -1, &trackRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Bar border
    {
        HPEN bp = CreatePen(PS_SOLID, 1, Theme::BORDER_DEFAULT);
        HPEN oldPen = (HPEN)SelectObject(hdc, bp);
        HBRUSH nullBr = (HBRUSH)GetStockObject(NULL_BRUSH);
        HBRUSH oldBr  = (HBRUSH)SelectObject(hdc, nullBr);
        Rectangle(hdc, barX, barY, barX + barW, barY + barH);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBr);
        DeleteObject(bp);
    }

    // Legend
    if (total > 0) {
        int legX = barX;
        int legY = barY + barH + 8;
        SelectObject(hdc, Theme::FontSmall());
        for (const auto& seg : segs) {
            if (seg.count <= 0) continue;
            HBRUSH swBr = CreateSolidBrush(seg.col);
            RECT swRc = { legX, legY + 3, legX + 10, legY + 11 };
            FillRect(hdc, &swRc, swBr);
            DeleteObject(swBr);
            wchar_t legTxt[40];
            swprintf_s(legTxt, L"%ls (%d)", seg.label, seg.count);
            SetTextColor(hdc, Theme::TEXT_SECONDARY);
            SIZE tsz = {};
            GetTextExtentPoint32(hdc, legTxt, (int)wcslen(legTxt), &tsz);
            RECT legRc = { legX + 14, legY, legX + 14 + tsz.cx + 4, legY + 14 };
            DrawText(hdc, legTxt, -1, &legRc, DT_LEFT | DT_SINGLELINE);
            legX += 14 + tsz.cx + 14;
            if (legX + 80 > barX + barW) { legX = barX; legY += 16; }
        }
    }

    // ── Network health strip ──────────────────────────────────────────────────
    int hlthY = rc.bottom - 52;

    {
        Gdiplus::Graphics g(hdc);
        Gdiplus::Pen divPen(Theme::GdipColor(Theme::BORDER_SUBTLE, 140), 1.0f);
        g.DrawLine(&divPen,
            (float)(rc.left + 12), (float)(hlthY - 8),
            (float)(rc.right - 12), (float)(hlthY - 8));
    }

    SelectObject(hdc, Theme::FontCaption());
    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    RECT hlthHdrRc = { rc.left + 16, hlthY - 4, rc.left + 200, hlthY + 12 };
    DrawText(hdc, L"NETWORK HEALTH", -1, &hlthHdrRc, DT_LEFT | DT_SINGLELINE);

    int dotX = rc.left + 16;
    int dotY = hlthY + 16;

    auto drawHealthItem = [&](const wchar_t* label, bool ok, const wchar_t* detail) {
        COLORREF dotCol = ok ? Theme::ACCENT_GREEN : Theme::ACCENT_RED;
        {
            Gdiplus::Graphics g(hdc);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::SolidBrush dotBr(Theme::GdipColor(dotCol));
            g.FillEllipse(&dotBr, (float)dotX, (float)(dotY + 3), 7.0f, 7.0f);
        }
        wchar_t txt[80];
        swprintf_s(txt, L"%ls: %ls", label, detail);
        SelectObject(hdc, Theme::FontBodySm());
        SetTextColor(hdc, Theme::TEXT_SECONDARY);
        SIZE tsz = {};
        GetTextExtentPoint32(hdc, txt, (int)wcslen(txt), &tsz);
        RECT hRc = { dotX + 12, dotY, dotX + 12 + tsz.cx + 4, dotY + 16 };
        DrawText(hdc, txt, -1, &hRc, DT_LEFT | DT_SINGLELINE);
        dotX += 12 + tsz.cx + 22;
    };

    wchar_t latBuf[24];
    if (_internetOnline && _internetLatency >= 0)
        swprintf_s(latBuf, L"%dms", _internetLatency);
    else
        wcscpy_s(latBuf, L"--");

    wchar_t alertBuf[24];
    swprintf_s(alertBuf, L"%d", _kpiVal[2]);

    drawHealthItem(L"Internet", _internetOnline, _internetOnline ? L"Online" : L"Offline");
    drawHealthItem(L"Gateway",  _kpiVal[3] >= 0, _kpiVal[3] >= 0 ? L"Reachable" : L"Unreachable");
    drawHealthItem(L"Latency",  _internetOnline && _internetLatency >= 0 && _internetLatency < 100, latBuf);
    drawHealthItem(L"Alerts",   _kpiVal[2] == 0, _kpiVal[2] == 0 ? L"None" : alertBuf);

    SelectObject(hdc, oldFont);
}

// ── OnPaint ───────────────────────────────────────────────────────────────────

LRESULT TabOverview::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdcScreen = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    int cx = rc.right, cy = rc.bottom;
    if (cx <= 0 || cy <= 0) { EndPaint(hwnd, &ps); return 0; }

    // Double buffer — paint to offscreen bitmap, then blit
    HDC hdc = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, cx, cy);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdc, hBmp);

    FillRect(hdc, &rc, Theme::BrushSurface());

    // Security dashboard (left portion of bottom area)
    if (_mapRect.right > _mapRect.left && _mapRect.bottom > _mapRect.top)
        DrawSecurityDashboard(hdc, _mapRect);

    // Section header "RECENT CHANGES" above the changes list
    {
        int tileW, pillY, btnY, listY;
        GetLayoutMetrics(rc.right, rc.bottom, tileW, pillY, btnY, listY);
        int mapW = (rc.right - 40) * 6 / 10;
        int listX = 16 + mapW + 8;

        RECT hdrRc = { listX, listY - 18, rc.right - 16, listY - 2 };
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::TEXT_SECONDARY);
        HFONT old = (HFONT)SelectObject(hdc, Theme::FontCaption());
        DrawText(hdc, L"RECENT CHANGES", -1, &hdrRc,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
    }

    // Blit to screen
    BitBlt(hdcScreen, 0, 0, cx, cy, hdc, 0, 0, SRCCOPY);

    SelectObject(hdc, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdc);

    EndPaint(hwnd, &ps);
    return 0;
}

static void UpdateModeDesc(HWND hDesc, bool isQuick, bool isStandard, bool isDeep) {
    if (!hDesc) return;
    if (isQuick)
        SetWindowText(hDesc,
            L"Quick — rapid device count, under 30 seconds, may miss some devices");
    else if (isDeep)
        SetWindowText(hDesc,
            L"Deep — port scan + banner grab, full detail, takes 2–5 minutes");
    else
        SetWindowText(hDesc,
            L"Standard ★ — Recommended: balanced discovery, finishes in about 60 seconds");
}

LRESULT TabOverview::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = LOWORD(wp);

    // Update mode description whenever a pill is clicked
    if ((id == 9200 || id == 9201 || id == 9202) && HIWORD(wp) == BN_CLICKED) {
        UpdateModeDesc(_hModeDesc, id == 9200, id == 9201, id == 9202);
        return 0;
    }

    switch (id) {
    case IDC_BTN_SCAN_QUICK:
        if (_mainWnd) _mainWnd->StartQuickScan();
        if (_hStatusText) SetWindowText(_hStatusText, L"Quick scan started...");
        if (_hProgressBar) SendMessage(_hProgressBar, PBM_SETPOS, 0, 0);
        break;

    case IDC_BTN_SCAN_DEEP:
        if (_mainWnd) {
            if (SendMessage(_hModeDeep, BM_GETCHECK, 0, 0) == BST_CHECKED)
                _mainWnd->StartDeepScan();
            else if (SendMessage(_hModeQuick, BM_GETCHECK, 0, 0) == BST_CHECKED)
                _mainWnd->StartQuickScan();
            else
                _mainWnd->StartStandardScan();  // Standard (★ Recommended) is default
        }
        if (_hStatusText) SetWindowText(_hStatusText, L"Scan started...");
        break;

    case IDC_BTN_MONITOR_START:
        if (_mainWnd) _mainWnd->StartMonitor();
        EnableWindow(_hBtnMonStart, FALSE);
        EnableWindow(_hBtnMonStop, TRUE);
        break;

    case IDC_BTN_MONITOR_STOP:
        if (_mainWnd) _mainWnd->StopMonitor();
        EnableWindow(_hBtnMonStart, TRUE);
        EnableWindow(_hBtnMonStop, FALSE);
        break;

    case IDC_COMBO_NIC_SELECT:
        if (HIWORD(wp) == CBN_SELCHANGE && _mainWnd && _hNicCombo && _hNicReason) {
            int sel = (int)SendMessage(_hNicCombo, CB_GETCURSEL, 0, 0);
            auto nets = ScanEngine::RankNetworkInterfaces();
            if (sel >= 0 && sel < (int)nets.size()) {
                _mainWnd->_selectedNicName = nets[sel].name;
                SetWindowText(_hNicReason, (L"Selected: " + nets[sel].name +
                    L" — " + nets[sel].reason).c_str());
            }
        }
        break;

    case IDC_BTN_NIC_PIN:
        if (_mainWnd && _hNicCombo) {
            int sel = (int)SendMessage(_hNicCombo, CB_GETCURSEL, 0, 0);
            auto nets = ScanEngine::RankNetworkInterfaces();
            if (sel >= 0 && sel < (int)nets.size()) {
                _mainWnd->_selectedNicName = nets[sel].name;
                MessageBox(hwnd, (L"Pinned adapter: " + nets[sel].name +
                    L"\nThis adapter will be preferred for future scans.").c_str(),
                    L"NIC Pinned", MB_OK | MB_ICONINFORMATION);
            }
        }
        break;

    case IDC_BTN_NOTIFICATIONS:
        if (_mainWnd) {
            _mainWnd->ShowNotifPanel(!_mainWnd->_notifPanelOpen);
            if (_hBtnNotif) InvalidateRect(_hBtnNotif, nullptr, FALSE);
        }
        break;

    case IDC_BTN_EXPORT:
        if (_mainWnd) {
            wchar_t path[MAX_PATH] = {};
            OPENFILENAME ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hwnd;
            ofn.lpstrFile   = path;
            ofn.nMaxFile    = MAX_PATH;
            ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
            ofn.lpstrDefExt = L"json";
            ofn.lpstrTitle  = L"Export Report";
            ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
            if (GetSaveFileName(&ofn)) {
                HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, nullptr,
                                          CREATE_ALWAYS, 0, nullptr);
                if (hFile != INVALID_HANDLE_VALUE) {
                    ScanResult r = _mainWnd->GetLastResult();
                    std::wstring json =
                        L"{\n  \"devices\": " + std::to_wstring(r.devices.size()) +
                        L",\n  \"scannedAt\": \"" + r.scannedAt + L"\"\n}";
                    std::string jn;
                    int n = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1,
                                                nullptr, 0, nullptr, nullptr);
                    if (n > 0) {
                        jn.resize(n - 1);
                        WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1,
                                            &jn[0], n, nullptr, nullptr);
                    }
                    DWORD written;
                    WriteFile(hFile, jn.c_str(), (DWORD)jn.size(), &written, nullptr);
                    CloseHandle(hFile);
                    MessageBox(hwnd, L"Report exported.", L"Export",
                               MB_OK | MB_ICONINFORMATION);
                }
            }
        }
        break;
    }

    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

LRESULT TabOverview::OnScanProgress(HWND hwnd, WPARAM pct, LPARAM msgPtr) {
    auto* msg = reinterpret_cast<std::wstring*>(msgPtr);
    if (msg) {
        if (_hStatusText)  SetWindowText(_hStatusText, msg->c_str());
        if (_hProgressBar) SendMessage(_hProgressBar, PBM_SETPOS, (WPARAM)pct, 0);
        delete msg;
    }
    return 0;
}

LRESULT TabOverview::OnScanComplete(HWND hwnd) {
    if (_mainWnd) {
        ScanResult r = _mainWnd->GetLastResult();
        RebuildSecurityData(r);
    }

    RefreshKPIs();
    if (_hStatusText)  SetWindowText(_hStatusText, L"Scan complete.");
    if (_hProgressBar) SendMessage(_hProgressBar, PBM_SETPOS, 100, 0);
    if (_hBtnNotif) InvalidateRect(_hBtnNotif, nullptr, FALSE);

    // Invalidate security dashboard area to repaint
    if (_mapRect.right > _mapRect.left)
        InvalidateRect(hwnd, &_mapRect, FALSE);

    // Populate changes list with anomalies + summary
    if (!_hChangesList || !_mainWnd) return 0;
    ListView_DeleteAllItems(_hChangesList);
    ScanResult r = _mainWnd->GetLastResult();

    // Count change types
    int nNew = 0, nDisappeared = 0, nPortChanged = 0, nHostChanged = 0, nIpMoved = 0;
    for (auto& a : r.anomalies) {
        if (a.type == L"new_device")       nNew++;
        else if (a.type == L"device_offline")  nDisappeared++;
        else if (a.type == L"port_changed")    nPortChanged++;
        else if (a.type == L"hostname_changed") nHostChanged++;
        else if (a.type == L"ip_changed")      nIpMoved++;
    }

    // Summary row first
    if (!r.anomalies.empty()) {
        wstring summary = L"Summary: ";
        if (nNew > 0) summary += std::to_wstring(nNew) + L" new, ";
        if (nDisappeared > 0) summary += std::to_wstring(nDisappeared) + L" disappeared, ";
        if (nPortChanged > 0) summary += std::to_wstring(nPortChanged) + L" ports changed, ";
        if (nHostChanged > 0) summary += std::to_wstring(nHostChanged) + L" hostname changed, ";
        if (nIpMoved > 0) summary += std::to_wstring(nIpMoved) + L" IP moved, ";
        if (summary.size() > 2 && summary.back() == L' ') { summary.pop_back(); summary.pop_back(); }

        LVITEM item = {};
        item.mask    = LVIF_TEXT;
        item.iItem   = 0;
        item.iSubItem = 0;
        item.pszText = (LPWSTR)r.scannedAt.c_str();
        ListView_InsertItem(_hChangesList, &item);
        ListView_SetItemText(_hChangesList, 0, 1, (LPWSTR)L"SCAN DIFF");
        ListView_SetItemText(_hChangesList, 0, 2, (LPWSTR)summary.c_str());
    }

    int row = r.anomalies.empty() ? 0 : 1;
    for (auto& a : r.anomalies) {
        LVITEM item = {};
        item.mask    = LVIF_TEXT;
        item.iItem   = row;
        item.iSubItem = 0;
        item.pszText = (LPWSTR)r.scannedAt.c_str();
        ListView_InsertItem(_hChangesList, &item);
        ListView_SetItemText(_hChangesList, row, 1, (LPWSTR)a.type.c_str());
        ListView_SetItemText(_hChangesList, row, 2, (LPWSTR)a.description.c_str());
        row++;
    }

    return 0;
}

LRESULT TabOverview::OnMonitorTick(HWND hwnd) {
    RefreshKPIs();
    if (_hBtnNotif) InvalidateRect(_hBtnNotif, nullptr, FALSE);
    return 0;
}

// ── KPI refresh ───────────────────────────────────────────────────────────────

static void PushHistory(std::vector<int>& v, int val, int maxLen = 7) {
    v.push_back(val);
    if ((int)v.size() > maxLen) v.erase(v.begin());
}

void TabOverview::RefreshKPIs() {
    if (!_mainWnd) return;
    ScanResult r = _mainWnd->GetLastResult();

    _kpiVal[0] = (int)r.devices.size();

    int unknown = 0;
    for (auto& d : r.devices) if (d.trustState == L"unknown") unknown++;
    _kpiVal[1] = unknown;

    _kpiVal[2] = (int)r.anomalies.size();

    // Gateway latency
    int lat = -1;
    auto nets = ScanEngine::GetLocalNetworks();
    if (!nets.empty() && !nets[0].gateway.empty()) {
        for (auto& d : r.devices)
            if (d.ip == nets[0].gateway) { lat = d.latencyMs; break; }
    }
    _kpiVal[3] = lat;

    // Update sparkline history
    PushHistory(_devicesOnlineHistory, _kpiVal[0]);
    PushHistory(_unknownDevHistory,    _kpiVal[1]);
    PushHistory(_alertHistory,         _kpiVal[2]);
    PushHistory(_latencyHistory,       lat >= 0 ? lat : 0);

    // Repaint all 4 KPI tiles
    for (int i = 0; i < 4; i++)
        if (_hKpi[i]) InvalidateRect(_hKpi[i], nullptr, FALSE);
}

void TabOverview::RefreshNetworkInfo() {
    if (!_hNetworkInfo) return;
    auto nets = ScanEngine::RankNetworkInterfaces();
    if (nets.empty()) {
        SetWindowText(_hNetworkInfo, L"No network adapters found.");
        return;
    }

    // Show top-ranked adapter info
    auto& best = nets[0];
    std::wstring info = L"Selected: " + best.name + L" (" + best.localIp + L"/" + best.cidr + L")";
    if (!best.gateway.empty()) info += L"  GW: " + best.gateway;
    SetWindowText(_hNetworkInfo, info.c_str());

    // Show reason
    if (_hNicReason) {
        std::wstring reason = L"Auto-selected — " + best.reason;
        if (nets.size() > 1)
            reason += L"  |  " + std::to_wstring(nets.size() - 1) + L" other adapter(s) available";
        SetWindowText(_hNicReason, reason.c_str());
    }

    // Populate NIC combo
    if (_hNicCombo) {
        SendMessage(_hNicCombo, CB_RESETCONTENT, 0, 0);
        for (size_t i = 0; i < nets.size(); i++) {
            auto& ni = nets[i];
            std::wstring label = ni.name + L"  " + ni.localIp + L"/" + ni.cidr;
            if (!ni.gateway.empty()) label += L" (GW)";
            if (i == 0) label += L"  \u2605";  // star for recommended
            label += L"  [" + std::to_wstring(ni.score) + L"pts]";
            SendMessage(_hNicCombo, CB_ADDSTRING, 0, (LPARAM)label.c_str());
        }
        SendMessage(_hNicCombo, CB_SETCURSEL, 0, 0);
    }
}

void TabOverview::UpdateFromResult(const ScanResult& result) {
    RebuildSecurityData(result);
    RefreshKPIs();
}

void TabOverview::UpdateMonitorStatus(bool running, const InternetStatus& is) {
    _internetOnline  = is.online;
    _internetLatency = is.latencyMs;

    if (_hMonitorStatus) {
        SetWindowText(_hMonitorStatus,
            running ? L"Monitoring: Active" : L"Monitoring: Stopped");
    }
    if (_hInternetStatus) {
        if (is.online) {
            wstring txt = L"Internet: Online (" + std::to_wstring(is.latencyMs) + L"ms)";
            SetWindowText(_hInternetStatus, txt.c_str());
        } else {
            SetWindowText(_hInternetStatus, L"Internet: Offline");
        }
    }
}
