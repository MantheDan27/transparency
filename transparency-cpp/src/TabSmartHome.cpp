#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <sstream>
#include <vector>
#include <thread>

#include "TabSmartHome.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"

using std::wstring;

const wchar_t* TabSmartHome::s_className = L"TabSmartHomeWnd";

// Helper: Create a section header
static HWND MakeSection(HWND parent, const wchar_t* text, int& y, int cx, HINSTANCE hInst) {
    HWND hw = CreateWindowEx(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, y, cx - 32, 20, parent, nullptr, hInst, nullptr);
    SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBold(), TRUE);
    return hw;
}

bool TabSmartHome::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
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

LRESULT CALLBACK TabSmartHome::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TabSmartHome* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<TabSmartHome*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TabSmartHome*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
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
    case WM_COMMAND:
        return self->OnCommand(hwnd, wp, lp);
    case WM_NOTIFY:
        return self->OnNotify(hwnd, reinterpret_cast<NMHDR*>(lp));
    case WM_SCAN_COMPLETE:
        self->RefreshDevices();
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SetBkColor(hdc, Theme::BG_SURFACE);
        return (LRESULT)Theme::BrushSurface();
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, Theme::BrushSurface());
        return 1;
    }
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
}

LRESULT TabSmartHome::OnCreate(HWND hwnd, LPCREATESTRUCT) {
    RECT rc; GetClientRect(hwnd, &rc);
    CreateControls(hwnd, rc.right, rc.bottom);
    PopulateSmartDevices();
    return 0;
}

LRESULT TabSmartHome::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

LRESULT TabSmartHome::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::BrushSurface());

    // Draw title
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TEXT_PRIMARY);
    HFONT old = (HFONT)SelectObject(hdc, Theme::FontHeader());
    RECT titleRc = { 20, 10, rc.right - 20, 45 };
    DrawText(hdc, L"Smart Home", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);

    SetTextColor(hdc, Theme::TEXT_MUTED);
    old = (HFONT)SelectObject(hdc, Theme::FontSmall());
    RECT subRc = { 20, 42, rc.right - 20, 58 };
    DrawText(hdc, L"Control smart devices, set up automations, and manage integrations", -1, &subRc,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);

    EndPaint(hwnd, &ps);
    return 0;
}

void TabSmartHome::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    auto mkBtn = [&](const wchar_t* text, int id, int x, int y, int w, int h) -> HWND {
        HWND hw = CreateWindowEx(0, L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return hw;
    };
    auto mkEdit = [&](const wchar_t* hint, int id, int x, int y, int w, int h, bool multi = false) -> HWND {
        HWND hw = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | (multi ? ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL : 0),
            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)(multi ? Theme::FontMono() : Theme::FontBody()), TRUE);
        if (hint && !multi) SendMessage(hw, EM_SETCUEBANNER, FALSE, (LPARAM)hint);
        Theme::ApplyDarkEdit(hw);
        return hw;
    };
    auto mkLbl = [&](const wchar_t* t, int x, int y, int w) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
            x, y, w, 18, hwnd, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
        return hw;
    };

    int y = 68;

    // -- Smart Devices Discovered --
    MakeSection(hwnd, L"Smart Devices on Network", y, cx, hInst);
    y += 24;

    _hDeviceList = CreateWindowEx(0, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        16, y, cx - 32, 140, hwnd, (HMENU)(INT_PTR)IDC_SMART_DEVICE_LIST, hInst, nullptr);
    SendMessage(_hDeviceList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hDeviceList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    Theme::ApplyDarkScrollbar(_hDeviceList);

    // Add columns
    LVCOLUMN col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (LPWSTR)L"IP Address";  col.cx = 130; ListView_InsertColumn(_hDeviceList, 0, &col);
    col.pszText = (LPWSTR)L"Name";        col.cx = 160; ListView_InsertColumn(_hDeviceList, 1, &col);
    col.pszText = (LPWSTR)L"Type";        col.cx = 120; ListView_InsertColumn(_hDeviceList, 2, &col);
    col.pszText = (LPWSTR)L"Platform";    col.cx = 100; ListView_InsertColumn(_hDeviceList, 3, &col);
    col.pszText = (LPWSTR)L"Status";      col.cx = 80;  ListView_InsertColumn(_hDeviceList, 4, &col);

    y += 150;

    // -- Amazon Alexa Integration --
    MakeSection(hwnd, L"Amazon Alexa Integration", y, cx, hInst);
    y += 24;

    _hAlexaStatus = mkLbl(L"Status: Not connected", 16, y, 300);
    _hBtnAlexaLink = mkBtn(L"Link Alexa Account", IDC_BTN_ALEXA_LINK, 16, y + 22, 160, 28);
    _hBtnAlexaDiscover = mkBtn(L"Discover Devices", IDC_BTN_ALEXA_DISCOVER, 184, y + 22, 150, 28);
    y += 54;

    mkLbl(L"ALEXA VOICE COMMANDS", 16, y, 200);
    y += 18;
    _hAlexaOut = mkEdit(nullptr, IDC_SMART_ALEXA_OUT, 16, y, cx - 32, 70, true);
    SetWindowText(_hAlexaOut,
        L"Available commands when linked:\r\n"
        L"  \"Alexa, ask Transparency how many devices are online\"\r\n"
        L"  \"Alexa, ask Transparency to scan my network\"\r\n"
        L"  \"Alexa, ask Transparency for security status\"");
    y += 78;

    // -- Google Home Integration --
    MakeSection(hwnd, L"Google Home Integration", y, cx, hInst);
    y += 24;

    _hGoogleStatus = mkLbl(L"Status: Not connected", 16, y, 300);
    _hBtnGoogleLink = mkBtn(L"Link Google Account", IDC_BTN_GOOGLE_LINK, 16, y + 22, 160, 28);
    _hBtnGoogleDiscover = mkBtn(L"Sync Devices", IDC_BTN_GOOGLE_DISCOVER, 184, y + 22, 150, 28);
    y += 54;

    mkLbl(L"GOOGLE HOME ACTIONS", 16, y, 200);
    y += 18;
    _hGoogleOut = mkEdit(nullptr, IDC_SMART_GOOGLE_OUT, 16, y, cx - 32, 70, true);
    SetWindowText(_hGoogleOut,
        L"Available actions when linked:\r\n"
        L"  \"Hey Google, ask Transparency for a network status\"\r\n"
        L"  \"Hey Google, ask Transparency to scan for new devices\"\r\n"
        L"  \"Hey Google, ask Transparency if my network is secure\"");
    y += 78;

    // -- Automation Triggers --
    MakeSection(hwnd, L"Automation Triggers", y, cx, hInst);
    y += 24;

    mkLbl(L"WHEN", 16, y, 50);
    _hComboTriggerEvent = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        60, y - 2, 180, 200, hwnd, (HMENU)(INT_PTR)IDC_SMART_TRIGGER_EVENT, hInst, nullptr);
    SendMessage(_hComboTriggerEvent, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hComboTriggerEvent, CB_ADDSTRING, 0, (LPARAM)L"New device joins");
    SendMessage(_hComboTriggerEvent, CB_ADDSTRING, 0, (LPARAM)L"Device goes offline");
    SendMessage(_hComboTriggerEvent, CB_ADDSTRING, 0, (LPARAM)L"Risky port detected");
    SendMessage(_hComboTriggerEvent, CB_ADDSTRING, 0, (LPARAM)L"Internet outage");
    SendMessage(_hComboTriggerEvent, CB_ADDSTRING, 0, (LPARAM)L"High latency");
    SendMessage(_hComboTriggerEvent, CB_ADDSTRING, 0, (LPARAM)L"Scan complete");
    SendMessage(_hComboTriggerEvent, CB_SETCURSEL, 0, 0);

    mkLbl(L"THEN", 250, y, 50);
    _hComboTriggerAction = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        294, y - 2, 200, 200, hwnd, (HMENU)(INT_PTR)IDC_SMART_TRIGGER_ACTION, hInst, nullptr);
    SendMessage(_hComboTriggerAction, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hComboTriggerAction, CB_ADDSTRING, 0, (LPARAM)L"Flash smart lights red");
    SendMessage(_hComboTriggerAction, CB_ADDSTRING, 0, (LPARAM)L"Announce on Alexa");
    SendMessage(_hComboTriggerAction, CB_ADDSTRING, 0, (LPARAM)L"Announce on Google Home");
    SendMessage(_hComboTriggerAction, CB_ADDSTRING, 0, (LPARAM)L"Turn off smart plug");
    SendMessage(_hComboTriggerAction, CB_ADDSTRING, 0, (LPARAM)L"Send push notification");
    SendMessage(_hComboTriggerAction, CB_ADDSTRING, 0, (LPARAM)L"Run custom webhook");
    SendMessage(_hComboTriggerAction, CB_SETCURSEL, 0, 0);

    y += 30;
    _hBtnAddTrigger = mkBtn(L"Add Trigger", IDC_BTN_SMART_ADD_TRIGGER, 16, y, 110, 28);
    _hBtnDelTrigger = mkBtn(L"Remove", IDC_BTN_SMART_DEL_TRIGGER, 134, y, 80, 28);
    y += 36;

    _hTriggerList = CreateWindowEx(0, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
        16, y, cx - 32, 100, hwnd, (HMENU)(INT_PTR)IDC_SMART_TRIGGER_LIST, hInst, nullptr);
    SendMessage(_hTriggerList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hTriggerList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    Theme::ApplyDarkScrollbar(_hTriggerList);

    col.pszText = (LPWSTR)L"Event";  col.cx = 180; ListView_InsertColumn(_hTriggerList, 0, &col);
    col.pszText = (LPWSTR)L"Action"; col.cx = 200; ListView_InsertColumn(_hTriggerList, 1, &col);
    col.pszText = (LPWSTR)L"Status"; col.cx = 80;  ListView_InsertColumn(_hTriggerList, 2, &col);

    y += 110;

    // -- Scenes --
    MakeSection(hwnd, L"Scenes", y, cx, hInst);
    y += 24;

    _hEditSceneName = mkEdit(L"Scene name (e.g. \"Night Mode\")", IDC_SMART_SCENE_NAME, 16, y, 250, 26);
    _hBtnAddScene = mkBtn(L"Create Scene", IDC_BTN_SMART_ADD_SCENE, 274, y, 110, 26);
    _hBtnRunScene = mkBtn(L"Run Scene", IDC_BTN_SMART_RUN_SCENE, 392, y, 100, 26);
    y += 34;

    _hSceneList = CreateWindowEx(0, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
        16, y, cx - 32, 90, hwnd, (HMENU)(INT_PTR)IDC_SMART_SCENE_LIST, hInst, nullptr);
    SendMessage(_hSceneList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hSceneList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    Theme::ApplyDarkScrollbar(_hSceneList);

    col.pszText = (LPWSTR)L"Scene";    col.cx = 200; ListView_InsertColumn(_hSceneList, 0, &col);
    col.pszText = (LPWSTR)L"Devices";  col.cx = 120; ListView_InsertColumn(_hSceneList, 1, &col);
    col.pszText = (LPWSTR)L"Actions";  col.cx = 150; ListView_InsertColumn(_hSceneList, 2, &col);
}

void TabSmartHome::LayoutControls(int cx, int cy) {
    // For now, fixed layout -- controls are positioned in CreateControls
}

LRESULT TabSmartHome::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = LOWORD(wp);
    switch (id) {
    case IDC_BTN_ALEXA_LINK:
        if (_hAlexaStatus)
            SetWindowText(_hAlexaStatus, L"Status: Linking... (OAuth flow would open here)");
        break;

    case IDC_BTN_ALEXA_DISCOVER:
        if (_hAlexaStatus)
            SetWindowText(_hAlexaStatus, L"Status: Discovering Alexa-compatible devices...");
        PopulateSmartDevices();
        break;

    case IDC_BTN_GOOGLE_LINK:
        if (_hGoogleStatus)
            SetWindowText(_hGoogleStatus, L"Status: Linking... (OAuth flow would open here)");
        break;

    case IDC_BTN_GOOGLE_DISCOVER:
        if (_hGoogleStatus)
            SetWindowText(_hGoogleStatus, L"Status: Syncing devices with Google Home...");
        PopulateSmartDevices();
        break;

    case IDC_BTN_SMART_ADD_TRIGGER: {
        if (!_hComboTriggerEvent || !_hComboTriggerAction || !_hTriggerList) break;
        wchar_t evtBuf[128], actBuf[128];
        int evtIdx = (int)SendMessage(_hComboTriggerEvent, CB_GETCURSEL, 0, 0);
        int actIdx = (int)SendMessage(_hComboTriggerAction, CB_GETCURSEL, 0, 0);
        SendMessage(_hComboTriggerEvent, CB_GETLBTEXT, evtIdx, (LPARAM)evtBuf);
        SendMessage(_hComboTriggerAction, CB_GETLBTEXT, actIdx, (LPARAM)actBuf);

        int idx = ListView_GetItemCount(_hTriggerList);
        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = idx;
        lvi.pszText = evtBuf;
        ListView_InsertItem(_hTriggerList, &lvi);
        ListView_SetItemText(_hTriggerList, idx, 1, actBuf);
        ListView_SetItemText(_hTriggerList, idx, 2, (LPWSTR)L"Active");
        break;
    }

    case IDC_BTN_SMART_DEL_TRIGGER: {
        if (!_hTriggerList) break;
        int sel = ListView_GetNextItem(_hTriggerList, -1, LVNI_SELECTED);
        if (sel >= 0) ListView_DeleteItem(_hTriggerList, sel);
        break;
    }

    case IDC_BTN_SMART_ADD_SCENE: {
        if (!_hEditSceneName || !_hSceneList) break;
        wchar_t name[128];
        GetWindowText(_hEditSceneName, name, 128);
        if (!name[0]) break;

        int idx = ListView_GetItemCount(_hSceneList);
        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = idx;
        lvi.pszText = name;
        ListView_InsertItem(_hSceneList, &lvi);
        ListView_SetItemText(_hSceneList, idx, 1, (LPWSTR)L"All");
        ListView_SetItemText(_hSceneList, idx, 2, (LPWSTR)L"(configure)");
        SetWindowText(_hEditSceneName, L"");
        break;
    }

    case IDC_BTN_SMART_RUN_SCENE: {
        if (!_hSceneList) break;
        int sel = ListView_GetNextItem(_hSceneList, -1, LVNI_SELECTED);
        if (sel >= 0) {
            wchar_t name[128];
            ListView_GetItemText(_hSceneList, sel, 0, name, 128);
            wstring msg = wstring(L"Running scene: ") + name;
            MessageBox(hwnd, msg.c_str(), L"Scene", MB_OK | MB_ICONINFORMATION);
        }
        break;
    }
    }
    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

LRESULT TabSmartHome::OnNotify(HWND hwnd, NMHDR* hdr) {
    return DefWindowProc(hwnd, WM_NOTIFY, 0, (LPARAM)hdr);
}

void TabSmartHome::PopulateSmartDevices() {
    if (!_hDeviceList || !_mainWnd) return;
    ListView_DeleteAllItems(_hDeviceList);

    ScanResult r = _mainWnd->GetLastResult();
    int idx = 0;
    for (auto& d : r.devices) {
        // Detect likely smart home devices by type or open ports
        bool isSmart = false;
        wstring platform = L"Unknown";

        // Check for common smart home indicators
        for (int p : d.openPorts) {
            if (p == 8008 || p == 8009 || p == 8443) { isSmart = true; platform = L"Google/Cast"; }
            if (p == 8123)   { isSmart = true; platform = L"Home Assistant"; }
            if (p == 1883 || p == 8883) { isSmart = true; platform = L"MQTT"; }
            if (p == 49152 || p == 49153) { isSmart = true; platform = L"UPnP"; }
        }
        if (d.deviceType == L"IoT Device" || d.deviceType == L"Smart Speaker" ||
            d.deviceType == L"Smart TV" || d.deviceType == L"Camera") {
            isSmart = true;
        }
        if (!d.ssdpInfo.empty()) {
            if (d.ssdpInfo.find(L"Amazon") != wstring::npos) { isSmart = true; platform = L"Amazon Alexa"; }
            if (d.ssdpInfo.find(L"Google") != wstring::npos) { isSmart = true; platform = L"Google Home"; }
        }
        if (!d.vendor.empty()) {
            if (d.vendor.find(L"Amazon") != wstring::npos) { isSmart = true; platform = L"Amazon Alexa"; }
            if (d.vendor.find(L"Google") != wstring::npos) { isSmart = true; platform = L"Google/Nest"; }
            if (d.vendor.find(L"Philips") != wstring::npos) { isSmart = true; platform = L"Hue"; }
            if (d.vendor.find(L"TP-Link") != wstring::npos || d.vendor.find(L"Kasa") != wstring::npos) { isSmart = true; platform = L"TP-Link/Kasa"; }
            if (d.vendor.find(L"Ring") != wstring::npos) { isSmart = true; platform = L"Ring"; }
            if (d.vendor.find(L"Sonos") != wstring::npos) { isSmart = true; platform = L"Sonos"; }
        }

        if (!isSmart) continue;

        wstring name = !d.customName.empty() ? d.customName
                     : !d.hostname.empty()   ? d.hostname
                     : !d.vendor.empty()     ? d.vendor
                     : L"Unknown Device";

        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = idx;
        lvi.pszText = (LPWSTR)d.ip.c_str();
        ListView_InsertItem(_hDeviceList, &lvi);
        ListView_SetItemText(_hDeviceList, idx, 1, (LPWSTR)name.c_str());
        ListView_SetItemText(_hDeviceList, idx, 2, (LPWSTR)d.deviceType.c_str());
        ListView_SetItemText(_hDeviceList, idx, 3, (LPWSTR)platform.c_str());
        ListView_SetItemText(_hDeviceList, idx, 4, (LPWSTR)(d.online ? L"Online" : L"Offline"));
        idx++;
    }
}

void TabSmartHome::RefreshDevices() {
    PopulateSmartDevices();
}
