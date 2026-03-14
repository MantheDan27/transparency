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
#include <map>
#include <thread>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")
#include <objbase.h>
#include <gdiplus.h>

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
    { L"\u25AB", L"Dashboard",    Tab::Overview  },
    { L"\u229E", L"Devices",      Tab::Devices   },
    { L"\u2691", L"Alerts",       Tab::Alerts    },
    { L"\u25CE", L"Topology",     Tab::SmartHome },
    { L"\u25B3", L"Diagnostics",  Tab::Tools     },
    { L"\u2630", L"Scan History", Tab::Ledger    },
};
static const int NAV_ITEM_COUNT = 6;

// Bottom nav items (pinned)
static const NavItem NAV_BOTTOM[] = {
    { L"\u2699", L"Settings",  Tab::Privacy   },
};

const wchar_t* MainWindow::NavLabel(int i) {
    if (i >= 0 && i < NAV_ITEM_COUNT) return NAV_ITEMS[i].label;
    return L"";
}
const wchar_t* MainWindow::NavIcon(int i) {
    if (i >= 0 && i < NAV_ITEM_COUNT) return NAV_ITEMS[i].icon;
    return L"";
}

// ─── Create ──────────────────────────────────────────────────────────────────

bool MainWindow::Create(HINSTANCE hInstance) {
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
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
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
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

    case WM_ERASEBKGND:
        // Suppress — OnPaint handles all drawing via double buffer
        return 1;

    case WM_LBUTTONDOWN:
        return self->OnLButtonDown(hwnd, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));

    case WM_MOUSEMOVE:
        return self->OnMouseMove(hwnd, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));

    case WM_MOUSELEAVE:
        self->_hoverNav = -1;
        self->_hoverTitleBtn = -1;
        self->_hoverWinBtn = -1;
        self->_trackingMouse = false;
        InvalidateRect(hwnd, nullptr, FALSE);
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
        if (wp == 2) {
            // Refresh status bar and content header every second
            RECT rc; GetClientRect(hwnd, &rc);
            RECT statusRc = { 0, rc.bottom - 26, rc.right, rc.bottom };
            InvalidateRect(hwnd, &statusRc, FALSE);
            RECT hdrRc = { self->GetSidebarWidth(), 38, rc.right, 78 };
            InvalidateRect(hwnd, &hdrRc, FALSE);
            return 0;
        }
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

    int sw = GetSidebarWidth();
    int contentTop = TITLEBAR_H + CONTENT_HDR_H;
    int contentBot = cy - STATUSBAR_H;
    int panelX = sw;
    int panelW = cx - sw;
    int panelH = contentBot - contentTop;

    _tabOverview = std::make_unique<TabOverview>();
    _tabDevices  = std::make_unique<TabDevices>();
    _tabAlerts   = std::make_unique<TabAlerts>();
    _tabTools    = std::make_unique<TabTools>();
    _tabLedger   = std::make_unique<TabLedger>();
    _tabPrivacy  = std::make_unique<TabPrivacy>();
    _tabSmartHome = std::make_unique<TabSmartHome>();

    _tabOverview->Create(hwnd, panelX, contentTop, panelW, panelH, this);
    _tabDevices ->Create(hwnd, panelX, contentTop, panelW, panelH, this);
    _tabAlerts  ->Create(hwnd, panelX, contentTop, panelW, panelH, this);
    _tabTools   ->Create(hwnd, panelX, contentTop, panelW, panelH, this);
    _tabLedger  ->Create(hwnd, panelX, contentTop, panelW, panelH, this);
    _tabPrivacy ->Create(hwnd, panelX, contentTop, panelW, panelH, this);
    _tabSmartHome->Create(hwnd, panelX, contentTop, panelW, panelH, this);

    ShowActivePanel();

    AddLedgerEntry(L"App Started", L"Transparency v4.1.0 initialized");
    SetTimer(hwnd, 1, 60000, nullptr);

    // Refresh status bar every 5 seconds (avoid excessive repaints)
    SetTimer(hwnd, 2, 5000, nullptr);

    StartQuickScan();
    return 0;
}

// ─── OnLButtonDown ───────────────────────────────────────────────────────────

LRESULT MainWindow::OnLButtonDown(HWND hwnd, int x, int y) {
    int sw = GetSidebarWidth();

    // Title bar toolbar buttons: Quick Scan, Deep Scan, Stop
    if (y < TITLEBAR_H && x > sw + 20) {
        int tbX = sw + 20;
        // Quick Scan button
        if (x >= tbX && x < tbX + 80) { StartQuickScan(); return 0; }
        tbX += 88;
        // Deep Scan button
        if (x >= tbX && x < tbX + 80) { StartDeepScan(); return 0; }
        tbX += 88;
        // Stop button
        if (x >= tbX && x < tbX + 50) { StopMonitor(); return 0; }
    }

    // Sidebar collapse toggle
    if (x >= 0 && x < sw && y >= TITLEBAR_H && y < TITLEBAR_H + 32) {
        _sidebarExpanded = !_sidebarExpanded;
        RECT rc; GetClientRect(hwnd, &rc);
        LayoutChildren(rc.right, rc.bottom);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    // Sidebar nav items
    if (x >= 0 && x < sw && y >= TITLEBAR_H + NAV_BTN_TOP) {
        for (int i = 0; i < NAV_ITEM_COUNT; i++) {
            int btnY = TITLEBAR_H + NAV_BTN_TOP + i * NAV_BTN_HEIGHT;
            if (y >= btnY && y < btnY + NAV_BTN_HEIGHT) {
                SwitchTab(NAV_ITEMS[i].tab);
                return 0;
            }
        }
        // Bottom nav (Settings)
        RECT rc; GetClientRect(hwnd, &rc);
        int botY = rc.bottom - STATUSBAR_H - 44;
        if (y >= botY && y < botY + NAV_BTN_HEIGHT) {
            SwitchTab(Tab::Privacy);
            return 0;
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

    int sw = GetSidebarWidth();
    int newHover = -1;
    if (x >= 0 && x < sw && y >= TITLEBAR_H + NAV_BTN_TOP) {
        for (int i = 0; i < NAV_ITEM_COUNT; i++) {
            int btnY = TITLEBAR_H + NAV_BTN_TOP + i * NAV_BTN_HEIGHT;
            if (y >= btnY && y < btnY + NAV_BTN_HEIGHT) {
                newHover = i;
                break;
            }
        }
    }

    if (newHover != _hoverNav) {
        _hoverNav = newHover;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;
}

// ─── LayoutChildren ──────────────────────────────────────────────────────────

void MainWindow::LayoutChildren(int cx, int cy) {
    int sw = GetSidebarWidth();
    int contentTop = TITLEBAR_H + CONTENT_HDR_H;
    int contentBot = cy - STATUSBAR_H;
    int panelX = sw;
    int panelW = cx - sw;
    int panelH = contentBot - contentTop;
    if (panelH < 1) panelH = 1;

    auto resize = [&](auto& tab) {
        if (tab && tab->GetHwnd())
            SetWindowPos(tab->GetHwnd(), nullptr, panelX, contentTop, panelW, panelH, SWP_NOZORDER);
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
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    return 0;
}

// ─── DrawTitleBar ─────────────────────────────────────────────────────────────

void MainWindow::DrawTitleBar(HDC hdc, int cx) {
    int sw = GetSidebarWidth();

    // Title bar background — bg_root
    RECT tbRc = { 0, 0, cx, TITLEBAR_H };
    FillRect(hdc, &tbRc, Theme::BrushRoot());

    // Bottom border
    RECT tbBorder = { 0, TITLEBAR_H - 1, cx, TITLEBAR_H };
    FillRect(hdc, &tbBorder, Theme::BrushBorderSubtle());

    SetBkMode(hdc, TRANSPARENT);

    // App icon (gradient square with "T")
    RECT iconRc = { 14, 10, 32, 28 };
    Theme::DrawGradientButton(hdc, iconRc, 5, Theme::ACCENT_BLUE, Theme::ACCENT_CYAN);
    SetTextColor(hdc, Theme::BG_ROOT);
    HFONT old = (HFONT)SelectObject(hdc, Theme::FontCaption());
    DrawText(hdc, L"T", -1, &iconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // App name
    SetTextColor(hdc, Theme::TEXT_SECONDARY);
    SelectObject(hdc, Theme::FontNavActive());
    RECT nameRc = { 40, 0, 160, TITLEBAR_H };
    DrawText(hdc, L"Transparency", -1, &nameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Toolbar actions — centered after sidebar
    int tbX = sw + 20;
    auto drawToolBtn = [&](const wchar_t* icon, const wchar_t* label, COLORREF accent, int w) {
        RECT btnRc = { tbX, 7, tbX + w, TITLEBAR_H - 7 };
        bool hover = false; // simplified for now
        if (hover)
            Theme::DrawRoundedCard(hdc, btnRc, 6, Theme::BG_OVERLAY, Theme::BORDER_DEFAULT, 0);

        SetTextColor(hdc, Theme::TEXT_TERTIARY);
        SelectObject(hdc, Theme::FontCaption());
        RECT irc = { tbX + 8, 7, tbX + 22, TITLEBAR_H - 7 };
        SetTextColor(hdc, accent);
        DrawText(hdc, icon, -1, &irc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SetTextColor(hdc, Theme::TEXT_TERTIARY);
        RECT lrc = { tbX + 22, 7, tbX + w - 4, TITLEBAR_H - 7 };
        DrawText(hdc, label, -1, &lrc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        tbX += w + 8;
    };

    drawToolBtn(L"\u25B8", L"Quick", Theme::ACCENT_GREEN, 72);
    drawToolBtn(L"\u25B8\u25B8", L"Deep", Theme::ACCENT_BLUE, 72);

    // Separator
    RECT sepRc = { tbX, 11, tbX + 1, TITLEBAR_H - 11 };
    FillRect(hdc, &sepRc, Theme::BrushBorderDefault());
    tbX += 12;

    drawToolBtn(L"\u25A0", L"Stop", Theme::ACCENT_RED, 56);

    // Command palette trigger (center-right area)
    int cpX = cx - 280;
    if (cpX > tbX + 40) {
        RECT cpRc = { cpX, 8, cpX + 200, TITLEBAR_H - 8 };
        Theme::DrawRoundedCard(hdc, cpRc, 6, Theme::BG_ELEVATED, Theme::BORDER_DEFAULT);
        SetTextColor(hdc, Theme::TEXT_TERTIARY);
        SelectObject(hdc, Theme::FontCaption());
        RECT cpText = { cpX + 10, 8, cpX + 180, TITLEBAR_H - 8 };
        DrawText(hdc, L"Search devices, commands...", -1, &cpText, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Ctrl+K badge
        RECT badge = { cpX + 152, 12, cpX + 194, TITLEBAR_H - 12 };
        Theme::DrawRoundedCard(hdc, badge, 3, Theme::BG_ROOT, Theme::BORDER_DEFAULT);
        RECT badgeText = { cpX + 152, 12, cpX + 194, TITLEBAR_H - 12 };
        DrawText(hdc, L"Ctrl+K", -1, &badgeText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, old);
}

// ─── DrawNavSidebar ──────────────────────────────────────────────────────────

void MainWindow::DrawNavSidebar(HDC hdc, const RECT& rc) {
    int sw = GetSidebarWidth();

    // Sidebar background — bg_root
    RECT sidebarRc = { 0, TITLEBAR_H, sw, rc.bottom - STATUSBAR_H };
    FillRect(hdc, &sidebarRc, Theme::BrushRoot());

    // Right border
    RECT borderRc = { sw - 1, TITLEBAR_H, sw, rc.bottom - STATUSBAR_H };
    FillRect(hdc, &borderRc, Theme::BrushBorderSubtle());

    SetBkMode(hdc, TRANSPARENT);

    // Collapse toggle (top of sidebar)
    RECT toggleRc = { 0, TITLEBAR_H, sw, TITLEBAR_H + 32 };
    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontMono());
    RECT togText = { 14, TITLEBAR_H, sw, TITLEBAR_H + 32 };
    DrawText(hdc, _sidebarExpanded ? L"\u25C2" : L"\u25B8", -1, &togText,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldFont);

    // Nav items
    for (int i = 0; i < NAV_ITEM_COUNT; i++) {
        int btnY = TITLEBAR_H + NAV_BTN_TOP + i * NAV_BTN_HEIGHT;
        RECT btnRc = { 4, btnY, sw - 4, btnY + NAV_BTN_HEIGHT - 1 };

        bool active  = (_currentTab == NAV_ITEMS[i].tab);
        bool hovered = (_hoverNav == i && !active);

        if (active)
            Theme::DrawRoundedCard(hdc, btnRc, 6, Theme::BG_NAV_ACTIVE, Theme::BORDER_SUBTLE, 0);
        else if (hovered)
            Theme::DrawRoundedCard(hdc, btnRc, 6, Theme::BG_OVERLAY, Theme::BORDER_SUBTLE, 0);

        // Icon — 14px, centered in 20px container
        COLORREF iconCol = active ? Theme::ACCENT_BLUE : Theme::TEXT_TERTIARY;
        SetTextColor(hdc, iconCol);
        SelectObject(hdc, Theme::FontBody());
        int iconLeft = _sidebarExpanded ? 10 : (sw - 14) / 2;
        RECT iconRc = { btnRc.left + iconLeft, btnRc.top, btnRc.left + iconLeft + 20, btnRc.bottom };
        DrawText(hdc, NAV_ITEMS[i].icon, -1, &iconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Label (only when expanded)
        if (_sidebarExpanded) {
            COLORREF textCol = active ? Theme::TEXT_PRIMARY : hovered ? Theme::TEXT_PRIMARY : Theme::TEXT_TERTIARY;
            SetTextColor(hdc, textCol);
            SelectObject(hdc, active ? Theme::FontNavActive() : Theme::FontNavInactive());
            RECT labelRc = { btnRc.left + 36, btnRc.top, btnRc.right - 4, btnRc.bottom };
            DrawText(hdc, NAV_ITEMS[i].label, -1, &labelRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    // Bottom separator
    int botSepY = rc.bottom - STATUSBAR_H - 56;
    RECT botSep = { 8, botSepY, sw - 8, botSepY + 1 };
    FillRect(hdc, &botSep, Theme::BrushBorderSubtle());

    // Bottom nav: Settings
    int botBtnY = rc.bottom - STATUSBAR_H - 48;
    RECT botBtnRc = { 4, botBtnY, sw - 4, botBtnY + NAV_BTN_HEIGHT - 1 };
    bool settingsActive = (_currentTab == Tab::Privacy);
    if (settingsActive)
        Theme::DrawRoundedCard(hdc, botBtnRc, 6, Theme::BG_NAV_ACTIVE, Theme::BORDER_SUBTLE, 0);

    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    SelectObject(hdc, Theme::FontBody());
    int sIconLeft = _sidebarExpanded ? 10 : (sw - 14) / 2;
    RECT sIconRc = { botBtnRc.left + sIconLeft, botBtnRc.top, botBtnRc.left + sIconLeft + 20, botBtnRc.bottom };
    DrawText(hdc, L"\u2699", -1, &sIconRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (_sidebarExpanded) {
        SetTextColor(hdc, settingsActive ? Theme::TEXT_PRIMARY : Theme::TEXT_TERTIARY);
        SelectObject(hdc, settingsActive ? Theme::FontNavActive() : Theme::FontNavInactive());
        RECT sLblRc = { botBtnRc.left + 36, botBtnRc.top, botBtnRc.right - 4, botBtnRc.bottom };
        DrawText(hdc, L"Settings", -1, &sLblRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

// ─── DrawContentHeader ───────────────────────────────────────────────────────

void MainWindow::DrawContentHeader(HDC hdc, int cx, int cy) {
    int sw = GetSidebarWidth();
    int hdrTop = TITLEBAR_H;

    // Background — bg_surface
    RECT hdrRc = { sw, hdrTop, cx, hdrTop + CONTENT_HDR_H };
    FillRect(hdc, &hdrRc, Theme::BrushSurface());

    // Bottom border
    RECT hdrBorder = { sw, hdrTop + CONTENT_HDR_H - 1, cx, hdrTop + CONTENT_HDR_H };
    FillRect(hdc, &hdrBorder, Theme::BrushBorderSubtle());

    SetBkMode(hdc, TRANSPARENT);

    // View title — 13px/700
    SetTextColor(hdc, Theme::TEXT_PRIMARY);
    HFONT old = (HFONT)SelectObject(hdc, Theme::FontNavActive());
    const wchar_t* viewTitle = L"Dashboard";
    switch (_currentTab) {
    case Tab::Devices:  viewTitle = L"Devices";     break;
    case Tab::Alerts:   viewTitle = L"Alerts";      break;
    case Tab::Tools:    viewTitle = L"Diagnostics";  break;
    case Tab::Ledger:   viewTitle = L"Scan History"; break;
    case Tab::Privacy:  viewTitle = L"Settings";     break;
    case Tab::SmartHome:viewTitle = L"Topology";     break;
    default: break;
    }
    RECT titleRc = { sw + 20, hdrTop, sw + 200, hdrTop + CONTENT_HDR_H };
    DrawText(hdc, viewTitle, -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Item count — mono 11px (read count directly, no full copy)
    int itemCount = 0;
    {
        std::lock_guard<std::mutex> lk(_dataMutex);
        itemCount = (int)_lastResult.devices.size();
    }
    wchar_t countStr[32];
    swprintf_s(countStr, L"%d found", itemCount);
    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    SelectObject(hdc, Theme::FontMono());
    RECT countRc = { sw + 120, hdrTop, sw + 240, hdrTop + CONTENT_HDR_H };
    DrawText(hdc, countStr, -1, &countRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Right side: last scan time
    DWORD elapsed = _lastScanTick ? (GetTickCount() - _lastScanTick) / 1000 : 0;
    wchar_t scanStr[64];
    if (_lastScanTick == 0)
        wcscpy_s(scanStr, L"NO SCAN YET");
    else if (elapsed < 60)
        swprintf_s(scanStr, L"LAST SCAN: %ds AGO", elapsed);
    else
        swprintf_s(scanStr, L"LAST SCAN: %dm AGO", elapsed / 60);

    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    SelectObject(hdc, Theme::FontCaption());
    RECT scanRc = { cx - 220, hdrTop, cx - 30, hdrTop + CONTENT_HDR_H };
    DrawText(hdc, scanStr, -1, &scanRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // Monitoring dot (green pulsing circle)
    if (_monitorActive) {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        // Glow
        Gdiplus::SolidBrush glow(Theme::GdipColor(Theme::ACCENT_GREEN, 100));
        g.FillEllipse(&glow, cx - 24, hdrTop + 16, 8, 8);
        // Dot
        Gdiplus::SolidBrush dot(Theme::GdipColor(Theme::ACCENT_GREEN));
        g.FillEllipse(&dot, cx - 22, hdrTop + 18, 4, 4);
    }

    SelectObject(hdc, old);
}

// ─── DrawStatusBar ───────────────────────────────────────────────────────────

void MainWindow::DrawStatusBar(HDC hdc, int cx, int cy) {
    int sbTop = cy - STATUSBAR_H;

    // Background — bg_root
    RECT sbRc = { 0, sbTop, cx, cy };
    FillRect(hdc, &sbRc, Theme::BrushRoot());

    // Top border
    RECT sbBorder = { 0, sbTop, cx, sbTop + 1 };
    FillRect(hdc, &sbBorder, Theme::BrushBorderSubtle());

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    HFONT old = (HFONT)SelectObject(hdc, Theme::FontMono());

    int x = 14;

    // Monitoring status with dot
    if (_monitorActive) {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::SolidBrush dot(Theme::GdipColor(Theme::ACCENT_GREEN));
        g.FillEllipse(&dot, x, sbTop + 10, 5, 5);
        x += 10;
        RECT monRc = { x, sbTop, x + 120, cy };
        DrawText(hdc, L"Monitoring Active", -1, &monRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        x += 130;
    } else {
        RECT monRc = { x, sbTop, x + 80, cy };
        DrawText(hdc, L"Idle", -1, &monRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        x += 50;
    }

    // Network info — use cached data to avoid expensive calls during paint
    static wstring s_cachedNetInfo;
    static DWORD s_netCacheTick = 0;
    DWORD now = GetTickCount();
    if (now - s_netCacheTick > 10000 || s_cachedNetInfo.empty()) {
        auto nets = ScanEngine::GetLocalNetworks();
        s_cachedNetInfo = nets.empty() ? L"" : nets[0].localIp + L"/" + nets[0].cidr;
        s_netCacheTick = now;
    }
    if (!s_cachedNetInfo.empty()) {
        RECT netRc = { x, sbTop, x + 140, cy };
        DrawText(hdc, s_cachedNetInfo.c_str(), -1, &netRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        x += 150;
    }

    // Device + alert count — read counts directly (fast, no copy)
    int devCount = 0, anomCount = 0;
    {
        std::lock_guard<std::mutex> lk(_dataMutex);
        devCount = (int)_lastResult.devices.size();
        anomCount = (int)_lastResult.anomalies.size();
    }
    wchar_t devStr[32];
    swprintf_s(devStr, L"%d devices", devCount);
    RECT devRc = { x, sbTop, x + 80, cy };
    DrawText(hdc, devStr, -1, &devRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    x += 90;

    // Alert count
    wchar_t alertStr[32];
    swprintf_s(alertStr, L"%d alerts", anomCount);
    RECT alertRc = { x, sbTop, x + 70, cy };
    DrawText(hdc, alertStr, -1, &alertRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Right side: gateway — cached
    static wstring s_cachedGw;
    if (now - s_netCacheTick < 1000) {
        // Use same cache cycle — already refreshed above
    } else if (s_cachedGw.empty()) {
        auto nets2 = ScanEngine::GetLocalNetworks();
        if (!nets2.empty() && !nets2[0].gateway.empty())
            s_cachedGw = L"GW: " + nets2[0].gateway;
    }
    if (!s_cachedGw.empty()) {
        int rx = cx - 14;
        RECT gwRc = { rx - 160, sbTop, rx, cy };
        DrawText(hdc, s_cachedGw.c_str(), -1, &gwRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    SelectObject(hdc, old);
}

// ─── OnPaint ─────────────────────────────────────────────────────────────────

LRESULT MainWindow::OnPaint(HWND hwnd) {
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

    // Fill entire background
    FillRect(hdc, &rc, Theme::BrushRoot());

    // Draw the 4 shell zones
    DrawTitleBar(hdc, cx);
    DrawNavSidebar(hdc, rc);
    DrawContentHeader(hdc, cx, cy);
    DrawStatusBar(hdc, cx, cy);

    // Blit to screen
    BitBlt(hdcScreen, 0, 0, cx, cy, hdc, 0, 0, SRCCOPY);

    SelectObject(hdc, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdc);

    EndPaint(hwnd, &ps);
    return 0;
}

LRESULT MainWindow::OnLButtonUp(HWND hwnd, int x, int y) {
    return DefWindowProc(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
}

LRESULT MainWindow::OnNcHitTest(HWND hwnd, int x, int y) {
    return DefWindowProc(hwnd, WM_NCHITTEST, 0, MAKELPARAM(x, y));
}

// ─── OnDestroy ───────────────────────────────────────────────────────────────

LRESULT MainWindow::OnDestroy(HWND hwnd) {
    KillTimer(hwnd, 1);
    KillTimer(hwnd, 2);
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
        InvalidateRect(_hwnd, nullptr, FALSE);
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

// ─── MergeDeviceHistory ──────────────────────────────────────────────────────
// Carry forward persistent fields from previous scan by matching on MAC.
static void MergeDeviceHistory(ScanResult& result, const ScanResult& prev) {
    // Build MAC -> device map from previous
    std::map<wstring, const Device*> prevByMac;
    for (auto& d : prev.devices) {
        if (!d.mac.empty()) prevByMac[d.mac] = &d;
    }

    for (auto& dev : result.devices) {
        if (dev.mac.empty()) continue;
        auto it = prevByMac.find(dev.mac);
        if (it == prevByMac.end()) continue;
        const Device* pd = it->second;

        // Carry forward user data
        if (dev.customName.empty() && !pd->customName.empty()) dev.customName = pd->customName;
        if (dev.notes.empty() && !pd->notes.empty()) dev.notes = pd->notes;
        if (dev.trustState == L"unknown" && pd->trustState != L"unknown") dev.trustState = pd->trustState;
        dev.tags = pd->tags;
        dev.location = pd->location;

        // Carry forward temporal data
        if (!pd->firstSeen.empty()) dev.firstSeen = pd->firstSeen;
        dev.prevHostname = pd->hostname;
        dev.prevPorts = pd->openPorts;
        dev.sightingCount = pd->sightingCount + 1;

        // Build IP history
        dev.ipHistory = pd->ipHistory;
        if (pd->ip != dev.ip && !pd->ip.empty()) {
            // Add old IP if not already tracked
            bool found = false;
            for (auto& h : dev.ipHistory) { if (h == pd->ip) { found = true; break; } }
            if (!found) {
                dev.ipHistory.push_back(pd->ip);
                if (dev.ipHistory.size() > 10) dev.ipHistory.erase(dev.ipHistory.begin());
            }
        }

        // Carry forward latency history
        dev.latencyHistory = pd->latencyHistory;
        if (dev.latencyMs >= 0) {
            dev.latencyHistory.push_back(dev.latencyMs);
            if (dev.latencyHistory.size() > 7)
                dev.latencyHistory.erase(dev.latencyHistory.begin());
        }
    }
}

void MainWindow::StartQuickScan() {
    AddLedgerEntry(L"Scan Started", L"Quick scan initiated");

    ScanResult prevResult;
    {
        std::lock_guard<std::mutex> lk(_dataMutex);
        prevResult = _lastResult;
    }

    auto* hwnd = _hwnd;
    std::thread([this, hwnd, prevResult]() {
        auto future = _scanner.QuickScan([hwnd](int pct, std::wstring msg) {
            wstring* msgPtr = new wstring(msg);
            PostMessage(hwnd, WM_SCAN_PROGRESS, (WPARAM)pct, (LPARAM)msgPtr);
        });

        ScanResult* result = new ScanResult(future.get());
        MergeDeviceHistory(*result, prevResult);
        if (!prevResult.devices.empty())
            result->anomalies = ScanEngine::AnalyzeAnomalies(*result, prevResult);
        {
            std::lock_guard<std::mutex> lk(_dataMutex);
            _previousResult = prevResult;
            _lastResult = *result;
        }
        PostMessage(hwnd, WM_SCAN_COMPLETE, 0, (LPARAM)result);
    }).detach();
}

void MainWindow::StartStandardScan() {
    AddLedgerEntry(L"Scan Started", L"Standard scan initiated");

    ScanResult prevResult;
    {
        std::lock_guard<std::mutex> lk(_dataMutex);
        prevResult = _lastResult;
    }

    auto* hwnd = _hwnd;
    std::thread([this, hwnd, prevResult]() {
        auto future = _scanner.StandardScan([hwnd](int pct, std::wstring msg) {
            wstring* msgPtr = new wstring(msg);
            PostMessage(hwnd, WM_SCAN_PROGRESS, (WPARAM)pct, (LPARAM)msgPtr);
        });

        ScanResult* result = new ScanResult(future.get());
        MergeDeviceHistory(*result, prevResult);
        if (!prevResult.devices.empty())
            result->anomalies = ScanEngine::AnalyzeAnomalies(*result, prevResult);
        {
            std::lock_guard<std::mutex> lk(_dataMutex);
            _previousResult = prevResult;
            _lastResult = *result;
        }
        PostMessage(hwnd, WM_SCAN_COMPLETE, 0, (LPARAM)result);
    }).detach();
}

void MainWindow::StartDeepScan() {
    AddLedgerEntry(L"Scan Started", L"Deep scan initiated");

    ScanResult prevResult;
    {
        std::lock_guard<std::mutex> lk(_dataMutex);
        prevResult = _lastResult;
    }

    auto* hwnd = _hwnd;
    std::thread([this, hwnd, prevResult]() {
        auto future = _scanner.DeepScan([hwnd](int pct, std::wstring msg) {
            wstring* msgPtr = new wstring(msg);
            PostMessage(hwnd, WM_SCAN_PROGRESS, (WPARAM)pct, (LPARAM)msgPtr);
        });

        ScanResult* result = new ScanResult(future.get());
        MergeDeviceHistory(*result, prevResult);
        if (!prevResult.devices.empty())
            result->anomalies = ScanEngine::AnalyzeAnomalies(*result, prevResult);
        {
            std::lock_guard<std::mutex> lk(_dataMutex);
            _previousResult = prevResult;
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
            body = "{\"status\":\"ok\",\"version\":\"4.1.0\"}\r\n";
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
                      + ",\"classificationReason\":\"" + jsonEsc(wstr2utf8(d.classificationReason)) + "\""
                      + ",\"subnet\":\"" + jsonEsc(wstr2utf8(d.subnet)) + "\""
                      + ",\"firstSeen\":\"" + jsonEsc(wstr2utf8(d.firstSeen)) + "\""
                      + ",\"sightings\":" + std::to_string(d.sightingCount)
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
    _lastScanTick = GetTickCount();
    InvalidateRect(hwnd, nullptr, FALSE);

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
