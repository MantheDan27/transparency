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
    LRESULT OnVScroll(HWND hwnd, WPARAM wp);
    LRESULT OnMouseWheel(HWND hwnd, int delta);

    void CreateControls(HWND hwnd, int cx, int cy);
    void LayoutControls(int cx, int cy);
    void UpdateScrollBar(HWND hwnd);
    void PopulateSmartDevices();

    // Alexa token retrieval
    void AlexaOpenAuth();
    void AlexaExchangeToken();
    void AlexaRefreshToken();
    static std::string WideToUtf8(const std::wstring& w);
    static std::wstring Utf8ToWide(const std::string& s);
    static std::string HttpPost(const std::wstring& host, const std::wstring& path,
                                const std::string& body);
    static std::string HttpPostJson(const std::wstring& host, const std::wstring& path,
                                    const std::string& json, const std::wstring& bearerToken);
    void AppendTokenLog(const std::wstring& text);

    // Alexa Smart Home Skill API
    void AlexaSHSendDiscovery();
    void AlexaSHSendStateReport();
    void AlexaSHSendChangeReport();
    void AppendSHLog(const std::wstring& text);
    std::wstring GetAlexaEventGatewayHost() const;
    std::string BuildDiscoveryPayload();
    std::string BuildStateReportPayload(const std::wstring& endpointId,
                                        const std::wstring& ip, bool online);
    std::string BuildChangeReportPayload(const std::wstring& endpointId,
                                         const std::wstring& ip, bool online);
    static std::string GenerateMessageId();

    // Google Home OAuth & Home Graph API
    void GoogleOpenAuth();
    void GoogleExchangeToken();
    void GoogleRefreshToken();
    void AppendGoogleTokenLog(const std::wstring& text);
    void GoogleHGRequestSync();
    void GoogleHGQueryDevices();
    void GoogleHGDisconnect();
    void AppendGoogleHGLog(const std::wstring& text);
    std::string BuildHomeGraphSyncPayload();
    std::string BuildHomeGraphQueryPayload(const std::wstring& endpointId);

    HWND _hwnd = nullptr;
    MainWindow* _mainWnd = nullptr;

    // Scrolling state
    int _scrollY = 0;
    int _contentHeight = 0;
    int _viewHeight = 0;

    // Smart device list
    HWND _hDeviceList = nullptr;

    // Integration status
    HWND _hAlexaStatus = nullptr;
    HWND _hGoogleStatus = nullptr;

    // Alexa controls
    HWND _hBtnAlexaLink = nullptr;
    HWND _hBtnAlexaDiscover = nullptr;
    HWND _hAlexaOut = nullptr;

    // Alexa Access Token Retrieval
    HWND _hAlexaClientId = nullptr;
    HWND _hAlexaClientSecret = nullptr;
    HWND _hAlexaAuthCode = nullptr;
    HWND _hAlexaRedirectUri = nullptr;
    HWND _hBtnAlexaOpenAuth = nullptr;
    HWND _hBtnAlexaGetToken = nullptr;
    HWND _hBtnAlexaRefresh = nullptr;
    HWND _hAlexaTokenOut = nullptr;

    // Stored tokens
    std::wstring _alexaAccessToken;
    std::wstring _alexaRefreshToken;

    // Alexa Smart Home Skill API controls
    HWND _hAlexaSHRegion = nullptr;
    HWND _hBtnAlexaSHDiscover = nullptr;
    HWND _hBtnAlexaSHState = nullptr;
    HWND _hBtnAlexaSHChange = nullptr;
    HWND _hAlexaSHLog = nullptr;

    // Google Home controls
    HWND _hBtnGoogleLink = nullptr;
    HWND _hBtnGoogleDiscover = nullptr;
    HWND _hGoogleOut = nullptr;

    // Google Home OAuth & Home Graph API
    HWND _hGoogleClientId = nullptr;
    HWND _hGoogleClientSecret = nullptr;
    HWND _hGoogleAuthCode = nullptr;
    HWND _hGoogleRedirectUri = nullptr;
    HWND _hBtnGoogleOpenAuth = nullptr;
    HWND _hBtnGoogleGetToken = nullptr;
    HWND _hBtnGoogleRefresh = nullptr;
    HWND _hGoogleTokenOut = nullptr;

    // Google Home Graph API controls
    HWND _hGoogleProjectId = nullptr;
    HWND _hBtnGoogleHGSync = nullptr;
    HWND _hBtnGoogleHGQuery = nullptr;
    HWND _hBtnGoogleHGDisconnect = nullptr;
    HWND _hGoogleHGLog = nullptr;

    // Stored Google tokens
    std::wstring _googleAccessToken;
    std::wstring _googleRefreshToken;

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
