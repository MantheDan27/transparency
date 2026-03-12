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

// Accent colors for each KPI tile (top border + sparkline)
static const COLORREF KPI_ACCENTS[] = {
    Theme::SUCCESS,  // Devices Online
    Theme::WARNING,  // Unknown Devices
    Theme::DANGER,   // Active Alerts
    Theme::ACCENT,   // Gateway Latency
};

bool TabOverview::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
    _mainWnd = mainWnd;

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.hbrBackground = Theme::BrushApp();
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
        FillRect((HDC)wp, &rc, Theme::BrushApp());
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
        SetBkColor(hdc, Theme::BG_APP);
        return (LRESULT)Theme::BrushApp();
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
static const int TILE_Y  = 100;
static const int TILE_H  = 90;
static const int PILL_Y_OFF = 40;
static const int BTN_H  = 36;

static void GetLayoutMetrics(int cx, int cy,
    int& tileW, int& pillY, int& btnY, int& listY) {
    tileW = (cx - 48) / 4;
    pillY = TILE_Y + TILE_H + PILL_Y_OFF;
    btnY  = pillY + 44;
    listY = btnY + BTN_H + 60;
}

static void GetBottomSplitMetrics(int cx, int& mapW, int& listX, int& listW) {
    mapW = (cx - 48) * 6 / 10;
    if (mapW < 260) mapW = 260;
    listX = 16 + mapW + 12;
    listW = cx - listX - 16;
}

void TabOverview::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    int tileW, pillY, btnY, listY;
    GetLayoutMetrics(cx, cy, tileW, pillY, btnY, listY);

    // KPI tiles
    for (int i = 0; i < 4; i++) {
        int x = 16 + i * (tileW + 10);
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

    // Action buttons (Premium Pills)
    auto createPill = [&](const wchar_t* lbl, int x, int w, int id) {
        HWND h = CreateWindowEx(0, L"BUTTON", lbl,
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            x, btnY, w, BTN_H, hwnd, (HMENU)id, hInst, nullptr);
        SendMessage(h, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return h;
    };

    _hBtnQuickScan = createPill(L"Quick Scan", 16, 110, IDC_BTN_SCAN_QUICK);
    _hBtnDeepScan  = createPill(L"Deep Scan", 134, 110, IDC_BTN_SCAN_DEEP);
    _hBtnMonStart  = createPill(L"Start Monitor", 252, 120, IDC_BTN_MONITOR_START);
    _hBtnMonStop   = createPill(L"Stop Monitor", 380, 120, IDC_BTN_MONITOR_STOP);
    _hBtnExport    = createPill(L"Export JSON", 508, 120, IDC_BTN_EXPORT);

    EnableWindow(_hBtnMonStop, FALSE);

    // Progress
    _hStatusText = CreateWindowEx(0, L"STATIC", L"Ready for network assessment.",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, btnY + BTN_H + 12, cx - 32, 20,
        hwnd, (HMENU)IDC_STATIC_STATUS, hInst, nullptr);
    SendMessage(_hStatusText, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        16, btnY + BTN_H + 36, cx - 32, 6,
        hwnd, nullptr, hInst, nullptr);
    SendMessage(_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(_hProgressBar, PBM_SETBKCOLOR, 0, Theme::BG_CARD);
    SendMessage(_hProgressBar, PBM_SETBARCOLOR, 0, Theme::ACCENT);

    // Header Info
    _hNetworkInfo = CreateWindowEx(0, L"STATIC", L"Discovering environment...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 16, cx - 400, 40,
        hwnd, (HMENU)IDC_STATIC_NET_INFO, hInst, nullptr);
    SendMessage(_hNetworkInfo, WM_SETFONT, (WPARAM)Theme::FontBold(), TRUE);

    _hNicCombo = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        16, 48, 380, 200, hwnd, (HMENU)IDC_COMBO_NIC_SELECT, hInst, nullptr);
    SendMessage(_hNicCombo, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hNicReason = CreateWindowEx(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 76, cx - 32, 20,
        hwnd, nullptr, hInst, nullptr);
    SendMessage(_hNicReason, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);

    // Changes List
    int mapW = 0, listX = 0, listW = 0;
    GetBottomSplitMetrics(cx, mapW, listX, listW);
    int listH = cy - listY - 16;

    _hChangesList = CreateWindowEx(
        0, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | WS_VSCROLL | LVS_NOCOLUMNHEADER,
        listX, listY, std::max(listW, 100), std::max(listH, 50),
        hwnd, (HMENU)IDC_LIST_CHANGES, hInst, nullptr);

    SendMessage(_hChangesList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hChangesList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    Theme::ApplyDarkListView(_hChangesList);

    LVCOLUMN col = {};
    col.mask = LVCF_WIDTH;
    col.cx = 80;  ListView_InsertColumn(_hChangesList, 0, &col);
    col.cx = 100; ListView_InsertColumn(_hChangesList, 1, &col);
    col.cx = 300; ListView_InsertColumn(_hChangesList, 2, &col);
}

void TabOverview::LayoutControls(int cx, int cy) {
    if (!_hwnd) return;

    int tileW, pillY, btnY, listY;
    GetLayoutMetrics(cx, cy, tileW, pillY, btnY, listY);

    for (int i = 0; i < 4; i++) {
        int x = 16 + i * (tileW + 10);
        if (_hKpi[i]) SetWindowPos(_hKpi[i], nullptr, x, TILE_Y, tileW, TILE_H, SWP_NOZORDER);
    }

    if (_hBtnQuickScan) SetWindowPos(_hBtnQuickScan, nullptr, 16, btnY, 110, BTN_H, SWP_NOZORDER);
    if (_hBtnDeepScan)  SetWindowPos(_hBtnDeepScan,  nullptr, 134, btnY, 110, BTN_H, SWP_NOZORDER);
    if (_hBtnMonStart)  SetWindowPos(_hBtnMonStart,  nullptr, 252, btnY, 120, BTN_H, SWP_NOZORDER);
    if (_hBtnMonStop)   SetWindowPos(_hBtnMonStop,   nullptr, 380, btnY, 120, BTN_H, SWP_NOZORDER);
    if (_hBtnExport)    SetWindowPos(_hBtnExport,    nullptr, 508, btnY, 120, BTN_H, SWP_NOZORDER);

    if (_hStatusText)  SetWindowPos(_hStatusText,  nullptr, 16, btnY + BTN_H + 12, cx - 32, 20, SWP_NOZORDER);
    if (_hProgressBar) SetWindowPos(_hProgressBar, nullptr, 16, btnY + BTN_H + 36, cx - 32, 6, SWP_NOZORDER);
    if (_hNetworkInfo) SetWindowPos(_hNetworkInfo, nullptr, 16, 16, cx - 400, 40, SWP_NOZORDER);
    if (_hNicCombo)    SetWindowPos(_hNicCombo,    nullptr, 16, 48, 380, 200, SWP_NOZORDER);
    if (_hNicReason)   SetWindowPos(_hNicReason,   nullptr, 16, 76, cx - 32, 20, SWP_NOZORDER);

    int mapW = 0, listX = 0, listW = 0;
    GetBottomSplitMetrics(cx, mapW, listX, listW);
    int mapH = cy - listY - 16;
    _mapRect = { 16, listY, 16 + mapW, listY + std::max(mapH, 80) };

    if (_hChangesList) {
        SetWindowPos(_hChangesList, nullptr, listX, listY,
                     std::max(listW, 100), std::max(mapH, 50), SWP_NOZORDER);
        ListView_SetColumnWidth(_hChangesList, 0, 104);
        ListView_SetColumnWidth(_hChangesList, 1, 120);
        ListView_SetColumnWidth(_hChangesList, 2, std::max(220, listW - 240));
    }
}

LRESULT TabOverview::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

// ── Owner-draw logic ─────────────────────────────────────────────────────────

LRESULT TabOverview::OnDrawItem(HWND hwnd, DRAWITEMSTRUCT* dis) {
    if (!dis) return 0;
    
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    
    // KPI Tiles
    if (dis->CtlID >= IDC_STATIC_KPI1 && dis->CtlID <= IDC_STATIC_KPI4) {
        int idx = dis->CtlID - IDC_STATIC_KPI1;
        COLORREF accent = KPI_ACCENTS[idx];

        // --- Glass card background ---
        Theme::DrawGlassCard(hdc, rc);

        // --- Left-edge accent bar (3px wide, full height inset by 2px top+bottom) ---
        {
            HBRUSH accentBrush = CreateSolidBrush(accent);
            RECT accentBar = { rc.left, rc.top + 10, rc.left + 3, rc.bottom - 10 };
            FillRect(hdc, &accentBar, accentBrush);
            DeleteObject(accentBrush);
        }

        // --- KPI value string ---
        wchar_t valStr[32];
        if (idx == 3) {
            if (_kpiVal[3] >= 0) swprintf_s(valStr, L"%dms", _kpiVal[3]);
            else                 wcscpy_s(valStr, L"--");
        } else {
            swprintf_s(valStr, L"%d", _kpiVal[idx]);
        }

        // Value in large font — centered in the upper 60% of the card
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontKPI());
        int cardInner = rc.left + 12; // offset past accent bar + padding
        RECT numRc = { cardInner, rc.top + 8, rc.right - 6, rc.top + 54 };
        DrawText(hdc, valStr, -1, &numRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        // --- KPI label in small muted font ---
        SelectObject(hdc, Theme::FontSmall());
        SetTextColor(hdc, Theme::TEXT_MUTED);
        RECT lblRc = { cardInner, rc.top + 55, rc.right - 6, rc.top + 71 };
        DrawText(hdc, KPI_LABELS[idx], -1, &lblRc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        // --- Sparkline bars at the bottom of the card ---
        const std::vector<int>* hist = nullptr;
        switch (idx) {
        case 0: hist = &_devicesOnlineHistory; break;
        case 1: hist = &_unknownDevHistory;    break;
        case 2: hist = &_alertHistory;         break;
        case 3: hist = &_latencyHistory;       break;
        }
        if (hist && !hist->empty()) {
            RECT spRc = { rc.left + 10, rc.top + 73, rc.right - 6, rc.bottom - 6 };
            DrawSparkline(hdc, spRc, *hist, accent);
        }

        SelectObject(hdc, oldFont);
        return TRUE;
    }

    // Action Buttons (Pills) — differentiate scan buttons (ACCENT) vs monitor/export (BG_ELEVATED)
    if (dis->CtlType == ODT_BUTTON) {
        bool pushed   = (dis->itemState & ODS_SELECTED) != 0;
        bool disabled = (dis->itemState & ODS_DISABLED)  != 0;

        // Determine button identity from its ID
        int ctlId = GetDlgCtrlID(dis->hwndItem);
        bool isScanBtn   = (ctlId == IDC_BTN_SCAN_QUICK || ctlId == IDC_BTN_SCAN_DEEP);
        bool isMonStart  = (ctlId == IDC_BTN_MONITOR_START);
        bool isMonStop   = (ctlId == IDC_BTN_MONITOR_STOP);
        bool isExport    = (ctlId == IDC_BTN_EXPORT);

        COLORREF bg, brd, fg;

        if (disabled) {
            bg  = Theme::BG_INPUT;
            brd = Theme::BORDER;
            fg  = Theme::TEXT_MUTED;
        } else if (isScanBtn) {
            // Primary action: accent-filled pill
            bg  = pushed ? Theme::BG_ROW_SEL : Theme::ACCENT;
            brd = Theme::ACCENT;
            fg  = RGB(255, 255, 255);
        } else if (isMonStart) {
            // Secondary action: elevated background
            bg  = pushed ? Theme::BG_ROW_SEL : Theme::BG_ELEVATED;
            brd = Theme::BORDER;
            fg  = Theme::TEXT_PRIMARY;
        } else if (isMonStop) {
            // Danger-tinted stop button
            bg  = pushed ? Theme::BG_ROW_SEL : Theme::BG_ELEVATED;
            brd = Theme::DANGER;
            fg  = Theme::DANGER;
        } else if (isExport) {
            bg  = pushed ? Theme::BG_ROW_SEL : Theme::BG_ELEVATED;
            brd = Theme::BORDER;
            fg  = Theme::TEXT_SECONDARY;
        } else {
            bg  = pushed ? Theme::BG_ROW_SEL : Theme::BG_CARD;
            brd = pushed ? Theme::ACCENT : Theme::BORDER;
            fg  = Theme::TEXT_PRIMARY;
        }

        int pillR = (rc.bottom - rc.top); // full height as corner radius for pill shape
        HBRUSH bgB  = CreateSolidBrush(bg);
        HPEN   brdP = CreatePen(PS_SOLID, 1, brd);
        HBRUSH oldB = (HBRUSH)SelectObject(hdc, bgB);
        HPEN   oldP = (HPEN)SelectObject(hdc, brdP);

        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, pillR, pillR);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, fg);
        HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontBold());

        wchar_t txt[128];
        GetWindowText(dis->hwndItem, txt, 128);
        DrawText(hdc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, oldFont);
        SelectObject(hdc, oldB);
        SelectObject(hdc, oldP);
        DeleteObject(bgB);
        DeleteObject(brdP);
        return TRUE;
    }

    return FALSE;
}

void TabOverview::DrawSparkline(HDC hdc, const RECT& rc,
                                const std::vector<int>& vals, COLORREF col) {
    if (vals.empty()) return;
    int n = (int)vals.size();
    int w = rc.right  - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    // Find max (at least 1 to avoid divide-by-zero)
    int maxVal = 1;
    for (int v : vals) if (v > maxVal) maxVal = v;

    // Draw a subtle trough background so empty bars are visible
    {
        HBRUSH troughBrush = CreateSolidBrush(Theme::BG_INPUT);
        FillRect(hdc, &rc, troughBrush);
        DeleteObject(troughBrush);
    }

    // Dim bar color: 55% accent blended over BG_CARD
    BYTE r = (BYTE)(GetRValue(col) * 55 / 100 + GetRValue(Theme::BG_CARD) * 45 / 100);
    BYTE g = (BYTE)(GetGValue(col) * 55 / 100 + GetGValue(Theme::BG_CARD) * 45 / 100);
    BYTE b = (BYTE)(GetBValue(col) * 55 / 100 + GetBValue(Theme::BG_CARD) * 45 / 100);
    HBRUSH dimBrush = CreateSolidBrush(RGB(r, g, b));

    // Bright bar for the most recent (last) value
    HBRUSH brightBrush = CreateSolidBrush(col);

    int slotW = w / n;
    if (slotW < 1) slotW = 1;
    int barW = std::max(2, slotW - 2);

    for (int i = 0; i < n; i++) {
        int barH = h;
        if (maxVal > 0) barH = (vals[i] * h) / maxVal;
        if (barH < 2 && vals[i] > 0) barH = 2;
        if (barH > h) barH = h;

        int x = rc.left + i * slotW + (slotW - barW) / 2;
        RECT barRc = { x, rc.bottom - barH, x + barW, rc.bottom };

        // Last bar draws in full accent color, older bars are dim
        FillRect(hdc, &barRc, (i == n - 1) ? brightBrush : dimBrush);
    }

    DeleteObject(dimBrush);
    DeleteObject(brightBrush);
}

// ── Topology Map ──────────────────────────────────────────────────────────────

static COLORREF DeviceNodeColor(const Device& d) {
    if (!d.online)                          return Theme::TEXT_MUTED;
    if (d.iotRisk)                          return Theme::WARNING;
    if (d.trustState == L"owned")           return Theme::SUCCESS;
    if (d.trustState == L"known")           return Theme::ACCENT;
    if (d.trustState == L"guest")           return Theme::WARNING;
    if (d.trustState == L"blocked")         return Theme::DANGER;
    if (d.trustState == L"watchlist")       return Theme::TRUST_WATCHLIST;
    return RGB(80, 90, 120);
}

// Helper: draw a filled circle node with a 1px darker border ring
static void DrawMapNode(HDC hdc, int cx, int cy, int radius, COLORREF fill) {
    // Outer glow ring — slightly larger, darker blend
    HBRUSH ringBrush = CreateSolidBrush(RGB(
        (BYTE)(GetRValue(fill) / 4),
        (BYTE)(GetGValue(fill) / 4),
        (BYTE)(GetBValue(fill) / 4)));
    HPEN   ringPen   = CreatePen(PS_SOLID, 1, fill);
    HBRUSH ob = (HBRUSH)SelectObject(hdc, ringBrush);
    HPEN   op = (HPEN)  SelectObject(hdc, ringPen);
    Ellipse(hdc, cx - radius - 3, cy - radius - 3, cx + radius + 3, cy + radius + 3);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(ringBrush);
    DeleteObject(ringPen);

    // Filled body
    HBRUSH nb = CreateSolidBrush(fill);
    HPEN   np = CreatePen(PS_SOLID, 1, fill);
    ob = (HBRUSH)SelectObject(hdc, nb);
    op = (HPEN)  SelectObject(hdc, np);
    Ellipse(hdc, cx - radius, cy - radius, cx + radius, cy + radius);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(nb);
    DeleteObject(np);
}

void TabOverview::DrawTopologyMap(HDC hdc, const RECT& rc) {
    // --- Glass card background ---
    Theme::DrawGlassCard(hdc, rc);

    // Card header label — FontBold (13pt semibold) fits the 16px header band cleanly
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TEXT_SECONDARY);
    HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontBold());
    RECT hdrRc = { rc.left + 10, rc.top + 6, rc.right - 10, rc.top + 24 };
    DrawText(hdc, L"NETWORK TOPOLOGY", -1, &hdrRc, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    // Thin separator line under header
    {
        HPEN sepPen = CreatePen(PS_SOLID, 1, Theme::BORDER);
        HPEN oldP = (HPEN)SelectObject(hdc, sepPen);
        MoveToEx(hdc, rc.left + 8,  rc.top + 26, nullptr);
        LineTo  (hdc, rc.right - 8, rc.top + 26);
        SelectObject(hdc, oldP);
        DeleteObject(sepPen);
    }

    HPEN oldPen = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));

    if (!_mainWnd) goto cleanup;
    {
        ScanResult r = _mainWnd->GetLastResult();
        if (r.devices.empty()) {
            SetTextColor(hdc, Theme::TEXT_MUTED);
            SelectObject(hdc, Theme::FontBody());
            RECT noRc = { rc.left, rc.top + 28, rc.right, rc.bottom };
            DrawText(hdc, L"Run a scan to populate the network map.",
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

        int mapLeft   = rc.left   + 14;
        int mapTop    = rc.top    + 30;
        int mapRight  = rc.right  - 14;
        int mapBottom = rc.bottom - 10;

        // Gateway node parameters
        const int GW_RADIUS  = 14;
        const int DEV_RADIUS = 8;

        if (subnetGroups.size() <= 1) {
            // ── Single subnet: classic radial layout ──────────────────────────
            int cx = (mapLeft + mapRight)  / 2;
            int cy = (mapTop  + mapBottom) / 2;
            int radius = (std::min(mapRight - mapLeft, mapBottom - mapTop) / 2) - 24;
            if (radius < 20) radius = 20;

            int n = std::min((int)r.devices.size(), 24);

            // Draw connection lines first (under nodes)
            {
                HPEN linePen = CreatePen(PS_SOLID, 1, Theme::BORDER);
                HPEN prevPen = (HPEN)SelectObject(hdc, linePen);
                for (int i = 0; i < n; i++) {
                    double angle = 2.0 * 3.14159265 * i / n - 3.14159265 / 2.0;
                    int nx = cx + (int)(radius * cos(angle));
                    int ny = cy + (int)(radius * sin(angle));
                    MoveToEx(hdc, cx, cy, nullptr);
                    LineTo(hdc, nx, ny);
                }
                SelectObject(hdc, prevPen);
                DeleteObject(linePen);
            }

            // Draw device nodes and labels
            SelectObject(hdc, Theme::FontSmall());
            for (int i = 0; i < n; i++) {
                auto& d = r.devices[i];
                double angle = 2.0 * 3.14159265 * i / n - 3.14159265 / 2.0;
                int nx = cx + (int)(radius * cos(angle));
                int ny = cy + (int)(radius * sin(angle));

                COLORREF col = DeviceNodeColor(d);
                DrawMapNode(hdc, nx, ny, DEV_RADIUS, col);

                wstring lbl = d.customName.empty()
                    ? (d.hostname.empty() ? d.ip : d.hostname)
                    : d.customName;
                if ((int)lbl.size() > 12) lbl = lbl.substr(0, 12);

                SetTextColor(hdc, Theme::TEXT_SECONDARY);
                RECT lblRc = { nx - 40, ny + DEV_RADIUS + 3,
                               nx + 40, ny + DEV_RADIUS + 15 };
                DrawText(hdc, lbl.c_str(), -1, &lblRc,
                         DT_CENTER | DT_SINGLELINE | DT_NOCLIP);
            }

            // Gateway center node (larger, accent-glowing)
            DrawMapNode(hdc, cx, cy, GW_RADIUS, Theme::ACCENT);

            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, Theme::FontSmall());
            RECT gwRc = { cx - GW_RADIUS, cy - GW_RADIUS,
                          cx + GW_RADIUS, cy + GW_RADIUS };
            DrawText(hdc, L"GW", -1, &gwRc,
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        } else {
            // ── Multi-subnet: horizontal bands with gateway at top center ─────
            int gwCx = (mapLeft + mapRight) / 2;
            int gwCy = mapTop + GW_RADIUS + 2;

            // Draw gateway first
            DrawMapNode(hdc, gwCx, gwCy, GW_RADIUS, Theme::ACCENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            SelectObject(hdc, Theme::FontSmall());
            RECT gwRc = { gwCx - GW_RADIUS, gwCy - GW_RADIUS,
                          gwCx + GW_RADIUS, gwCy + GW_RADIUS };
            DrawText(hdc, L"GW", -1, &gwRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            int bandTop    = gwCy + GW_RADIUS + 10;
            int totalH     = mapBottom - bandTop;
            int numSubnets = std::min((int)subnetGroups.size(), 4);
            int bandH      = totalH / std::max(numSubnets, 1);

            int subIdx = 0;
            for (auto& [subName, devIndices] : subnetGroups) {
                if (subIdx >= 4) break;
                int bTop = bandTop + subIdx * bandH;
                int bBot = bTop + bandH - 4;
                int subCx = (mapLeft + mapRight) / 2;
                int subCy = (bTop + 18 + bBot) / 2;

                // Line from gateway to this subnet midpoint
                {
                    HPEN linePen = CreatePen(PS_SOLID, 1, Theme::BORDER);
                    HPEN prevPen = (HPEN)SelectObject(hdc, linePen);
                    MoveToEx(hdc, gwCx, gwCy + GW_RADIUS, nullptr);
                    LineTo(hdc, subCx, bTop + 18);
                    SelectObject(hdc, prevPen);
                    DeleteObject(linePen);
                }

                // Subnet label — FontSmall in accent color for compact band headers
                SetTextColor(hdc, Theme::ACCENT_BRIGHT);
                SelectObject(hdc, Theme::FontSmall());
                RECT subRc = { mapLeft, bTop + 2, mapRight, bTop + 16 };
                DrawText(hdc, subName.c_str(), -1, &subRc, DT_LEFT | DT_SINGLELINE);

                // Dashed separator
                {
                    HPEN sepPen = CreatePen(PS_DOT, 1, Theme::BORDER);
                    HPEN prevPen = (HPEN)SelectObject(hdc, sepPen);
                    MoveToEx(hdc, mapLeft, bTop + 17, nullptr);
                    LineTo(hdc, mapRight, bTop + 17);
                    SelectObject(hdc, prevPen);
                    DeleteObject(sepPen);
                }

                // Devices in this subnet — horizontal spread
                int n     = std::min((int)devIndices.size(), 12);
                int nodeW = (mapRight - mapLeft) / std::max(n, 1);

                for (int i = 0; i < n; i++) {
                    const Device& d = r.devices[devIndices[i]];
                    int nx = mapLeft + nodeW / 2 + i * nodeW;
                    int ny = subCy;

                    // Line from subnet band top down to device
                    {
                        HPEN lp2 = CreatePen(PS_SOLID, 1, Theme::BORDER);
                        HPEN prevPen = (HPEN)SelectObject(hdc, lp2);
                        MoveToEx(hdc, subCx, bTop + 18, nullptr);
                        LineTo(hdc, nx, ny);
                        SelectObject(hdc, prevPen);
                        DeleteObject(lp2);
                    }

                    COLORREF col = DeviceNodeColor(d);
                    DrawMapNode(hdc, nx, ny, DEV_RADIUS, col);

                    wstring lbl = d.customName.empty()
                        ? (d.hostname.empty() ? d.ip : d.hostname)
                        : d.customName;
                    if ((int)lbl.size() > 9) lbl = lbl.substr(0, 9);

                    SetTextColor(hdc, Theme::TEXT_SECONDARY);
                    SelectObject(hdc, Theme::FontSmall());
                    RECT lblRc = { nx - 34, ny + DEV_RADIUS + 3,
                                   nx + 34, ny + DEV_RADIUS + 15 };
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
}

// ── OnPaint ───────────────────────────────────────────────────────────────────

LRESULT TabOverview::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);

    // App-level background fill
    FillRect(hdc, &rc, Theme::BrushApp());

    SetBkMode(hdc, TRANSPARENT);

    int tileW, pillY, btnY, listY;
    GetLayoutMetrics(rc.right, rc.bottom, tileW, pillY, btnY, listY);
    int mapW = 0, listX = 0, listW = 0;
    GetBottomSplitMetrics(rc.right, mapW, listX, listW);

    // ── Section label: "NETWORK OVERVIEW" above the KPI tiles ───────────────
    // Use FontBold (13pt semibold) — fits precisely in the 18px header slot
    {
        HFONT oldF = (HFONT)SelectObject(hdc, Theme::FontBold());
        SetTextColor(hdc, Theme::TEXT_SECONDARY);
        RECT kpiHdrRc = { 16, TILE_Y - 18, rc.right - 16, TILE_Y - 2 };
        DrawText(hdc, L"NETWORK OVERVIEW", -1, &kpiHdrRc,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
    }

    // ── Section label: "SCAN CONTROLS" above the pill buttons ────────────────
    {
        HFONT oldF = (HFONT)SelectObject(hdc, Theme::FontBold());
        SetTextColor(hdc, Theme::TEXT_SECONDARY);
        RECT scanHdrRc = { 16, pillY - 18, rc.right - 16, pillY - 2 };
        DrawText(hdc, L"SCAN CONTROLS", -1, &scanHdrRc,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
    }

    // ── Thin separator between scan controls and bottom panels ───────────────
    {
        int sepY = listY - 6;
        HPEN sepPen = CreatePen(PS_SOLID, 1, Theme::BORDER);
        HPEN oldP = (HPEN)SelectObject(hdc, sepPen);
        MoveToEx(hdc, 16, sepY, nullptr);
        LineTo(hdc, rc.right - 16, sepY);
        SelectObject(hdc, oldP);
        DeleteObject(sepPen);
    }

    // ── Section label: "NETWORK TOPOLOGY" above the map ─────────────────────
    {
        HFONT oldF = (HFONT)SelectObject(hdc, Theme::FontBold());
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        RECT mapHdrRc = { 16, listY - 20, 16 + mapW, listY - 4 };
        DrawText(hdc, L"NETWORK TOPOLOGY", -1, &mapHdrRc,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
    }

    // ── Section label: "RECENT CHANGES" above the changes list ───────────────
    {
        HFONT oldF = (HFONT)SelectObject(hdc, Theme::FontBold());
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        RECT hdrRc = { listX, listY - 20, rc.right - 16, listY - 4 };
        DrawText(hdc, L"RECENT CHANGES", -1, &hdrRc,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF);
    }

    // ── Topology map (left portion of bottom area) ────────────────────────────
    if (_mapRect.right > _mapRect.left && _mapRect.bottom > _mapRect.top)
        DrawTopologyMap(hdc, _mapRect);

    // ── Status strip at the very bottom ──────────────────────────────────────
    {
        const int STRIP_H = 24;
        RECT stripRc = { 0, rc.bottom - STRIP_H, rc.right, rc.bottom };

        // Background: elevated dark strip
        HBRUSH stripBrush = CreateSolidBrush(Theme::BG_ELEVATED);
        FillRect(hdc, &stripRc, stripBrush);
        DeleteObject(stripBrush);

        // Top border line
        HPEN stripBorderPen = CreatePen(PS_SOLID, 1, Theme::BORDER);
        HPEN oldP = (HPEN)SelectObject(hdc, stripBorderPen);
        MoveToEx(hdc, 0, rc.bottom - STRIP_H, nullptr);
        LineTo(hdc, rc.right, rc.bottom - STRIP_H);
        SelectObject(hdc, oldP);
        DeleteObject(stripBorderPen);

        // Status message — reuse whatever the status text control shows,
        // or a static "Ready" message if nothing is active
        wchar_t statusBuf[256] = L"Ready for network assessment.";
        if (_hStatusText) GetWindowText(_hStatusText, statusBuf, 256);

        SetTextColor(hdc, Theme::TEXT_SECONDARY);
        HFONT oldF = (HFONT)SelectObject(hdc, Theme::FontSmall());
        RECT statusTxtRc = { 12, rc.bottom - STRIP_H, rc.right - 12, rc.bottom };
        DrawText(hdc, statusBuf, -1, &statusTxtRc,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, oldF);

        // Right-side "TRANSPARENCY" branding
        SetTextColor(hdc, Theme::TEXT_MUTED);
        HFONT oldF2 = (HFONT)SelectObject(hdc, Theme::FontSmall());
        RECT brandRc = { rc.right - 160, rc.bottom - STRIP_H, rc.right - 8, rc.bottom };
        DrawText(hdc, Theme::VERSION_FULL, -1, &brandRc,
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, oldF2);
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
