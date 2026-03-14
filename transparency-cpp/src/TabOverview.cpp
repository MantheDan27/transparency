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
    Theme::ACCENT_BLUE_GREEN,   // Devices Online  — success/healthy
    Theme::ACCENT_BLUE_AMBER,   // Unknown Devices — warning/caution
    Theme::ACCENT_BLUE_RED,     // Active Alerts   — critical
    Theme::ACCENT_BLUE_BLUE,    // Gateway Latency — primary/info
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
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
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
    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, Theme::BrushSurface());
        return 1;
    }
    case WM_COMMAND:
        return self->OnCommand(hwnd, wp, lp);
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
static const int TILE_Y  = 120;  // pushed down for NIC selector row
static const int TILE_H  = 80;  // taller to fit sparkline
static const int PILL_Y_OFF = 36;
static const int BTN_H  = 32;

static void GetLayoutMetrics(int cx, int cy,
    int& tileW, int& pillY, int& btnY, int& listY) {
    tileW = (cx - 40) / 4;
    pillY = TILE_Y + TILE_H + PILL_Y_OFF;
    btnY  = pillY + 34;
    listY = btnY + BTN_H + 52;
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
        16, pillY, 80, 24, hwnd, (HMENU)9200, hInst, nullptr);
    SendMessage(_hModeQuick, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hModeQuick, BM_SETCHECK, BST_CHECKED, 0);

    _hModeStandard = CreateWindowEx(0, L"BUTTON", L"Standard",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        102, pillY, 90, 24, hwnd, (HMENU)9201, hInst, nullptr);
    SendMessage(_hModeStandard, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hModeDeep = CreateWindowEx(0, L"BUTTON", L"Deep",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        198, pillY, 75, 24, hwnd, (HMENU)9202, hInst, nullptr);
    SendMessage(_hModeDeep, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hCheckGentle = CreateWindowEx(0, L"BUTTON", L"Gentle Mode",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        290, pillY, 110, 24, hwnd, (HMENU)IDC_CHECK_GENTLE, hInst, nullptr);
    SendMessage(_hCheckGentle, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    // Action buttons
    _hBtnQuickScan = CreateWindowEx(0, L"BUTTON", L"Quick Scan",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        16, btnY, 100, BTN_H, hwnd, (HMENU)IDC_BTN_SCAN_QUICK, hInst, nullptr);
    SendMessage(_hBtnQuickScan, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hBtnDeepScan = CreateWindowEx(0, L"BUTTON", L"Deep Scan",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        122, btnY, 100, BTN_H, hwnd, (HMENU)IDC_BTN_SCAN_DEEP, hInst, nullptr);
    SendMessage(_hBtnDeepScan, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hBtnMonStart = CreateWindowEx(0, L"BUTTON", L"Start Monitor",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        228, btnY, 110, BTN_H, hwnd, (HMENU)IDC_BTN_MONITOR_START, hInst, nullptr);
    SendMessage(_hBtnMonStart, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hBtnMonStop = CreateWindowEx(0, L"BUTTON", L"Stop Monitor",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        344, btnY, 110, BTN_H, hwnd, (HMENU)IDC_BTN_MONITOR_STOP, hInst, nullptr);
    SendMessage(_hBtnMonStop, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    EnableWindow(_hBtnMonStop, FALSE);

    _hBtnExport = CreateWindowEx(0, L"BUTTON", L"Export Report",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        460, btnY, 110, BTN_H, hwnd, (HMENU)IDC_BTN_EXPORT, hInst, nullptr);
    SendMessage(_hBtnExport, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

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

    int tileW, pillY, btnY, listY;
    GetLayoutMetrics(cx, cy, tileW, pillY, btnY, listY);

    for (int i = 0; i < 4; i++) {
        int x = 16 + i * (tileW + 8);
        if (_hKpi[i]) SetWindowPos(_hKpi[i], nullptr, x, TILE_Y, tileW, TILE_H, SWP_NOZORDER);
    }

    if (_hStatusText) SetWindowPos(_hStatusText, nullptr, 16, btnY + BTN_H + 8,  cx - 32, 20, SWP_NOZORDER);
    if (_hProgressBar)SetWindowPos(_hProgressBar, nullptr, 16, btnY + BTN_H + 32, cx - 32,  8, SWP_NOZORDER);
    if (_hNetworkInfo) SetWindowPos(_hNetworkInfo, nullptr, 16, 16, cx - 400, 40, SWP_NOZORDER);
    if (_hNicCombo) SetWindowPos(_hNicCombo, nullptr, 16, 44, 380, 160, SWP_NOZORDER);
    if (_hNicPin) SetWindowPos(_hNicPin, nullptr, 402, 44, 50, 24, SWP_NOZORDER);
    if (_hNicReason) SetWindowPos(_hNicReason, nullptr, 16, 72, cx - 32, 30, SWP_NOZORDER);

    // Topology map rect (left 60%)
    int mapW = (cx - 40) * 6 / 10;
    int mapH = cy - listY - 16;
    _mapRect = { 16, listY, 16 + mapW, listY + std::max(mapH, 80) };

    // Changes list (right 40%)
    int listX = 16 + mapW + 8;
    int listW = cx - listX - 16;
    if (_hChangesList)
        SetWindowPos(_hChangesList, nullptr, listX, listY,
                     std::max(listW, 100), std::max(mapH, 50), SWP_NOZORDER);
}

LRESULT TabOverview::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

// ── Owner-draw KPI tiles ──────────────────────────────────────────────────────

LRESULT TabOverview::OnDrawItem(HWND hwnd, DRAWITEMSTRUCT* dis) {
    if (!dis) return 0;
    int idx = dis->CtlID - IDC_STATIC_KPI1;
    if (idx < 0 || idx > 3) return 0;

    HDC   hdc = dis->hDC;
    RECT  rc  = dis->rcItem;
    COLORREF accent = KPI_ACCENTS[idx];

    // Card fill — bg_elevated (Layer 3)
    FillRect(hdc, &rc, Theme::BrushElevated());

    // 3px top accent border
    RECT topBar = { rc.left, rc.top, rc.right, rc.top + 3 };
    HBRUSH accBrush = CreateSolidBrush(accent);
    FillRect(hdc, &topBar, accBrush);
    DeleteObject(accBrush);

    // Card border — border_default
    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BORDER_DEFAULT_DEFAULT);
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    MoveToEx(hdc, rc.left,     rc.top,      nullptr);
    LineTo  (hdc, rc.right - 1, rc.top);
    LineTo  (hdc, rc.right - 1, rc.bottom - 1);
    LineTo  (hdc, rc.left,      rc.bottom - 1);
    LineTo  (hdc, rc.left,      rc.top);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    // Big number
    wchar_t valStr[32];
    if (idx == 3) {
        if (_kpiVal[3] >= 0) swprintf_s(valStr, L"%dms", _kpiVal[3]);
        else                 wcscpy_s(valStr, L"--");
    } else {
        swprintf_s(valStr, L"%d", _kpiVal[idx]);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TEXT_PRIMARY);
    HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontHeader());
    RECT numRc = { rc.left + 4, rc.top + 4, rc.right - 4, rc.top + 42 };
    DrawText(hdc, valStr, -1, &numRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    // Label
    SelectObject(hdc, Theme::FontSmall());
    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    RECT lblRc = { rc.left + 4, rc.top + 44, rc.right - 4, rc.top + 60 };
    DrawText(hdc, KPI_LABELS[idx], -1, &lblRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    SelectObject(hdc, oldFont);

    // Sparkline (7 bars) in the bottom ~18px of the tile
    const std::vector<int>* hist = nullptr;
    switch (idx) {
    case 0: hist = &_devicesOnlineHistory; break;
    case 1: hist = &_unknownDevHistory;    break;
    case 2: hist = &_alertHistory;         break;
    case 3: hist = &_latencyHistory;       break;
    }
    if (hist && !hist->empty()) {
        RECT spRc = { rc.left + 6, rc.top + 62, rc.right - 6, rc.bottom - 4 };
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
        FillRect(hdc, &barRc, barBrush);
    }
    DeleteObject(barBrush);
}

// ── Topology Map ──────────────────────────────────────────────────────────────

static COLORREF DeviceNodeColor(const Device& d) {
    if (!d.online)                          return Theme::TEXT_TERTIARY;
    if (d.iotRisk)                          return Theme::ACCENT_BLUE_AMBER;
    if (d.trustState == L"owned")           return Theme::ACCENT_BLUE_GREEN;
    if (d.trustState == L"known")           return Theme::ACCENT_BLUE_BLUE;
    if (d.trustState == L"guest")           return Theme::ACCENT_BLUE_AMBER;
    if (d.trustState == L"blocked")         return Theme::ACCENT_BLUE_RED;
    if (d.trustState == L"watchlist")       return Theme::ACCENT_BLUE_PURPLE;
    return Theme::TEXT_SECONDARY;
}

void TabOverview::DrawTopologyMap(HDC hdc, const RECT& rc) {
    // Card fill — bg_elevated (Layer 3)
    FillRect(hdc, &rc, Theme::BrushElevated());

    // Card border — border_default
    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::BORDER_DEFAULT_DEFAULT);
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    MoveToEx(hdc, rc.left,      rc.top,      nullptr);
    LineTo  (hdc, rc.right - 1, rc.top);
    LineTo  (hdc, rc.right - 1, rc.bottom - 1);
    LineTo  (hdc, rc.left,      rc.bottom - 1);
    LineTo  (hdc, rc.left,      rc.top);

    // Section label
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontSmall());
    RECT hdrRc = { rc.left + 8, rc.top + 6, rc.right - 8, rc.top + 20 };
    DrawText(hdc, L"NETWORK MAP", -1, &hdrRc, DT_LEFT | DT_SINGLELINE);

    if (!_mainWnd) goto cleanup;
    {
        ScanResult r = _mainWnd->GetLastResult();
        if (r.devices.empty()) {
            SetTextColor(hdc, Theme::TEXT_TERTIARY);
            RECT noRc = { rc.left, rc.top + 24, rc.right, rc.bottom };
            DrawText(hdc, L"Run a scan to see the network map.",
                     -1, &noRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            goto cleanup;
        }

        // Group devices by subnet
        std::map<wstring, std::vector<int>> subnetGroups;
        for (int i = 0; i < (int)r.devices.size(); i++) {
            wstring sub = r.devices[i].subnet;
            if (sub.empty()) sub = L"default";
            subnetGroups[sub].push_back(i);
        }

        int mapLeft   = rc.left   + 12;
        int mapTop    = rc.top    + 28;
        int mapRight  = rc.right  - 12;
        int mapBottom = rc.bottom - 12;

        if (subnetGroups.size() <= 1) {
            // Single subnet — classic radial layout
            int cx = (mapLeft + mapRight)  / 2;
            int cy = (mapTop  + mapBottom) / 2;
            int radius = (std::min(mapRight - mapLeft, mapBottom - mapTop) / 2) - 22;
            if (radius < 20) radius = 20;

            int n = std::min((int)r.devices.size(), 24);

            HPEN linePen = CreatePen(PS_SOLID, 1, Theme::BORDER_DEFAULT);
            SelectObject(hdc, linePen);
            for (int i = 0; i < n; i++) {
                double angle = 2.0 * 3.14159265 * i / n - 3.14159265 / 2.0;
                int nx = cx + (int)(radius * cos(angle));
                int ny = cy + (int)(radius * sin(angle));
                MoveToEx(hdc, cx, cy, nullptr);
                LineTo(hdc, nx, ny);
            }
            DeleteObject(linePen);

            for (int i = 0; i < n; i++) {
                auto& d = r.devices[i];
                double angle = 2.0 * 3.14159265 * i / n - 3.14159265 / 2.0;
                int nx = cx + (int)(radius * cos(angle));
                int ny = cy + (int)(radius * sin(angle));

                COLORREF col = DeviceNodeColor(d);
                HBRUSH nb = CreateSolidBrush(col);
                HPEN   np = CreatePen(PS_SOLID, 1, col);
                HBRUSH ob = (HBRUSH)SelectObject(hdc, nb);
                HPEN   op = (HPEN)SelectObject(hdc, np);
                Ellipse(hdc, nx - 6, ny - 6, nx + 6, ny + 6);
                SelectObject(hdc, ob); SelectObject(hdc, op);
                DeleteObject(nb); DeleteObject(np);

                wstring lbl = d.customName.empty()
                    ? (d.hostname.empty() ? d.ip : d.hostname)
                    : d.customName;
                if ((int)lbl.size() > 10) lbl = lbl.substr(0, 10);

                SetTextColor(hdc, Theme::TEXT_TERTIARY);
                SelectObject(hdc, Theme::FontSmall());
                RECT lblRc = { nx - 38, ny + 8, nx + 38, ny + 20 };
                DrawText(hdc, lbl.c_str(), -1, &lblRc,
                         DT_CENTER | DT_SINGLELINE | DT_NOCLIP);
            }

            // Gateway center node
            HBRUSH gwb = CreateSolidBrush(Theme::ACCENT_BLUE_GLOW);
            HPEN   gwp = CreatePen(PS_SOLID, 2, Theme::ACCENT_BLUE_GLOW);
            HBRUSH ob2 = (HBRUSH)SelectObject(hdc, gwb);
            HPEN   op2 = (HPEN)SelectObject(hdc, gwp);
            Ellipse(hdc, cx - 14, cy - 14, cx + 14, cy + 14);
            SelectObject(hdc, ob2); SelectObject(hdc, op2);
            DeleteObject(gwb); DeleteObject(gwp);

            SetTextColor(hdc, Theme::BG_APP);
            SelectObject(hdc, Theme::FontSmall());
            RECT gwRc = { cx - 14, cy - 8, cx + 14, cy + 8 };
            DrawText(hdc, L"GW", -1, &gwRc,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else {
            // Multi-subnet layout — horizontal bands per subnet with gateway at top center
            int gwCx = (mapLeft + mapRight) / 2;
            int gwCy = mapTop + 16;

            // Gateway node
            HBRUSH gwb = CreateSolidBrush(Theme::ACCENT_BLUE_GLOW);
            HPEN   gwp = CreatePen(PS_SOLID, 2, Theme::ACCENT_BLUE_GLOW);
            HBRUSH ob2 = (HBRUSH)SelectObject(hdc, gwb);
            HPEN   op2 = (HPEN)SelectObject(hdc, gwp);
            Ellipse(hdc, gwCx - 12, gwCy - 12, gwCx + 12, gwCy + 12);
            SelectObject(hdc, ob2); SelectObject(hdc, op2);
            DeleteObject(gwb); DeleteObject(gwp);

            SetTextColor(hdc, Theme::BG_APP);
            SelectObject(hdc, Theme::FontSmall());
            RECT gwRc = { gwCx - 12, gwCy - 7, gwCx + 12, gwCy + 7 };
            DrawText(hdc, L"GW", -1, &gwRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            int bandTop = gwCy + 20;
            int totalH = mapBottom - bandTop;
            int numSubnets = std::min((int)subnetGroups.size(), 4);
            int bandH = totalH / std::max(numSubnets, 1);

            int subIdx = 0;
            for (auto& [subName, devIndices] : subnetGroups) {
                if (subIdx >= 4) break;
                int bTop = bandTop + subIdx * bandH;
                int bBot = bTop + bandH - 4;

                // Subnet label
                SetTextColor(hdc, Theme::ACCENT_BLUE);
                SelectObject(hdc, Theme::FontSmall());
                RECT subRc = { mapLeft, bTop, mapRight, bTop + 14 };
                DrawText(hdc, subName.c_str(), -1, &subRc, DT_LEFT | DT_SINGLELINE);

                // Dashed separator
                HPEN sepPen = CreatePen(PS_DOT, 1, Theme::BORDER_DEFAULT);
                SelectObject(hdc, sepPen);
                MoveToEx(hdc, mapLeft, bTop + 15, nullptr);
                LineTo(hdc, mapRight, bTop + 15);
                DeleteObject(sepPen);

                // Line from gateway down to this subnet center
                int subCx = (mapLeft + mapRight) / 2;
                int subCy = (bTop + 16 + bBot) / 2;
                HPEN linePen = CreatePen(PS_SOLID, 1, Theme::BORDER_DEFAULT);
                SelectObject(hdc, linePen);
                MoveToEx(hdc, gwCx, gwCy + 12, nullptr);
                LineTo(hdc, subCx, bTop + 16);
                DeleteObject(linePen);

                // Devices in this subnet — horizontal spread
                int n = std::min((int)devIndices.size(), 12);
                int nodeW = (mapRight - mapLeft) / std::max(n, 1);
                for (int i = 0; i < n; i++) {
                    const Device& d = r.devices[devIndices[i]];
                    int nx = mapLeft + nodeW / 2 + i * nodeW;
                    int ny = subCy;

                    // Line from subnet center
                    HPEN lp2 = CreatePen(PS_SOLID, 1, Theme::BORDER_DEFAULT);
                    SelectObject(hdc, lp2);
                    MoveToEx(hdc, subCx, bTop + 16, nullptr);
                    LineTo(hdc, nx, ny);
                    DeleteObject(lp2);

                    COLORREF col = DeviceNodeColor(d);
                    HBRUSH nb = CreateSolidBrush(col);
                    HPEN   np = CreatePen(PS_SOLID, 1, col);
                    HBRUSH ob = (HBRUSH)SelectObject(hdc, nb);
                    HPEN   op = (HPEN)SelectObject(hdc, np);
                    Ellipse(hdc, nx - 5, ny - 5, nx + 5, ny + 5);
                    SelectObject(hdc, ob); SelectObject(hdc, op);
                    DeleteObject(nb); DeleteObject(np);

                    wstring lbl = d.customName.empty()
                        ? (d.hostname.empty() ? d.ip : d.hostname)
                        : d.customName;
                    if ((int)lbl.size() > 8) lbl = lbl.substr(0, 8);

                    SetTextColor(hdc, Theme::TEXT_TERTIARY);
                    SelectObject(hdc, Theme::FontSmall());
                    RECT lblRc = { nx - 32, ny + 7, nx + 32, ny + 18 };
                    DrawText(hdc, lbl.c_str(), -1, &lblRc,
                             DT_CENTER | DT_SINGLELINE | DT_NOCLIP);
                }

                subIdx++;
            }
        }
    }

cleanup:
    SelectObject(hdc, oldFont);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);
}

// ── OnPaint ───────────────────────────────────────────────────────────────────

LRESULT TabOverview::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::BrushSurface());

    // Topology map (left portion of bottom area)
    if (_mapRect.right > _mapRect.left && _mapRect.bottom > _mapRect.top)
        DrawTopologyMap(hdc, _mapRect);

    // Section header "RECENT CHANGES" above the changes list
    {
        int tileW, pillY, btnY, listY;
        GetLayoutMetrics(rc.right, rc.bottom, tileW, pillY, btnY, listY);
        int mapW = (rc.right - 40) * 6 / 10;
        int listX = 16 + mapW + 8;

        RECT hdrRc = { listX, listY - 18, rc.right - 16, listY - 2 };
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::TEXT_SECONDARY);
        HFONT old = (HFONT)SelectObject(hdc, Theme::FontSmall());
        DrawText(hdc, L"RECENT CHANGES", -1, &hdrRc,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, old);
    }

    EndPaint(hwnd, &ps);
    return 0;
}

LRESULT TabOverview::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = LOWORD(wp);

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
            else if (SendMessage(_hModeStandard, BM_GETCHECK, 0, 0) == BST_CHECKED)
                _mainWnd->StartStandardScan();
            else
                _mainWnd->StartQuickScan();
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
    RefreshKPIs();
    if (_hStatusText)  SetWindowText(_hStatusText, L"Scan complete.");
    if (_hProgressBar) SendMessage(_hProgressBar, PBM_SETPOS, 100, 0);

    // Invalidate map area to repaint topology
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
    RefreshKPIs();
}

void TabOverview::UpdateMonitorStatus(bool running, const InternetStatus& is) {
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
