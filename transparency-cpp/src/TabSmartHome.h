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

class TabSmartHome {
public:
    TabSmartHome() = default;
    ~TabSmartHome() = default;

    bool Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd);
    HWND GetHwnd() const { return _hwnd; }

    void RefreshDevices();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnCreate(HWND hwnd, LPCREATESTRUCT cs);
    LRESULT OnSize(HWND hwnd, int cx, int cy);
    LRESULT OnPaint(HWND hwnd);
    LRESULT OnCommand(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnNotify(HWND hwnd, NMHDR* hdr);

    void CreateControls(HWND hwnd, int cx, int cy);
    void LayoutControls(int cx, int cy);
    void PopulateSmartDevices();

    HWND _hwnd = nullptr;
    MainWindow* _mainWnd = nullptr;

    // Smart device list
    HWND _hDeviceList = nullptr;

    // Integration status
    HWND _hAlexaStatus = nullptr;
    HWND _hGoogleStatus = nullptr;

    // Alexa controls
    HWND _hBtnAlexaLink = nullptr;
    HWND _hBtnAlexaDiscover = nullptr;
    HWND _hAlexaOut = nullptr;

    // Google Home controls
    HWND _hBtnGoogleLink = nullptr;
    HWND _hBtnGoogleDiscover = nullptr;
    HWND _hGoogleOut = nullptr;

    // Automation / triggers
    HWND _hTriggerList = nullptr;
    HWND _hBtnAddTrigger = nullptr;
    HWND _hBtnDelTrigger = nullptr;
    HWND _hComboTriggerEvent = nullptr;
    HWND _hComboTriggerAction = nullptr;
    HWND _hComboTriggerDevice = nullptr;

    // Scene controls
    HWND _hSceneList = nullptr;
    HWND _hBtnAddScene = nullptr;
    HWND _hBtnRunScene = nullptr;
    HWND _hEditSceneName = nullptr;

    static const wchar_t* s_className;
};
