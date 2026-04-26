#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
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
    LRESULT OnVScroll(HWND hwnd, WPARAM wp);
    LRESULT OnMouseWheel(HWND hwnd, int delta);

    void CreateControls(HWND hwnd, int cx, int cy);
    void LayoutControls(int cx, int cy);
    void UpdateScrollBar(HWND hwnd);

    void PopulateSmartDevices();
    void PopulateSecurityLog();

    // Home Assistant (local REST API — URL + long-lived token)
    void HaConnect();
    void HaSync();
    void HaAppendLog(const std::wstring& text);

    // Philips Hue (local bridge — discover, pair, sync)
    void HueDiscover();
    void HuePair();
    void HueSync();
    void HueAppendLog(const std::wstring& text);

    // HTTP helpers
    static std::string  WideToUtf8(const std::wstring& w);
    static std::wstring Utf8ToWide(const std::string& s);
    static std::string  HttpGetJson(const std::wstring& fullUrl,
                                    const std::wstring& bearerToken = L"");
    static std::string  HttpPostLocal(const std::wstring& host, INTERNET_PORT port,
                                      const std::wstring& path, const std::string& json);
    static std::string  HttpPostJson(const std::wstring& host, const std::wstring& path,
                                     const std::string& json, const std::wstring& bearerToken);
    static std::string  JsonExtract(const std::string& json, const std::string& key);

    // Amazon Alexa (local — no cloud account)
    void AlexaRefresh();

    // Google Home / Cast (local — no cloud account)
    void GoogleSync();
    void GoogleAppendLog(const std::wstring& text);

    // Smart device helpers
    static std::wstring SmartPlatform(const Device& d);
    static std::wstring SecurityNote(const Device& d);
    static bool         IsSmartDevice(const Device& d);
    static bool         IsAlexaDevice(const Device& d);
    static bool         IsGoogleDevice(const Device& d);

    HWND _hwnd    = nullptr;
    MainWindow* _mainWnd = nullptr;

    // Scroll
    int _scrollY      = 0;
    int _contentHeight = 0;
    int _viewHeight   = 0;

    // Smart device list
    HWND _hDeviceList = nullptr;

    // Home Assistant
    HWND _hHaStatus      = nullptr;
    HWND _hHaUrl         = nullptr;
    HWND _hHaToken       = nullptr;
    HWND _hBtnHaConnect  = nullptr;
    HWND _hBtnHaSync     = nullptr;
    HWND _hHaLog         = nullptr;
    std::wstring _haBaseUrl;
    std::wstring _haToken;

    // Philips Hue
    HWND _hHueStatus       = nullptr;
    HWND _hHueBridgeIp     = nullptr;
    HWND _hBtnHueDiscover  = nullptr;
    HWND _hBtnHuePair      = nullptr;
    HWND _hBtnHueSync      = nullptr;
    HWND _hHueLog          = nullptr;
    std::wstring _hueBridgeIp;
    std::wstring _hueUsername;

    // Amazon Alexa
    HWND _hAlexaList    = nullptr;

    // Google Home
    HWND _hGoogleList   = nullptr;
    HWND _hGoogleLog    = nullptr;

    // Device security log
    HWND _hSecurityLog = nullptr;

    // Automation triggers
    HWND _hTriggerList        = nullptr;
    HWND _hBtnAddTrigger      = nullptr;
    HWND _hBtnDelTrigger      = nullptr;
    HWND _hComboTriggerEvent  = nullptr;
    HWND _hComboTriggerAction = nullptr;

    static const wchar_t* s_className;
};
