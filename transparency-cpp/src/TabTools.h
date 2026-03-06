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

class TabTools {
public:
    TabTools() = default;
    ~TabTools() = default;

    bool Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd);
    HWND GetHwnd() const { return _hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT OnCreate(HWND hwnd, LPCREATESTRUCT cs);
    LRESULT OnSize(HWND hwnd, int cx, int cy);
    LRESULT OnPaint(HWND hwnd);
    LRESULT OnCommand(HWND hwnd, WPARAM wp, LPARAM lp);
    LRESULT OnToolResult(HWND hwnd, WPARAM wp, LPARAM lp);

    void CreateControls(HWND hwnd, int cx, int cy);
    void LayoutControls(int cx, int cy);

    void RunPing(const std::wstring& target, int count);
    void RunTraceroute(const std::wstring& target);
    void RunDnsLookup(const std::wstring& host, const std::wstring& type);
    void RunTcpConnect(const std::wstring& host, int port);
    void RunHttpTest(const std::wstring& url);
    void RunPortScan(const std::wstring& target, const std::wstring& preset, const std::wstring& custom);
    void RunWakeOnLan(const std::wstring& mac, const std::wstring& broadcast);
    void RunReverseDns(const std::wstring& ip);
    void RefreshWifiInfo();
    void RefreshGatewayInfo();
    void AppendOutput(HWND hEdit, const std::wstring& text);
    void ShowGuidedFlow(int idx);

    HWND _hwnd = nullptr;
    MainWindow* _mainWnd = nullptr;

    // Ping
    HWND _hPingTarget = nullptr, _hPingCount = nullptr;
    HWND _hBtnPing = nullptr, _hPingOut = nullptr;

    // Traceroute
    HWND _hTraceTarget = nullptr;
    HWND _hBtnTrace = nullptr, _hTraceOut = nullptr;

    // DNS
    HWND _hDnsHost = nullptr, _hDnsType = nullptr;
    HWND _hBtnDns = nullptr, _hDnsOut = nullptr;

    // TCP Connect
    HWND _hTcpHost = nullptr, _hTcpPort = nullptr;
    HWND _hBtnTcp = nullptr, _hTcpOut = nullptr;

    // HTTP Test
    HWND _hHttpUrl = nullptr;
    HWND _hBtnHttp = nullptr, _hHttpOut = nullptr;

    // Wi-Fi info
    HWND _hWifiInfo = nullptr;

    // Gateway info
    HWND _hGwInfo = nullptr;

    // Guided flow
    HWND _hFlowOut = nullptr;

    // Port Scanner
    HWND _hPortScanTarget = nullptr;
    HWND _hPortScanPreset = nullptr;  // combo: common / top100 / custom
    HWND _hPortScanCustom = nullptr;  // custom range edit
    HWND _hBtnPortScan    = nullptr;
    HWND _hPortScanOut    = nullptr;

    // Wake-on-LAN
    HWND _hWolMac       = nullptr;
    HWND _hWolBcast     = nullptr;
    HWND _hBtnWol       = nullptr;
    HWND _hWolOut       = nullptr;

    // Reverse DNS
    HWND _hRevDnsIp  = nullptr;
    HWND _hBtnRevDns = nullptr;
    HWND _hRevDnsOut = nullptr;

    static const wchar_t* s_className;
};
