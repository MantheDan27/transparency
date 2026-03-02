#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <iphlpapi.h>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

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
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
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

void TabOverview::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    // KPI tiles (4 across, below header)
    int tileY = 110;
    int tileW = (cx - 40) / 4;
    int tileH = 72;

    for (int i = 0; i < 4; i++) {
        int x = 16 + i * (tileW + 8);
        _hKpi[i] = CreateWindowEx(WS_EX_STATICEDGE, L"STATIC", L"0",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            x, tileY, tileW, tileH,
            hwnd, (HMENU)(IDC_STATIC_KPI1 + i), hInst, nullptr);
        SendMessage(_hKpi[i], WM_SETFONT, (WPARAM)Theme::FontHeader(), TRUE);

        _hKpiLabel[i] = CreateWindowEx(0, L"STATIC", KPI_LABELS[i],
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            x, tileY + tileH + 2, tileW, 18,
            hwnd, (HMENU)(9100 + i), hInst, nullptr);
        SendMessage(_hKpiLabel[i], WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
    }

    // Scan mode pills
    int pillY = tileY + tileH + 32;
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
    int btnY = pillY + 34;
    int btnH = 32;

    _hBtnQuickScan = CreateWindowEx(0, L"BUTTON", L"Quick Scan",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        16, btnY, 100, btnH, hwnd, (HMENU)IDC_BTN_SCAN_QUICK, hInst, nullptr);
    SendMessage(_hBtnQuickScan, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hBtnDeepScan = CreateWindowEx(0, L"BUTTON", L"Deep Scan",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        122, btnY, 100, btnH, hwnd, (HMENU)IDC_BTN_SCAN_DEEP, hInst, nullptr);
    SendMessage(_hBtnDeepScan, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hBtnMonStart = CreateWindowEx(0, L"BUTTON", L"Start Monitor",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        228, btnY, 110, btnH, hwnd, (HMENU)IDC_BTN_MONITOR_START, hInst, nullptr);
    SendMessage(_hBtnMonStart, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    _hBtnMonStop = CreateWindowEx(0, L"BUTTON", L"Stop Monitor",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        344, btnY, 110, btnH, hwnd, (HMENU)IDC_BTN_MONITOR_STOP, hInst, nullptr);
    SendMessage(_hBtnMonStop, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    EnableWindow(_hBtnMonStop, FALSE);

    _hBtnExport = CreateWindowEx(0, L"BUTTON", L"Export Report",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        460, btnY, 110, btnH, hwnd, (HMENU)IDC_BTN_EXPORT, hInst, nullptr);
    SendMessage(_hBtnExport, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    // Progress / status text
    _hStatusText = CreateWindowEx(0, L"STATIC", L"Ready. Run a scan to discover devices.",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, btnY + btnH + 8, cx - 32, 20,
        hwnd, (HMENU)IDC_STATIC_STATUS, hInst, nullptr);
    SendMessage(_hStatusText, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    // Progress bar
    _hProgressBar = CreateWindowEx(0, PROGRESS_CLASS, nullptr,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        16, btnY + btnH + 32, cx - 32, 8,
        hwnd, nullptr, hInst, nullptr);
    SendMessage(_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessage(_hProgressBar, PBM_SETPOS, 0, 0);

    // Network info
    _hNetworkInfo = CreateWindowEx(0, L"STATIC", L"Detecting network...",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 16, cx - 32, 80,
        hwnd, (HMENU)IDC_STATIC_NET_INFO, hInst, nullptr);
    SendMessage(_hNetworkInfo, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);

    // Recent changes list
    int listY = btnY + btnH + 52;
    int listH = cy - listY - 16;

    _hChangesList = CreateWindowEx(
        WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | WS_VSCROLL,
        16, listY, cx - 32, listH,
        hwnd, (HMENU)IDC_LIST_CHANGES, hInst, nullptr);

    SendMessage(_hChangesList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hChangesList,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    Theme::ApplyDarkScrollbar(_hChangesList);

    // Columns for recent changes
    LVCOLUMN col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    col.fmt  = LVCFMT_LEFT;

    col.cx = 140; col.pszText = (LPWSTR)L"Time";
    ListView_InsertColumn(_hChangesList, 0, &col);

    col.cx = 160; col.pszText = (LPWSTR)L"Change";
    ListView_InsertColumn(_hChangesList, 1, &col);

    col.cx = 400; col.pszText = (LPWSTR)L"Details";
    ListView_InsertColumn(_hChangesList, 2, &col);
}

void TabOverview::LayoutControls(int cx, int cy) {
    if (!_hwnd) return;

    int tileY = 110;
    int tileW = (cx - 40) / 4;
    int tileH = 72;

    for (int i = 0; i < 4; i++) {
        int x = 16 + i * (tileW + 8);
        if (_hKpi[i]) SetWindowPos(_hKpi[i], nullptr, x, tileY, tileW, tileH, SWP_NOZORDER);
        if (_hKpiLabel[i]) SetWindowPos(_hKpiLabel[i], nullptr, x, tileY + tileH + 2, tileW, 18, SWP_NOZORDER);
    }

    if (_hStatusText) SetWindowPos(_hStatusText, nullptr, 16, 0, cx - 32, 20, SWP_NOMOVE | SWP_NOZORDER);
    if (_hProgressBar) SetWindowPos(_hProgressBar, nullptr, 16, 0, cx - 32, 8, SWP_NOMOVE | SWP_NOZORDER);
    if (_hNetworkInfo) SetWindowPos(_hNetworkInfo, nullptr, 16, 16, cx - 32, 80, SWP_NOZORDER);

    // Recalculate list position
    int pillY = tileY + tileH + 32;
    int btnY = pillY + 34;
    int btnH = 32;
    int listY = btnY + btnH + 52;
    int listH = cy - listY - 16;

    if (_hChangesList) SetWindowPos(_hChangesList, nullptr, 16, listY, cx - 32, max(listH, 50), SWP_NOZORDER);
}

LRESULT TabOverview::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

LRESULT TabOverview::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::BrushApp());

    // KPI tile backgrounds
    int tileW = (rc.right - 40) / 4;
    int tileH = 72;
    int tileY = 110;

    for (int i = 0; i < 4; i++) {
        int x = 16 + i * (tileW + 8);
        RECT tileRc = { x, tileY, x + tileW, tileY + tileH };
        FillRect(hdc, &tileRc, Theme::BrushCard());

        // Left accent line for first tile
        if (i == 0) {
            RECT acc = { x, tileY, x + 3, tileY + tileH };
            FillRect(hdc, &acc, Theme::BrushAccent());
        }

        // Border
        HPEN pen = CreatePen(PS_SOLID, 1, Theme::BORDER);
        HPEN old = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, x, tileY, nullptr);
        LineTo(hdc, x + tileW, tileY);
        LineTo(hdc, x + tileW, tileY + tileH);
        LineTo(hdc, x, tileY + tileH);
        LineTo(hdc, x, tileY);
        SelectObject(hdc, old);
        DeleteObject(pen);
    }

    // Section header "Recent Changes"
    RECT scanRc;
    GetClientRect(hwnd, &scanRc);
    int pillY = tileY + tileH + 32;
    int btnY = pillY + 34;
    int btnH = 32;
    int listY = btnY + btnH + 52;

    RECT hdrRc = { 16, listY - 22, scanRc.right - 16, listY - 4 };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TEXT_SECONDARY);
    HFONT old = (HFONT)SelectObject(hdc, Theme::FontSmall());
    DrawText(hdc, L"RECENT CHANGES", -1, &hdrRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);

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

    case IDC_BTN_EXPORT:
        // Show save dialog
        if (_mainWnd) {
            wchar_t path[MAX_PATH] = {};
            OPENFILENAME ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = path;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
            ofn.lpstrDefExt = L"json";
            ofn.lpstrTitle = L"Export Report";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

            if (GetSaveFileName(&ofn)) {
                // Write minimal JSON
                HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
                if (hFile != INVALID_HANDLE_VALUE) {
                    ScanResult r = _mainWnd->GetLastResult();
                    std::wstring json = L"{\n  \"devices\": " + std::to_wstring(r.devices.size()) + L",\n  \"scannedAt\": \"" + r.scannedAt + L"\"\n}";
                    std::string jn;
                    int n = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    if (n > 0) { jn.resize(n - 1); WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1, &jn[0], n, nullptr, nullptr); }
                    DWORD written;
                    WriteFile(hFile, jn.c_str(), (DWORD)jn.size(), &written, nullptr);
                    CloseHandle(hFile);
                    MessageBox(hwnd, L"Report exported successfully.", L"Export", MB_OK | MB_ICONINFORMATION);
                }
            }
        }
        break;
    }

    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

LRESULT TabOverview::OnDrawItem(HWND hwnd, DRAWITEMSTRUCT* dis) {
    return 0;
}

LRESULT TabOverview::OnScanProgress(HWND hwnd, WPARAM pct, LPARAM msgPtr) {
    auto* msg = reinterpret_cast<std::wstring*>(msgPtr);
    if (msg) {
        if (_hStatusText) SetWindowText(_hStatusText, msg->c_str());
        if (_hProgressBar) SendMessage(_hProgressBar, PBM_SETPOS, (WPARAM)pct, 0);
        delete msg;
    }
    return 0;
}

LRESULT TabOverview::OnScanComplete(HWND hwnd) {
    RefreshKPIs();
    if (_hStatusText) SetWindowText(_hStatusText, L"Scan complete.");
    if (_hProgressBar) SendMessage(_hProgressBar, PBM_SETPOS, 100, 0);

    // Refresh changes list with anomalies
    if (!_hChangesList || !_mainWnd) return 0;

    ListView_DeleteAllItems(_hChangesList);

    ScanResult r = _mainWnd->GetLastResult();
    int row = 0;
    for (auto& a : r.anomalies) {
        LVITEM item = {};
        item.mask = LVIF_TEXT;
        item.iItem = row;
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

void TabOverview::RefreshKPIs() {
    if (!_mainWnd) return;
    ScanResult r = _mainWnd->GetLastResult();

    // KPI 0: Devices online
    wchar_t buf[64];
    swprintf_s(buf, L"%d", (int)r.devices.size());
    if (_hKpi[0]) SetWindowText(_hKpi[0], buf);

    // KPI 1: Unknown devices
    int unknown = 0;
    for (auto& d : r.devices) if (d.trustState == L"unknown") unknown++;
    swprintf_s(buf, L"%d", unknown);
    if (_hKpi[1]) SetWindowText(_hKpi[1], buf);

    // KPI 2: Active alerts
    swprintf_s(buf, L"%d", (int)r.anomalies.size());
    if (_hKpi[2]) SetWindowText(_hKpi[2], buf);

    // KPI 3: Gateway latency
    int lat = -1;
    auto nets = ScanEngine::GetLocalNetworks();
    if (!nets.empty() && !nets[0].gateway.empty()) {
        // Try to find gateway in results
        for (auto& d : r.devices) {
            if (d.ip == nets[0].gateway) { lat = d.latencyMs; break; }
        }
    }
    if (lat >= 0) swprintf_s(buf, L"%dms", lat);
    else wcscpy_s(buf, L"--");
    if (_hKpi[3]) SetWindowText(_hKpi[3], buf);
}

void TabOverview::RefreshNetworkInfo() {
    if (!_hNetworkInfo) return;

    auto nets = ScanEngine::GetLocalNetworks();
    if (nets.empty()) {
        SetWindowText(_hNetworkInfo, L"No network adapters found.");
        return;
    }

    std::wstring info;
    for (auto& ni : nets) {
        info += L"Adapter: " + ni.name +
                L"   IP: " + ni.localIp + L"/" + ni.cidr +
                L"   Gateway: " + (ni.gateway.empty() ? L"N/A" : ni.gateway) + L"\n";
    }

    SetWindowText(_hNetworkInfo, info.c_str());
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
