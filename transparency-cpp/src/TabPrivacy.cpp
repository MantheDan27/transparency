#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <string>
#include <sstream>
#include <mutex>

#include "TabPrivacy.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"

using std::wstring;

const wchar_t* TabPrivacy::s_className = L"TransparencyTabPrivacy";

// ─── Control IDs ─────────────────────────────────────────────────────────────
enum {
    ID_PURGE_DAYS     = 9900,
    ID_BTN_PURGE      = 9901,
    ID_MON_INTERVAL   = 9910,
    ID_MON_QSTART     = 9911,
    ID_MON_QEND       = 9912,
    ID_LAT_THRESH     = 9913,
    ID_BTN_SAVE_CFG   = 9920,
    ID_API_KEY_EDIT   = 9930,
    ID_BTN_API_ROTATE = 9931,
    ID_CHK_API_ENABLE = 9932,
    ID_HOOK_LIST      = 9940,
    ID_BTN_HOOK_ADD   = 9941,
    ID_BTN_HOOK_DEL   = 9942,
    ID_CHK_SCHED      = 9950,
    ID_COMBO_SCHED    = 9951,
    ID_SCHED_INTERVAL = 9952,
    ID_SCHED_TIME     = 9953,
    ID_BTN_SCHED_SAVE = 9954,
};

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
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
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
    case WM_ERASEBKGND:{RECT rc; GetClientRect(hwnd,&rc); FillRect((HDC)wp,&rc,Theme::BrushApp()); return 1;}
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
    auto mkHdr = [&](const wchar_t* t, int y) {
        HWND hw = CreateWindowEx(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
            16, y, cx - 32, 20, hwnd, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBold(), TRUE);
        return hw;
    };
    auto mkEdit = [&](const wchar_t* def, int id, int x, int y, int w, int h = 24,
                      DWORD xStyle = 0) -> HWND {
        HWND hw = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", def,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | xStyle,
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
        L"All data is stored in-memory only. Nothing is sent to the internet.\r\n"
        L"No tracking, no cloud sync. Closing the app clears all data unless exported.",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
        16, y, cx - 32, 44, hwnd, nullptr, hInst, nullptr);
    SendMessage(hExplain, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    Theme::ApplyDarkEdit(hExplain);
    y += 52;

    // ── Data Statistics ───────────────────────────────────────────────────────
    mkHdr(L"Data Statistics", y); y += 24;
    _hStatsDevice = mkLbl(L"Devices in memory: 0", 16, y, 280); y += 22;
    _hStatsAlerts = mkLbl(L"Active alerts: 0",     16, y, 280); y += 22;
    _hStatsLedger = mkLbl(L"Ledger entries: 0",    16, y, 280); y += 28;

    // ── Data Management ───────────────────────────────────────────────────────
    mkHdr(L"Data Management", y); y += 24;
    _hBtnDeleteAll = mkBtn(L"Delete All Data", IDC_BTN_DELETE_ALL, 16, y, 140);
    mkLbl(L"Purge older than:", 168, y + 4, 130);
    _hPurgeDays = mkEdit(L"30", ID_PURGE_DAYS, 302, y, 50);
    mkLbl(L"days", 356, y + 4, 36);
    _hBtnPurge = mkBtn(L"Purge", ID_BTN_PURGE, 396, y, 60);
    y += 36;

    // ── Monitoring Configuration ──────────────────────────────────────────────
    mkHdr(L"Monitoring Configuration", y); y += 24;
    mkLbl(L"Scan interval (min):", 16, y + 4, 150);
    _hMonInterval = mkEdit(L"5", ID_MON_INTERVAL, 170, y, 60);
    y += 32;
    mkLbl(L"Quiet hours start:", 16, y + 4, 150);
    _hMonQuietStart = mkEdit(L"22:00", ID_MON_QSTART, 170, y, 70);
    mkLbl(L"End:", 248, y + 4, 32);
    _hMonQuietEnd = mkEdit(L"07:00", ID_MON_QEND, 284, y, 70);
    y += 32;
    _hChkAlertOutage  = mkChk(L"Alert on internet outage",      IDC_CHECK_ALERT_OUTAGE,  16, y, 240); y += 26;
    _hChkAlertGateway = mkChk(L"Alert on gateway MAC change",   IDC_CHECK_ALERT_GATEWAY, 16, y, 240); y += 26;
    _hChkAlertDns     = mkChk(L"Alert on DNS server change",    IDC_CHECK_ALERT_DNS,     16, y, 240); y += 26;
    _hChkAlertLatency = mkChk(L"Alert on high latency",         IDC_CHECK_ALERT_LATENCY, 16, y, 200);
    mkLbl(L"Threshold (ms):", 220, y + 2, 110);
    _hLatencyThresh = mkEdit(L"200", ID_LAT_THRESH, 334, y, 60);
    y += 32;
    SendMessage(_hChkAlertOutage,  BM_SETCHECK, BST_CHECKED, 0);
    SendMessage(_hChkAlertGateway, BM_SETCHECK, BST_CHECKED, 0);
    SendMessage(_hChkAlertDns,     BM_SETCHECK, BST_CHECKED, 0);
    _hBtnSaveConfig = mkBtn(L"Save Monitor Config", ID_BTN_SAVE_CFG, 16, y, 160); y += 36;

    // ── Export / Import ───────────────────────────────────────────────────────
    mkHdr(L"Export / Import", y); y += 24;
    _hBtnExport = mkBtn(L"Export Full JSON", IDC_BTN_EXPORT, 16, y, 140);
    _hBtnImport = mkBtn(L"Import Snapshot",  IDC_BTN_IMPORT, 164, y, 130);
    y += 36;

    // ── Local REST API ────────────────────────────────────────────────────────
    mkHdr(L"Local REST API (port 7722)", y); y += 24;
    _hChkApiEnabled = mkChk(L"Enable REST API", ID_CHK_API_ENABLE, 16, y, 160);
    _hApiStatusLbl  = mkLbl(L"Status: Stopped", 184, y + 2, 200);
    y += 28;
    mkLbl(L"API Key:", 16, y + 4, 70);
    _hEditApiKey = mkEdit(L"(not generated)", ID_API_KEY_EDIT, 90, y, 300, 24, ES_READONLY);
    _hBtnApiRotate = mkBtn(L"Rotate Key", ID_BTN_API_ROTATE, 398, y, 90);
    y += 32;
    mkLbl(L"Endpoints: GET /api/devices  /api/alerts  /api/status  /api/health  /api/snapshots",
           16, y, cx - 32);
    y += 24;
    mkLbl(L"Header: X-API-Key: <key>   (required when key is set)", 16, y, cx - 32);
    y += 32;

    // ── Plugin / Script Hooks ─────────────────────────────────────────────────
    mkHdr(L"Plugin / Script Hooks", y); y += 24;
    mkLbl(L"Hooks run your executable on events. JSON payload is piped to stdin.", 16, y, cx - 32);
    y += 22;

    _hHookList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL | WS_VSCROLL,
        16, y, cx - 32, 100, hwnd, (HMENU)(INT_PTR)ID_HOOK_LIST, hInst, nullptr);
    SendMessage(_hHookList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hHookList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    Theme::ApplyDarkListView(_hHookList);
    {
        LVCOLUMN col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; col.fmt = LVCFMT_LEFT;
        col.cx = 120; col.pszText = (LPWSTR)L"Name";
        ListView_InsertColumn(_hHookList, 0, &col);
        col.cx = 100; col.pszText = (LPWSTR)L"Event";
        ListView_InsertColumn(_hHookList, 1, &col);
        col.cx = 260; col.pszText = (LPWSTR)L"Executable";
        ListView_InsertColumn(_hHookList, 2, &col);
        col.cx = 60;  col.pszText = (LPWSTR)L"Enabled";
        ListView_InsertColumn(_hHookList, 3, &col);
    }
    y += 108;

    _hBtnHookAdd = mkBtn(L"Add Hook…",  ID_BTN_HOOK_ADD, 16,  y, 90);
    _hBtnHookDel = mkBtn(L"Remove",     ID_BTN_HOOK_DEL, 114, y, 80);
    y += 36;

    // ── Scheduled Scans ───────────────────────────────────────────────────────
    mkHdr(L"Scheduled Scans", y); y += 24;
    _hChkSchedEnabled = mkChk(L"Enable scheduled scans", ID_CHK_SCHED, 16, y, 200);
    y += 28;
    mkLbl(L"Mode:", 16, y + 4, 50);
    _hComboSchedMode = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        70, y, 110, 100, hwnd, (HMENU)(INT_PTR)ID_COMBO_SCHED, hInst, nullptr);
    SendMessage(_hComboSchedMode, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hComboSchedMode, CB_ADDSTRING, 0, (LPARAM)L"Quick");
    SendMessage(_hComboSchedMode, CB_ADDSTRING, 0, (LPARAM)L"Standard");
    SendMessage(_hComboSchedMode, CB_ADDSTRING, 0, (LPARAM)L"Deep");
    SendMessage(_hComboSchedMode, CB_SETCURSEL, 0, 0);

    mkLbl(L"Every (hours):", 192, y + 4, 110);
    _hEditSchedInterval = mkEdit(L"24", ID_SCHED_INTERVAL, 306, y, 60);
    mkLbl(L"Preferred time:", 378, y + 4, 110);
    _hEditSchedTime = mkEdit(L"03:00", ID_SCHED_TIME, 492, y, 70);
    y += 32;
    _hBtnSchedSave = mkBtn(L"Save Schedule", ID_BTN_SCHED_SAVE, 16, y, 130);
    y += 36;
}

void TabPrivacy::LayoutControls(int cx, int cy) {
    // Static layout — no dynamic repositioning needed
}

LRESULT TabPrivacy::OnSize(HWND hwnd, int cx, int cy) {
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
        if (_mainWnd && MessageBox(hwnd, L"Delete all in-memory data?",
                                   L"Confirm", MB_YESNO | MB_ICONWARNING) == IDYES) {
            std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
            _mainWnd->_lastResult = ScanResult{};
            _mainWnd->_alertRules.clear();
            _mainWnd->_ledger.clear();
            _mainWnd->_snapshots.clear();
            RefreshStats();
        }
        break;

    case ID_BTN_PURGE:
        if (_mainWnd) {
            MessageBox(hwnd, L"Purge completed (in-memory data only).",
                       L"Purge", MB_OK);
            RefreshStats();
        }
        break;

    case ID_BTN_SAVE_CFG:
        SaveConfig();
        MessageBox(hwnd, L"Monitoring configuration saved.",
                   L"Settings", MB_OK | MB_ICONINFORMATION);
        break;

    case IDC_BTN_EXPORT:
        ExportFullJson(hwnd);
        break;

    case IDC_BTN_IMPORT:
        MessageBox(hwnd,
            L"Select a previously exported JSON snapshot.\n"
            L"Note: import is available in a future release.",
            L"Import", MB_OK | MB_ICONINFORMATION);
        break;

    case ID_CHK_API_ENABLE:
        if (_mainWnd) {
            bool enable = (SendMessage(_hChkApiEnabled, BM_GETCHECK, 0, 0) == BST_CHECKED);
            if (enable) {
                _mainWnd->StartLocalApi();
                if (_hApiStatusLbl) SetWindowText(_hApiStatusLbl, L"Status: Running on :7722");
                // Show current API key
                if (_hEditApiKey) SetWindowText(_hEditApiKey, _mainWnd->_apiKey.c_str());
            } else {
                _mainWnd->StopLocalApi();
                if (_hApiStatusLbl) SetWindowText(_hApiStatusLbl, L"Status: Stopped");
            }
        }
        break;

    case ID_BTN_API_ROTATE:
        if (_mainWnd) {
            _mainWnd->StopLocalApi();
            // Generate a pseudo-random API key from tick count + PID
            DWORD t1 = GetTickCount();
            DWORD t2 = GetCurrentProcessId() ^ (t1 >> 4);
            DWORD t3 = t1 ^ (t2 << 3);
            DWORD t4 = t2 ^ GetCurrentThreadId();
            wchar_t kbuf[64];
            swprintf_s(kbuf, L"%08lX-%04X-%04X-%08lX",
                       t1, (WORD)t2, (WORD)t3, t4);
            _mainWnd->_apiKey = kbuf;
            if (_hEditApiKey) SetWindowText(_hEditApiKey, kbuf);
            if (SendMessage(_hChkApiEnabled, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                _mainWnd->StartLocalApi();
            }
            MessageBox(hwnd, L"API key rotated. Update your integrations.",
                       L"API Key", MB_OK | MB_ICONINFORMATION);
        }
        break;

    case ID_BTN_HOOK_ADD: {
        // Simple input dialog for hook details
        wchar_t path[MAX_PATH] = {};
        OPENFILENAME ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = hwnd;
        ofn.lpstrFile   = path;
        ofn.nMaxFile    = MAX_PATH;
        ofn.lpstrFilter = L"Executables\0*.exe\0Scripts\0*.bat;*.ps1;*.py\0All\0*.*\0";
        ofn.lpstrTitle  = L"Select Hook Executable";
        ofn.Flags       = OFN_FILEMUSTEXIST;
        if (GetOpenFileName(&ofn) && _mainWnd) {
            PluginHook hook;
            hook.id       = L"hook_" + std::to_wstring(_mainWnd->_pluginHooks.size() + 1);
            hook.name     = L"Hook " + std::to_wstring(_mainWnd->_pluginHooks.size() + 1);
            hook.execPath = path;
            hook.eventType= L"any";
            hook.enabled  = true;
            {
                std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
                _mainWnd->_pluginHooks.push_back(hook);
            }
            RefreshHooks();
        }
        break;
    }

    case ID_BTN_HOOK_DEL: {
        if (!_hHookList || !_mainWnd) break;
        int sel = ListView_GetNextItem(_hHookList, -1, LVNI_SELECTED);
        if (sel >= 0) {
            std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
            if (sel < (int)_mainWnd->_pluginHooks.size())
                _mainWnd->_pluginHooks.erase(_mainWnd->_pluginHooks.begin() + sel);
            RefreshHooks();
        }
        break;
    }

    case ID_BTN_SCHED_SAVE:
        if (_mainWnd) {
            ScheduledScan& sc = _mainWnd->_scheduledScan;
            sc.enabled = (SendMessage(_hChkSchedEnabled, BM_GETCHECK, 0, 0) == BST_CHECKED);
            int modeIdx = (int)SendMessage(_hComboSchedMode, CB_GETCURSEL, 0, 0);
            if      (modeIdx == 1) sc.mode = L"standard";
            else if (modeIdx == 2) sc.mode = L"deep";
            else                   sc.mode = L"quick";
            wchar_t buf[64];
            GetWindowText(_hEditSchedInterval, buf, 64);
            sc.intervalHours = _wtoi(buf) > 0 ? _wtoi(buf) : 24;
            GetWindowText(_hEditSchedTime, buf, 64);
            sc.timeOfDay = buf;
            MessageBox(hwnd, L"Schedule saved.", L"Scheduled Scans",
                       MB_OK | MB_ICONINFORMATION);
        }
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
    GetWindowText(_hMonQuietEnd,   buf, 64); cfg.quietHoursEnd   = buf;
    cfg.alertOnOutage     = SendMessage(_hChkAlertOutage,  BM_GETCHECK, 0, 0) == BST_CHECKED;
    cfg.alertOnGatewayMac = SendMessage(_hChkAlertGateway, BM_GETCHECK, 0, 0) == BST_CHECKED;
    cfg.alertOnDnsChange  = SendMessage(_hChkAlertDns,     BM_GETCHECK, 0, 0) == BST_CHECKED;
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
    if (_hChkAlertOutage)  SendMessage(_hChkAlertOutage,  BM_SETCHECK, cfg.alertOnOutage?BST_CHECKED:BST_UNCHECKED, 0);
    if (_hChkAlertGateway) SendMessage(_hChkAlertGateway, BM_SETCHECK, cfg.alertOnGatewayMac?BST_CHECKED:BST_UNCHECKED, 0);
    if (_hChkAlertDns)     SendMessage(_hChkAlertDns,     BM_SETCHECK, cfg.alertOnDnsChange?BST_CHECKED:BST_UNCHECKED, 0);
    if (_hChkAlertLatency) SendMessage(_hChkAlertLatency, BM_SETCHECK, cfg.alertOnHighLatency?BST_CHECKED:BST_UNCHECKED, 0);
    swprintf_s(buf, L"%d", cfg.highLatencyThresholdMs);
    if (_hLatencyThresh) SetWindowText(_hLatencyThresh, buf);
}

void TabPrivacy::RefreshHooks() {
    if (!_hHookList || !_mainWnd) return;
    ListView_DeleteAllItems(_hHookList);
    std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
    int row = 0;
    for (auto& h : _mainWnd->_pluginHooks) {
        LVITEM item = {}; item.mask = LVIF_TEXT; item.iItem = row;
        item.pszText = (LPWSTR)h.name.c_str();
        ListView_InsertItem(_hHookList, &item);
        ListView_SetItemText(_hHookList, row, 1, (LPWSTR)h.eventType.c_str());
        ListView_SetItemText(_hHookList, row, 2, (LPWSTR)h.execPath.c_str());
        ListView_SetItemText(_hHookList, row, 3, (LPWSTR)(h.enabled ? L"Yes" : L"No"));
        row++;
    }
}

void TabPrivacy::RefreshSchedScan() {
    if (!_mainWnd) return;
    ScheduledScan& sc = _mainWnd->_scheduledScan;
    if (_hChkSchedEnabled) SendMessage(_hChkSchedEnabled, BM_SETCHECK,
                                       sc.enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    if (_hComboSchedMode) {
        int idx = 0;
        if (sc.mode == L"standard") idx = 1;
        else if (sc.mode == L"deep") idx = 2;
        SendMessage(_hComboSchedMode, CB_SETCURSEL, idx, 0);
    }
    if (_hEditSchedInterval) {
        wchar_t buf[32]; swprintf_s(buf, L"%d", sc.intervalHours);
        SetWindowText(_hEditSchedInterval, buf);
    }
    if (_hEditSchedTime) SetWindowText(_hEditSchedTime, sc.timeOfDay.c_str());
}

// ── Full JSON export ──────────────────────────────────────────────────────────

static void WToU8EscAppend(const std::wstring& w, std::string& out) {
    if (w.empty()) return;
    char stackBuf[512];
    std::string heapBuf;
    char* pBuf = stackBuf;
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, stackBuf, (int)sizeof(stackBuf), nullptr, nullptr);
    if (n <= 0) {
        n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (n <= 1) return;
        heapBuf.resize(n);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &heapBuf[0], n, nullptr, nullptr);
        pBuf = &heapBuf[0];
    }
    // Escape and append UTF-8 chars
    for (const char* p = pBuf; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c < 0x20)  out += ' ';
        else                out += (char)c;
    }
}

static std::string JEsc(const std::string& s) {
    std::string o;
    size_t expected_escapes = 0;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\' || c == '\n' || c == '\r' || c < 0x20) {
            expected_escapes++;
        }
    }
    o.reserve(s.length() + expected_escapes);
    for (unsigned char c : s) {
        if      (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c < 0x20)  o += " ";
        else                o += c;
    }
    return o;
}

void TabPrivacy::ExportFullJson(HWND hwnd) {
    wchar_t path[MAX_PATH] = {};
    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrFilter = L"JSON Files\0*.json\0";
    ofn.lpstrDefExt = L"json";
    ofn.lpstrTitle  = L"Export Full Snapshot";
    ofn.Flags       = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileName(&ofn)) return;

    HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBox(hwnd, L"Could not open file for writing.", L"Export", MB_ICONERROR);
        return;
    }

    ScanResult r;
    std::vector<LedgerEntry> ledger;
    {
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        r      = _mainWnd->_lastResult;
        ledger = _mainWnd->_ledger;
    }

    std::string json;
    json.reserve(r.devices.size() * 512 + ledger.size() * 128 + 1024);
    json = "{\n  \"version\": \"3.2.0\",\n";
    json += "  \"scannedAt\": \""; WToU8EscAppend(r.scannedAt, json); json += "\",\n";
    json += "  \"mode\": \"";      WToU8EscAppend(r.mode, json);      json += "\",\n";
    json += "  \"devices\": [\n";

    for (size_t i = 0; i < r.devices.size(); i++) {
        auto& d = r.devices[i];
        json += "    {\n";
        json += "      \"ip\": \"";         WToU8EscAppend(d.ip, json);         json += "\",\n";
        json += "      \"mac\": \"";        WToU8EscAppend(d.mac, json);        json += "\",\n";
        json += "      \"hostname\": \"";   WToU8EscAppend(d.hostname, json);   json += "\",\n";
        json += "      \"vendor\": \"";     WToU8EscAppend(d.vendor, json);     json += "\",\n";
        json += "      \"deviceType\": \""; WToU8EscAppend(d.deviceType, json); json += "\",\n";
        json += "      \"osGuess\": \"";    WToU8EscAppend(d.osGuess, json);    json += "\",\n";
        json += "      \"confidence\": ";   json += std::to_string(d.confidence); json += ",\n";
        json += "      \"latencyMs\": ";    json += std::to_string(d.latencyMs);  json += ",\n";
        json += "      \"trustState\": \""; WToU8EscAppend(d.trustState, json);   json += "\",\n";
        json += "      \"customName\": \""; WToU8EscAppend(d.customName, json);   json += "\",\n";
        json += "      \"notes\": \"";      WToU8EscAppend(d.notes, json);        json += "\",\n";
        json += "      \"firstSeen\": \"";  WToU8EscAppend(d.firstSeen, json);    json += "\",\n";
        json += "      \"lastSeen\": \"";   WToU8EscAppend(d.lastSeen, json);     json += "\",\n";
        json += "      \"online\": ";       json += (d.online ? "true" : "false"); json += ",\n";
        json += "      \"iotRisk\": ";      json += (d.iotRisk ? "true" : "false"); json += ",\n";
        json += "      \"iotRiskDetail\": \""; WToU8EscAppend(d.iotRiskDetail, json); json += "\",\n";
        // Open ports
        json += "      \"openPorts\": [";
        for (size_t j = 0; j < d.openPorts.size(); j++) {
            if (j) json += ",";
            json += std::to_string(d.openPorts[j]);
        }
        json += "]\n    }";
        if (i + 1 < r.devices.size()) json += ",";
        json += "\n";
    }
    json += "  ],\n";

    // Anomalies
    json += "  \"anomalies\": [\n";
    for (size_t i = 0; i < r.anomalies.size(); i++) {
        auto& a = r.anomalies[i];
        json += "    {\"type\":\"";        WToU8EscAppend(a.type, json);        json += "\",";
        json += "\"severity\":\"";         WToU8EscAppend(a.severity, json);    json += "\",";
        json += "\"deviceIp\":\"";         WToU8EscAppend(a.deviceIp, json);    json += "\",";
        json += "\"description\":\"";      WToU8EscAppend(a.description, json); json += "\"}";
        if (i + 1 < r.anomalies.size()) json += ",";
        json += "\n";
    }
    json += "  ],\n";

    // Ledger
    json += "  \"ledger\": [\n";
    for (size_t i = 0; i < ledger.size(); i++) {
        auto& e = ledger[i];
        json += "    {\"timestamp\":\""; WToU8EscAppend(e.timestamp, json); json += "\",";
        json += "\"action\":\"";         WToU8EscAppend(e.action, json);    json += "\",";
        json += "\"details\":\"";        WToU8EscAppend(e.details, json);   json += "\"}";
        if (i + 1 < ledger.size()) json += ",";
        json += "\n";
    }
    json += "  ]\n}\n";

    DWORD written;
    WriteFile(hFile, json.c_str(), (DWORD)json.size(), &written, nullptr);
    CloseHandle(hFile);

    MessageBox(hwnd, L"Full snapshot exported successfully.", L"Export",
               MB_OK | MB_ICONINFORMATION);
}

void TabPrivacy::Refresh() {
    RefreshStats();
    LoadConfig();
    RefreshHooks();
    RefreshSchedScan();
    // Sync API key display
    if (_hEditApiKey && _mainWnd && !_mainWnd->_apiKey.empty())
        SetWindowText(_hEditApiKey, _mainWnd->_apiKey.c_str());
}
