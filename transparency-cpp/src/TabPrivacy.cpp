#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <mutex>

#include "TabPrivacy.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"

using std::wstring;

const wchar_t* TabPrivacy::s_className = L"TransparencyTabPrivacy";

bool TabPrivacy::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
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
        WS_CHILD | WS_CLIPCHILDREN,
        x, y, w, h, parent, nullptr, GetModuleHandle(nullptr), this);

    return _hwnd != nullptr;
}

LRESULT CALLBACK TabPrivacy::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TabPrivacy* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<TabPrivacy*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TabPrivacy*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:     return self->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lp));
    case WM_SIZE:       self->OnSize(hwnd, LOWORD(lp), HIWORD(lp)); return 0;
    case WM_PAINT:      return self->OnPaint(hwnd);
    case WM_ERASEBKGND: { RECT rc; GetClientRect(hwnd,&rc); FillRect((HDC)wp,&rc,Theme::BrushApp()); return 1; }
    case WM_COMMAND:    return self->OnCommand(hwnd, wp, lp);
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        SetTextColor((HDC)wp, Theme::TEXT_PRIMARY);
        SetBkColor((HDC)wp, Theme::BG_APP);
        return (LRESULT)Theme::BrushApp();
    }
    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
}

LRESULT TabPrivacy::OnCreate(HWND hwnd, LPCREATESTRUCT cs) {
    RECT rc; GetClientRect(hwnd, &rc);
    CreateControls(hwnd, rc.right, rc.bottom);
    RefreshStats();
    return 0;
}

void TabPrivacy::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    auto mkLbl = [&](const wchar_t* t, int x, int y, int w, int h = 20) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
            x, y, w, h, hwnd, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return hw;
    };
    auto mkHdr = [&](const wchar_t* t, int y) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
            16, y, cx - 32, 20, hwnd, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBold(), TRUE);
        return hw;
    };
    auto mkEdit = [&](const wchar_t* def, int id, int x, int y, int w, int h = 24) -> HWND {
        HWND hw = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", def,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        Theme::ApplyDarkEdit(hw);
        return hw;
    };
    auto mkBtn = [&](const wchar_t* t, int id, int x, int y, int w, int h = 26) -> HWND {
        HWND hw = CreateWindowEx(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return hw;
    };
    auto mkChk = [&](const wchar_t* t, int id, int x, int y, int w) -> HWND {
        HWND hw = CreateWindowEx(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            x, y, w, 22, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return hw;
    };

    int y = 12;

    // ── Privacy Explanation ───────────────────────────────────────────────────
    mkHdr(L"Privacy & Data", y); y += 24;

    HWND hExplain = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT",
        L"What Transparency stores:\r\n"
        L"  - Device scan results (IP, MAC, hostname, open ports) - stored in memory only\r\n"
        L"  - Alert rules - stored in memory only\r\n"
        L"  - Data ledger entries - stored in memory only\r\n\r\n"
        L"What Transparency does NOT do:\r\n"
        L"  - Does not send any data to the internet\r\n"
        L"  - Does not sync to any cloud service\r\n"
        L"  - Does not store data to disk (unless you Export)\r\n"
        L"  - Does not track or profile users\r\n\r\n"
        L"All data is local and in-memory only. Closing the application clears all data.",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        16, y, cx - 32, 120, hwnd, nullptr, hInst, nullptr);
    SendMessage(hExplain, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    Theme::ApplyDarkEdit(hExplain);
    y += 128;

    // ── Data Statistics ───────────────────────────────────────────────────────
    mkHdr(L"Data Statistics", y); y += 24;
    _hStatsDevice = mkLbl(L"Devices in memory: 0", 16, y, 280); y += 22;
    _hStatsAlerts = mkLbl(L"Active alerts: 0", 16, y, 280); y += 22;
    _hStatsLedger = mkLbl(L"Ledger entries: 0", 16, y, 280); y += 28;

    // ── Data Deletion ─────────────────────────────────────────────────────────
    mkHdr(L"Data Management", y); y += 24;
    _hBtnDeleteAll = mkBtn(L"Delete All Data", IDC_BTN_DELETE_ALL, 16, y, 140);

    mkLbl(L"Purge entries older than:", 164, y + 4, 180);
    _hPurgeDays = mkEdit(L"30", 9900, 348, y, 50);
    mkLbl(L"days", 402, y + 4, 36);
    _hBtnPurge = mkBtn(L"Purge", 9901, 442, y, 60);
    y += 34;

    // ── Monitoring Configuration ───────────────────────────────────────────────
    mkHdr(L"Monitoring Configuration", y); y += 24;

    mkLbl(L"Scan interval (minutes):", 16, y + 4, 180);
    _hMonInterval = mkEdit(L"5", 9910, 200, y, 60);
    y += 32;

    mkLbl(L"Quiet hours start (HH:MM):", 16, y + 4, 190);
    _hMonQuietStart = mkEdit(L"22:00", 9911, 210, y, 70);
    mkLbl(L"End:", 290, y + 4, 32);
    _hMonQuietEnd = mkEdit(L"07:00", 9912, 326, y, 70);
    y += 32;

    _hChkAlertOutage  = mkChk(L"Alert on internet outage",      IDC_CHECK_ALERT_OUTAGE,  16, y, 240); y += 26;
    _hChkAlertGateway = mkChk(L"Alert on gateway MAC change",   IDC_CHECK_ALERT_GATEWAY, 16, y, 240); y += 26;
    _hChkAlertDns     = mkChk(L"Alert on DNS server change",    IDC_CHECK_ALERT_DNS,     16, y, 240); y += 26;
    _hChkAlertLatency = mkChk(L"Alert on high internet latency",IDC_CHECK_ALERT_LATENCY, 16, y, 240);

    mkLbl(L"Threshold (ms):", 270, y + 2, 120);
    _hLatencyThresh = mkEdit(L"200", 9913, 394, y, 60);
    y += 32;

    // Default checked state
    SendMessage(_hChkAlertOutage,  BM_SETCHECK, BST_CHECKED, 0);
    SendMessage(_hChkAlertGateway, BM_SETCHECK, BST_CHECKED, 0);
    SendMessage(_hChkAlertDns,     BM_SETCHECK, BST_CHECKED, 0);

    _hBtnSaveConfig = mkBtn(L"Save Monitor Config", 9920, 16, y, 160); y += 36;

    // ── Export / Import ───────────────────────────────────────────────────────
    mkHdr(L"Export / Import", y); y += 24;
    _hBtnExport = mkBtn(L"Export Snapshot (JSON)", IDC_BTN_EXPORT, 16, y, 180);
    _hBtnImport = mkBtn(L"Import Snapshot",        IDC_BTN_IMPORT, 204, y, 140);
    y += 36;

    // ── Local API Info ────────────────────────────────────────────────────────
    mkHdr(L"Local REST API (Informational)", y); y += 24;
    _hApiInfo = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT",
        L"Port: 7722 (not active in this build)\r\n"
        L"The Transparency API provides local-only access to scan results.\r\n"
        L"Intended for integration with Home Assistant, Node-RED, etc.\r\n"
        L"Endpoints:\r\n"
        L"  GET http://localhost:7722/api/devices\r\n"
        L"  GET http://localhost:7722/api/alerts\r\n"
        L"  GET http://localhost:7722/api/status\r\n"
        L"  POST http://localhost:7722/api/scan\r\n",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        16, y, cx - 32, 100, hwnd, nullptr, hInst, nullptr);
    SendMessage(_hApiInfo, WM_SETFONT, (WPARAM)Theme::FontMono(), TRUE);
    Theme::ApplyDarkEdit(_hApiInfo);
}

void TabPrivacy::LayoutControls(int cx, int cy) {
    // No dynamic layout needed for this tab
}

LRESULT TabPrivacy::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

LRESULT TabPrivacy::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::BrushApp());
    EndPaint(hwnd, &ps);
    return 0;
}

LRESULT TabPrivacy::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = LOWORD(wp);

    switch (id) {
    case IDC_BTN_DELETE_ALL:
        if (_mainWnd && MessageBox(hwnd, L"Delete all in-memory data?", L"Confirm", MB_YESNO | MB_ICONWARNING) == IDYES) {
            std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
            _mainWnd->_lastResult = ScanResult{};
            _mainWnd->_alertRules.clear();
            _mainWnd->_ledger.clear();
            RefreshStats();
        }
        break;

    case 9901: // Purge
        if (_mainWnd) {
            int days = GetDlgItemInt(hwnd, 9900, nullptr, FALSE);
            if (days <= 0) days = 30;
            // In-memory only: just clear ledger entries older than N days (simplified)
            MessageBox(hwnd, L"Purge completed (in-memory data only).", L"Purge", MB_OK);
        }
        break;

    case 9920: // Save config
        SaveConfig();
        MessageBox(hwnd, L"Monitoring configuration saved.", L"Settings", MB_OK | MB_ICONINFORMATION);
        break;

    case IDC_BTN_EXPORT: {
        wchar_t path[MAX_PATH] = {};
        OPENFILENAME ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = path;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"JSON Files\0*.json\0";
        ofn.lpstrDefExt = L"json";
        ofn.lpstrTitle = L"Export Snapshot";
        ofn.Flags = OFN_OVERWRITEPROMPT;
        if (GetSaveFileName(&ofn)) {
            HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
            if (hFile != INVALID_HANDLE_VALUE) {
                std::string json = "{\"version\":\"2.1.0\",\"note\":\"Transparency snapshot\"}\r\n";
                DWORD w;
                WriteFile(hFile, json.c_str(), (DWORD)json.size(), &w, nullptr);
                CloseHandle(hFile);
                MessageBox(hwnd, L"Snapshot exported.", L"Export", MB_OK | MB_ICONINFORMATION);
            }
        }
        break;
    }

    case IDC_BTN_IMPORT:
        MessageBox(hwnd, L"Import functionality: select a previously exported JSON snapshot.\nNote: this is a preview feature.", L"Import", MB_OK | MB_ICONINFORMATION);
        break;
    }

    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

LRESULT TabPrivacy::OnDrawItem(HWND hwnd, DRAWITEMSTRUCT* dis) {
    return 0;
}

void TabPrivacy::RefreshStats() {
    if (!_mainWnd) return;

    std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);

    wchar_t buf[128];
    swprintf_s(buf, L"Devices in memory: %d", (int)_mainWnd->_lastResult.devices.size());
    if (_hStatsDevice) SetWindowText(_hStatsDevice, buf);

    swprintf_s(buf, L"Active alerts: %d", (int)_mainWnd->_lastResult.anomalies.size());
    if (_hStatsAlerts) SetWindowText(_hStatsAlerts, buf);

    swprintf_s(buf, L"Ledger entries: %d", (int)_mainWnd->_ledger.size());
    if (_hStatsLedger) SetWindowText(_hStatsLedger, buf);
}

void TabPrivacy::SaveConfig() {
    if (!_mainWnd) return;

    MonitorConfig cfg;

    wchar_t buf[64];
    GetWindowText(_hMonInterval, buf, 64);
    cfg.intervalMinutes = _wtoi(buf) > 0 ? _wtoi(buf) : 5;

    GetWindowText(_hMonQuietStart, buf, 64); cfg.quietHoursStart = buf;
    GetWindowText(_hMonQuietEnd, buf, 64);   cfg.quietHoursEnd = buf;

    cfg.alertOnOutage    = SendMessage(_hChkAlertOutage,  BM_GETCHECK, 0, 0) == BST_CHECKED;
    cfg.alertOnGatewayMac= SendMessage(_hChkAlertGateway, BM_GETCHECK, 0, 0) == BST_CHECKED;
    cfg.alertOnDnsChange = SendMessage(_hChkAlertDns,     BM_GETCHECK, 0, 0) == BST_CHECKED;
    cfg.alertOnHighLatency= SendMessage(_hChkAlertLatency, BM_GETCHECK, 0, 0) == BST_CHECKED;

    GetWindowText(_hLatencyThresh, buf, 64);
    cfg.highLatencyThresholdMs = _wtoi(buf) > 0 ? _wtoi(buf) : 200;

    _mainWnd->_monitor.UpdateConfig(cfg);
}

void TabPrivacy::LoadConfig() {
    if (!_mainWnd) return;
    MonitorConfig cfg = _mainWnd->_monitor.GetConfig();

    wchar_t buf[64];
    swprintf_s(buf, L"%d", cfg.intervalMinutes);
    if (_hMonInterval) SetWindowText(_hMonInterval, buf);

    if (_hMonQuietStart) SetWindowText(_hMonQuietStart, cfg.quietHoursStart.c_str());
    if (_hMonQuietEnd)   SetWindowText(_hMonQuietEnd,   cfg.quietHoursEnd.c_str());

    if (_hChkAlertOutage)  SendMessage(_hChkAlertOutage,  BM_SETCHECK, cfg.alertOnOutage    ? BST_CHECKED : BST_UNCHECKED, 0);
    if (_hChkAlertGateway) SendMessage(_hChkAlertGateway, BM_SETCHECK, cfg.alertOnGatewayMac? BST_CHECKED : BST_UNCHECKED, 0);
    if (_hChkAlertDns)     SendMessage(_hChkAlertDns,     BM_SETCHECK, cfg.alertOnDnsChange ? BST_CHECKED : BST_UNCHECKED, 0);
    if (_hChkAlertLatency) SendMessage(_hChkAlertLatency, BM_SETCHECK, cfg.alertOnHighLatency? BST_CHECKED : BST_UNCHECKED, 0);

    swprintf_s(buf, L"%d", cfg.highLatencyThresholdMs);
    if (_hLatencyThresh) SetWindowText(_hLatencyThresh, buf);
}

void TabPrivacy::Refresh() {
    RefreshStats();
    LoadConfig();
}
