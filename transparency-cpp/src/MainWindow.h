#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "Models.h"
#include "Scanner.h"
#include "Monitor.h"
#include "Resource.h"
#include "Theme.h"

// Forward declare tab panel classes
class TabOverview;
class TabDevices;
class TabAlerts;
class TabTools;
class TabLedger;
class TabPrivacy;

enum class Tab {
    Overview,
    Devices,
    Alerts,
    Tools,
    Ledger,
    Privacy,
    COUNT
};

class MainWindow {
public:
    static MainWindow* s_instance;

    static bool Create(HINSTANCE hInstance);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    HWND GetHwnd() const { return _hwnd; }

    // Called from tab panels
    void StartQuickScan();
    void StartStandardScan();
    void StartDeepScan();
    void StartMonitor();
    void StopMonitor();
    void SwitchTab(Tab tab);

    ScanResult GetLastResult() const;
    void AddLedgerEntry(const std::wstring& action, const std::wstring& details);

    ScanEngine  _scanner;
    Monitor     _monitor;

    ScanResult              _lastResult;
    std::vector<AlertRule>  _alertRules;
    std::vector<LedgerEntry> _ledger;
    mutable std::mutex      _dataMutex;

    Tab _currentTab = Tab::Overview;

private:
    HWND _hwnd = nullptr;
    HINSTANCE _hInstance = nullptr;

    // Child windows
    HWND _hSidebar = nullptr;

    // Sidebar nav buttons
    HWND _hNavBtns[(int)Tab::COUNT] = {};

    // Tab panels (child windows)
    std::unique_ptr<TabOverview> _tabOverview;
    std::unique_ptr<TabDevices>  _tabDevices;
    std::unique_ptr<TabAlerts>   _tabAlerts;
    std::unique_ptr<TabTools>    _tabTools;
    std::unique_ptr<TabLedger>   _tabLedger;
    std::unique_ptr<TabPrivacy>  _tabPrivacy;

    // Status bar
    HWND _hStatus = nullptr;

    // Message handlers
    LRESULT OnCreate(HWND hwnd, LPCREATESTRUCT cs);
    LRESULT OnSize(HWND hwnd, int cx, int cy);
    LRESULT OnPaint(HWND hwnd);
    LRESULT OnDestroy(HWND hwnd);
    LRESULT OnCommand(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnDrawItem(HWND hwnd, DRAWITEMSTRUCT* dis);
    LRESULT OnScanComplete(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnScanProgress(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnMonitorTick(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnInternetStatus(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnGatewayChanged(HWND hwnd, WPARAM wp, LPARAM lp);

    void CreateSidebar(HWND parent, int width, int height);
    void LayoutChildren(int cx, int cy);
    void ShowActivePanel();
    void SetNavButtonFont();
    static void ApplyDarkTheme(HWND hwnd);

    static const int SIDEBAR_WIDTH = 210;
    static const int NAV_BTN_HEIGHT = 46;
    static const int NAV_BTN_TOP = 70;
};
