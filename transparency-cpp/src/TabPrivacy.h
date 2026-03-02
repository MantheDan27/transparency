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

class TabPrivacy {
public:
    TabPrivacy() = default;
    ~TabPrivacy() = default;

    bool Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd);
    HWND GetHwnd() const { return _hwnd; }

    void Refresh();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnCreate(HWND hwnd, LPCREATESTRUCT cs);
    LRESULT OnSize(HWND hwnd, int cx, int cy);
    LRESULT OnPaint(HWND hwnd);
    LRESULT OnCommand(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnDrawItem(HWND hwnd, DRAWITEMSTRUCT* dis);

    void CreateControls(HWND hwnd, int cx, int cy);
    void LayoutControls(int cx, int cy);
    void RefreshStats();
    void SaveConfig();
    void LoadConfig();

    HWND _hwnd = nullptr;
    MainWindow* _mainWnd = nullptr;

    // Stats
    HWND _hStatsDevice = nullptr;
    HWND _hStatsAlerts = nullptr;
    HWND _hStatsLedger = nullptr;

    // Data deletion
    HWND _hBtnDeleteAll = nullptr;
    HWND _hPurgeDays = nullptr;
    HWND _hBtnPurge = nullptr;

    // Monitor config
    HWND _hMonInterval = nullptr;
    HWND _hMonQuietStart = nullptr;
    HWND _hMonQuietEnd = nullptr;
    HWND _hChkAlertOutage = nullptr;
    HWND _hChkAlertGateway = nullptr;
    HWND _hChkAlertDns = nullptr;
    HWND _hChkAlertLatency = nullptr;
    HWND _hLatencyThresh = nullptr;
    HWND _hBtnSaveConfig = nullptr;

    // Export/Import
    HWND _hBtnExport = nullptr;
    HWND _hBtnImport = nullptr;

    // API info
    HWND _hApiInfo = nullptr;

    static const wchar_t* s_className;
};
