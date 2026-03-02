#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windns.h>
#include <wlanapi.h>
#include <mmsystem.h>
#include <commdlg.h>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "winmm.lib")

#include "TabTools.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"
#include "Scanner.h"

using std::wstring;

const wchar_t* TabTools::s_className = L"TransparencyTabTools";

static wstring ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return L"";
    wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

bool TabTools::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
    _mainWnd = mainWnd;

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.hbrBackground = Theme::BrushApp();
    wc.lpszClassName = s_className;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    RegisterClassEx(&wc);

    _hwnd = CreateWindowEx(0, s_className, nullptr,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VSCROLL,
        x, y, w, h, parent, nullptr, GetModuleHandle(nullptr), this);

    return _hwnd != nullptr;
}

LRESULT CALLBACK TabTools::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TabTools* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<TabTools*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TabTools*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:     return self->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lp));
    case WM_SIZE:       self->OnSize(hwnd, LOWORD(lp), HIWORD(lp)); return 0;
    case WM_PAINT:      return self->OnPaint(hwnd);
    case WM_ERASEBKGND: { RECT rc; GetClientRect(hwnd,&rc); FillRect((HDC)wp,&rc,Theme::BrushApp()); return 1; }
    case WM_COMMAND:    return self->OnCommand(hwnd, wp, lp);
    case WM_TOOL_RESULT: return self->OnToolResult(hwnd, wp, lp);
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        SetTextColor((HDC)wp, Theme::TEXT_PRIMARY);
        SetBkColor((HDC)wp, Theme::BG_APP);
        return (LRESULT)Theme::BrushApp();
    }
    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
}

LRESULT TabTools::OnCreate(HWND hwnd, LPCREATESTRUCT cs) {
    RECT rc; GetClientRect(hwnd, &rc);
    CreateControls(hwnd, rc.right, rc.bottom);
    RefreshWifiInfo();
    RefreshGatewayInfo();
    return 0;
}

static HWND MakeSection(HWND parent, const wchar_t* title, int y, int cx, HINSTANCE hInst) {
    HWND hw = CreateWindowEx(0, L"STATIC", title,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, y, cx - 32, 18, parent, nullptr, hInst, nullptr);
    SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBold(), TRUE);
    return hw;
}

void TabTools::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    auto mkEdit = [&](const wchar_t* hint, int id, int x, int y, int w, int h, bool multi = false) -> HWND {
        HWND hw = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | (multi ? ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL : 0),
            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)(multi ? Theme::FontMono() : Theme::FontBody()), TRUE);
        if (hint && !multi) SendMessage(hw, EM_SETCUEBANNER, FALSE, (LPARAM)hint);
        Theme::ApplyDarkEdit(hw);
        return hw;
    };
    auto mkBtn = [&](const wchar_t* text, int id, int x, int y, int w, int h) -> HWND {
        HWND hw = CreateWindowEx(0, L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return hw;
    };
    auto mkLbl = [&](const wchar_t* t, int x, int y, int w) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
            x, y, w, 18, hwnd, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
        return hw;
    };

    int y = 10;

    // ── Guided Flows ──────────────────────────────────────────────────────────
    MakeSection(hwnd, L"Guided Troubleshooting", y, cx, hInst);
    y += 22;

    const wchar_t* flows[] = { L"Device is offline", L"Internet is slow", L"Unknown device appeared", L"Printer won't print" };
    int btnX = 16;
    for (int i = 0; i < 4; i++) {
        mkBtn(flows[i], 9700 + i, btnX, y, 180, 26);
        btnX += 184;
    }
    y += 32;

    _hFlowOut = mkEdit(nullptr, 9710, 16, y, cx - 32, 60, true);
    y += 68;

    // ── Ping ─────────────────────────────────────────────────────────────────
    MakeSection(hwnd, L"Ping", y, cx, hInst);
    y += 22;
    mkLbl(L"Target:", 16, y + 3, 50);
    _hPingTarget = mkEdit(L"hostname or IP", IDC_EDIT_PING_TARGET, 70, y, 220, 24);
    mkLbl(L"Count:", 300, y + 3, 44);
    _hPingCount = mkEdit(L"4", IDC_EDIT_PING_COUNT, 348, y, 40, 24);
    SetWindowText(_hPingCount, L"4");
    _hBtnPing = mkBtn(L"Run", IDC_BTN_PING_RUN, 396, y, 60, 24);
    y += 30;
    _hPingOut = mkEdit(nullptr, IDC_EDIT_PING_OUTPUT, 16, y, cx - 32, 80, true);
    y += 88;

    // ── Traceroute ───────────────────────────────────────────────────────────
    MakeSection(hwnd, L"Traceroute", y, cx, hInst);
    y += 22;
    mkLbl(L"Target:", 16, y + 3, 50);
    _hTraceTarget = mkEdit(L"hostname or IP", IDC_EDIT_TRACE_TARGET, 70, y, 300, 24);
    _hBtnTrace = mkBtn(L"Run", IDC_BTN_TRACE_RUN, 378, y, 60, 24);
    y += 30;
    _hTraceOut = mkEdit(nullptr, IDC_EDIT_TRACE_OUTPUT, 16, y, cx - 32, 90, true);
    y += 98;

    // ── DNS Lookup ───────────────────────────────────────────────────────────
    MakeSection(hwnd, L"DNS Lookup", y, cx, hInst);
    y += 22;
    mkLbl(L"Host:", 16, y + 3, 40);
    _hDnsHost = mkEdit(L"hostname", IDC_EDIT_DNS_HOST, 60, y, 250, 24);
    mkLbl(L"Type:", 318, y + 3, 36);
    _hDnsType = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        358, y, 70, 100, hwnd, (HMENU)IDC_COMBO_DNS_TYPE, hInst, nullptr);
    SendMessage(_hDnsType, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hDnsType, CB_ADDSTRING, 0, (LPARAM)L"A");
    SendMessage(_hDnsType, CB_ADDSTRING, 0, (LPARAM)L"PTR");
    SendMessage(_hDnsType, CB_ADDSTRING, 0, (LPARAM)L"ANY");
    SendMessage(_hDnsType, CB_SETCURSEL, 0, 0);
    _hBtnDns = mkBtn(L"Run", IDC_BTN_DNS_RUN, 436, y, 60, 24);
    y += 30;
    _hDnsOut = mkEdit(nullptr, IDC_EDIT_DNS_OUTPUT, 16, y, cx - 32, 60, true);
    y += 68;

    // ── TCP Connect ──────────────────────────────────────────────────────────
    MakeSection(hwnd, L"TCP Connect Test", y, cx, hInst);
    y += 22;
    mkLbl(L"Host:", 16, y + 3, 40);
    _hTcpHost = mkEdit(L"hostname or IP", IDC_EDIT_TCP_HOST, 60, y, 220, 24);
    mkLbl(L"Port:", 288, y + 3, 34);
    _hTcpPort = mkEdit(L"80", IDC_EDIT_TCP_PORT, 326, y, 60, 24);
    SetWindowText(_hTcpPort, L"80");
    _hBtnTcp = mkBtn(L"Run", IDC_BTN_TCP_RUN, 394, y, 60, 24);
    y += 30;
    _hTcpOut = mkEdit(nullptr, IDC_EDIT_TCP_OUTPUT, 16, y, cx - 32, 44, true);
    y += 52;

    // ── HTTP Test ─────────────────────────────────────────────────────────────
    MakeSection(hwnd, L"HTTP Test", y, cx, hInst);
    y += 22;
    mkLbl(L"URL:", 16, y + 3, 34);
    _hHttpUrl = mkEdit(L"http://...", IDC_EDIT_HTTP_URL, 54, y, 340, 24);
    _hBtnHttp = mkBtn(L"Run", IDC_BTN_HTTP_RUN, 402, y, 60, 24);
    y += 30;
    _hHttpOut = mkEdit(nullptr, IDC_EDIT_HTTP_OUTPUT, 16, y, cx - 32, 60, true);
    y += 68;

    // ── Wi-Fi Info ────────────────────────────────────────────────────────────
    MakeSection(hwnd, L"Wi-Fi Info", y, cx, hInst);
    y += 22;
    _hWifiInfo = mkEdit(nullptr, 9720, 16, y, cx - 32, 70, true);
    y += 78;

    // ── Gateway Info ──────────────────────────────────────────────────────────
    MakeSection(hwnd, L"Gateway & DNS Info", y, cx, hInst);
    y += 22;
    _hGwInfo = mkEdit(nullptr, 9721, 16, y, cx - 32, 80, true);
}

void TabTools::LayoutControls(int cx, int cy) {
    // Reflow all controls - simplified: just resize output boxes
    if (_hPingOut)  SetWindowPos(_hPingOut,  nullptr, 16, 0, cx - 32, 80, SWP_NOMOVE | SWP_NOZORDER);
    if (_hTraceOut) SetWindowPos(_hTraceOut, nullptr, 16, 0, cx - 32, 90, SWP_NOMOVE | SWP_NOZORDER);
    if (_hDnsOut)   SetWindowPos(_hDnsOut,   nullptr, 16, 0, cx - 32, 60, SWP_NOMOVE | SWP_NOZORDER);
    if (_hTcpOut)   SetWindowPos(_hTcpOut,   nullptr, 16, 0, cx - 32, 44, SWP_NOMOVE | SWP_NOZORDER);
    if (_hHttpOut)  SetWindowPos(_hHttpOut,  nullptr, 16, 0, cx - 32, 60, SWP_NOMOVE | SWP_NOZORDER);
    if (_hWifiInfo) SetWindowPos(_hWifiInfo, nullptr, 16, 0, cx - 32, 70, SWP_NOMOVE | SWP_NOZORDER);
    if (_hGwInfo)   SetWindowPos(_hGwInfo,   nullptr, 16, 0, cx - 32, 80, SWP_NOMOVE | SWP_NOZORDER);
    if (_hFlowOut)  SetWindowPos(_hFlowOut,  nullptr, 16, 0, cx - 32, 60, SWP_NOMOVE | SWP_NOZORDER);
}

LRESULT TabTools::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

LRESULT TabTools::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::BrushApp());
    EndPaint(hwnd, &ps);
    return 0;
}

void TabTools::AppendOutput(HWND hEdit, const wstring& text) {
    if (!hEdit) return;
    int len = GetWindowTextLength(hEdit);
    SendMessage(hEdit, EM_SETSEL, len, len);
    SendMessage(hEdit, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    SendMessage(hEdit, EM_SCROLL, SB_BOTTOM, 0);
}

LRESULT TabTools::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = LOWORD(wp);

    switch (id) {
    case IDC_BTN_PING_RUN: {
        wchar_t target[256], count[8];
        GetWindowText(_hPingTarget, target, 256);
        GetWindowText(_hPingCount, count, 8);
        if (target[0]) {
            SetWindowText(_hPingOut, L"");
            RunPing(target, _wtoi(count) > 0 ? _wtoi(count) : 4);
        }
        break;
    }
    case IDC_BTN_TRACE_RUN: {
        wchar_t target[256];
        GetWindowText(_hTraceTarget, target, 256);
        if (target[0]) {
            SetWindowText(_hTraceOut, L"");
            RunTraceroute(target);
        }
        break;
    }
    case IDC_BTN_DNS_RUN: {
        wchar_t host[256];
        GetWindowText(_hDnsHost, host, 256);
        int sel = (int)SendMessage(_hDnsType, CB_GETCURSEL, 0, 0);
        wchar_t typeBuf[16] = L"A";
        SendMessage(_hDnsType, CB_GETLBTEXT, sel, (LPARAM)typeBuf);
        if (host[0]) {
            SetWindowText(_hDnsOut, L"");
            RunDnsLookup(host, typeBuf);
        }
        break;
    }
    case IDC_BTN_TCP_RUN: {
        wchar_t host[256], portBuf[16];
        GetWindowText(_hTcpHost, host, 256);
        GetWindowText(_hTcpPort, portBuf, 16);
        if (host[0]) {
            SetWindowText(_hTcpOut, L"");
            RunTcpConnect(host, _wtoi(portBuf));
        }
        break;
    }
    case IDC_BTN_HTTP_RUN: {
        wchar_t url[512];
        GetWindowText(_hHttpUrl, url, 512);
        if (url[0]) {
            SetWindowText(_hHttpOut, L"");
            RunHttpTest(url);
        }
        break;
    }

    case 9700: ShowGuidedFlow(0); break;
    case 9701: ShowGuidedFlow(1); break;
    case 9702: ShowGuidedFlow(2); break;
    case 9703: ShowGuidedFlow(3); break;
    }

    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

LRESULT TabTools::OnToolResult(HWND hwnd, WPARAM wp, LPARAM lp) {
    // wp = target edit HWND, lp = wstring* result
    auto* result = reinterpret_cast<wstring*>(lp);
    if (result) {
        HWND hEdit = reinterpret_cast<HWND>(wp);
        AppendOutput(hEdit, *result);
        delete result;
    }
    return 0;
}

// ─── Ping ─────────────────────────────────────────────────────────────────────

void TabTools::RunPing(const wstring& target, int count) {
    HWND hwnd = _hwnd;
    HWND hOut = _hPingOut;

    std::thread([=]() {
        // Resolve hostname
        std::string tgt;
        {
            int n = WideCharToMultiByte(CP_UTF8, 0, target.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (n > 0) { tgt.resize(n - 1); WideCharToMultiByte(CP_UTF8, 0, target.c_str(), -1, &tgt[0], n, nullptr, nullptr); }
        }

        struct in_addr addr;
        if (inet_pton(AF_INET, tgt.c_str(), &addr) != 1) {
            // Resolve
            addrinfo hints = {}, *res = nullptr;
            hints.ai_family = AF_INET;
            if (getaddrinfo(tgt.c_str(), nullptr, &hints, &res) != 0 || !res) {
                PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut,
                    (LPARAM)new wstring(L"Could not resolve: " + target + L"\r\n"));
                return;
            }
            addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
            freeaddrinfo(res);
        }

        HANDLE hIcmp = IcmpCreateFile();
        if (hIcmp == INVALID_HANDLE_VALUE) {
            PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut,
                (LPARAM)new wstring(L"Failed to create ICMP handle (run as admin?)\r\n"));
            return;
        }

        char sendData[32] = "TransparencyPing";
        DWORD repBufSz = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8;
        auto repBuf = std::make_unique<BYTE[]>(repBufSz);

        wchar_t ipStr[INET_ADDRSTRLEN];
        InetNtop(AF_INET, &addr, ipStr, INET_ADDRSTRLEN);

        wstring header = L"Pinging " + wstring(ipStr) + L" with " +
                         std::to_wstring(sizeof(sendData)) + L" bytes:\r\n";
        PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut, (LPARAM)new wstring(header));

        int success = 0;
        long long totalMs = 0;
        int minMs = INT_MAX, maxMs = 0;

        for (int i = 0; i < count; i++) {
            DWORD start = timeGetTime();
            DWORD res = IcmpSendEcho(hIcmp, addr.S_un.S_addr, sendData, sizeof(sendData),
                nullptr, repBuf.get(), repBufSz, 3000);
            DWORD elapsed = timeGetTime() - start;

            wstring line;
            if (res > 0) {
                auto* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(repBuf.get());
                if (reply->Status == IP_SUCCESS) {
                    success++;
                    long long ms = (long long)elapsed;
                    totalMs += ms;
                    minMs = std::min(minMs, (int)ms);
                    maxMs = std::max(maxMs, (int)ms);
                    line = L"Reply from " + wstring(ipStr) + L": bytes=" +
                           std::to_wstring(sizeof(sendData)) + L" time=" +
                           std::to_wstring(ms) + L"ms TTL=" +
                           std::to_wstring(reply->Options.Ttl) + L"\r\n";
                } else {
                    line = L"Request timed out.\r\n";
                }
            } else {
                line = L"Request timed out.\r\n";
            }
            PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut, (LPARAM)new wstring(line));
            Sleep(1000);
        }

        IcmpCloseHandle(hIcmp);

        int lost = count - success;
        wstring stats = L"\r\nPing statistics for " + wstring(ipStr) + L":\r\n";
        stats += L"  Sent = " + std::to_wstring(count) +
                 L", Received = " + std::to_wstring(success) +
                 L", Lost = " + std::to_wstring(lost) +
                 L" (" + std::to_wstring(lost * 100 / count) + L"% loss)\r\n";
        if (success > 0) {
            stats += L"Approximate round trip times in ms:\r\n";
            stats += L"  Minimum = " + std::to_wstring(minMs) +
                     L"ms, Maximum = " + std::to_wstring(maxMs) +
                     L"ms, Average = " + std::to_wstring(totalMs / success) + L"ms\r\n";
        }
        PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut, (LPARAM)new wstring(stats));
    }).detach();
}

// ─── Traceroute ───────────────────────────────────────────────────────────────

void TabTools::RunTraceroute(const wstring& target) {
    HWND hwnd = _hwnd;
    HWND hOut = _hTraceOut;

    std::thread([=]() {
        std::string tgt;
        {
            int n = WideCharToMultiByte(CP_UTF8, 0, target.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (n > 0) { tgt.resize(n - 1); WideCharToMultiByte(CP_UTF8, 0, target.c_str(), -1, &tgt[0], n, nullptr, nullptr); }
        }

        struct in_addr destAddr;
        if (inet_pton(AF_INET, tgt.c_str(), &destAddr) != 1) {
            addrinfo hints = {}, *res = nullptr;
            hints.ai_family = AF_INET;
            if (getaddrinfo(tgt.c_str(), nullptr, &hints, &res) != 0 || !res) {
                PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut,
                    (LPARAM)new wstring(L"Could not resolve: " + target + L"\r\n"));
                return;
            }
            destAddr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
            freeaddrinfo(res);
        }

        wchar_t ipStr[INET_ADDRSTRLEN];
        InetNtop(AF_INET, &destAddr, ipStr, INET_ADDRSTRLEN);

        PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut,
            (LPARAM)new wstring(wstring(L"Tracing route to ") + ipStr + L"\r\n\r\n"));

        HANDLE hIcmp = IcmpCreateFile();
        if (hIcmp == INVALID_HANDLE_VALUE) return;

        char sendData[32] = "TraceRoute";
        DWORD repBufSz = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8;
        auto repBuf = std::make_unique<BYTE[]>(repBufSz);

        IP_OPTION_INFORMATION opts = {};
        opts.Ttl = 1;

        for (int hop = 1; hop <= 30 && opts.Ttl <= 30; hop++, opts.Ttl++) {
            DWORD start = timeGetTime();
            DWORD res = IcmpSendEcho(hIcmp, destAddr.S_un.S_addr, sendData, sizeof(sendData),
                &opts, repBuf.get(), repBufSz, 3000);
            DWORD elapsed = timeGetTime() - start;

            auto* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(repBuf.get());
            wstring line;

            if (res > 0) {
                struct in_addr hopAddr;
                hopAddr.S_un.S_addr = reply->Address;
                wchar_t hopStr[INET_ADDRSTRLEN];
                InetNtop(AF_INET, &hopAddr, hopStr, INET_ADDRSTRLEN);

                line = std::to_wstring(hop) + L"   " + std::to_wstring(elapsed) +
                       L" ms   " + hopStr;

                // Try reverse DNS
                char hopNarrow[INET_ADDRSTRLEN];
                sockaddr_in sa{};
                sa.sin_family = AF_INET;
                sa.sin_addr = hopAddr;
                char host[NI_MAXHOST];
                if (getnameinfo((sockaddr*)&sa, sizeof(sa), host, NI_MAXHOST, nullptr, 0, NI_NAMEREQD) == 0)
                    line += wstring(L" [") + ToWide(host) + L"]";

                line += L"\r\n";

                if (reply->Status == IP_SUCCESS) {
                    PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut, (LPARAM)new wstring(line));
                    break;
                }
            } else {
                line = std::to_wstring(hop) + L"   *   *   *   Request timed out.\r\n";
            }
            PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut, (LPARAM)new wstring(line));
        }

        IcmpCloseHandle(hIcmp);
        PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut,
            (LPARAM)new wstring(L"\r\nTrace complete.\r\n"));
    }).detach();
}

// ─── DNS Lookup ───────────────────────────────────────────────────────────────

void TabTools::RunDnsLookup(const wstring& host, const wstring& type) {
    HWND hwnd = _hwnd;
    HWND hOut = _hDnsOut;

    std::thread([=]() {
        std::string hostNarrow;
        {
            int n = WideCharToMultiByte(CP_UTF8, 0, host.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (n > 0) { hostNarrow.resize(n - 1); WideCharToMultiByte(CP_UTF8, 0, host.c_str(), -1, &hostNarrow[0], n, nullptr, nullptr); }
        }

        WORD qtype = DNS_TYPE_A;
        if (type == L"PTR") qtype = DNS_TYPE_PTR;
        else if (type == L"ANY") qtype = DNS_TYPE_ANY;

        PDNS_RECORD pRecord = nullptr;
        DNS_STATUS status = DnsQuery_A(hostNarrow.c_str(), qtype, DNS_QUERY_STANDARD, nullptr, &pRecord, nullptr);

        if (status != ERROR_SUCCESS) {
            wchar_t buf[64];
            swprintf_s(buf, L"DNS query failed. Error: %d\r\n", status);
            PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut, (LPARAM)new wstring(buf));
            return;
        }

        wstring result;
        for (DNS_RECORD* rec = pRecord; rec; rec = rec->pNext) {
            wchar_t line[512];
            if (rec->wType == DNS_TYPE_A) {
                struct in_addr addr;
                addr.S_un.S_addr = rec->Data.A.IpAddress;
                wchar_t ip[INET_ADDRSTRLEN];
                InetNtop(AF_INET, &addr, ip, INET_ADDRSTRLEN);
                swprintf_s(line, L"A  %s  TTL=%d\r\n", ip, rec->dwTtl);
            } else if (rec->wType == DNS_TYPE_PTR) {
                swprintf_s(line, L"PTR  %s  TTL=%d\r\n",
                    rec->Data.PTR.pNameHost ? rec->Data.PTR.pNameHost : L"(null)",
                    rec->dwTtl);
            } else if (rec->wType == DNS_TYPE_AAAA) {
                wchar_t ip6[INET6_ADDRSTRLEN];
                InetNtop(AF_INET6, &rec->Data.AAAA.Ip6Address, ip6, INET6_ADDRSTRLEN);
                swprintf_s(line, L"AAAA  %s  TTL=%d\r\n", ip6, rec->dwTtl);
            } else {
                swprintf_s(line, L"Type=%d  TTL=%d\r\n", rec->wType, rec->dwTtl);
            }
            result += line;
        }

        DnsRecordListFree(pRecord, DnsFreeRecordList);

        if (result.empty()) result = L"No records found.\r\n";
        PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut, (LPARAM)new wstring(result));
    }).detach();
}

// ─── TCP Connect ──────────────────────────────────────────────────────────────

void TabTools::RunTcpConnect(const wstring& host, int port) {
    HWND hwnd = _hwnd;
    HWND hOut = _hTcpOut;

    std::thread([=]() {
        std::string hostNarrow;
        {
            int n = WideCharToMultiByte(CP_UTF8, 0, host.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (n > 0) { hostNarrow.resize(n - 1); WideCharToMultiByte(CP_UTF8, 0, host.c_str(), -1, &hostNarrow[0], n, nullptr, nullptr); }
        }

        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) {
            PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut,
                (LPARAM)new wstring(L"Socket creation failed.\r\n"));
            return;
        }

        struct addrinfo hints = {}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char portStr[8];
        sprintf_s(portStr, "%d", port);

        if (getaddrinfo(hostNarrow.c_str(), portStr, &hints, &res) != 0 || !res) {
            closesocket(s);
            PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut,
                (LPARAM)new wstring(L"Could not resolve host.\r\n"));
            return;
        }

        u_long mode = 1;
        ioctlsocket(s, FIONBIO, &mode);

        DWORD start = timeGetTime();
        connect(s, res->ai_addr, (int)res->ai_addrlen);
        freeaddrinfo(res);

        fd_set wfds, efds;
        FD_ZERO(&wfds); FD_ZERO(&efds);
        FD_SET(s, &wfds); FD_SET(s, &efds);

        timeval tv;
        tv.tv_sec = 5; tv.tv_usec = 0;

        int sel = select(0, nullptr, &wfds, &efds, &tv);
        DWORD elapsed = timeGetTime() - start;

        bool connected = false;
        if (sel > 0 && FD_ISSET(s, &wfds) && !FD_ISSET(s, &efds)) {
            int err = 0, errLen = sizeof(err);
            getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &errLen);
            connected = (err == 0);
        }

        closesocket(s);

        wstring result = connected ?
            (L"Connected to " + host + L":" + std::to_wstring(port) +
             L" in " + std::to_wstring(elapsed) + L"ms\r\n") :
            (L"Connection to " + host + L":" + std::to_wstring(port) + L" FAILED\r\n");

        PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut, (LPARAM)new wstring(result));
    }).detach();
}

// ─── HTTP Test ────────────────────────────────────────────────────────────────

void TabTools::RunHttpTest(const wstring& url) {
    HWND hwnd = _hwnd;
    HWND hOut = _hHttpOut;

    std::thread([=]() {
        // Parse URL - basic
        wstring host, path;
        int port = 80;
        bool secure = false;

        wstring u = url;
        if (u.substr(0, 8) == L"https://") { secure = true; port = 443; u = u.substr(8); }
        else if (u.substr(0, 7) == L"http://") { u = u.substr(7); }

        size_t slash = u.find(L'/');
        if (slash != wstring::npos) { host = u.substr(0, slash); path = u.substr(slash); }
        else { host = u; path = L"/"; }

        size_t colon = host.find(L':');
        if (colon != wstring::npos) { port = _wtoi(host.substr(colon + 1).c_str()); host = host.substr(0, colon); }

        std::string hostNarrow;
        {
            int n = WideCharToMultiByte(CP_UTF8, 0, host.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (n > 0) { hostNarrow.resize(n - 1); WideCharToMultiByte(CP_UTF8, 0, host.c_str(), -1, &hostNarrow[0], n, nullptr, nullptr); }
        }
        std::string pathNarrow;
        {
            int n = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (n > 0) { pathNarrow.resize(n - 1); WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &pathNarrow[0], n, nullptr, nullptr); }
        }

        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) return;

        addrinfo hints = {}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        char portStr[8]; sprintf_s(portStr, "%d", port);

        if (getaddrinfo(hostNarrow.c_str(), portStr, &hints, &res) != 0 || !res) {
            closesocket(s);
            PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut,
                (LPARAM)new wstring(L"Could not resolve: " + host + L"\r\n"));
            return;
        }

        u_long mode = 1;
        ioctlsocket(s, FIONBIO, &mode);

        DWORD start = timeGetTime();
        connect(s, res->ai_addr, (int)res->ai_addrlen);
        freeaddrinfo(res);

        fd_set wfds;
        FD_ZERO(&wfds); FD_SET(s, &wfds);
        timeval tv; tv.tv_sec = 10; tv.tv_usec = 0;

        if (select(0, nullptr, &wfds, nullptr, &tv) <= 0) {
            closesocket(s);
            PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut,
                (LPARAM)new wstring(L"Connection timed out.\r\n"));
            return;
        }

        DWORD connTime = timeGetTime() - start;

        // Send HTTP request
        mode = 0; ioctlsocket(s, FIONBIO, &mode);
        DWORD recvTo = 5000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTo, sizeof(recvTo));

        std::string req = "GET " + pathNarrow + " HTTP/1.0\r\nHost: " + hostNarrow + "\r\nUser-Agent: Transparency/2.1\r\n\r\n";
        send(s, req.c_str(), (int)req.size(), 0);

        char buf[4096] = {};
        int recvd = recv(s, buf, sizeof(buf) - 1, 0);
        DWORD totalTime = timeGetTime() - start;
        closesocket(s);

        wstring result;
        if (recvd > 0) {
            buf[recvd] = '\0';
            std::string response = buf;

            // Extract status line
            size_t crlf = response.find("\r\n");
            std::string statusLine = crlf != std::string::npos ? response.substr(0, crlf) : response;

            // Extract Server header
            std::string server = "Unknown";
            size_t svrPos = response.find("Server:");
            if (svrPos != std::string::npos) {
                size_t svrEnd = response.find("\r\n", svrPos);
                if (svrEnd != std::string::npos)
                    server = response.substr(svrPos + 8, svrEnd - svrPos - 8);
                // Trim spaces
                while (!server.empty() && (server[0] == ' ')) server = server.substr(1);
            }

            result += ToWide(statusLine) + L"\r\n";
            result += L"Server: " + ToWide(server) + L"\r\n";
            result += L"Connect: " + std::to_wstring(connTime) + L"ms | Total: " + std::to_wstring(totalTime) + L"ms\r\n";
        } else {
            result = L"No response received.\r\n";
        }

        PostMessage(hwnd, WM_TOOL_RESULT, (WPARAM)hOut, (LPARAM)new wstring(result));
    }).detach();
}

// ─── Wi-Fi Info ───────────────────────────────────────────────────────────────

void TabTools::RefreshWifiInfo() {
    if (!_hWifiInfo) return;

    wstring info = L"Wi-Fi information requires WLAN API access.\r\n";

    HANDLE hClient = nullptr;
    DWORD version = 0;
    if (WlanOpenHandle(2, nullptr, &version, &hClient) == ERROR_SUCCESS) {
        PWLAN_INTERFACE_INFO_LIST pIfList = nullptr;
        if (WlanEnumInterfaces(hClient, nullptr, &pIfList) == ERROR_SUCCESS && pIfList) {
            for (DWORD i = 0; i < pIfList->dwNumberOfItems; i++) {
                auto& iface = pIfList->InterfaceInfo[i];
                if (iface.isState == wlan_interface_state_connected) {
                    PWLAN_CONNECTION_ATTRIBUTES pConnAttr = nullptr;
                    DWORD attrSz = sizeof(WLAN_CONNECTION_ATTRIBUTES);
                    WLAN_OPCODE_VALUE_TYPE ot;

                    if (WlanQueryInterface(hClient, &iface.InterfaceGuid,
                        wlan_intf_opcode_current_connection, nullptr, &attrSz,
                        (PVOID*)&pConnAttr, &ot) == ERROR_SUCCESS && pConnAttr) {

                        wchar_t ssid[256] = {};
                        int ssidLen = pConnAttr->wlanAssociationAttributes.dot11Ssid.uSSIDLength;
                        MultiByteToWideChar(CP_UTF8, 0,
                            (LPCCH)pConnAttr->wlanAssociationAttributes.dot11Ssid.ucSSID,
                            ssidLen, ssid, 255);

                        ULONG signal = pConnAttr->wlanAssociationAttributes.wlanSignalQuality;
                        ULONG speed = pConnAttr->wlanAssociationAttributes.ulRxRate;

                        info = L"SSID: " + wstring(ssid) + L"\r\n";
                        info += L"Signal Quality: " + std::to_wstring(signal) + L"%\r\n";
                        info += L"Rx Speed: " + std::to_wstring(speed / 1000) + L" Mbps\r\n";

                        WlanFreeMemory(pConnAttr);
                    }
                    break;
                }
            }
            WlanFreeMemory(pIfList);
        }
        WlanCloseHandle(hClient, nullptr);
    } else {
        info = L"WLAN service not available or not connected to Wi-Fi.\r\n";
    }

    SetWindowText(_hWifiInfo, info.c_str());
}

// ─── Gateway Info ─────────────────────────────────────────────────────────────

void TabTools::RefreshGatewayInfo() {
    if (!_hGwInfo) return;

    wstring info;

    // Get default gateway from routing table
    ULONG sz = 0;
    GetIpForwardTable(nullptr, &sz, FALSE);
    if (sz > 0) {
        auto buf = std::make_unique<BYTE[]>(sz);
        auto* table = reinterpret_cast<MIB_IPFORWARDTABLE*>(buf.get());
        if (GetIpForwardTable(table, &sz, FALSE) == NO_ERROR) {
            for (DWORD i = 0; i < table->dwNumEntries; i++) {
                if (table->table[i].dwForwardDest == 0 && table->table[i].dwForwardMask == 0) {
                    struct in_addr gwAddr;
                    gwAddr.S_un.S_addr = table->table[i].dwForwardNextHop;
                    wchar_t gwStr[INET_ADDRSTRLEN];
                    InetNtop(AF_INET, &gwAddr, gwStr, INET_ADDRSTRLEN);
                    info += L"Gateway: " + wstring(gwStr) + L"\r\n";
                    break;
                }
            }
        }
    }

    // DNS servers
    FIXED_INFO* fi = nullptr;
    ULONG fiSz = 0;
    GetNetworkParams(fi, &fiSz);
    if (fiSz > 0) {
        auto buf = std::make_unique<BYTE[]>(fiSz);
        fi = reinterpret_cast<FIXED_INFO*>(buf.get());
        if (GetNetworkParams(fi, &fiSz) == ERROR_SUCCESS) {
            info += L"DNS Servers:\r\n";
            IP_ADDR_STRING* dns = &fi->DnsServerList;
            while (dns) {
                if (dns->IpAddress.String[0])
                    info += L"  " + ToWide(dns->IpAddress.String) + L"\r\n";
                dns = dns->Next;
            }
        }
    }

    if (info.empty()) info = L"Could not retrieve gateway info.\r\n";
    SetWindowText(_hGwInfo, info.c_str());
}

// ─── Guided Flows ─────────────────────────────────────────────────────────────

void TabTools::ShowGuidedFlow(int idx) {
    if (!_hFlowOut) return;

    static const wchar_t* flows[] = {
        // 0: Device is offline
        L"DEVICE IS OFFLINE - Troubleshooting Steps:\r\n"
        L"1. Check that the device is powered on and not in sleep/hibernate mode.\r\n"
        L"2. Check the physical cable or Wi-Fi signal.\r\n"
        L"3. Run a Ping to the device's last known IP address.\r\n"
        L"4. Check if the IP address has changed (router DHCP may have reassigned it).\r\n"
        L"5. Try pinging the gateway to verify network connectivity.\r\n"
        L"6. Restart the device and rescan.\r\n"
        L"7. Check router DHCP lease table for the device's MAC address.\r\n",

        // 1: Internet is slow
        L"INTERNET IS SLOW - Troubleshooting Steps:\r\n"
        L"1. Ping 1.1.1.1 and 8.8.8.8 to check latency (use Ping tool above).\r\n"
        L"2. Run Traceroute to identify where slowdowns occur.\r\n"
        L"3. Test DNS speed: ping your DNS server vs public DNS.\r\n"
        L"4. Check for unknown devices on the network consuming bandwidth.\r\n"
        L"5. Restart your router and modem (unplug 30 seconds).\r\n"
        L"6. Check with your ISP for outages in your area.\r\n"
        L"7. Look for high-bandwidth devices (streaming, backups, updates).\r\n",

        // 2: Unknown device appeared
        L"UNKNOWN DEVICE APPEARED - Troubleshooting Steps:\r\n"
        L"1. Note the MAC address OUI prefix to identify the manufacturer.\r\n"
        L"2. Check the hostname for clues about the device type.\r\n"
        L"3. Look at open ports - a web server on 80/443 might be a smart device.\r\n"
        L"4. Check your router's DHCP table for when it first appeared.\r\n"
        L"5. Check mDNS services for device type hints.\r\n"
        L"6. If truly unknown: block it on router MAC filter and investigate.\r\n"
        L"7. Change your Wi-Fi password if you suspect an intruder.\r\n",

        // 3: Printer won't print
        L"PRINTER WON'T PRINT - Troubleshooting Steps:\r\n"
        L"1. Verify the printer appears in the Devices scan (is it online?).\r\n"
        L"2. Ping the printer's IP address to check connectivity.\r\n"
        L"3. Run TCP Connect test to port 9100 (PDL printing) and 631 (IPP).\r\n"
        L"4. Check if ports 9100, 631 (IPP), or 515 (LPD) are open.\r\n"
        L"5. Try accessing the printer's web interface (http://printer-ip).\r\n"
        L"6. Restart the Print Spooler service on Windows (net stop/start spooler).\r\n"
        L"7. Delete stuck print jobs in Control Panel > Devices and Printers.\r\n"
        L"8. Reinstall printer driver if all else fails.\r\n",
    };

    SetWindowText(_hFlowOut, idx < 4 ? flows[idx] : L"");
}
