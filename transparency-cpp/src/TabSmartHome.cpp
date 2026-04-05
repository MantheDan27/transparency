#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <winhttp.h>
#include <shellapi.h>
#include <string>
#include <sstream>
#include <vector>
#include <thread>

#pragma comment(lib, "winhttp.lib")

#include "TabSmartHome.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"

using std::wstring;
using std::string;

const wchar_t* TabSmartHome::s_className = L"TabSmartHomeWnd";

// ── Helpers ──────────────────────────────────────────────────────────────────

static HWND MakeSection(HWND parent, const wchar_t* text, int& y, int cx, HINSTANCE hInst) {
    HWND hw = CreateWindowEx(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, y, cx - 32, 20, parent, nullptr, hInst, nullptr);
    SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBold(), TRUE);
    return hw;
}

std::string TabSmartHome::WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring TabSmartHome::Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return L"";
    std::wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

// URL-encode a UTF-8 string
static std::string UrlEncode(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            char hex[4];
            sprintf_s(hex, "%%%02X", c);
            out += hex;
        }
    }
    return out;
}

// Simple JSON value extractor (no dependency needed for flat responses)
static std::string JsonExtract(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == std::string::npos) return "";
    pos++;
    // skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        // string value
        pos++;
        auto end = json.find('"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    } else {
        // numeric value
        auto end = json.find_first_of(",} \t\r\n", pos);
        if (end == std::string::npos) end = json.size();
        return json.substr(pos, end - pos);
    }
}

// ── WinHTTP POST ─────────────────────────────────────────────────────────────

std::string TabSmartHome::HttpPost(const std::wstring& host, const std::wstring& path,
                                   const std::string& body) {
    std::string result;

    HINTERNET hSession = WinHttpOpen(L"Transparency/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "Error: WinHttpOpen failed";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return "Error: WinHttpConnect failed"; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "Error: WinHttpOpenRequest failed";
    }

    const wchar_t* hdrs = L"Content-Type: application/x-www-form-urlencoded";
    BOOL sent = WinHttpSendRequest(hRequest, hdrs, (DWORD)-1,
        (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);

    if (sent) {
        WinHttpReceiveResponse(hRequest, nullptr);

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
            WINHTTP_NO_HEADER_INDEX);

        // Read response body
        DWORD bytesAvail = 0;
        do {
            WinHttpQueryDataAvailable(hRequest, &bytesAvail);
            if (bytesAvail > 0) {
                std::vector<char> buf(bytesAvail + 1, 0);
                DWORD bytesRead = 0;
                WinHttpReadData(hRequest, buf.data(), bytesAvail, &bytesRead);
                result.append(buf.data(), bytesRead);
            }
        } while (bytesAvail > 0);

        if (statusCode != 200) {
            result = "HTTP " + std::to_string(statusCode) + ": " + result;
        }
    } else {
        result = "Error: WinHttpSendRequest failed (" + std::to_string(GetLastError()) + ")";
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

// ── WinHTTP POST (JSON + Bearer auth) ────────────────────────────────────────

std::string TabSmartHome::HttpPostJson(const std::wstring& host, const std::wstring& path,
                                       const std::string& json, const std::wstring& bearerToken) {
    std::string result;

    HINTERNET hSession = WinHttpOpen(L"Transparency/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "Error: WinHttpOpen failed";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return "Error: WinHttpConnect failed"; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "Error: WinHttpOpenRequest failed";
    }

    // Build headers: Content-Type + Authorization
    wstring hdrs = L"Content-Type: application/json\r\n"
                   L"Authorization: Bearer " + bearerToken;

    BOOL sent = WinHttpSendRequest(hRequest, hdrs.c_str(), (DWORD)-1,
        (LPVOID)json.c_str(), (DWORD)json.size(), (DWORD)json.size(), 0);

    if (sent) {
        WinHttpReceiveResponse(hRequest, nullptr);

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
            WINHTTP_NO_HEADER_INDEX);

        DWORD bytesAvail = 0;
        do {
            WinHttpQueryDataAvailable(hRequest, &bytesAvail);
            if (bytesAvail > 0) {
                std::vector<char> buf(bytesAvail + 1, 0);
                DWORD bytesRead = 0;
                WinHttpReadData(hRequest, buf.data(), bytesAvail, &bytesRead);
                result.append(buf.data(), bytesRead);
            }
        } while (bytesAvail > 0);

        // For event gateway, 202 Accepted is success (no body)
        if (statusCode == 200 || statusCode == 202) {
            if (result.empty()) result = "OK (HTTP " + std::to_string(statusCode) + ")";
        } else {
            result = "HTTP " + std::to_string(statusCode) + ": " + result;
        }
    } else {
        result = "Error: WinHttpSendRequest failed (" + std::to_string(GetLastError()) + ")";
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

// ── Window setup ─────────────────────────────────────────────────────────────

bool TabSmartHome::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
    _mainWnd = mainWnd;

    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.hbrBackground = Theme::BrushSurface();
    wc.lpszClassName = s_className;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    RegisterClassEx(&wc);

    _hwnd = CreateWindowEx(0, s_className, nullptr,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VSCROLL,
        x, y, w, h, parent, nullptr, GetModuleHandle(nullptr), this);

    return _hwnd != nullptr;
}

LRESULT CALLBACK TabSmartHome::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TabSmartHome* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<TabSmartHome*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TabSmartHome*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:
        return self->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lp));
    case WM_SIZE:
        self->OnSize(hwnd, LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_PAINT:
        return self->OnPaint(hwnd);
    case WM_COMMAND:
        return self->OnCommand(hwnd, wp, lp);
    case WM_NOTIFY:
        return self->OnNotify(hwnd, reinterpret_cast<NMHDR*>(lp));
    case WM_SCAN_COMPLETE:
        self->RefreshDevices();
        return 0;
    case WM_VSCROLL:
        return self->OnVScroll(hwnd, wp);
    case WM_MOUSEWHEEL:
        return self->OnMouseWheel(hwnd, GET_WHEEL_DELTA_WPARAM(wp));
    // Async token exchange result
    case WM_APP + 100: {
        wstring* resp = reinterpret_cast<wstring*>(lp);
        if (resp) {
            std::string narrow = WideToUtf8(*resp);
            std::string token = JsonExtract(narrow, "access_token");
            std::string refresh = JsonExtract(narrow, "refresh_token");
            std::string expires = JsonExtract(narrow, "expires_in");
            std::string error = JsonExtract(narrow, "error");

            if (!token.empty()) {
                self->_alexaAccessToken = Utf8ToWide(token);
                self->_alexaRefreshToken = Utf8ToWide(refresh);
                self->AppendTokenLog(L"\r\n[SUCCESS] Access token retrieved!");
                self->AppendTokenLog(L"[TOKEN] " + Utf8ToWide(token.substr(0, 20)) + L"...");
                self->AppendTokenLog(L"[REFRESH] " + Utf8ToWide(refresh.substr(0, 20)) + L"...");
                self->AppendTokenLog(L"[EXPIRES] " + Utf8ToWide(expires) + L" seconds");
                if (self->_hAlexaStatus)
                    SetWindowText(self->_hAlexaStatus, L"Status: Connected (token acquired)");
            } else {
                self->AppendTokenLog(L"\r\n[ERROR] Token exchange failed");
                if (!error.empty())
                    self->AppendTokenLog(L"[DETAIL] " + Utf8ToWide(error));
                self->AppendTokenLog(L"[RESPONSE] " + *resp);
                if (self->_hAlexaStatus)
                    SetWindowText(self->_hAlexaStatus, L"Status: Token exchange failed");
            }
            delete resp;
        }
        return 0;
    }
    // Async token refresh result
    case WM_APP + 101: {
        wstring* resp = reinterpret_cast<wstring*>(lp);
        if (resp) {
            std::string narrow = WideToUtf8(*resp);
            std::string token = JsonExtract(narrow, "access_token");
            std::string refresh = JsonExtract(narrow, "refresh_token");
            std::string expires = JsonExtract(narrow, "expires_in");
            std::string error = JsonExtract(narrow, "error");

            if (!token.empty()) {
                self->_alexaAccessToken = Utf8ToWide(token);
                if (!refresh.empty())
                    self->_alexaRefreshToken = Utf8ToWide(refresh);
                self->AppendTokenLog(L"\r\n[SUCCESS] Token refreshed!");
                self->AppendTokenLog(L"[TOKEN] " + Utf8ToWide(token.substr(0, 20)) + L"...");
                self->AppendTokenLog(L"[EXPIRES] " + Utf8ToWide(expires) + L" seconds");
                if (self->_hAlexaStatus)
                    SetWindowText(self->_hAlexaStatus, L"Status: Connected (token refreshed)");
            } else {
                self->AppendTokenLog(L"\r\n[ERROR] Token refresh failed");
                if (!error.empty())
                    self->AppendTokenLog(L"[DETAIL] " + Utf8ToWide(error));
                if (self->_hAlexaStatus)
                    SetWindowText(self->_hAlexaStatus, L"Status: Refresh failed");
            }
            delete resp;
        }
        return 0;
    }
    // Async Smart Home Skill API responses
    case WM_APP + 102: { // Discovery response
        wstring* resp = reinterpret_cast<wstring*>(lp);
        if (resp) {
            if (resp->find(L"OK") == 0 || resp->find(L"HTTP 2") != wstring::npos) {
                self->AppendSHLog(L"[SUCCESS] Discovery AddOrUpdateReport accepted by Alexa");
                self->AppendSHLog(L"[RESPONSE] " + *resp);
            } else {
                self->AppendSHLog(L"[ERROR] Discovery failed: " + *resp);
            }
            delete resp;
        }
        return 0;
    }
    case WM_APP + 103: { // State report response
        wstring* resp = reinterpret_cast<wstring*>(lp);
        if (resp) {
            if (resp->find(L"OK") == 0 || resp->find(L"HTTP 2") != wstring::npos) {
                self->AppendSHLog(L"[SUCCESS] StateReport accepted");
            } else {
                self->AppendSHLog(L"[ERROR] StateReport failed: " + *resp);
            }
            delete resp;
        }
        return 0;
    }
    case WM_APP + 104: { // Change report response
        wstring* resp = reinterpret_cast<wstring*>(lp);
        if (resp) {
            if (resp->find(L"OK") == 0 || resp->find(L"HTTP 2") != wstring::npos) {
                self->AppendSHLog(L"[SUCCESS] ChangeReport (proactive) accepted by Event Gateway");
            } else {
                self->AppendSHLog(L"[ERROR] ChangeReport failed: " + *resp);
            }
            delete resp;
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SetBkColor(hdc, Theme::BG_SURFACE);
        return (LRESULT)Theme::BrushSurface();
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, Theme::BrushSurface());
        return 1;
    }
    default:
        return DefWindowProc(hwnd, msg, wp, lp);
    }
}

LRESULT TabSmartHome::OnCreate(HWND hwnd, LPCREATESTRUCT) {
    RECT rc; GetClientRect(hwnd, &rc);
    CreateControls(hwnd, rc.right, rc.bottom);
    PopulateSmartDevices();
    return 0;
}

LRESULT TabSmartHome::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

LRESULT TabSmartHome::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::BrushSurface());

    int sOff = -_scrollY;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TEXT_PRIMARY);
    HFONT old = (HFONT)SelectObject(hdc, Theme::FontH2());
    RECT titleRc = { Theme::SP4, Theme::SP3 + sOff, rc.right - Theme::SP4, 42 + sOff };
    DrawText(hdc, L"Smart Home", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);

    SetTextColor(hdc, Theme::TEXT_SECONDARY);
    old = (HFONT)SelectObject(hdc, Theme::FontBodySm());
    RECT subRc = { Theme::SP4, 42 + sOff, rc.right - Theme::SP4, 58 + sOff };
    DrawText(hdc, L"Control smart devices, set up automations, and manage integrations", -1, &subRc,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);

    RECT sep2 = { Theme::SP4, 62 + sOff, rc.right - Theme::SP4, 63 + sOff };
    FillRect(hdc, &sep2, Theme::BrushBorderSubtle());

    EndPaint(hwnd, &ps);
    return 0;
}

// ── Scroll support ───────────────────────────────────────────────────────────

void TabSmartHome::UpdateScrollBar(HWND hwnd) {
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = _contentHeight;
    si.nPage  = _viewHeight;
    si.nPos   = _scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

LRESULT TabSmartHome::OnVScroll(HWND hwnd, WPARAM wp) {
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);

    int oldPos = _scrollY;
    switch (LOWORD(wp)) {
    case SB_LINEUP:     _scrollY -= 30; break;
    case SB_LINEDOWN:   _scrollY += 30; break;
    case SB_PAGEUP:     _scrollY -= si.nPage; break;
    case SB_PAGEDOWN:   _scrollY += si.nPage; break;
    case SB_THUMBTRACK: _scrollY = si.nTrackPos; break;
    }

    int maxScroll = _contentHeight - _viewHeight;
    if (maxScroll < 0) maxScroll = 0;
    if (_scrollY < 0) _scrollY = 0;
    if (_scrollY > maxScroll) _scrollY = maxScroll;

    if (_scrollY != oldPos) {
        RECT rc; GetClientRect(hwnd, &rc);
        LayoutControls(rc.right, rc.bottom);
        InvalidateRect(hwnd, nullptr, TRUE);
    }
    return 0;
}

LRESULT TabSmartHome::OnMouseWheel(HWND hwnd, int delta) {
    int oldPos = _scrollY;
    _scrollY -= delta / 2;

    int maxScroll = _contentHeight - _viewHeight;
    if (maxScroll < 0) maxScroll = 0;
    if (_scrollY < 0) _scrollY = 0;
    if (_scrollY > maxScroll) _scrollY = maxScroll;

    if (_scrollY != oldPos) {
        RECT rc; GetClientRect(hwnd, &rc);
        LayoutControls(rc.right, rc.bottom);
        InvalidateRect(hwnd, nullptr, TRUE);
    }
    return 0;
}

// ── Controls ─────────────────────────────────────────────────────────────────

void TabSmartHome::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    auto mkBtn = [&](const wchar_t* text, int id, int x, int y, int w, int h) -> HWND {
        HWND hw = CreateWindowEx(0, L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return hw;
    };
    auto mkEdit = [&](const wchar_t* hint, int id, int x, int y, int w, int h, bool multi = false) -> HWND {
        HWND hw = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | (multi ? ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL : 0),
            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)(multi ? Theme::FontMono() : Theme::FontBody()), TRUE);
        if (hint && !multi) SendMessage(hw, EM_SETCUEBANNER, FALSE, (LPARAM)hint);
        Theme::ApplyDarkEdit(hw);
        return hw;
    };
    auto mkLbl = [&](const wchar_t* t, int x, int y, int w) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
            x, y, w, 18, hwnd, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
        return hw;
    };

    int y = 68;

    // ── Smart Devices Discovered ─────────────────────────────────────────────
    MakeSection(hwnd, L"Smart Devices on Network", y, cx, hInst);
    y += 24;

    _hDeviceList = CreateWindowEx(0, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        16, y, cx - 32, 140, hwnd, (HMENU)(INT_PTR)IDC_SMART_DEVICE_LIST, hInst, nullptr);
    SendMessage(_hDeviceList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hDeviceList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    Theme::ApplyDarkScrollbar(_hDeviceList);

    LVCOLUMN col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (LPWSTR)L"IP Address";  col.cx = 130; ListView_InsertColumn(_hDeviceList, 0, &col);
    col.pszText = (LPWSTR)L"Name";        col.cx = 160; ListView_InsertColumn(_hDeviceList, 1, &col);
    col.pszText = (LPWSTR)L"Type";        col.cx = 120; ListView_InsertColumn(_hDeviceList, 2, &col);
    col.pszText = (LPWSTR)L"Platform";    col.cx = 100; ListView_InsertColumn(_hDeviceList, 3, &col);
    col.pszText = (LPWSTR)L"Status";      col.cx = 80;  ListView_InsertColumn(_hDeviceList, 4, &col);

    y += 150;

    // ── Amazon Alexa Integration ─────────────────────────────────────────────
    MakeSection(hwnd, L"Amazon Alexa Integration", y, cx, hInst);
    y += 24;

    _hAlexaStatus = mkLbl(L"Status: Not connected", 16, y, 300);
    _hBtnAlexaLink = mkBtn(L"Link Alexa Account", IDC_BTN_ALEXA_LINK, 16, y + 22, 160, 28);
    _hBtnAlexaDiscover = mkBtn(L"Discover Devices", IDC_BTN_ALEXA_DISCOVER, 184, y + 22, 150, 28);
    y += 54;

    mkLbl(L"ALEXA VOICE COMMANDS", 16, y, 200);
    y += 18;
    _hAlexaOut = mkEdit(nullptr, IDC_SMART_ALEXA_OUT, 16, y, cx - 32, 70, true);
    SetWindowText(_hAlexaOut,
        L"Available commands when linked:\r\n"
        L"  \"Alexa, ask Transparency how many devices are online\"\r\n"
        L"  \"Alexa, ask Transparency to scan my network\"\r\n"
        L"  \"Alexa, ask Transparency for security status\"");
    y += 78;

    // ── Alexa Access Token Retrieval ─────────────────────────────────────────
    MakeSection(hwnd, L"Alexa Access Token Retrieval (OAuth 2.0)", y, cx, hInst);
    y += 24;

    mkLbl(L"CLIENT ID", 16, y, 80);
    _hAlexaClientId = mkEdit(L"amzn1.application-oa2-client.xxxxx", IDC_ALEXA_CLIENT_ID,
        100, y - 2, cx - 116, 24);
    y += 28;

    mkLbl(L"CLIENT SECRET", 16, y, 100);
    _hAlexaClientSecret = mkEdit(L"Your client secret", IDC_ALEXA_CLIENT_SECRET,
        120, y - 2, cx - 136, 24);
    // Mask the secret input
    SendMessage(_hAlexaClientSecret, EM_SETPASSWORDCHAR, (WPARAM)L'\x2022', 0);
    y += 28;

    mkLbl(L"REDIRECT URI", 16, y, 100);
    _hAlexaRedirectUri = mkEdit(L"https://localhost/callback", IDC_ALEXA_REDIRECT_URI,
        120, y - 2, cx - 136, 24);
    SetWindowText(_hAlexaRedirectUri, L"https://localhost/callback");
    y += 28;

    _hBtnAlexaOpenAuth = mkBtn(L"1. Open Authorization URL", IDC_BTN_ALEXA_OPEN_AUTH,
        16, y, 200, 28);
    y += 36;

    mkLbl(L"AUTH CODE", 16, y, 80);
    _hAlexaAuthCode = mkEdit(L"Paste authorization code from redirect", IDC_ALEXA_AUTH_CODE,
        100, y - 2, cx - 116, 24);
    y += 28;

    _hBtnAlexaGetToken = mkBtn(L"2. Exchange for Token", IDC_BTN_ALEXA_GET_TOKEN,
        16, y, 180, 28);
    _hBtnAlexaRefresh = mkBtn(L"3. Refresh Token", IDC_BTN_ALEXA_REFRESH,
        204, y, 150, 28);
    y += 36;

    mkLbl(L"TOKEN LOG", 16, y, 80);
    y += 18;
    _hAlexaTokenOut = mkEdit(nullptr, IDC_ALEXA_TOKEN_OUT, 16, y, cx - 32, 100, true);
    SetWindowText(_hAlexaTokenOut,
        L"Alexa Access Token Retrieval API\r\n"
        L"─────────────────────────────────\r\n"
        L"1. Enter your Client ID & Secret from the Alexa Developer Console\r\n"
        L"2. Click \"Open Authorization URL\" to authenticate in your browser\r\n"
        L"3. Paste the authorization code from the redirect URL\r\n"
        L"4. Click \"Exchange for Token\" to retrieve access & refresh tokens\r\n"
        L"5. Use \"Refresh Token\" when the access token expires (1 hour)\r\n"
        L"\r\n"
        L"Endpoint: https://api.amazon.com/auth/o2/token\r\n"
        L"Scope: alexa::all");
    y += 108;

    // ── Alexa Smart Home Skill API ──────────────────────────────────────────
    MakeSection(hwnd, L"Alexa Smart Home Skill API (Event Gateway)", y, cx, hInst);
    y += 24;

    mkLbl(L"REGION", 16, y, 60);
    _hAlexaSHRegion = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        80, y - 2, 240, 120, hwnd, (HMENU)(INT_PTR)IDC_ALEXA_SH_REGION, hInst, nullptr);
    SendMessage(_hAlexaSHRegion, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hAlexaSHRegion, CB_ADDSTRING, 0, (LPARAM)L"North America (api.amazonalexa.com)");
    SendMessage(_hAlexaSHRegion, CB_ADDSTRING, 0, (LPARAM)L"Europe (api.eu.amazonalexa.com)");
    SendMessage(_hAlexaSHRegion, CB_ADDSTRING, 0, (LPARAM)L"Far East (api.fe.amazonalexa.com)");
    SendMessage(_hAlexaSHRegion, CB_SETCURSEL, 0, 0);
    y += 30;

    _hBtnAlexaSHDiscover = mkBtn(L"Send Discovery", IDC_BTN_ALEXA_SH_DISCOVER, 16, y, 140, 28);
    _hBtnAlexaSHState = mkBtn(L"Send State Report", IDC_BTN_ALEXA_SH_STATE, 164, y, 150, 28);
    _hBtnAlexaSHChange = mkBtn(L"Send Change Report", IDC_BTN_ALEXA_SH_CHANGE, 322, y, 160, 28);
    y += 36;

    mkLbl(L"SKILL API LOG", 16, y, 100);
    y += 18;
    _hAlexaSHLog = mkEdit(nullptr, IDC_ALEXA_SH_LOG, 16, y, cx - 32, 140, true);
    SetWindowText(_hAlexaSHLog,
        L"Alexa Smart Home Skill API\r\n"
        L"──────────────────────────────\r\n"
        L"Interfaces: Alexa.Discovery, Alexa.EndpointHealth, Alexa.PowerController\r\n"
        L"\r\n"
        L"Send Discovery:      POST AddOrUpdateReport to Event Gateway\r\n"
        L"                     Registers network devices as Alexa endpoints\r\n"
        L"                     with connectivity & power capabilities\r\n"
        L"\r\n"
        L"Send State Report:   POST Alexa.StateReport for selected device\r\n"
        L"                     Reports current connectivity & reachability\r\n"
        L"\r\n"
        L"Send Change Report:  POST Alexa.ChangeReport (proactive)\r\n"
        L"                     Notifies Alexa when device state changes\r\n"
        L"                     (online/offline, new device, port changes)\r\n"
        L"\r\n"
        L"Requires a valid access token from the section above.");
    y += 148;

    // ── Google Home Integration ──────────────────────────────────────────────
    MakeSection(hwnd, L"Google Home Integration", y, cx, hInst);
    y += 24;

    _hGoogleStatus = mkLbl(L"Status: Not connected", 16, y, 300);
    _hBtnGoogleLink = mkBtn(L"Link Google Account", IDC_BTN_GOOGLE_LINK, 16, y + 22, 160, 28);
    _hBtnGoogleDiscover = mkBtn(L"Sync Devices", IDC_BTN_GOOGLE_DISCOVER, 184, y + 22, 150, 28);
    y += 54;

    mkLbl(L"GOOGLE HOME ACTIONS", 16, y, 200);
    y += 18;
    _hGoogleOut = mkEdit(nullptr, IDC_SMART_GOOGLE_OUT, 16, y, cx - 32, 70, true);
    SetWindowText(_hGoogleOut,
        L"Available actions when linked:\r\n"
        L"  \"Hey Google, ask Transparency for a network status\"\r\n"
        L"  \"Hey Google, ask Transparency to scan for new devices\"\r\n"
        L"  \"Hey Google, ask Transparency if my network is secure\"");
    y += 78;

    // ── Automation Triggers ──────────────────────────────────────────────────
    MakeSection(hwnd, L"Automation Triggers", y, cx, hInst);
    y += 24;

    mkLbl(L"WHEN", 16, y, 50);
    _hComboTriggerEvent = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        60, y - 2, 180, 200, hwnd, (HMENU)(INT_PTR)IDC_SMART_TRIGGER_EVENT, hInst, nullptr);
    SendMessage(_hComboTriggerEvent, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hComboTriggerEvent, CB_ADDSTRING, 0, (LPARAM)L"New device joins");
    SendMessage(_hComboTriggerEvent, CB_ADDSTRING, 0, (LPARAM)L"Device goes offline");
    SendMessage(_hComboTriggerEvent, CB_ADDSTRING, 0, (LPARAM)L"Risky port detected");
    SendMessage(_hComboTriggerEvent, CB_ADDSTRING, 0, (LPARAM)L"Internet outage");
    SendMessage(_hComboTriggerEvent, CB_ADDSTRING, 0, (LPARAM)L"High latency");
    SendMessage(_hComboTriggerEvent, CB_ADDSTRING, 0, (LPARAM)L"Scan complete");
    SendMessage(_hComboTriggerEvent, CB_SETCURSEL, 0, 0);

    mkLbl(L"THEN", 250, y, 50);
    _hComboTriggerAction = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        294, y - 2, 200, 200, hwnd, (HMENU)(INT_PTR)IDC_SMART_TRIGGER_ACTION, hInst, nullptr);
    SendMessage(_hComboTriggerAction, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hComboTriggerAction, CB_ADDSTRING, 0, (LPARAM)L"Flash smart lights red");
    SendMessage(_hComboTriggerAction, CB_ADDSTRING, 0, (LPARAM)L"Announce on Alexa");
    SendMessage(_hComboTriggerAction, CB_ADDSTRING, 0, (LPARAM)L"Announce on Google Home");
    SendMessage(_hComboTriggerAction, CB_ADDSTRING, 0, (LPARAM)L"Turn off smart plug");
    SendMessage(_hComboTriggerAction, CB_ADDSTRING, 0, (LPARAM)L"Send push notification");
    SendMessage(_hComboTriggerAction, CB_ADDSTRING, 0, (LPARAM)L"Run custom webhook");
    SendMessage(_hComboTriggerAction, CB_SETCURSEL, 0, 0);

    y += 30;
    _hBtnAddTrigger = mkBtn(L"Add Trigger", IDC_BTN_SMART_ADD_TRIGGER, 16, y, 110, 28);
    _hBtnDelTrigger = mkBtn(L"Remove", IDC_BTN_SMART_DEL_TRIGGER, 134, y, 80, 28);
    y += 36;

    _hTriggerList = CreateWindowEx(0, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
        16, y, cx - 32, 100, hwnd, (HMENU)(INT_PTR)IDC_SMART_TRIGGER_LIST, hInst, nullptr);
    SendMessage(_hTriggerList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hTriggerList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    Theme::ApplyDarkScrollbar(_hTriggerList);

    col.pszText = (LPWSTR)L"Event";  col.cx = 180; ListView_InsertColumn(_hTriggerList, 0, &col);
    col.pszText = (LPWSTR)L"Action"; col.cx = 200; ListView_InsertColumn(_hTriggerList, 1, &col);
    col.pszText = (LPWSTR)L"Status"; col.cx = 80;  ListView_InsertColumn(_hTriggerList, 2, &col);

    y += 110;

    // ── Scenes ───────────────────────────────────────────────────────────────
    MakeSection(hwnd, L"Scenes", y, cx, hInst);
    y += 24;

    _hEditSceneName = mkEdit(L"Scene name (e.g. \"Night Mode\")", IDC_SMART_SCENE_NAME, 16, y, 250, 26);
    _hBtnAddScene = mkBtn(L"Create Scene", IDC_BTN_SMART_ADD_SCENE, 274, y, 110, 26);
    _hBtnRunScene = mkBtn(L"Run Scene", IDC_BTN_SMART_RUN_SCENE, 392, y, 100, 26);
    y += 34;

    _hSceneList = CreateWindowEx(0, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
        16, y, cx - 32, 90, hwnd, (HMENU)(INT_PTR)IDC_SMART_SCENE_LIST, hInst, nullptr);
    SendMessage(_hSceneList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hSceneList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    Theme::ApplyDarkScrollbar(_hSceneList);

    col.pszText = (LPWSTR)L"Scene";    col.cx = 200; ListView_InsertColumn(_hSceneList, 0, &col);
    col.pszText = (LPWSTR)L"Devices";  col.cx = 120; ListView_InsertColumn(_hSceneList, 1, &col);
    col.pszText = (LPWSTR)L"Actions";  col.cx = 150; ListView_InsertColumn(_hSceneList, 2, &col);

    y += 100;
    _contentHeight = y;
}

void TabSmartHome::LayoutControls(int cx, int cy) {
    _viewHeight = cy;

    int sOff = -_scrollY;

    // Shift every child window by scroll offset
    HWND child = GetWindow(_hwnd, GW_CHILD);
    while (child) {
        RECT rc;
        GetWindowRect(child, &rc);
        MapWindowPoints(HWND_DESKTOP, _hwnd, (LPPOINT)&rc, 2);

        int origY = (int)(INT_PTR)GetProp(child, L"OrigY");
        if (!GetProp(child, L"OrigYSet")) {
            origY = rc.top;
            SetProp(child, L"OrigY", (HANDLE)(INT_PTR)origY);
            SetProp(child, L"OrigYSet", (HANDLE)(INT_PTR)1);
        }

        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        // Resize multiline readonly edits and listviews to track width
        DWORD style = GetWindowLong(child, GWL_STYLE);
        if ((style & ES_MULTILINE) && (style & ES_READONLY)) {
            w = cx - 32;
        }

        SetWindowPos(child, nullptr, rc.left, origY + sOff, w, h, SWP_NOZORDER);
        child = GetWindow(child, GW_HWNDNEXT);
    }

    UpdateScrollBar(_hwnd);
}

// ── Alexa Token Retrieval ────────────────────────────────────────────────────

void TabSmartHome::AppendTokenLog(const std::wstring& text) {
    if (!_hAlexaTokenOut) return;
    int len = GetWindowTextLength(_hAlexaTokenOut);
    SendMessage(_hAlexaTokenOut, EM_SETSEL, len, len);
    SendMessage(_hAlexaTokenOut, EM_REPLACESEL, FALSE, (LPARAM)(L"\r\n" + text).c_str());
}

void TabSmartHome::AlexaOpenAuth() {
    wchar_t clientId[512] = {};
    wchar_t redirectUri[512] = {};
    GetWindowText(_hAlexaClientId, clientId, 512);
    GetWindowText(_hAlexaRedirectUri, redirectUri, 512);

    if (!clientId[0]) {
        AppendTokenLog(L"[ERROR] Client ID is required");
        return;
    }

    // Build the authorization URL
    std::string cid = UrlEncode(WideToUtf8(clientId));
    std::string ruri = UrlEncode(WideToUtf8(redirectUri));

    std::string url = "https://www.amazon.com/ap/oa?"
        "client_id=" + cid +
        "&scope=alexa%3A%3Aall"
        "&response_type=code"
        "&redirect_uri=" + ruri;

    wstring wurl = Utf8ToWide(url);
    ShellExecute(nullptr, L"open", wurl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    AppendTokenLog(L"[INFO] Authorization URL opened in browser");
    AppendTokenLog(L"[INFO] After login, copy the 'code' parameter from the redirect URL");
    if (_hAlexaStatus)
        SetWindowText(_hAlexaStatus, L"Status: Waiting for authorization code...");
}

void TabSmartHome::AlexaExchangeToken() {
    wchar_t clientId[512] = {}, clientSecret[512] = {};
    wchar_t authCode[1024] = {}, redirectUri[512] = {};
    GetWindowText(_hAlexaClientId, clientId, 512);
    GetWindowText(_hAlexaClientSecret, clientSecret, 512);
    GetWindowText(_hAlexaAuthCode, authCode, 1024);
    GetWindowText(_hAlexaRedirectUri, redirectUri, 512);

    if (!clientId[0] || !clientSecret[0] || !authCode[0]) {
        AppendTokenLog(L"[ERROR] Client ID, Client Secret, and Auth Code are all required");
        return;
    }

    AppendTokenLog(L"[INFO] Exchanging authorization code for tokens...");
    if (_hAlexaStatus)
        SetWindowText(_hAlexaStatus, L"Status: Exchanging token...");

    // Build POST body
    std::string body =
        "grant_type=authorization_code"
        "&code=" + UrlEncode(WideToUtf8(authCode)) +
        "&client_id=" + UrlEncode(WideToUtf8(clientId)) +
        "&client_secret=" + UrlEncode(WideToUtf8(clientSecret)) +
        "&redirect_uri=" + UrlEncode(WideToUtf8(redirectUri));

    // Run in background thread to avoid blocking UI
    HWND hwnd = _hwnd;
    std::thread([this, body, hwnd]() {
        std::string resp = HttpPost(L"api.amazon.com", L"/auth/o2/token", body);

        // Post result back to UI thread
        wstring* result = new wstring(Utf8ToWide(resp));
        PostMessage(hwnd, WM_APP + 100, 0, (LPARAM)result);
    }).detach();
}

void TabSmartHome::AlexaRefreshToken() {
    if (_alexaRefreshToken.empty()) {
        AppendTokenLog(L"[ERROR] No refresh token available. Exchange an auth code first.");
        return;
    }

    wchar_t clientId[512] = {}, clientSecret[512] = {};
    GetWindowText(_hAlexaClientId, clientId, 512);
    GetWindowText(_hAlexaClientSecret, clientSecret, 512);

    if (!clientId[0] || !clientSecret[0]) {
        AppendTokenLog(L"[ERROR] Client ID and Client Secret are required for refresh");
        return;
    }

    AppendTokenLog(L"[INFO] Refreshing access token...");
    if (_hAlexaStatus)
        SetWindowText(_hAlexaStatus, L"Status: Refreshing token...");

    std::string body =
        "grant_type=refresh_token"
        "&refresh_token=" + UrlEncode(WideToUtf8(_alexaRefreshToken)) +
        "&client_id=" + UrlEncode(WideToUtf8(clientId)) +
        "&client_secret=" + UrlEncode(WideToUtf8(clientSecret));

    HWND hwnd = _hwnd;
    std::thread([this, body, hwnd]() {
        std::string resp = HttpPost(L"api.amazon.com", L"/auth/o2/token", body);
        wstring* result = new wstring(Utf8ToWide(resp));
        PostMessage(hwnd, WM_APP + 101, 0, (LPARAM)result);
    }).detach();
}

// ── Alexa Smart Home Skill API ───────────────────────────────────────────────

void TabSmartHome::AppendSHLog(const std::wstring& text) {
    if (!_hAlexaSHLog) return;
    int len = GetWindowTextLength(_hAlexaSHLog);
    SendMessage(_hAlexaSHLog, EM_SETSEL, len, len);
    SendMessage(_hAlexaSHLog, EM_REPLACESEL, FALSE, (LPARAM)(L"\r\n" + text).c_str());
}

std::wstring TabSmartHome::GetAlexaEventGatewayHost() const {
    int sel = _hAlexaSHRegion ? (int)SendMessage(_hAlexaSHRegion, CB_GETCURSEL, 0, 0) : 0;
    switch (sel) {
    case 1:  return L"api.eu.amazonalexa.com";
    case 2:  return L"api.fe.amazonalexa.com";
    default: return L"api.amazonalexa.com";
    }
}

std::string TabSmartHome::GenerateMessageId() {
    // Simple pseudo-unique ID using timestamp + random
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    char buf[64];
    sprintf_s(buf, "%08x-%04x-%04x-%04x-%012llx",
        (unsigned)(li.QuadPart & 0xFFFFFFFF),
        (unsigned)((li.QuadPart >> 32) & 0xFFFF),
        0x4000 | (unsigned)(li.QuadPart & 0x0FFF),
        0x8000 | (unsigned)((li.QuadPart >> 12) & 0x3FFF),
        (unsigned long long)(GetTickCount64()));
    return buf;
}

std::string TabSmartHome::BuildDiscoveryPayload() {
    // Build Alexa.Discovery.AddOrUpdateReport event
    // Registers all discovered smart devices as Alexa endpoints

    if (!_mainWnd) return "{}";
    ScanResult r = _mainWnd->GetLastResult();
    std::string msgId = GenerateMessageId();
    std::string token = WideToUtf8(_alexaAccessToken);

    // Build endpoints array
    std::string endpoints;
    int count = 0;
    for (auto& d : r.devices) {
        bool isSmart = false;
        for (int p : d.openPorts) {
            if (p == 8008 || p == 8009 || p == 8443 || p == 8123 ||
                p == 1883 || p == 8883 || p == 49152 || p == 49153)
                isSmart = true;
        }
        if (d.deviceType == L"IoT Device" || d.deviceType == L"Smart Speaker" ||
            d.deviceType == L"Smart TV" || d.deviceType == L"Camera")
            isSmart = true;
        if (!d.vendor.empty() &&
            (d.vendor.find(L"Amazon") != wstring::npos || d.vendor.find(L"Google") != wstring::npos ||
             d.vendor.find(L"Philips") != wstring::npos || d.vendor.find(L"TP-Link") != wstring::npos ||
             d.vendor.find(L"Ring") != wstring::npos || d.vendor.find(L"Sonos") != wstring::npos))
            isSmart = true;

        if (!isSmart) continue;

        std::string epId = WideToUtf8(d.mac.empty() ? d.ip : d.mac);
        std::string name = WideToUtf8(
            !d.customName.empty() ? d.customName :
            !d.hostname.empty()   ? d.hostname :
            !d.vendor.empty()     ? d.vendor : L"Network Device");
        std::string desc = WideToUtf8(d.deviceType) + " at " + WideToUtf8(d.ip);
        std::string manufacturer = WideToUtf8(d.vendor.empty() ? L"Unknown" : d.vendor);

        // Map device type to Alexa display category
        std::string category = "OTHER";
        std::string dType = WideToUtf8(d.deviceType);
        if (dType.find("Speaker") != string::npos) category = "SPEAKER";
        else if (dType.find("TV") != string::npos) category = "TV";
        else if (dType.find("Camera") != string::npos) category = "CAMERA";
        else if (dType.find("Light") != string::npos) category = "LIGHT";
        else if (dType.find("Switch") != string::npos || dType.find("Plug") != string::npos) category = "SMARTPLUG";

        if (count > 0) endpoints += ",";
        endpoints +=
            "{"
            "\"endpointId\":\"" + epId + "\","
            "\"manufacturerName\":\"" + manufacturer + "\","
            "\"friendlyName\":\"" + name + "\","
            "\"description\":\"" + desc + "\","
            "\"displayCategories\":[\"" + category + "\"],"
            "\"capabilities\":["
                "{"
                    "\"type\":\"AlexaInterface\","
                    "\"interface\":\"Alexa.EndpointHealth\","
                    "\"version\":\"3.2\","
                    "\"properties\":{"
                        "\"supported\":[{\"name\":\"connectivity\"}],"
                        "\"proactivelyReported\":true,"
                        "\"retrievable\":true"
                    "}"
                "},"
                "{"
                    "\"type\":\"AlexaInterface\","
                    "\"interface\":\"Alexa.PowerController\","
                    "\"version\":\"3\","
                    "\"properties\":{"
                        "\"supported\":[{\"name\":\"powerState\"}],"
                        "\"proactivelyReported\":true,"
                        "\"retrievable\":true"
                    "}"
                "},"
                "{"
                    "\"type\":\"AlexaInterface\","
                    "\"interface\":\"Alexa\","
                    "\"version\":\"3\""
                "}"
            "]"
            "}";
        count++;
    }

    // Full AddOrUpdateReport event
    return
        "{"
        "\"event\":{"
            "\"header\":{"
                "\"namespace\":\"Alexa.Discovery\","
                "\"name\":\"AddOrUpdateReport\","
                "\"payloadVersion\":\"3\","
                "\"messageId\":\"" + msgId + "\""
            "},"
            "\"payload\":{"
                "\"endpoints\":[" + endpoints + "],"
                "\"scope\":{"
                    "\"type\":\"BearerToken\","
                    "\"token\":\"" + token + "\""
                "}"
            "}"
        "}"
        "}";
}

std::string TabSmartHome::BuildStateReportPayload(const std::wstring& endpointId,
                                                   const std::wstring& ip, bool online) {
    std::string msgId = GenerateMessageId();
    std::string corrId = GenerateMessageId();
    std::string token = WideToUtf8(_alexaAccessToken);
    std::string epId = WideToUtf8(endpointId);
    std::string connectivity = online ? "OK" : "UNREACHABLE";
    std::string powerState = online ? "ON" : "OFF";

    // Get current ISO timestamp
    SYSTEMTIME st;
    GetSystemTime(&st);
    char ts[64];
    sprintf_s(ts, "%04d-%02d-%02dT%02d:%02d:%02dZ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    return
        "{"
        "\"event\":{"
            "\"header\":{"
                "\"namespace\":\"Alexa\","
                "\"name\":\"StateReport\","
                "\"payloadVersion\":\"3\","
                "\"messageId\":\"" + msgId + "\","
                "\"correlationToken\":\"" + corrId + "\""
            "},"
            "\"endpoint\":{"
                "\"endpointId\":\"" + epId + "\","
                "\"scope\":{"
                    "\"type\":\"BearerToken\","
                    "\"token\":\"" + token + "\""
                "}"
            "},"
            "\"payload\":{}"
        "},"
        "\"context\":{"
            "\"properties\":["
                "{"
                    "\"namespace\":\"Alexa.EndpointHealth\","
                    "\"name\":\"connectivity\","
                    "\"value\":{\"value\":\"" + connectivity + "\"},"
                    "\"timeOfSample\":\"" + string(ts) + "\","
                    "\"uncertaintyInMilliseconds\":0"
                "},"
                "{"
                    "\"namespace\":\"Alexa.PowerController\","
                    "\"name\":\"powerState\","
                    "\"value\":\"" + powerState + "\","
                    "\"timeOfSample\":\"" + string(ts) + "\","
                    "\"uncertaintyInMilliseconds\":0"
                "}"
            "]"
        "}"
        "}";
}

std::string TabSmartHome::BuildChangeReportPayload(const std::wstring& endpointId,
                                                    const std::wstring& ip, bool online) {
    std::string msgId = GenerateMessageId();
    std::string token = WideToUtf8(_alexaAccessToken);
    std::string epId = WideToUtf8(endpointId);
    std::string connectivity = online ? "OK" : "UNREACHABLE";
    std::string powerState = online ? "ON" : "OFF";

    SYSTEMTIME st;
    GetSystemTime(&st);
    char ts[64];
    sprintf_s(ts, "%04d-%02d-%02dT%02d:%02d:%02dZ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    // ChangeReport: proactive state update pushed to Alexa Event Gateway
    // cause.type = PHYSICAL_INTERACTION when device changes state on its own
    return
        "{"
        "\"event\":{"
            "\"header\":{"
                "\"namespace\":\"Alexa\","
                "\"name\":\"ChangeReport\","
                "\"payloadVersion\":\"3\","
                "\"messageId\":\"" + msgId + "\""
            "},"
            "\"endpoint\":{"
                "\"endpointId\":\"" + epId + "\","
                "\"scope\":{"
                    "\"type\":\"BearerToken\","
                    "\"token\":\"" + token + "\""
                "}"
            "},"
            "\"payload\":{"
                "\"change\":{"
                    "\"cause\":{\"type\":\"PHYSICAL_INTERACTION\"},"
                    "\"properties\":["
                        "{"
                            "\"namespace\":\"Alexa.EndpointHealth\","
                            "\"name\":\"connectivity\","
                            "\"value\":{\"value\":\"" + connectivity + "\"},"
                            "\"timeOfSample\":\"" + string(ts) + "\","
                            "\"uncertaintyInMilliseconds\":0"
                        "}"
                    "]"
                "}"
            "}"
        "},"
        "\"context\":{"
            "\"properties\":["
                "{"
                    "\"namespace\":\"Alexa.PowerController\","
                    "\"name\":\"powerState\","
                    "\"value\":\"" + powerState + "\","
                    "\"timeOfSample\":\"" + string(ts) + "\","
                    "\"uncertaintyInMilliseconds\":0"
                "}"
            "]"
        "}"
        "}";
}

void TabSmartHome::AlexaSHSendDiscovery() {
    if (_alexaAccessToken.empty()) {
        AppendSHLog(L"[ERROR] No access token. Complete OAuth flow first.");
        return;
    }

    AppendSHLog(L"[INFO] Building Alexa.Discovery.AddOrUpdateReport...");
    std::string payload = BuildDiscoveryPayload();

    wstring host = GetAlexaEventGatewayHost();
    wstring token = _alexaAccessToken;
    AppendSHLog(L"[INFO] POST to " + host + L"/v3/events");

    HWND hwnd = _hwnd;
    std::thread([this, payload, host, token, hwnd]() {
        std::string resp = HttpPostJson(host, L"/v3/events", payload, token);
        wstring* result = new wstring(Utf8ToWide(resp));
        PostMessage(hwnd, WM_APP + 102, 0, (LPARAM)result);
    }).detach();
}

void TabSmartHome::AlexaSHSendStateReport() {
    if (_alexaAccessToken.empty()) {
        AppendSHLog(L"[ERROR] No access token. Complete OAuth flow first.");
        return;
    }

    // Get selected device from device list
    if (!_hDeviceList) return;
    int sel = ListView_GetNextItem(_hDeviceList, -1, LVNI_SELECTED);
    if (sel < 0) {
        AppendSHLog(L"[ERROR] Select a device from the Smart Devices list first.");
        return;
    }

    wchar_t ipBuf[128], nameBuf[128], statusBuf[32];
    ListView_GetItemText(_hDeviceList, sel, 0, ipBuf, 128);
    ListView_GetItemText(_hDeviceList, sel, 1, nameBuf, 128);
    ListView_GetItemText(_hDeviceList, sel, 4, statusBuf, 32);
    bool online = (wcscmp(statusBuf, L"Online") == 0);

    // Use IP as endpoint ID (or MAC if available from scan)
    wstring endpointId = ipBuf;
    ScanResult r = _mainWnd->GetLastResult();
    for (auto& d : r.devices) {
        if (d.ip == ipBuf && !d.mac.empty()) { endpointId = d.mac; break; }
    }

    AppendSHLog(wstring(L"[INFO] Sending Alexa.StateReport for ") + nameBuf +
                L" (" + ipBuf + L") — " + (online ? L"OK" : L"UNREACHABLE"));

    std::string payload = BuildStateReportPayload(endpointId, ipBuf, online);
    wstring host = GetAlexaEventGatewayHost();
    wstring token = _alexaAccessToken;

    HWND hwnd = _hwnd;
    std::thread([this, payload, host, token, hwnd]() {
        std::string resp = HttpPostJson(host, L"/v3/events", payload, token);
        wstring* result = new wstring(Utf8ToWide(resp));
        PostMessage(hwnd, WM_APP + 103, 0, (LPARAM)result);
    }).detach();
}

void TabSmartHome::AlexaSHSendChangeReport() {
    if (_alexaAccessToken.empty()) {
        AppendSHLog(L"[ERROR] No access token. Complete OAuth flow first.");
        return;
    }

    if (!_hDeviceList) return;
    int sel = ListView_GetNextItem(_hDeviceList, -1, LVNI_SELECTED);
    if (sel < 0) {
        AppendSHLog(L"[ERROR] Select a device from the Smart Devices list first.");
        return;
    }

    wchar_t ipBuf[128], nameBuf[128], statusBuf[32];
    ListView_GetItemText(_hDeviceList, sel, 0, ipBuf, 128);
    ListView_GetItemText(_hDeviceList, sel, 1, nameBuf, 128);
    ListView_GetItemText(_hDeviceList, sel, 4, statusBuf, 32);
    bool online = (wcscmp(statusBuf, L"Online") == 0);

    wstring endpointId = ipBuf;
    ScanResult r = _mainWnd->GetLastResult();
    for (auto& d : r.devices) {
        if (d.ip == ipBuf && !d.mac.empty()) { endpointId = d.mac; break; }
    }

    AppendSHLog(wstring(L"[INFO] Sending Alexa.ChangeReport (proactive) for ") + nameBuf +
                L" — connectivity: " + (online ? L"OK" : L"UNREACHABLE"));

    std::string payload = BuildChangeReportPayload(endpointId, ipBuf, online);
    wstring host = GetAlexaEventGatewayHost();
    wstring token = _alexaAccessToken;

    HWND hwnd = _hwnd;
    std::thread([this, payload, host, token, hwnd]() {
        std::string resp = HttpPostJson(host, L"/v3/events", payload, token);
        wstring* result = new wstring(Utf8ToWide(resp));
        PostMessage(hwnd, WM_APP + 104, 0, (LPARAM)result);
    }).detach();
}

// ── Command handling ─────────────────────────────────────────────────────────

LRESULT TabSmartHome::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = LOWORD(wp);
    switch (id) {
    case IDC_BTN_ALEXA_LINK:
    case IDC_BTN_ALEXA_OPEN_AUTH:
        AlexaOpenAuth();
        break;

    case IDC_BTN_ALEXA_GET_TOKEN:
        AlexaExchangeToken();
        break;

    case IDC_BTN_ALEXA_REFRESH:
        AlexaRefreshToken();
        break;

    case IDC_BTN_ALEXA_DISCOVER:
        if (_hAlexaStatus)
            SetWindowText(_hAlexaStatus, L"Status: Discovering Alexa-compatible devices...");
        PopulateSmartDevices();
        break;

    case IDC_BTN_ALEXA_SH_DISCOVER:
        AlexaSHSendDiscovery();
        break;

    case IDC_BTN_ALEXA_SH_STATE:
        AlexaSHSendStateReport();
        break;

    case IDC_BTN_ALEXA_SH_CHANGE:
        AlexaSHSendChangeReport();
        break;

    case IDC_BTN_GOOGLE_LINK:
        if (_hGoogleStatus)
            SetWindowText(_hGoogleStatus, L"Status: Linking... (OAuth flow would open here)");
        break;

    case IDC_BTN_GOOGLE_DISCOVER:
        if (_hGoogleStatus)
            SetWindowText(_hGoogleStatus, L"Status: Syncing devices with Google Home...");
        PopulateSmartDevices();
        break;

    case IDC_BTN_SMART_ADD_TRIGGER: {
        if (!_hComboTriggerEvent || !_hComboTriggerAction || !_hTriggerList) break;
        wchar_t evtBuf[128], actBuf[128];
        int evtIdx = (int)SendMessage(_hComboTriggerEvent, CB_GETCURSEL, 0, 0);
        int actIdx = (int)SendMessage(_hComboTriggerAction, CB_GETCURSEL, 0, 0);
        SendMessage(_hComboTriggerEvent, CB_GETLBTEXT, evtIdx, (LPARAM)evtBuf);
        SendMessage(_hComboTriggerAction, CB_GETLBTEXT, actIdx, (LPARAM)actBuf);

        int idx = ListView_GetItemCount(_hTriggerList);
        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = idx;
        lvi.pszText = evtBuf;
        ListView_InsertItem(_hTriggerList, &lvi);
        ListView_SetItemText(_hTriggerList, idx, 1, actBuf);
        ListView_SetItemText(_hTriggerList, idx, 2, (LPWSTR)L"Active");
        break;
    }

    case IDC_BTN_SMART_DEL_TRIGGER: {
        if (!_hTriggerList) break;
        int sel = ListView_GetNextItem(_hTriggerList, -1, LVNI_SELECTED);
        if (sel >= 0) ListView_DeleteItem(_hTriggerList, sel);
        break;
    }

    case IDC_BTN_SMART_ADD_SCENE: {
        if (!_hEditSceneName || !_hSceneList) break;
        wchar_t name[128];
        GetWindowText(_hEditSceneName, name, 128);
        if (!name[0]) break;

        int idx = ListView_GetItemCount(_hSceneList);
        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = idx;
        lvi.pszText = name;
        ListView_InsertItem(_hSceneList, &lvi);
        ListView_SetItemText(_hSceneList, idx, 1, (LPWSTR)L"All");
        ListView_SetItemText(_hSceneList, idx, 2, (LPWSTR)L"(configure)");
        SetWindowText(_hEditSceneName, L"");
        break;
    }

    case IDC_BTN_SMART_RUN_SCENE: {
        if (!_hSceneList) break;
        int sel = ListView_GetNextItem(_hSceneList, -1, LVNI_SELECTED);
        if (sel >= 0) {
            wchar_t name[128];
            ListView_GetItemText(_hSceneList, sel, 0, name, 128);
            wstring msg = wstring(L"Running scene: ") + name;
            MessageBox(hwnd, msg.c_str(), L"Scene", MB_OK | MB_ICONINFORMATION);
        }
        break;
    }
    }

    // Handle async token response (WM_APP+100 = exchange, WM_APP+101 = refresh)
    // These are dispatched via WM_COMMAND fallthrough — actually handled in WndProc
    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

LRESULT TabSmartHome::OnNotify(HWND hwnd, NMHDR* hdr) {
    return DefWindowProc(hwnd, WM_NOTIFY, 0, (LPARAM)hdr);
}

void TabSmartHome::PopulateSmartDevices() {
    if (!_hDeviceList || !_mainWnd) return;
    ListView_DeleteAllItems(_hDeviceList);

    ScanResult r = _mainWnd->GetLastResult();
    int idx = 0;
    for (auto& d : r.devices) {
        bool isSmart = false;
        wstring platform = L"Unknown";

        for (int p : d.openPorts) {
            if (p == 8008 || p == 8009 || p == 8443) { isSmart = true; platform = L"Google/Cast"; }
            if (p == 8123)   { isSmart = true; platform = L"Home Assistant"; }
            if (p == 1883 || p == 8883) { isSmart = true; platform = L"MQTT"; }
            if (p == 49152 || p == 49153) { isSmart = true; platform = L"UPnP"; }
        }
        if (d.deviceType == L"IoT Device" || d.deviceType == L"Smart Speaker" ||
            d.deviceType == L"Smart TV" || d.deviceType == L"Camera") {
            isSmart = true;
        }
        if (!d.ssdpInfo.empty()) {
            if (d.ssdpInfo.find(L"Amazon") != wstring::npos) { isSmart = true; platform = L"Amazon Alexa"; }
            if (d.ssdpInfo.find(L"Google") != wstring::npos) { isSmart = true; platform = L"Google Home"; }
        }
        if (!d.vendor.empty()) {
            if (d.vendor.find(L"Amazon") != wstring::npos) { isSmart = true; platform = L"Amazon Alexa"; }
            if (d.vendor.find(L"Google") != wstring::npos) { isSmart = true; platform = L"Google/Nest"; }
            if (d.vendor.find(L"Philips") != wstring::npos) { isSmart = true; platform = L"Hue"; }
            if (d.vendor.find(L"TP-Link") != wstring::npos || d.vendor.find(L"Kasa") != wstring::npos) { isSmart = true; platform = L"TP-Link/Kasa"; }
            if (d.vendor.find(L"Ring") != wstring::npos) { isSmart = true; platform = L"Ring"; }
            if (d.vendor.find(L"Sonos") != wstring::npos) { isSmart = true; platform = L"Sonos"; }
        }

        if (!isSmart) continue;

        wstring name = !d.customName.empty() ? d.customName
                     : !d.hostname.empty()   ? d.hostname
                     : !d.vendor.empty()     ? d.vendor
                     : L"Unknown Device";

        LVITEM lvi = {};
        lvi.mask = LVIF_TEXT;
        lvi.iItem = idx;
        lvi.pszText = (LPWSTR)d.ip.c_str();
        ListView_InsertItem(_hDeviceList, &lvi);
        ListView_SetItemText(_hDeviceList, idx, 1, (LPWSTR)name.c_str());
        ListView_SetItemText(_hDeviceList, idx, 2, (LPWSTR)d.deviceType.c_str());
        ListView_SetItemText(_hDeviceList, idx, 3, (LPWSTR)platform.c_str());
        ListView_SetItemText(_hDeviceList, idx, 4, (LPWSTR)(d.online ? L"Online" : L"Offline"));
        idx++;
    }
}

void TabSmartHome::RefreshDevices() {
    PopulateSmartDevices();
}
