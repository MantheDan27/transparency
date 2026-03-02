#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "Models.h"
#include "Resource.h"
#include "Theme.h"

class MainWindow;

class TabOverview {
public:
    TabOverview() = default;
    ~TabOverview() = default;

    bool Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd);
    HWND GetHwnd() const { return _hwnd; }

    void UpdateFromResult(const ScanResult& result);
    void UpdateMonitorStatus(bool running, const InternetStatus& is);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnCreate(HWND hwnd, LPCREATESTRUCT cs);
    LRESULT OnSize(HWND hwnd, int cx, int cy);
    LRESULT OnPaint(HWND hwnd);
    LRESULT OnCommand(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnScanProgress(HWND hwnd, WPARAM pct, LPARAM msgPtr);
    LRESULT OnScanComplete(HWND hwnd);
    LRESULT OnMonitorTick(HWND hwnd);
    LRESULT OnDrawItem(HWND hwnd, DRAWITEMSTRUCT* dis);

    void CreateControls(HWND hwnd, int cx, int cy);
    void LayoutControls(int cx, int cy);
    void RefreshKPIs();
    void RefreshNetworkInfo();

    HWND _hwnd = nullptr;
    MainWindow* _mainWnd = nullptr;

    // KPI tiles
    HWND _hKpi[4] = {};
    HWND _hKpiLabel[4] = {};

    // Scan mode pills
    HWND _hModeQuick = nullptr, _hModeStandard = nullptr, _hModeDeep = nullptr;
    HWND _hCheckGentle = nullptr;

    // Action buttons
    HWND _hBtnQuickScan = nullptr;
    HWND _hBtnDeepScan  = nullptr;
    HWND _hBtnMonStart  = nullptr;
    HWND _hBtnMonStop   = nullptr;
    HWND _hBtnExport    = nullptr;

    // Status text
    HWND _hStatusText = nullptr;
    HWND _hProgressBar = nullptr;

    // Network info static
    HWND _hNetworkInfo = nullptr;

    // Recent changes list
    HWND _hChangesList = nullptr;

    // Monitor status card
    HWND _hMonitorCard = nullptr;
    HWND _hMonitorStatus = nullptr;
    HWND _hInternetStatus = nullptr;

    // Current scan mode
    int _scanMode = 0; // 0=Quick, 1=Standard, 2=Deep

    static const wchar_t* s_className;
};
