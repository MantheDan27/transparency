#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>
#include <sstream>
#include <memory>
#include <mutex>
#include <future>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")

#include "MainWindow.h"
#include "TabOverview.h"
#include "TabDevices.h"
#include "TabAlerts.h"
#include "TabTools.h"
#include "TabLedger.h"
#include "TabPrivacy.h"
#include "Theme.h"
#include "Resource.h"

using std::wstring;

MainWindow* MainWindow::s_instance = nullptr;

// ─── Nav button labels/icons ──────────────────────────────────────────────────
struct NavItem {
    const wchar_t* icon;
    const wchar_t* label;
    Tab             tab;
};

static const NavItem NAV_ITEMS[] = {
    { L"\u25A6", L"Overview",  Tab::Overview  },
    { L"\u25A1", L"Devices",   Tab::Devices   },
    { L"\u25B2", L"Alerts",    Tab::Alerts    },
    { L"\u25C6", L"Tools",     Tab::Tools     },
    { L"\u25A4", L"Ledger",    Tab::Ledger    },
    { L"\u25CB", L"Privacy",   Tab::Privacy   },
};

// ─── Create ──────────────────────────────────────────────────────────────────

bool MainWindow::Create(HINSTANCE hInstance) {
    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = Theme::BrushApp();
    wc.lpszClassName = L"TransparencyMainWnd";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) return false;

    // Center the window
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = 1400, wh = 860;
    int wx = (sw - ww) / 2, wy = (sh - wh) / 2;

    auto* self = new MainWindow();
    self->_hInstance = hInstance;
    s_instance = self;

    HWND hwnd = CreateWindowEx(
        0,
        L"TransparencyMainWnd",
        L"Transparency - Network Monitor",
        WS_OVERLAPPEDWINDOW,
        wx, wy, ww, wh,
        nullptr, nullptr, hInstance, self);

    if (!hwnd) { delete self; s_instance = nullptr; return false; }

    // Dark titlebar
    Theme::SetDarkTitlebar(hwnd);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return true;
}

// ─── WndProc ─────────────────────────────────────────────────────────────────

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
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
        HDC hdc = (HDC)wp;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, Theme::BrushApp());
        return 1;
    }

    case WM_COMMAND:
        return self->OnCommand(hwnd, wp, lp);

    case WM_DRAWITEM:
        self->OnDrawItem(hwnd, reinterpret_cast<DRAWITEMSTRUCT*>(lp));
        return TRUE;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SetBkColor(hdc, Theme::BG_APP);
        return (LRESULT)Theme::BrushApp();
    }

    case WM_SCAN_COMPLETE:
        return self->OnScanComplete(hwnd, wp, lp);

    case WM_SCAN_PROGRESS:
        return self->OnScanProgress(hwnd, wp, lp);

    case WM_MONITOR_TICK:
        return self->OnMonitorTick(hwnd, wp, lp);

    case WM_INTERNET_STATUS:
        return self->OnInternetStatus(hwnd, wp, lp);

    case WM_GATEWAY_CHANGED:
        return self->OnGatewayChanged(hwnd, wp, lp);

    case WM_DESTROY:
        return self->OnDestroy(hwnd);

    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
}

// ─── OnCreate ────────────────────────────────────────────────────────────────

LRESULT MainWindow::OnCreate(HWND hwnd, LPCREATESTRUCT) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int cx = rc.right, cy = rc.bottom;

    // Create sidebar
    CreateSidebar(hwnd, cx, cy);

    // Create tab panels
    _tabOverview = std::make_unique<TabOverview>();
    _tabDevices  = std::make_unique<TabDevices>();
    _tabAlerts   = std::make_unique<TabAlerts>();
    _tabTools    = std::make_unique<TabTools>();
    _tabLedger   = std::make_unique<TabLedger>();
    _tabPrivacy  = std::make_unique<TabPrivacy>();

    int panelX = SIDEBAR_WIDTH;
    int panelW = cx - SIDEBAR_WIDTH;

    _tabOverview->Create(hwnd, panelX, 0, panelW, cy, this);
    _tabDevices ->Create(hwnd, panelX, 0, panelW, cy, this);
    _tabAlerts  ->Create(hwnd, panelX, 0, panelW, cy, this);
    _tabTools   ->Create(hwnd, panelX, 0, panelW, cy, this);
    _tabLedger  ->Create(hwnd, panelX, 0, panelW, cy, this);
    _tabPrivacy ->Create(hwnd, panelX, 0, panelW, cy, this);

    ShowActivePanel();

    // Add welcome ledger entry
    AddLedgerEntry(L"App Started", L"Transparency v2.1.0 initialized");

    return 0;
}

// ─── CreateSidebar ───────────────────────────────────────────────────────────

void MainWindow::CreateSidebar(HWND parent, int cx, int cy) {
    _hSidebar = CreateWindowEx(
        0, L"STATIC", nullptr,
        WS_CHILD | WS_VISIBLE | SS_NOTIFY,
        0, 0, SIDEBAR_WIDTH, cy,
        parent, (HMENU)IDC_PANEL_OVERVIEW, _hInstance, nullptr);

    // Brand label (static text, drawn via WM_PAINT override is complex,
    // so we handle sidebar painting in parent WM_PAINT / WM_ERASEBKGND)

    // Create nav buttons (owner-drawn)
    for (int i = 0; i < (int)Tab::COUNT; i++) {
        int y = NAV_BTN_TOP + i * NAV_BTN_HEIGHT;
        _hNavBtns[i] = CreateWindowEx(
            0, L"BUTTON", NAV_ITEMS[i].label,
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            8, y, SIDEBAR_WIDTH - 16, NAV_BTN_HEIGHT - 2,
            parent, (HMENU)(IDC_NAV_OVERVIEW + i), _hInstance, nullptr);
        SendMessage(_hNavBtns[i], WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    }

    // Version label
    _hStatus = CreateWindowEx(
        0, L"STATIC", L"v2.1.0",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        4, cy - 28, SIDEBAR_WIDTH - 8, 22,
        parent, (HMENU)IDC_STATIC_VERSION, _hInstance, nullptr);
    SendMessage(_hStatus, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
}

// ─── LayoutChildren ──────────────────────────────────────────────────────────

void MainWindow::LayoutChildren(int cx, int cy) {
    if (_hSidebar) SetWindowPos(_hSidebar, nullptr, 0, 0, SIDEBAR_WIDTH, cy, SWP_NOZORDER);

    // Reposition nav buttons
    for (int i = 0; i < (int)Tab::COUNT; i++) {
        if (_hNavBtns[i]) {
            int y = NAV_BTN_TOP + i * NAV_BTN_HEIGHT;
            SetWindowPos(_hNavBtns[i], nullptr, 8, y, SIDEBAR_WIDTH - 16, NAV_BTN_HEIGHT - 2, SWP_NOZORDER);
        }
    }

    if (_hStatus) SetWindowPos(_hStatus, nullptr, 4, cy - 28, SIDEBAR_WIDTH - 8, 22, SWP_NOZORDER);

    int panelX = SIDEBAR_WIDTH;
    int panelW = cx - SIDEBAR_WIDTH;

    auto resize = [&](auto& tab) {
        if (tab && tab->GetHwnd())
            SetWindowPos(tab->GetHwnd(), nullptr, panelX, 0, panelW, cy, SWP_NOZORDER);
    };

    resize(_tabOverview);
    resize(_tabDevices);
    resize(_tabAlerts);
    resize(_tabTools);
    resize(_tabLedger);
    resize(_tabPrivacy);
}

// ─── OnSize ──────────────────────────────────────────────────────────────────

LRESULT MainWindow::OnSize(HWND hwnd, int cx, int cy) {
    if (cx > 0 && cy > 0) LayoutChildren(cx, cy);
    return 0;
}

// ─── OnPaint ─────────────────────────────────────────────────────────────────

LRESULT MainWindow::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);

    // Fill app background
    FillRect(hdc, &rc, Theme::BrushApp());

    // Draw sidebar background
    RECT sidebarRc = { 0, 0, SIDEBAR_WIDTH, rc.bottom };
    FillRect(hdc, &sidebarRc, Theme::BrushSidebar());

    // Sidebar border (right edge)
    RECT borderRc = { SIDEBAR_WIDTH - 1, 0, SIDEBAR_WIDTH, rc.bottom };
    FillRect(hdc, &borderRc, Theme::BrushBorder());

    // Brand name "Transparency"
    RECT brandRc = { 0, 12, SIDEBAR_WIDTH, NAV_BTN_TOP - 4 };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::ACCENT);
    HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontBrand());
    DrawText(hdc, L"Transparency", -1, &brandRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    // Separator line under brand
    RECT sepRc = { 10, NAV_BTN_TOP - 6, SIDEBAR_WIDTH - 10, NAV_BTN_TOP - 5 };
    FillRect(hdc, &sepRc, Theme::BrushBorder());

    EndPaint(hwnd, &ps);
    return 0;
}

// ─── OnDestroy ───────────────────────────────────────────────────────────────

LRESULT MainWindow::OnDestroy(HWND hwnd) {
    _monitor.Stop();
    _scanner.Cancel();
    PostQuitMessage(0);
    return 0;
}

// ─── OnCommand ───────────────────────────────────────────────────────────────

LRESULT MainWindow::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = LOWORD(wp);

    for (int i = 0; i < (int)Tab::COUNT; i++) {
        if (id == IDC_NAV_OVERVIEW + i) {
            SwitchTab((Tab)i);
            return 0;
        }
    }

    switch (id) {
    case IDM_FILE_EXIT:
        DestroyWindow(hwnd);
        break;
    }

    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

// ─── OnDrawItem ──────────────────────────────────────────────────────────────

LRESULT MainWindow::OnDrawItem(HWND, DRAWITEMSTRUCT* dis) {
    if (!dis) return 0;

    // Find which nav button
    int idx = -1;
    for (int i = 0; i < (int)Tab::COUNT; i++) {
        if (dis->hwndItem == _hNavBtns[i]) { idx = i; break; }
    }
    if (idx < 0) return 0;

    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool active = (_currentTab == (Tab)idx);
    bool hovered = (dis->itemState & ODS_HOTLIGHT) != 0;
    bool pressed = (dis->itemState & ODS_SELECTED) != 0;

    // Background
    COLORREF bg = active ? Theme::BG_ROW_SEL : (pressed ? Theme::BG_ROW_HOV : Theme::BG_SIDEBAR);
    HBRUSH bgBrush = CreateSolidBrush(bg);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Active indicator: blue left border
    if (active) {
        RECT indicator = { rc.left, rc.top + 4, rc.left + 3, rc.bottom - 4 };
        HBRUSH accentBrush = CreateSolidBrush(Theme::ACCENT);
        FillRect(hdc, &indicator, accentBrush);
        DeleteObject(accentBrush);
    }

    // Icon
    SetBkMode(hdc, TRANSPARENT);
    COLORREF textColor = active ? Theme::ACCENT : (hovered ? Theme::TEXT_PRIMARY : Theme::TEXT_SECONDARY);
    SetTextColor(hdc, textColor);

    HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontBody());

    // Draw icon
    RECT iconRc = { rc.left + 12, rc.top, rc.left + 28, rc.bottom };
    DrawText(hdc, NAV_ITEMS[idx].icon, -1, &iconRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Draw label
    RECT labelRc = { rc.left + 30, rc.top, rc.right - 4, rc.bottom };
    DrawText(hdc, NAV_ITEMS[idx].label, -1, &labelRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, oldFont);

    // Focus rect
    if (dis->itemState & ODS_FOCUS) {
        DrawFocusRect(hdc, &rc);
    }

    return TRUE;
}

// ─── SwitchTab ───────────────────────────────────────────────────────────────

void MainWindow::SwitchTab(Tab tab) {
    _currentTab = tab;
    ShowActivePanel();

    // Redraw sidebar to update active button
    if (_hwnd) {
        RECT sidebarRc = { 0, 0, SIDEBAR_WIDTH, 9999 };
        InvalidateRect(_hwnd, &sidebarRc, FALSE);
        UpdateWindow(_hwnd);
    }

    // Redraw all nav buttons
    for (auto& btn : _hNavBtns) {
        if (btn) InvalidateRect(btn, nullptr, TRUE);
    }
}

void MainWindow::ShowActivePanel() {
    auto show = [](auto& panel, bool visible) {
        if (panel && panel->GetHwnd())
            ShowWindow(panel->GetHwnd(), visible ? SW_SHOW : SW_HIDE);
    };

    show(_tabOverview, _currentTab == Tab::Overview);
    show(_tabDevices,  _currentTab == Tab::Devices);
    show(_tabAlerts,   _currentTab == Tab::Alerts);
    show(_tabTools,    _currentTab == Tab::Tools);
    show(_tabLedger,   _currentTab == Tab::Ledger);
    show(_tabPrivacy,  _currentTab == Tab::Privacy);
}

// ─── Scan helpers ─────────────────────────────────────────────────────────────

void MainWindow::StartQuickScan() {
    AddLedgerEntry(L"Scan Started", L"Quick scan initiated");

    auto* hwnd = _hwnd;
    std::thread([this, hwnd]() {
        auto future = _scanner.QuickScan([hwnd](int pct, std::wstring msg) {
            wstring* msgPtr = new wstring(msg);
            PostMessage(hwnd, WM_SCAN_PROGRESS, (WPARAM)pct, (LPARAM)msgPtr);
        });

        ScanResult* result = new ScanResult(future.get());
        {
            std::lock_guard<std::mutex> lk(_dataMutex);
            _lastResult = *result;
        }
        PostMessage(hwnd, WM_SCAN_COMPLETE, 0, (LPARAM)result);
    }).detach();
}

void MainWindow::StartStandardScan() {
    AddLedgerEntry(L"Scan Started", L"Standard scan initiated");

    auto* hwnd = _hwnd;
    std::thread([this, hwnd]() {
        auto future = _scanner.StandardScan([hwnd](int pct, std::wstring msg) {
            wstring* msgPtr = new wstring(msg);
            PostMessage(hwnd, WM_SCAN_PROGRESS, (WPARAM)pct, (LPARAM)msgPtr);
        });

        ScanResult* result = new ScanResult(future.get());
        {
            std::lock_guard<std::mutex> lk(_dataMutex);
            _lastResult = *result;
        }
        PostMessage(hwnd, WM_SCAN_COMPLETE, 0, (LPARAM)result);
    }).detach();
}

void MainWindow::StartDeepScan() {
    AddLedgerEntry(L"Scan Started", L"Deep scan initiated");

    auto* hwnd = _hwnd;
    std::thread([this, hwnd]() {
        auto future = _scanner.DeepScan([hwnd](int pct, std::wstring msg) {
            wstring* msgPtr = new wstring(msg);
            PostMessage(hwnd, WM_SCAN_PROGRESS, (WPARAM)pct, (LPARAM)msgPtr);
        });

        ScanResult* result = new ScanResult(future.get());
        {
            std::lock_guard<std::mutex> lk(_dataMutex);
            _lastResult = *result;
        }
        PostMessage(hwnd, WM_SCAN_COMPLETE, 0, (LPARAM)result);
    }).detach();
}

void MainWindow::StartMonitor() {
    MonitorConfig cfg;
    cfg.enabled = true;
    cfg.intervalMinutes = 5;
    cfg.alertOnOutage = true;
    cfg.alertOnGatewayMac = true;
    cfg.alertOnDnsChange = true;

    auto* hwnd = _hwnd;

    _monitor.Start(
        cfg,
        [hwnd](ScanResult sr) {
            ScanResult* p = new ScanResult(std::move(sr));
            PostMessage(hwnd, WM_SCAN_COMPLETE, 0, (LPARAM)p);
            PostMessage(hwnd, WM_MONITOR_TICK, 0, 0);
        },
        [hwnd](InternetStatus is) {
            InternetStatus* p = new InternetStatus(is);
            PostMessage(hwnd, WM_INTERNET_STATUS, 0, (LPARAM)p);
        },
        [hwnd](std::wstring oldMac, std::wstring newMac) {
            std::wstring* p = new std::wstring(oldMac + L"|" + newMac);
            PostMessage(hwnd, WM_GATEWAY_CHANGED, 0, (LPARAM)p);
        }
    );

    AddLedgerEntry(L"Monitor Started", L"Network monitoring enabled");
}

void MainWindow::StopMonitor() {
    _monitor.Stop();
    AddLedgerEntry(L"Monitor Stopped", L"Network monitoring disabled");
}

void MainWindow::AddLedgerEntry(const std::wstring& action, const std::wstring& details) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[64];
    swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    LedgerEntry entry;
    entry.timestamp = buf;
    entry.action    = action;
    entry.details   = details;

    std::lock_guard<std::mutex> lk(_dataMutex);
    _ledger.push_back(entry);
}

ScanResult MainWindow::GetLastResult() const {
    std::lock_guard<std::mutex> lk(_dataMutex);
    return _lastResult;
}

// ─── Scan/Monitor Message Handlers ───────────────────────────────────────────

LRESULT MainWindow::OnScanComplete(HWND hwnd, WPARAM, LPARAM lp) {
    auto* result = reinterpret_cast<ScanResult*>(lp);
    if (!result) return 0;

    {
        std::lock_guard<std::mutex> lk(_dataMutex);
        _lastResult = *result;
    }
    delete result;

    // Notify all panels
    if (_tabOverview && _tabOverview->GetHwnd())
        SendMessage(_tabOverview->GetHwnd(), WM_SCAN_COMPLETE, 0, 0);
    if (_tabDevices && _tabDevices->GetHwnd())
        SendMessage(_tabDevices->GetHwnd(), WM_SCAN_COMPLETE, 0, 0);
    if (_tabAlerts && _tabAlerts->GetHwnd())
        SendMessage(_tabAlerts->GetHwnd(), WM_SCAN_COMPLETE, 0, 0);
    if (_tabLedger && _tabLedger->GetHwnd())
        SendMessage(_tabLedger->GetHwnd(), WM_SCAN_COMPLETE, 0, 0);

    ScanResult r = GetLastResult();
    AddLedgerEntry(L"Scan Complete",
        L"Found " + std::to_wstring(r.devices.size()) + L" devices, " +
        std::to_wstring(r.anomalies.size()) + L" anomalies (" + r.mode + L")");

    return 0;
}

LRESULT MainWindow::OnScanProgress(HWND hwnd, WPARAM wp, LPARAM lp) {
    auto* msg = reinterpret_cast<std::wstring*>(lp);
    if (msg) {
        // Forward to overview panel
        if (_tabOverview && _tabOverview->GetHwnd())
            SendMessage(_tabOverview->GetHwnd(), WM_SCAN_PROGRESS, wp, (LPARAM)msg);
        else
            delete msg;
    }
    return 0;
}

LRESULT MainWindow::OnMonitorTick(HWND, WPARAM, LPARAM) {
    if (_tabOverview && _tabOverview->GetHwnd())
        SendMessage(_tabOverview->GetHwnd(), WM_MONITOR_TICK, 0, 0);
    return 0;
}

LRESULT MainWindow::OnInternetStatus(HWND, WPARAM, LPARAM lp) {
    auto* status = reinterpret_cast<InternetStatus*>(lp);
    if (status) {
        AddLedgerEntry(L"Internet Status",
            status->online ?
            (L"Online, " + std::to_wstring(status->latencyMs) + L"ms") :
            L"Offline");
        delete status;
    }
    return 0;
}

LRESULT MainWindow::OnGatewayChanged(HWND, WPARAM, LPARAM lp) {
    auto* data = reinterpret_cast<std::wstring*>(lp);
    if (data) {
        AddLedgerEntry(L"Gateway MAC Changed", *data);
        delete data;
    }
    return 0;
}
