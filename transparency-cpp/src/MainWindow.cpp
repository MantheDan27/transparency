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
#include "TabSmartHome.h"
#include "Theme.h"
#include "Resource.h"

using std::wstring;

MainWindow* MainWindow::s_instance = nullptr;

// ─── Nav item definitions ─────────────────────────────────────────────────────
struct NavItem { const wchar_t* icon; const wchar_t* label; Tab tab; };

static const NavItem NAV_ITEMS[] = {
    { L"\u25A6", L"Overview",  Tab::Overview  },
    { L"\u25A1", L"Devices",   Tab::Devices   },
    { L"\u25B2", L"Alerts",    Tab::Alerts    },
    { L"\u25C6", L"Tools",     Tab::Tools     },
    { L"\u25A4", L"Ledger",    Tab::Ledger    },
    { L"\u25CB", L"Privacy",   Tab::Privacy   },
    { L"\u25C8", L"Smart Home", Tab::SmartHome },
};

// ─── Create ──────────────────────────────────────────────────────────────────

bool MainWindow::Create(HINSTANCE hInstance) {
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

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = 1400, wh = 860;
    int wx = (sw - ww) / 2, wy = (sh - wh) / 2;

    auto* self = new MainWindow();
    self->_hInstance = hInstance;
    s_instance = self;

    HWND hwnd = CreateWindowEx(
        0, L"TransparencyMainWnd",
        L"Transparency - Network Monitor",
        WS_OVERLAPPEDWINDOW,
        wx, wy, ww, wh,
        nullptr, nullptr, hInstance, self);

    if (!hwnd) { delete self; s_instance = nullptr; return false; }

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

    case WM_LBUTTONDOWN:
        return self->OnLButtonDown(hwnd, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));

    case WM_MOUSEMOVE:
        return self->OnMouseMove(hwnd, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));

    case WM_MOUSELEAVE:
        self->_hoverNav = -1;
        self->_trackingMouse = false;
        {
            RECT sidebarRc = { 0, 0, SIDEBAR_WIDTH, 9999 };
            InvalidateRect(hwnd, &sidebarRc, FALSE);
        }
        return 0;

    case WM_COMMAND:
        return self->OnCommand(hwnd, wp, lp);

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

    case WM_TIMER:
        if (wp == 1) {
            // Scheduled scan check — fires every 60 seconds
            if (self->_scheduledScan.enabled) {
                SYSTEMTIME st; GetLocalTime(&st);
                wchar_t nowHHMM[8];
                swprintf_s(nowHHMM, L"%02d:%02d", st.wHour, st.wMinute);

                // Check if preferred time matches (within the same minute) and
                // enough hours have elapsed since last run.
                bool timeMatch = (self->_scheduledScan.timeOfDay == nowHHMM);
                bool hoursOk   = true;
                if (!self->_scheduledScan.lastRun.empty()) {
                    // Simple check: compare wstring date prefix (YYYY-MM-DD HH:MM)
                    wstring last = self->_scheduledScan.lastRun;
                    int lastHour = 0, lastMin = 0, lastDay = 0, lastMon = 0, lastYr = 0;
                    swscanf_s(last.c_str(), L"%d-%d-%d %d:%d",
                               &lastYr, &lastMon, &lastDay, &lastHour, &lastMin);
                    int elapsedH = (st.wYear  - lastYr)  * 8760
                                 + (st.wMonth - lastMon) * 720
                                 + (st.wDay   - lastDay) * 24
                                 + (st.wHour  - lastHour);
                    hoursOk = (elapsedH >= self->_scheduledScan.intervalHours);
                }

                if (timeMatch && hoursOk) {
                    wchar_t ts[32];
                    swprintf_s(ts, L"%04d-%02d-%02d %02d:%02d",
                               st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
                    self->_scheduledScan.lastRun = ts;

                    if      (self->_scheduledScan.mode == L"deep")     self->StartDeepScan();
                    else if (self->_scheduledScan.mode == L"standard") self->StartStandardScan();
                    else                                                self->StartQuickScan();

                    self->AddLedgerEntry(L"Scheduled Scan",
                        L"Auto-triggered " + self->_scheduledScan.mode + L" scan");
                }
            }
        }
        return 0;

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

    // Create tab panels — no child windows in the sidebar at all;
    // sidebar is painted directly and hit-tested via WM_LBUTTONDOWN.
    _tabOverview = std::make_unique<TabOverview>();
    _tabDevices  = std::make_unique<TabDevices>();
    _tabAlerts   = std::make_unique<TabAlerts>();
    _tabTools    = std::make_unique<TabTools>();
    _tabLedger   = std::make_unique<TabLedger>();
    _tabPrivacy  = std::make_unique<TabPrivacy>();
    _tabSmartHome = std::make_unique<TabSmartHome>();

    int panelX = SIDEBAR_WIDTH;
    int panelW = cx - SIDEBAR_WIDTH;

    _tabOverview->Create(hwnd, panelX, 0, panelW, cy, this);
    _tabDevices ->Create(hwnd, panelX, 0, panelW, cy, this);
    _tabAlerts  ->Create(hwnd, panelX, 0, panelW, cy, this);
    _tabTools   ->Create(hwnd, panelX, 0, panelW, cy, this);
    _tabLedger  ->Create(hwnd, panelX, 0, panelW, cy, this);
    _tabPrivacy ->Create(hwnd, panelX, 0, panelW, cy, this);
    _tabSmartHome->Create(hwnd, panelX, 0, panelW, cy, this);

    ShowActivePanel();

    AddLedgerEntry(L"App Started", L"Transparency v3.5.0 initialized");

    // Check scheduled scans every 60 seconds
    SetTimer(hwnd, 1, 60000, nullptr);

    // Auto-run quick scan on launch so tabs have data immediately
    StartQuickScan();

    return 0;
}

// ─── OnLButtonDown ───────────────────────────────────────────────────────────

LRESULT MainWindow::OnLButtonDown(HWND hwnd, int x, int y) {
    if (x >= 0 && x < SIDEBAR_WIDTH) {
        for (int i = 0; i < (int)Tab::COUNT; i++) {
            int btnY = NAV_BTN_TOP + i * NAV_BTN_HEIGHT;
            if (y >= btnY && y < btnY + NAV_BTN_HEIGHT - 2) {
                SwitchTab((Tab)i);
                return 0;
            }
        }
    }
    return DefWindowProc(hwnd, WM_LBUTTONDOWN, 0, MAKELPARAM(x, y));
}

// ─── OnMouseMove ─────────────────────────────────────────────────────────────

LRESULT MainWindow::OnMouseMove(HWND hwnd, int x, int y) {
    if (!_trackingMouse) {
        TRACKMOUSEEVENT tme = {};
        tme.cbSize    = sizeof(tme);
        tme.dwFlags   = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        _trackingMouse = true;
    }

    int newHover = -1;
    if (x >= 0 && x < SIDEBAR_WIDTH) {
        for (int i = 0; i < (int)Tab::COUNT; i++) {
            int btnY = NAV_BTN_TOP + i * NAV_BTN_HEIGHT;
            if (y >= btnY && y < btnY + NAV_BTN_HEIGHT - 2) {
                newHover = i;
                break;
            }
        }
    }

    if (newHover != _hoverNav) {
        _hoverNav = newHover;
        RECT sidebarRc = { 0, 0, SIDEBAR_WIDTH, 9999 };
        InvalidateRect(hwnd, &sidebarRc, FALSE);
    }
    return 0;
}

// ─── LayoutChildren ──────────────────────────────────────────────────────────

void MainWindow::LayoutChildren(int cx, int cy) {
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
    resize(_tabSmartHome);
}

// ─── OnSize ──────────────────────────────────────────────────────────────────

LRESULT MainWindow::OnSize(HWND hwnd, int cx, int cy) {
    if (cx > 0 && cy > 0) {
        LayoutChildren(cx, cy);
        // Redraw sidebar (it has no child windows, must be repainted on resize)
        RECT sidebarRc = { 0, 0, SIDEBAR_WIDTH, cy };
        InvalidateRect(hwnd, &sidebarRc, FALSE);
    }
    return 0;
}

// ─── DrawNavSidebar ──────────────────────────────────────────────────────────

void MainWindow::DrawNavSidebar(HDC hdc, const RECT& rc) {
    // Sidebar background
    RECT sidebarRc = { 0, 0, SIDEBAR_WIDTH, rc.bottom };
    FillRect(hdc, &sidebarRc, Theme::BrushSidebar());

    // Right border
    RECT borderRc = { SIDEBAR_WIDTH - 1, 0, SIDEBAR_WIDTH, rc.bottom };
    FillRect(hdc, &borderRc, Theme::BrushBorder());

    // Brand name
    RECT brandRc = { 0, 12, SIDEBAR_WIDTH, NAV_BTN_TOP - 4 };
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::ACCENT);
    HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontBrand());
    DrawText(hdc, L"Transparency", -1, &brandRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    // Separator under brand
    RECT sepRc = { 10, NAV_BTN_TOP - 6, SIDEBAR_WIDTH - 10, NAV_BTN_TOP - 5 };
    FillRect(hdc, &sepRc, Theme::BrushBorder());

    // Nav buttons
    for (int i = 0; i < (int)Tab::COUNT; i++) {
        int btnY  = NAV_BTN_TOP + i * NAV_BTN_HEIGHT;
        RECT btnRc = { 8, btnY, SIDEBAR_WIDTH - 8, btnY + NAV_BTN_HEIGHT - 2 };

        bool active  = (_currentTab == (Tab)i);
        bool hovered = (_hoverNav == i && !active);

        COLORREF bg = active  ? Theme::BG_ROW_SEL
                    : hovered ? Theme::BG_ROW_HOV
                    :           Theme::BG_SIDEBAR;
        HBRUSH bgBrush = CreateSolidBrush(bg);
        FillRect(hdc, &btnRc, bgBrush);
        DeleteObject(bgBrush);

        if (active) {
            RECT accent = { btnRc.left, btnRc.top + 4, btnRc.left + 3, btnRc.bottom - 4 };
            HBRUSH accentBrush = CreateSolidBrush(Theme::ACCENT);
            FillRect(hdc, &accent, accentBrush);
            DeleteObject(accentBrush);
        }

        SetBkMode(hdc, TRANSPARENT);
        COLORREF textColor = active  ? Theme::ACCENT
                           : hovered ? Theme::TEXT_PRIMARY
                           :           Theme::TEXT_SECONDARY;
        SetTextColor(hdc, textColor);

        HFONT navFont = (HFONT)SelectObject(hdc, Theme::FontBody());

        RECT iconRc  = { btnRc.left + 12, btnRc.top, btnRc.left + 28, btnRc.bottom };
        DrawText(hdc, NAV_ITEMS[i].icon, -1, &iconRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT labelRc = { btnRc.left + 30, btnRc.top, btnRc.right - 4, btnRc.bottom };
        DrawText(hdc, NAV_ITEMS[i].label, -1, &labelRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(hdc, navFont);
    }

    // Version label
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TEXT_SECONDARY);
    RECT verRc = { 4, rc.bottom - 28, SIDEBAR_WIDTH - 4, rc.bottom - 6 };
    HFONT verFont = (HFONT)SelectObject(hdc, Theme::FontSmall());
    DrawText(hdc, L"v3.5.0", -1, &verRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, verFont);
}

// ─── OnPaint ─────────────────────────────────────────────────────────────────

LRESULT MainWindow::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);

    FillRect(hdc, &rc, Theme::BrushApp());
    DrawNavSidebar(hdc, rc);

    EndPaint(hwnd, &ps);
    return 0;
}

// ─── OnDestroy ───────────────────────────────────────────────────────────────

LRESULT MainWindow::OnDestroy(HWND hwnd) {
    KillTimer(hwnd, 1);
    StopLocalApi();
    _monitor.Stop();
    _scanner.Cancel();
    PostQuitMessage(0);
    return 0;
}

// ─── OnCommand ───────────────────────────────────────────────────────────────

LRESULT MainWindow::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = LOWORD(wp);
    switch (id) {
    case IDM_FILE_EXIT:
        DestroyWindow(hwnd);
        break;
    }
    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

// ─── SwitchTab ───────────────────────────────────────────────────────────────

void MainWindow::SwitchTab(Tab tab) {
    _currentTab = tab;
    ShowActivePanel();

    if (_hwnd) {
        RECT sidebarRc = { 0, 0, SIDEBAR_WIDTH, 9999 };
        InvalidateRect(_hwnd, &sidebarRc, FALSE);
        UpdateWindow(_hwnd);
    }
}

void MainWindow::ShowActivePanel() {
    auto show = [](auto& panel, bool visible) {
        if (!panel || !panel->GetHwnd()) return;
        HWND hw = panel->GetHwnd();
        if (visible) {
            SetWindowPos(hw, HWND_TOP, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        } else {
            SetWindowPos(hw, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW | SWP_NOZORDER);
        }
    };

    show(_tabOverview, _currentTab == Tab::Overview);
    show(_tabDevices,  _currentTab == Tab::Devices);
    show(_tabAlerts,   _currentTab == Tab::Alerts);
    show(_tabTools,    _currentTab == Tab::Tools);
    show(_tabLedger,   _currentTab == Tab::Ledger);
    show(_tabPrivacy,  _currentTab == Tab::Privacy);
    show(_tabSmartHome, _currentTab == Tab::SmartHome);
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

// ─── SaveSnapshot ─────────────────────────────────────────────────────────────

void MainWindow::SaveSnapshot() {
    std::lock_guard<std::mutex> lk(_dataMutex);
    _snapshots.push_back(_lastResult);
    // Keep last 20 snapshots
    if (_snapshots.size() > 20) _snapshots.erase(_snapshots.begin());
}

// ─── FirePluginHooks ──────────────────────────────────────────────────────────

void MainWindow::FirePluginHooks(const std::wstring& eventType, const std::wstring& deviceIp) {
    std::vector<PluginHook> hooks;
    {
        std::lock_guard<std::mutex> lk(_dataMutex);
        hooks = _pluginHooks;
    }

    for (auto& hook : hooks) {
        if (!hook.enabled) continue;
        if (hook.eventType != L"any" && hook.eventType != eventType) continue;

        // Build JSON payload for stdin
        std::string json = "{\"event\":\"";
        int n = WideCharToMultiByte(CP_UTF8, 0, eventType.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (n > 0) { std::string ev(n-1,0); WideCharToMultiByte(CP_UTF8,0,eventType.c_str(),-1,&ev[0],n,nullptr,nullptr); json += ev; }
        json += "\",\"device\":\"";
        n = WideCharToMultiByte(CP_UTF8, 0, deviceIp.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (n > 0) { std::string ip(n-1,0); WideCharToMultiByte(CP_UTF8,0,deviceIp.c_str(),-1,&ip[0],n,nullptr,nullptr); json += ip; }
        json += "\"}\n";

        // Create pipe for stdin
        HANDLE hRead, hWrite;
        SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) continue;
        SetHandleInformation(hWrite, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFO si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdInput = hRead;
        si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};

        // Security: Prevent unquoted path execution and command injection
        // by enforcing quotes and rejecting paths that contain quotes.
        if (hook.execPath.find(L"\"") != std::wstring::npos) continue;

        std::wstring cmd = L"\"" + hook.execPath + L"\"";
        if (CreateProcess(nullptr, &cmd[0], nullptr, nullptr, TRUE,
                          CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            DWORD written;
            WriteFile(hWrite, json.c_str(), (DWORD)json.size(), &written, nullptr);
            CloseHandle(hWrite);
            WaitForSingleObject(pi.hProcess, 5000); // max 5s wait
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        } else {
            CloseHandle(hWrite);
        }
        CloseHandle(hRead);
    }
}

// ─── LocalApiThreadProc ────────────────────────────────────────────────────────
// Minimal HTTP/1.0 REST server on port 7722.

DWORD WINAPI MainWindow::LocalApiThreadProc(LPVOID param) {
    auto* self = static_cast<MainWindow*>(param);

    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv == INVALID_SOCKET) return 1;

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)self->_apiPort);
    addr.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) != 0 ||
        listen(srv, 4) != 0) {
        closesocket(srv);
        return 1;
    }

    while (self->_apiEnabled) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(srv, &rfds);
        timeval tv{ 1, 0 };
        if (select(0, &rfds, nullptr, nullptr, &tv) <= 0) continue;

        SOCKET client = accept(srv, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;

        // Read request (up to 4KB)
        char buf[4096] = {};
        int recvd = recv(client, buf, sizeof(buf)-1, 0);
        if (recvd <= 0) { closesocket(client); continue; }
        buf[recvd] = '\0';
        std::string req = buf;

        // Check API key header
        std::string keyHdr;
        {
            int n = WideCharToMultiByte(CP_UTF8,0,self->_apiKey.c_str(),-1,nullptr,0,nullptr,nullptr);
            if(n>0){keyHdr.resize(n-1);WideCharToMultiByte(CP_UTF8,0,self->_apiKey.c_str(),-1,&keyHdr[0],n,nullptr,nullptr);}
        }
        bool authorized = keyHdr.empty() || req.find("X-API-Key: " + keyHdr) != std::string::npos;

        if (!authorized) {
            std::string resp = "HTTP/1.0 401 Unauthorized\r\nContent-Type: application/json\r\n\r\n{\"error\":\"Unauthorized\"}\r\n";
            send(client, resp.c_str(), (int)resp.size(), 0);
            closesocket(client);
            continue;
        }

        // Route
        std::string body;
        bool found = false;

        auto route = [&](const char* path) -> bool {
            return req.find(std::string("GET ") + path) == 0 ||
                   req.find(std::string("POST ") + path) == 0;
        };

        ScanResult r;
        {
            std::lock_guard<std::mutex> lk(self->_dataMutex);
            r = self->_lastResult;
        }

        auto wstr2utf8 = [](const std::wstring& w) -> std::string {
            int n = WideCharToMultiByte(CP_UTF8,0,w.c_str(),-1,nullptr,0,nullptr,nullptr);
            if(n<=0) return "";
            std::string s(n-1,0);
            WideCharToMultiByte(CP_UTF8,0,w.c_str(),-1,&s[0],n,nullptr,nullptr);
            return s;
        };
        auto jsonEsc = [](std::string s) -> std::string {
            std::string out;
            for (char c : s) {
                if (c=='"') out+="\\\"";
                else if (c=='\\') out+="\\\\";
                else if (c=='\r'||c=='\n') out+=" ";
                else out+=c;
            }
            return out;
        };

        if (route("/api/health")) {
            body = "{\"status\":\"ok\",\"version\":\"3.5.0\"}\r\n";
            found = true;
        } else if (route("/api/status")) {
            body = "{\"devices\":" + std::to_string(r.devices.size()) +
                   ",\"anomalies\":" + std::to_string(r.anomalies.size()) +
                   ",\"scannedAt\":\"" + jsonEsc(wstr2utf8(r.scannedAt)) + "\"}\r\n";
            found = true;
        } else if (route("/api/devices")) {
            body = "[";
            for (size_t i = 0; i < r.devices.size(); i++) {
                auto& d = r.devices[i];
                if (i) body += ",";
                body += "{\"ip\":\"" + jsonEsc(wstr2utf8(d.ip)) + "\""
                      + ",\"mac\":\"" + jsonEsc(wstr2utf8(d.mac)) + "\""
                      + ",\"hostname\":\"" + jsonEsc(wstr2utf8(d.hostname.empty()?d.customName:d.hostname)) + "\""
                      + ",\"vendor\":\"" + jsonEsc(wstr2utf8(d.vendor)) + "\""
                      + ",\"type\":\"" + jsonEsc(wstr2utf8(d.deviceType)) + "\""
                      + ",\"trust\":\"" + jsonEsc(wstr2utf8(d.trustState)) + "\""
                      + ",\"online\":" + (d.online?"true":"false")
                      + ",\"latencyMs\":" + std::to_string(d.latencyMs)
                      + ",\"confidence\":" + std::to_string(d.confidence)
                      + "}";
            }
            body += "]\r\n";
            found = true;
        } else if (route("/api/alerts")) {
            body = "[";
            for (size_t i = 0; i < r.anomalies.size(); i++) {
                auto& a = r.anomalies[i];
                if (i) body += ",";
                body += "{\"type\":\"" + jsonEsc(wstr2utf8(a.type)) + "\""
                      + ",\"severity\":\"" + jsonEsc(wstr2utf8(a.severity)) + "\""
                      + ",\"device\":\"" + jsonEsc(wstr2utf8(a.deviceIp)) + "\""
                      + ",\"description\":\"" + jsonEsc(wstr2utf8(a.description)) + "\""
                      + "}";
            }
            body += "]\r\n";
            found = true;
        } else if (route("/api/snapshots")) {
            std::lock_guard<std::mutex> lk(self->_dataMutex);
            body = "[";
            for (size_t i = 0; i < self->_snapshots.size(); i++) {
                if (i) body += ",";
                body += "{\"scannedAt\":\"" + jsonEsc(wstr2utf8(self->_snapshots[i].scannedAt)) + "\""
                      + ",\"devices\":" + std::to_string(self->_snapshots[i].devices.size())
                      + "}";
            }
            body += "]\r\n";
            found = true;
        }

        std::string resp;
        if (found) {
            resp = "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: "
                 + std::to_string(body.size()) + "\r\n\r\n" + body;
        } else {
            resp = "HTTP/1.0 404 Not Found\r\nContent-Type: application/json\r\n\r\n{\"error\":\"Not found\"}\r\n";
        }
        send(client, resp.c_str(), (int)resp.size(), 0);
        closesocket(client);
    }

    closesocket(srv);
    return 0;
}

void MainWindow::StartLocalApi() {
    if (_apiEnabled) return;
    _apiEnabled = true;
    // Generate a simple API key if none exists
    if (_apiKey.empty()) {
        SYSTEMTIME st; GetSystemTime(&st);
        wchar_t buf[64];
        swprintf_s(buf, L"tr-%04x%04x%04x", st.wMilliseconds ^ st.wSecond,
                   (WORD)GetCurrentProcessId(), st.wMinute ^ st.wHour);
        _apiKey = buf;
    }
    _apiThread = CreateThread(nullptr, 0, LocalApiThreadProc, this, 0, nullptr);
}

void MainWindow::StopLocalApi() {
    _apiEnabled = false;
    if (_apiThread) {
        WaitForSingleObject(_apiThread, 2000);
        CloseHandle(_apiThread);
        _apiThread = nullptr;
    }
}

LRESULT MainWindow::OnScanComplete(HWND hwnd, WPARAM, LPARAM lp) {
    auto* result = reinterpret_cast<ScanResult*>(lp);
    if (!result) return 0;

    {
        std::lock_guard<std::mutex> lk(_dataMutex);
        _lastResult = *result;
    }
    delete result;

    if (_tabOverview && _tabOverview->GetHwnd())
        SendMessage(_tabOverview->GetHwnd(), WM_SCAN_COMPLETE, 0, 0);
    if (_tabDevices && _tabDevices->GetHwnd())
        SendMessage(_tabDevices->GetHwnd(), WM_SCAN_COMPLETE, 0, 0);
    if (_tabAlerts && _tabAlerts->GetHwnd())
        SendMessage(_tabAlerts->GetHwnd(), WM_SCAN_COMPLETE, 0, 0);
    if (_tabLedger && _tabLedger->GetHwnd())
        SendMessage(_tabLedger->GetHwnd(), WM_SCAN_COMPLETE, 0, 0);
    if (_tabTools && _tabTools->GetHwnd())
        SendMessage(_tabTools->GetHwnd(), WM_SCAN_COMPLETE, 0, 0);
    if (_tabSmartHome && _tabSmartHome->GetHwnd())
        SendMessage(_tabSmartHome->GetHwnd(), WM_SCAN_COMPLETE, 0, 0);

    ScanResult r = GetLastResult();
    AddLedgerEntry(L"Scan Complete",
        L"Found " + std::to_wstring(r.devices.size()) + L" devices, " +
        std::to_wstring(r.anomalies.size()) + L" anomalies (" + r.mode + L")");

    return 0;
}

LRESULT MainWindow::OnScanProgress(HWND hwnd, WPARAM wp, LPARAM lp) {
    auto* msg = reinterpret_cast<std::wstring*>(lp);
    if (msg) {
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
