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
class TabSmartHome;

enum class Tab {
    Overview,
    Devices,
    Alerts,
    Tools,
    Ledger,
    Privacy,
    SmartHome,
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
    void SaveSnapshot();
    void StartLocalApi();
    void StopLocalApi();
    void FirePluginHooks(const std::wstring& eventType, const std::wstring& deviceIp);
    static DWORD WINAPI LocalApiThreadProc(LPVOID param);

    ScanEngine  _scanner;
    Monitor     _monitor;

    ScanResult              _lastResult;
    ScanResult              _previousResult;
    std::vector<AlertRule>  _alertRules;
    std::vector<LedgerEntry> _ledger;
    std::vector<ScanResult> _snapshots;
    std::vector<PluginHook> _pluginHooks;
    ScheduledScan           _scheduledScan;
    std::vector<NicPreference> _nicPrefs;
    std::wstring            _selectedNicName;
    mutable std::mutex      _dataMutex;

    // REST API
    bool     _apiEnabled  = false;
    int      _apiPort     = 7722;
    wstring  _apiKey;
    HANDLE   _apiThread   = nullptr;

    Tab _currentTab = Tab::Overview;

    // Layout state
    bool _sidebarExpanded = true;
    bool _monitorActive   = false;

private:
    HWND _hwnd = nullptr;
    HINSTANCE _hInstance = nullptr;

    // Tab panels (child windows)
    std::unique_ptr<TabOverview> _tabOverview;
    std::unique_ptr<TabDevices>  _tabDevices;
    std::unique_ptr<TabAlerts>   _tabAlerts;
    std::unique_ptr<TabTools>    _tabTools;
    std::unique_ptr<TabLedger>   _tabLedger;
    std::unique_ptr<TabPrivacy>  _tabPrivacy;
    std::unique_ptr<TabSmartHome> _tabSmartHome;

    // Hover tracking
    int  _hoverNav       = -1;
    int  _hoverTitleBtn  = -1;  // title bar button hover
    int  _hoverWinBtn    = -1;  // window control hover (-1=none, 0=min, 1=max, 2=close)
    bool _trackingMouse  = false;

    // Last scan timestamp for content header
    DWORD _lastScanTick  = 0;

    // Message handlers
    LRESULT OnCreate(HWND hwnd, LPCREATESTRUCT cs);
    LRESULT OnSize(HWND hwnd, int cx, int cy);
    LRESULT OnPaint(HWND hwnd);
    LRESULT OnDestroy(HWND hwnd);
    LRESULT OnCommand(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnLButtonDown(HWND hwnd, int x, int y);
    LRESULT OnLButtonUp(HWND hwnd, int x, int y);
    LRESULT OnMouseMove(HWND hwnd, int x, int y);
    LRESULT OnScanComplete(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnScanProgress(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnMonitorTick(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnInternetStatus(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnGatewayChanged(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnNcHitTest(HWND hwnd, int x, int y);

    void LayoutChildren(int cx, int cy);
    void ShowActivePanel();

    // Drawing
    void DrawTitleBar(HDC hdc, int cx);
    void DrawNavSidebar(HDC hdc, const RECT& rc);
    void DrawContentHeader(HDC hdc, int cx, int cy);
    void DrawStatusBar(HDC hdc, int cx, int cy);

    // Layout constants — layout architecture spec
    static const int TITLEBAR_H     = 38;
    static const int CONTENT_HDR_H  = 40;
    static const int STATUSBAR_H    = 26;
    static const int SIDEBAR_W_EXPANDED  = 200;
    static const int SIDEBAR_W_COLLAPSED = 52;
    static const int NAV_BTN_HEIGHT = 36;
    static const int NAV_BTN_TOP    = 44;   // below collapse toggle

    int GetSidebarWidth() const { return _sidebarExpanded ? SIDEBAR_W_EXPANDED : SIDEBAR_W_COLLAPSED; }
    int GetContentTop() const { return TITLEBAR_H; }
    int GetContentBottom(int cy) const { return cy - STATUSBAR_H; }

    // Nav item labels
    static const wchar_t* NavLabel(int i);
    static const wchar_t* NavIcon(int i);
};
