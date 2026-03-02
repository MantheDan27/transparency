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

class TabAlerts {
public:
    TabAlerts() = default;
    ~TabAlerts() = default;

    bool Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd);
    HWND GetHwnd() const { return _hwnd; }

    void RefreshAlerts();
    void RefreshRules();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnCreate(HWND hwnd, LPCREATESTRUCT cs);
    LRESULT OnSize(HWND hwnd, int cx, int cy);
    LRESULT OnPaint(HWND hwnd);
    LRESULT OnCommand(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnNotify(HWND hwnd, NMHDR* hdr);
    LRESULT OnScanComplete(HWND hwnd);

    void CreateControls(HWND hwnd, int cx, int cy);
    void LayoutControls(int cx, int cy);
    void ShowRuleDialog(const AlertRule* existing = nullptr);
    void PopulateAlerts();
    void PopulateRules();

    HWND _hwnd = nullptr;
    MainWindow* _mainWnd = nullptr;

    HWND _hAlertList = nullptr;
    HWND _hRuleList  = nullptr;
    HWND _hBtnAddRule = nullptr;
    HWND _hBtnEditRule = nullptr;
    HWND _hBtnDelRule = nullptr;
    HWND _hBtnClearAll = nullptr;
    HWND _hFilterBtns[5] = {};

    int _alertFilter = 0; // 0=All, 1=High, 2=Medium, 3=Low, 4=Unack

    static const wchar_t* s_className;

    // Rule builder dialog
    static INT_PTR CALLBACK RuleDlgProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp);
};
