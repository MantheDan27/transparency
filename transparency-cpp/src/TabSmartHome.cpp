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
#include <algorithm>
#include <utility>

#pragma comment(lib, "winhttp.lib")

#include "TabSmartHome.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"

using std::wstring;
using std::string;

const wchar_t* TabSmartHome::s_className = L"TabSmartHomeWnd";

// WM_APP async response IDs
static const UINT WM_HA_CONNECT  = WM_APP + 120;
static const UINT WM_HA_SYNC     = WM_APP + 121;
static const UINT WM_HUE_DISC    = WM_APP + 122;
static const UINT WM_HUE_PAIR    = WM_APP + 123;
static const UINT WM_HUE_SYNC    = WM_APP + 124;
static const UINT WM_GOOGLE_CAST = WM_APP + 125; // lp = new pair<wstring ip, wstring json>*

// ── String helpers ────────────────────────────────────────────────────────────

string TabSmartHome::WideToUtf8(const wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

wstring TabSmartHome::Utf8ToWide(const string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return L"";
    wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

string TabSmartHome::JsonExtract(const string& json, const string& key) {
    string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == string::npos) return "";
    pos = json.find(':', pos + search.size());
    if (pos == string::npos) return "";
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        pos++;
        auto end = json.find('"', pos);
        if (end == string::npos) return "";
        return json.substr(pos, end - pos);
    }
    auto end = json.find_first_of(",} \t\r\n", pos);
    if (end == string::npos) end = json.size();
    return json.substr(pos, end - pos);
}

// ── HTTP helpers ──────────────────────────────────────────────────────────────

// Simple URL parser — returns {secure, host, port, path}
struct ParsedUrl { bool secure; wstring host; INTERNET_PORT port; wstring path; bool valid; };

static ParsedUrl ParseUrl(const wstring& url) {
    ParsedUrl r = {};
    wstring u = url;
    if (u.size() >= 8 && u.substr(0, 8) == L"https://") {
        r.secure = true; r.port = INTERNET_DEFAULT_HTTPS_PORT; u = u.substr(8);
    } else if (u.size() >= 7 && u.substr(0, 7) == L"http://") {
        r.secure = false; r.port = INTERNET_DEFAULT_HTTP_PORT; u = u.substr(7);
    } else { return r; }
    auto slash = u.find(L'/');
    wstring hostPort = (slash != wstring::npos) ? u.substr(0, slash) : u;
    r.path = (slash != wstring::npos) ? u.substr(slash) : L"/";
    auto colon = hostPort.rfind(L':');
    if (colon != wstring::npos) {
        r.host = hostPort.substr(0, colon);
        try { r.port = (INTERNET_PORT)std::stoi(hostPort.substr(colon + 1)); } catch (...) {}
    } else {
        r.host = hostPort;
    }
    r.valid = !r.host.empty();
    return r;
}

string TabSmartHome::HttpGetJson(const wstring& fullUrl, const wstring& bearerToken) {
    ParsedUrl u = ParseUrl(fullUrl);
    if (!u.valid) return "Error: invalid URL";

    HINTERNET hSess = WinHttpOpen(L"Transparency/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return "Error: WinHttpOpen failed";

    HINTERNET hConn = WinHttpConnect(hSess, u.host.c_str(), u.port, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return "Error: connect failed"; }

    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", u.path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        u.secure ? WINHTTP_FLAG_SECURE : 0);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return "Error: open request failed"; }

    wstring hdrs = L"Content-Type: application/json\r\n";
    if (!bearerToken.empty()) hdrs += L"Authorization: Bearer " + bearerToken;

    string result;
    if (WinHttpSendRequest(hReq, hdrs.c_str(), (DWORD)-1, nullptr, 0, 0, 0) &&
        WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        DWORD avail = 0;
        do {
            WinHttpQueryDataAvailable(hReq, &avail);
            if (avail > 0) {
                std::vector<char> buf(avail + 1, 0);
                DWORD read = 0;
                WinHttpReadData(hReq, buf.data(), avail, &read);
                result.append(buf.data(), read);
            }
        } while (avail > 0);
        if (status != 200 && status != 201)
            result = "HTTP " + std::to_string(status) + ": " + result;
    } else {
        result = "Error: request failed (" + std::to_string(GetLastError()) + ")";
    }
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
    return result;
}

string TabSmartHome::HttpPostLocal(const wstring& host, INTERNET_PORT port,
                                    const wstring& path, const string& json) {
    HINTERNET hSess = WinHttpOpen(L"Transparency/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return "Error: WinHttpOpen failed";

    HINTERNET hConn = WinHttpConnect(hSess, host.c_str(), port, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return "Error: connect failed"; }

    HINTERNET hReq = WinHttpOpenRequest(hConn, L"POST", path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return "Error: open request failed"; }

    const wchar_t* hdrs = L"Content-Type: application/json";
    string result;
    if (WinHttpSendRequest(hReq, hdrs, (DWORD)-1,
        (LPVOID)json.c_str(), (DWORD)json.size(), (DWORD)json.size(), 0) &&
        WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD avail = 0;
        do {
            WinHttpQueryDataAvailable(hReq, &avail);
            if (avail > 0) {
                std::vector<char> buf(avail + 1, 0);
                DWORD read = 0;
                WinHttpReadData(hReq, buf.data(), avail, &read);
                result.append(buf.data(), read);
            }
        } while (avail > 0);
    } else {
        result = "Error: request failed (" + std::to_string(GetLastError()) + ")";
    }
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
    return result;
}

string TabSmartHome::HttpPostJson(const wstring& host, const wstring& path,
                                   const string& json, const wstring& bearerToken) {
    HINTERNET hSess = WinHttpOpen(L"Transparency/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSess) return "Error: WinHttpOpen failed";
    HINTERNET hConn = WinHttpConnect(hSess, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConn) { WinHttpCloseHandle(hSess); return "Error: connect failed"; }
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"POST", path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) { WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess); return "Error: open request failed"; }
    wstring hdrs = L"Content-Type: application/json\r\nAuthorization: Bearer " + bearerToken;
    string result;
    if (WinHttpSendRequest(hReq, hdrs.c_str(), (DWORD)-1,
        (LPVOID)json.c_str(), (DWORD)json.size(), (DWORD)json.size(), 0) &&
        WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        DWORD avail = 0;
        do {
            WinHttpQueryDataAvailable(hReq, &avail);
            if (avail > 0) {
                std::vector<char> buf(avail + 1, 0);
                DWORD read = 0;
                WinHttpReadData(hReq, buf.data(), avail, &read);
                result.append(buf.data(), read);
            }
        } while (avail > 0);
        if (status == 200 || status == 202) { if (result.empty()) result = "OK"; }
        else result = "HTTP " + std::to_string(status) + ": " + result;
    } else {
        result = "Error: request failed (" + std::to_string(GetLastError()) + ")";
    }
    WinHttpCloseHandle(hReq); WinHttpCloseHandle(hConn); WinHttpCloseHandle(hSess);
    return result;
}

// ── Smart device classification helpers ──────────────────────────────────────

wstring TabSmartHome::SmartPlatform(const Device& d) {
    for (auto& svc : d.mdnsServices) {
        if (svc.find(L"_googlecast") != wstring::npos) return L"Chromecast";
        if (svc.find(L"_hue")        != wstring::npos) return L"Philips Hue";
        if (svc.find(L"_airplay")    != wstring::npos) return L"Apple";
        if (svc.find(L"_spotify")    != wstring::npos) return L"Sonos/Spotify";
        if (svc.find(L"_amzn")       != wstring::npos) return L"Amazon Echo";
        if (svc.find(L"_ewelink")    != wstring::npos) return L"Sonoff/eWeLink";
        if (svc.find(L"_tuya")       != wstring::npos) return L"Tuya";
    }
    for (int p : d.openPorts) {
        if (p == 8123)                return L"Home Assistant";
        if (p == 8009 || p == 8008)   return L"Google/Cast";
        if (p == 1883 || p == 8883)   return L"MQTT Broker";
        if (p == 1400)                return L"Sonos";
        if (p == 4070)                return L"Spotify Connect";
        if (p == 55443)               return L"Mi/Xiaomi";
    }
    wstring v = d.vendor;
    if (v.find(L"Amazon")   != wstring::npos) return L"Amazon Echo";
    if (v.find(L"Google")   != wstring::npos) return L"Google/Nest";
    if (v.find(L"Philips")  != wstring::npos) return L"Philips Hue";
    if (v.find(L"Sonos")    != wstring::npos) return L"Sonos";
    if (v.find(L"Ring")     != wstring::npos) return L"Ring";
    if (v.find(L"Nest")     != wstring::npos) return L"Nest";
    if (v.find(L"TP-Link")  != wstring::npos || v.find(L"Kasa") != wstring::npos) return L"TP-Link Kasa";
    if (v.find(L"Tuya")     != wstring::npos) return L"Tuya";
    if (v.find(L"Shelly")   != wstring::npos) return L"Shelly";
    if (v.find(L"Belkin")   != wstring::npos) return L"Wemo";
    if (v.find(L"Wemo")     != wstring::npos) return L"Wemo";
    if (v.find(L"Ecobee")   != wstring::npos) return L"Ecobee";
    if (v.find(L"Lutron")   != wstring::npos) return L"Lutron";
    if (!d.ssdpInfo.empty()) {
        if (d.ssdpInfo.find(L"Amazon") != wstring::npos) return L"Amazon Echo";
        if (d.ssdpInfo.find(L"Google") != wstring::npos) return L"Google";
    }
    return L"IoT Device";
}

wstring TabSmartHome::SecurityNote(const Device& d) {
    bool hasTelnet = false, hasHttp = false, hasHttps = false, hasFtp = false, hasRtsp = false;
    for (int p : d.openPorts) {
        if (p == 23)                hasTelnet = true;
        if (p == 80 || p == 8080)  hasHttp   = true;
        if (p == 443 || p == 8443) hasHttps  = true;
        if (p == 21)               hasFtp    = true;
        if (p == 554)              hasRtsp   = true;
    }
    if (hasTelnet)            return L"\u26A0 Telnet exposed";
    if (hasFtp)               return L"\u26A0 FTP exposed";
    if (hasHttp && !hasHttps) return L"\u26A0 HTTP only";
    if (hasRtsp)              return L"\u26A0 Camera stream";
    return L"";
}

bool TabSmartHome::IsSmartDevice(const Device& d) {
    for (auto& svc : d.mdnsServices) {
        if (svc.find(L"_googlecast") != wstring::npos ||
            svc.find(L"_hue")        != wstring::npos ||
            svc.find(L"_airplay")    != wstring::npos ||
            svc.find(L"_spotify")    != wstring::npos ||
            svc.find(L"_amzn")       != wstring::npos ||
            svc.find(L"_tuya")       != wstring::npos)
            return true;
    }
    for (int p : d.openPorts) {
        if (p == 8008 || p == 8009 || p == 8123 || p == 1883 ||
            p == 8883 || p == 1400 || p == 4070 || p == 55443)
            return true;
    }
    wstring v = d.vendor;
    const wchar_t* vendors[] = {
        L"Amazon", L"Google", L"Philips", L"Sonos", L"Ring", L"Nest",
        L"TP-Link", L"Kasa", L"Tuya", L"Shelly", L"Belkin", L"Wemo",
        L"Ecobee", L"Lutron", L"iRobot", L"Wyze", nullptr
    };
    for (int i = 0; vendors[i]; i++)
        if (v.find(vendors[i]) != wstring::npos) return true;
    wstring type = d.deviceType;
    if (type == L"IoT Device" || type == L"Smart Speaker" ||
        type == L"Smart TV"   || type == L"Camera") return true;
    return false;
}

bool TabSmartHome::IsAlexaDevice(const Device& d) {
    for (auto& svc : d.mdnsServices)
        if (svc.find(L"_amzn") != wstring::npos || svc.find(L"_alexa") != wstring::npos) return true;
    if (!d.ssdpInfo.empty() && d.ssdpInfo.find(L"Amazon") != wstring::npos) return true;
    wstring v = d.vendor;
    return v.find(L"Amazon") != wstring::npos;
}

bool TabSmartHome::IsGoogleDevice(const Device& d) {
    for (auto& svc : d.mdnsServices)
        if (svc.find(L"_googlecast") != wstring::npos) return true;
    for (int p : d.openPorts)
        if (p == 8008 || p == 8009) return true;
    if (!d.ssdpInfo.empty() && d.ssdpInfo.find(L"Google") != wstring::npos) return true;
    wstring v = d.vendor;
    return v.find(L"Google") != wstring::npos || v.find(L"Nest") != wstring::npos;
}

// ── Window setup ──────────────────────────────────────────────────────────────

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
    case WM_CREATE:      return self->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lp));
    case WM_SIZE:        self->OnSize(hwnd, LOWORD(lp), HIWORD(lp)); return 0;
    case WM_PAINT:       return self->OnPaint(hwnd);
    case WM_COMMAND:     return self->OnCommand(hwnd, wp, lp);
    case WM_NOTIFY:      return self->OnNotify(hwnd, reinterpret_cast<NMHDR*>(lp));
    case WM_SCAN_COMPLETE: self->RefreshDevices(); return 0;
    case WM_VSCROLL:     return self->OnVScroll(hwnd, wp);
    case WM_MOUSEWHEEL:  return self->OnMouseWheel(hwnd, GET_WHEEL_DELTA_WPARAM(wp));
    case WM_ERASEBKGND:  return 1;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN: {
        SetTextColor((HDC)wp, Theme::TEXT_PRIMARY);
        SetBkColor((HDC)wp, Theme::BG_SURFACE);
        return (LRESULT)Theme::BrushSurface();
    }

    // ── Async HTTP responses ──────────────────────────────────────────────────
    case WM_APP + 120: { // HA connect
        wstring* resp = reinterpret_cast<wstring*>(lp);
        if (!resp) return 0;
        string narrow = WideToUtf8(*resp);
        if (resp->find(L"HTTP 4") == wstring::npos && resp->find(L"Error") == wstring::npos) {
            string ver = JsonExtract(narrow, "version");
            wstring msg2 = L"[OK] Connected to Home Assistant";
            if (!ver.empty()) msg2 += L"  \u2014  version " + Utf8ToWide(ver);
            self->HaAppendLog(msg2);
            if (self->_hHaStatus)
                SetWindowText(self->_hHaStatus, (L"Status: Connected  (HA " + Utf8ToWide(ver) + L")").c_str());
        } else {
            self->HaAppendLog(L"[ERROR] Could not connect: " + *resp);
            if (self->_hHaStatus) SetWindowText(self->_hHaStatus, L"Status: Connection failed");
        }
        delete resp; return 0;
    }
    case WM_APP + 121: { // HA sync
        wstring* resp = reinterpret_cast<wstring*>(lp);
        if (!resp) return 0;
        if (resp->find(L"Error") != wstring::npos || resp->find(L"HTTP 4") != wstring::npos) {
            self->HaAppendLog(L"[ERROR] Sync failed: " + *resp);
            delete resp; return 0;
        }
        // Parse entity list and add to device list
        string json = WideToUtf8(*resp);
        ListView_DeleteAllItems(self->_hDeviceList);
        self->PopulateSmartDevices(); // keep LAN devices
        int row = ListView_GetItemCount(self->_hDeviceList);
        size_t pos = 0;
        int count = 0;
        while (pos < json.size()) {
            auto eid = json.find("\"entity_id\"", pos);
            if (eid == string::npos) break;
            string entityId = JsonExtract(json.substr(eid), "entity_id");
            // Only care about actionable domains
            bool relevant = entityId.find("light.") == 0 || entityId.find("switch.") == 0 ||
                            entityId.find("sensor.") == 0 || entityId.find("binary_sensor.") == 0 ||
                            entityId.find("media_player.") == 0 || entityId.find("climate.") == 0 ||
                            entityId.find("cover.") == 0 || entityId.find("lock.") == 0;
            if (relevant) {
                auto statePos = json.find("\"state\"", eid);
                string state = (statePos != string::npos && statePos < eid + 2000)
                                ? JsonExtract(json.substr(statePos), "state") : "";
                auto fnPos = json.find("\"friendly_name\"", eid);
                string fname = (fnPos != string::npos && fnPos < eid + 2000)
                                ? JsonExtract(json.substr(fnPos), "friendly_name") : entityId;
                // domain = part before '.'
                string domain = entityId.substr(0, entityId.find('.'));

                LVITEM lvi = {};
                lvi.mask = LVIF_TEXT; lvi.iItem = row;
                lvi.pszText = (LPWSTR)L"";
                ListView_InsertItem(self->_hDeviceList, &lvi);
                ListView_SetItemText(self->_hDeviceList, row, 1, (LPWSTR)Utf8ToWide(fname).c_str());
                ListView_SetItemText(self->_hDeviceList, row, 2, (LPWSTR)Utf8ToWide(domain).c_str());
                ListView_SetItemText(self->_hDeviceList, row, 3, (LPWSTR)L"Home Assistant");
                ListView_SetItemText(self->_hDeviceList, row, 4, (LPWSTR)L"");
                ListView_SetItemText(self->_hDeviceList, row, 5, (LPWSTR)Utf8ToWide(state).c_str());
                row++; count++;
            }
            pos = eid + entityId.size() + 1;
        }
        self->HaAppendLog(L"[OK] Synced " + std::to_wstring(count) + L" entities from Home Assistant");
        delete resp; return 0;
    }
    case WM_APP + 122: { // Hue discover — find bridge IP from SSDP description
        wstring* resp = reinterpret_cast<wstring*>(lp);
        if (!resp) return 0;
        // resp holds either "found:<ip>" or an error string
        if (resp->substr(0, 6) == L"found:") {
            wstring ip = resp->substr(6);
            self->_hueBridgeIp = ip;
            if (self->_hHueBridgeIp) SetWindowText(self->_hHueBridgeIp, ip.c_str());
            if (self->_hHueStatus) SetWindowText(self->_hHueStatus, (L"Bridge found: " + ip).c_str());
            self->HueAppendLog(L"[OK] Bridge at " + ip + L" \u2014 press button on bridge, then click Pair");
        } else {
            self->HueAppendLog(L"[INFO] No bridge auto-detected. Enter the bridge IP manually and click Pair.");
            if (self->_hHueStatus) SetWindowText(self->_hHueStatus, L"Status: Not found \u2014 enter IP manually");
        }
        delete resp; return 0;
    }
    case WM_APP + 123: { // Hue pair
        wstring* resp = reinterpret_cast<wstring*>(lp);
        if (!resp) return 0;
        string json = WideToUtf8(*resp);
        // Hue returns [{"success":{"username":"abc..."}}] or [{"error":...}]
        string username = JsonExtract(json, "username");
        if (!username.empty()) {
            self->_hueUsername = Utf8ToWide(username);
            self->HueAppendLog(L"[OK] Paired! Username: " + Utf8ToWide(username));
            if (self->_hHueStatus)
                SetWindowText(self->_hHueStatus, (L"Paired with " + self->_hueBridgeIp).c_str());
            self->HueAppendLog(L"[INFO] Click Sync Lights to load your bulbs.");
        } else {
            string errDesc = JsonExtract(json, "description");
            if (errDesc.find("link button") != string::npos)
                self->HueAppendLog(L"[ERROR] Press the button on your Hue bridge first, then click Pair.");
            else
                self->HueAppendLog(L"[ERROR] Pairing failed: " + Utf8ToWide(errDesc.empty() ? json : errDesc));
        }
        delete resp; return 0;
    }
    case WM_APP + 124: { // Hue sync lights
        wstring* resp = reinterpret_cast<wstring*>(lp);
        if (!resp) return 0;
        if (resp->find(L"Error") != wstring::npos || resp->find(L"HTTP 4") != wstring::npos) {
            self->HueAppendLog(L"[ERROR] Sync failed: " + *resp);
            delete resp; return 0;
        }
        string json = WideToUtf8(*resp);
        // Hue /lights returns {"1":{"name":"...","state":{"on":true,...}},...}
        int count = 0;
        int row = ListView_GetItemCount(self->_hDeviceList);
        size_t pos = 0;
        while (pos < json.size()) {
            auto namePos = json.find("\"name\"", pos);
            if (namePos == string::npos) break;
            string name = JsonExtract(json.substr(namePos), "name");
            auto onPos = json.find("\"on\"", namePos);
            string onState = (onPos != string::npos && onPos < namePos + 400)
                              ? JsonExtract(json.substr(onPos), "on") : "false";
            if (!name.empty()) {
                LVITEM lvi = {};
                lvi.mask = LVIF_TEXT; lvi.iItem = row;
                lvi.pszText = (LPWSTR)self->_hueBridgeIp.c_str();
                ListView_InsertItem(self->_hDeviceList, &lvi);
                ListView_SetItemText(self->_hDeviceList, row, 1, (LPWSTR)Utf8ToWide(name).c_str());
                ListView_SetItemText(self->_hDeviceList, row, 2, (LPWSTR)L"Light");
                ListView_SetItemText(self->_hDeviceList, row, 3, (LPWSTR)L"Philips Hue");
                ListView_SetItemText(self->_hDeviceList, row, 4, (LPWSTR)L"");
                ListView_SetItemText(self->_hDeviceList, row, 5,
                    (LPWSTR)(onState == "true" ? L"On" : L"Off"));
                row++; count++;
            }
            pos = namePos + name.size() + 1;
        }
        self->HueAppendLog(L"[OK] Synced " + std::to_wstring(count) + L" lights from Hue bridge");
        delete resp; return 0;
    }

    case WM_APP + 125: { // Google Cast eureka_info response
        auto* p = reinterpret_cast<std::pair<wstring, string>*>(lp);
        if (!p || !self->_hGoogleList) { delete p; return 0; }
        wstring ip  = p->first;
        string json = p->second;
        delete p;

        if (json.find("Error") == 0) { self->GoogleAppendLog(L"[INFO] " + ip + L": no Cast info"); return 0; }

        string name    = JsonExtract(json, "name");
        string model   = JsonExtract(json, "model_name");
        string version = JsonExtract(json, "cast_build_revision");
        if (name.empty()) name = "Google Device";

        int row = ListView_GetItemCount(self->_hGoogleList);
        LVITEM lvi = {}; lvi.mask = LVIF_TEXT; lvi.iItem = row;
        lvi.pszText = (LPWSTR)ip.c_str();
        ListView_InsertItem(self->_hGoogleList, &lvi);
        ListView_SetItemText(self->_hGoogleList, row, 1, (LPWSTR)Utf8ToWide(name).c_str());
        ListView_SetItemText(self->_hGoogleList, row, 2, (LPWSTR)Utf8ToWide(model).c_str());
        ListView_SetItemText(self->_hGoogleList, row, 3, (LPWSTR)L"Online");
        ListView_SetItemText(self->_hGoogleList, row, 4,
            version.empty() ? (LPWSTR)L"" : (LPWSTR)Utf8ToWide(version).c_str());
        return 0;
    }

    default: return DefWindowProc(hwnd, msg, wp, lp);
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
    HDC hdcScreen = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    int cx = rc.right, cy = rc.bottom;
    if (cx <= 0 || cy <= 0) { EndPaint(hwnd, &ps); return 0; }

    HDC hdc = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, cx, cy);
    HBITMAP hOld = (HBITMAP)SelectObject(hdc, hBmp);

    FillRect(hdc, &rc, Theme::BrushSurface());
    int sOff = -_scrollY;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TEXT_PRIMARY);
    HFONT old = (HFONT)SelectObject(hdc, Theme::FontH2());
    RECT title = { 16, 12 + sOff, cx - 16, 40 + sOff };
    DrawText(hdc, L"Smart Home", -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);

    SetTextColor(hdc, Theme::TEXT_SECONDARY);
    old = (HFONT)SelectObject(hdc, Theme::FontBodySm());
    RECT sub = { 16, 40 + sOff, cx - 16, 56 + sOff };
    DrawText(hdc, L"Local-first smart home \u2014 no cloud accounts needed", -1, &sub,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, old);

    RECT sep = { 16, 60 + sOff, cx - 16, 61 + sOff };
    FillRect(hdc, &sep, Theme::BrushBorderSubtle());

    BitBlt(hdcScreen, 0, 0, cx, cy, hdc, 0, 0, SRCCOPY);
    SelectObject(hdc, hOld);
    DeleteObject(hBmp);
    DeleteDC(hdc);
    EndPaint(hwnd, &ps);
    return 0;
}

// ── Scroll ────────────────────────────────────────────────────────────────────

void TabSmartHome::UpdateScrollBar(HWND hwnd) {
    SCROLLINFO si = {};
    si.cbSize = sizeof(si); si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0; si.nMax = _contentHeight; si.nPage = _viewHeight; si.nPos = _scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

LRESULT TabSmartHome::OnVScroll(HWND hwnd, WPARAM wp) {
    SCROLLINFO si = {}; si.cbSize = sizeof(si); si.fMask = SIF_ALL;
    GetScrollInfo(hwnd, SB_VERT, &si);
    int old = _scrollY;
    switch (LOWORD(wp)) {
    case SB_LINEUP:     _scrollY -= 30; break;
    case SB_LINEDOWN:   _scrollY += 30; break;
    case SB_PAGEUP:     _scrollY -= (int)si.nPage; break;
    case SB_PAGEDOWN:   _scrollY += (int)si.nPage; break;
    case SB_THUMBTRACK: _scrollY = si.nTrackPos; break;
    }
    int maxScroll = std::max(0, _contentHeight - _viewHeight);
    _scrollY = std::max(0, std::min(maxScroll, _scrollY));
    if (_scrollY != old) {
        ScrollWindowEx(hwnd, 0, old - _scrollY, nullptr, nullptr, nullptr, nullptr,
                       SW_SCROLLCHILDREN | SW_INVALIDATE);
        UpdateScrollBar(hwnd);
    }
    return 0;
}

LRESULT TabSmartHome::OnMouseWheel(HWND hwnd, int delta) {
    int old = _scrollY;
    _scrollY -= delta / 2;
    int maxScroll = std::max(0, _contentHeight - _viewHeight);
    _scrollY = std::max(0, std::min(maxScroll, _scrollY));
    if (_scrollY != old) {
        ScrollWindowEx(hwnd, 0, old - _scrollY, nullptr, nullptr, nullptr, nullptr,
                       SW_SCROLLCHILDREN | SW_INVALIDATE);
        UpdateScrollBar(hwnd);
    }
    return 0;
}

// ── Controls ──────────────────────────────────────────────────────────────────

void TabSmartHome::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    auto mkBtn = [&](const wchar_t* txt, int id, int x, int y, int w, int h) -> HWND {
        HWND hw = CreateWindowEx(0, L"BUTTON", txt, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return hw;
    };
    auto mkEdit = [&](const wchar_t* hint, int id, int x, int y, int w, int h,
                      bool multi = false, bool pwd = false) -> HWND {
        DWORD style = WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL |
                      (multi ? ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL : 0);
        HWND hw = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr, style,
            x, y, w, h, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)(multi ? Theme::FontMono() : Theme::FontBody()), TRUE);
        if (hint && !multi) SendMessage(hw, EM_SETCUEBANNER, FALSE, (LPARAM)hint);
        if (pwd) SendMessage(hw, EM_SETPASSWORDCHAR, (WPARAM)L'\x2022', 0);
        Theme::ApplyDarkEdit(hw);
        return hw;
    };
    auto mkLbl = [&](const wchar_t* t, int x, int y, int w) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
            x, y, w, 18, hwnd, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontSmall(), TRUE);
        return hw;
    };
    auto mkSection = [&](const wchar_t* t, int y) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", t, WS_CHILD | WS_VISIBLE | SS_LEFT,
            16, y, cx - 32, 20, hwnd, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBold(), TRUE);
        return hw;
    };

    int y = 68;

    // ── SMART DEVICES ON NETWORK ─────────────────────────────────────────────
    mkSection(L"Smart Devices on Network", y); y += 24;

    _hDeviceList = CreateWindowEx(0, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        16, y, cx - 32, 150, hwnd, (HMENU)(INT_PTR)IDC_SMART_DEVICE_LIST, hInst, nullptr);
    SendMessage(_hDeviceList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hDeviceList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    Theme::ApplyDarkScrollbar(_hDeviceList);

    LVCOLUMN col = {}; col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; col.fmt = LVCFMT_LEFT;
    struct ColDef { const wchar_t* name; int w; };
    static const ColDef COLS[] = {
        {L"IP / ID", 120}, {L"Name", 160}, {L"Type", 110},
        {L"Platform", 120}, {L"Security", 120}, {L"Status", 70}
    };
    for (int i = 0; i < 6; i++) {
        col.cx = COLS[i].w; col.pszText = (LPWSTR)COLS[i].name;
        ListView_InsertColumn(_hDeviceList, i, &col);
    }
    y += 160;

    // ── HOME ASSISTANT ───────────────────────────────────────────────────────
    y += 8;
    mkSection(L"Home Assistant  (local \u2014 no cloud account needed)", y); y += 24;

    _hHaStatus = mkLbl(L"Status: Not connected", 16, y, 400); y += 22;

    mkLbl(L"URL", 16, y, 36);
    _hHaUrl = mkEdit(L"http://192.168.1.x:8123", IDC_EDIT_HA_URL, 56, y - 2, cx - 280, 24);
    _hBtnHaConnect = mkBtn(L"Connect", IDC_BTN_HA_CONNECT, cx - 218, y - 2, 96, 26);
    _hBtnHaSync    = mkBtn(L"Sync Devices", IDC_BTN_HA_SYNC, cx - 116, y - 2, 100, 26);
    y += 30;

    mkLbl(L"Token", 16, y, 44);
    _hHaToken = mkEdit(L"Paste Long-Lived Access Token from HA profile \u2192 Security", IDC_EDIT_HA_TOKEN,
                       64, y - 2, cx - 80, 24, false, true);
    y += 30;

    _hHaLog = mkEdit(nullptr, IDC_SMART_HA_LOG, 16, y, cx - 32, 80, true);
    SetWindowText(_hHaLog,
        L"Home Assistant  \u2014  local REST API\r\n"
        L"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\r\n"
        L"1. In HA: Profile \u2192 Security \u2192 Long-lived access tokens \u2192 Create token\r\n"
        L"2. Paste the token above and enter your HA URL\r\n"
        L"3. Click Connect to verify, then Sync Devices to import entities");
    y += 88;

    // ── PHILIPS HUE ──────────────────────────────────────────────────────────
    y += 8;
    mkSection(L"Philips Hue  (local \u2014 no cloud account needed)", y); y += 24;

    _hHueStatus = mkLbl(L"Status: Not connected", 16, y, 400); y += 22;

    mkLbl(L"Bridge IP", 16, y, 64);
    _hHueBridgeIp = mkEdit(L"192.168.1.x  (auto-filled by Discover)", IDC_EDIT_HUE_IP,
                            84, y - 2, cx - 378, 24);
    _hBtnHueDiscover = mkBtn(L"Discover", IDC_BTN_HUE_DISCOVER, cx - 288, y - 2, 86, 26);
    _hBtnHuePair     = mkBtn(L"Pair", IDC_BTN_HUE_PAIR, cx - 196, y - 2, 86, 26);
    _hBtnHueSync     = mkBtn(L"Sync Lights", IDC_BTN_HUE_SYNC, cx - 104, y - 2, 88, 26);
    y += 30;

    _hHueLog = mkEdit(nullptr, IDC_SMART_HUE_LOG, 16, y, cx - 32, 80, true);
    SetWindowText(_hHueLog,
        L"Philips Hue  \u2014  local bridge API\r\n"
        L"\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\r\n"
        L"1. Click Discover to find your bridge on the network\r\n"
        L"2. Press the physical button on the bridge, then click Pair\r\n"
        L"3. Click Sync Lights to load your bulbs into the device list");
    y += 88;

    // ── AMAZON ALEXA ─────────────────────────────────────────────────────────
    y += 8;
    mkSection(L"Amazon Alexa  (local discovery \u2014 no cloud account needed)", y); y += 24;

    mkLbl(L"Alexa devices detected on your network. Manage them in the Alexa app or web.", 16, y, cx - 32); y += 20;

    _hAlexaList = CreateWindowEx(0, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        16, y, cx - 32, 120, hwnd, (HMENU)(INT_PTR)IDC_SMART_ALEXA_LIST, hInst, nullptr);
    SendMessage(_hAlexaList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hAlexaList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    Theme::ApplyDarkScrollbar(_hAlexaList);
    {
        LVCOLUMN col2 = {}; col2.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; col2.fmt = LVCFMT_LEFT;
        struct ColDef2 { const wchar_t* name; int w; };
        static const ColDef2 AC[] = {
            {L"IP Address", 130}, {L"Name / Hostname", 180}, {L"Model / Vendor", 160}, {L"Status", 70}
        };
        for (int i = 0; i < 4; i++) { col2.cx = AC[i].w; col2.pszText = (LPWSTR)AC[i].name; ListView_InsertColumn(_hAlexaList, i, &col2); }
    }
    y += 128;

    {
        HWND btn1 = mkBtn(L"\u21BB Refresh", IDC_BTN_ALEXA_REFRESH, 16, y, 100, 26);
        HWND btn2 = mkBtn(L"\U0001F310 Open Alexa Web", IDC_BTN_ALEXA_OPEN, 124, y, 148, 26);
        (void)btn1; (void)btn2;
    }
    y += 34;

    // ── GOOGLE HOME / CAST ───────────────────────────────────────────────────
    y += 8;
    mkSection(L"Google Home / Chromecast  (local \u2014 no cloud account needed)", y); y += 24;

    mkLbl(L"Google Cast devices respond locally on port 8008. Click Sync to query device info.", 16, y, cx - 32); y += 20;

    _hGoogleList = CreateWindowEx(0, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        16, y, cx - 32, 120, hwnd, (HMENU)(INT_PTR)IDC_SMART_GOOGLE_LIST, hInst, nullptr);
    SendMessage(_hGoogleList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hGoogleList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES);
    Theme::ApplyDarkScrollbar(_hGoogleList);
    {
        LVCOLUMN col2 = {}; col2.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT; col2.fmt = LVCFMT_LEFT;
        struct ColDef2 { const wchar_t* name; int w; };
        static const ColDef2 GC[] = {
            {L"IP Address", 130}, {L"Device Name", 180}, {L"Model", 160}, {L"Status", 70}, {L"Firmware", 100}
        };
        for (int i = 0; i < 5; i++) { col2.cx = GC[i].w; col2.pszText = (LPWSTR)GC[i].name; ListView_InsertColumn(_hGoogleList, i, &col2); }
    }
    y += 128;

    {
        HWND btn1 = mkBtn(L"\u21BB Sync Cast Info", IDC_BTN_GOOGLE_SYNC, 16, y, 130, 26);
        HWND btn2 = mkBtn(L"\U0001F310 Open Google Home", IDC_BTN_GOOGLE_OPEN, 154, y, 156, 26);
        (void)btn1; (void)btn2;
    }
    y += 34;

    _hGoogleLog = mkEdit(nullptr, IDC_SMART_GOOGLE_LOG, 16, y, cx - 32, 60, true);
    SetWindowText(_hGoogleLog,
        L"Sync queries each discovered Google/Nest/Chromecast device for Cast device info.\r\n"
        L"No Google account required \u2014 queries the local Cast companion port (8008) directly.");
    y += 68;

    // ── DEVICE SECURITY ──────────────────────────────────────────────────────
    y += 8;
    mkSection(L"Device Security Assessment", y); y += 24;

    _hSecurityLog = mkEdit(nullptr, 0, 16, y, cx - 32, 100, true);
    SetWindowText(_hSecurityLog,
        L"Run a network scan to see per-device security notes.\r\n"
        L"Checks: Telnet exposed, FTP exposed, HTTP-only admin panels, open camera streams.");
    y += 108;

    // ── AUTOMATION TRIGGERS ──────────────────────────────────────────────────
    y += 8;
    mkSection(L"Automation Triggers", y); y += 24;

    mkLbl(L"WHEN", 16, y, 46);
    _hComboTriggerEvent = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        60, y - 2, 190, 200, hwnd, (HMENU)(INT_PTR)IDC_SMART_TRIGGER_EVENT, hInst, nullptr);
    SendMessage(_hComboTriggerEvent, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    const wchar_t* events[] = {
        L"New device joins", L"Device goes offline", L"Risky port detected",
        L"Internet outage", L"High latency", L"Scan complete", nullptr
    };
    for (int i = 0; events[i]; i++) SendMessage(_hComboTriggerEvent, CB_ADDSTRING, 0, (LPARAM)events[i]);
    SendMessage(_hComboTriggerEvent, CB_SETCURSEL, 0, 0);

    mkLbl(L"\u2192 THEN", 258, y, 52);
    _hComboTriggerAction = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        314, y - 2, 190, 200, hwnd, (HMENU)(INT_PTR)IDC_SMART_TRIGGER_ACTION, hInst, nullptr);
    SendMessage(_hComboTriggerAction, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    const wchar_t* actions[] = {
        L"Flash Hue lights red", L"Send webhook notification",
        L"Log to ledger", L"Run port scan on device",
        L"Send push notification", nullptr
    };
    for (int i = 0; actions[i]; i++) SendMessage(_hComboTriggerAction, CB_ADDSTRING, 0, (LPARAM)actions[i]);
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
    col.pszText = (LPWSTR)L"When";   col.cx = 190; ListView_InsertColumn(_hTriggerList, 0, &col);
    col.pszText = (LPWSTR)L"Then";   col.cx = 200; ListView_InsertColumn(_hTriggerList, 1, &col);
    col.pszText = (LPWSTR)L"Status"; col.cx = 80;  ListView_InsertColumn(_hTriggerList, 2, &col);
    y += 110;

    _contentHeight = y + 20;
}

void TabSmartHome::LayoutControls(int cx, int cy) {
    _viewHeight = cy;
    UpdateScrollBar(_hwnd);
}

// ── Device population ─────────────────────────────────────────────────────────

void TabSmartHome::PopulateSmartDevices() {
    if (!_hDeviceList || !_mainWnd) return;
    ListView_DeleteAllItems(_hDeviceList);

    ScanResult r = _mainWnd->GetLastResult();
    int row = 0;
    for (auto& d : r.devices) {
        if (!IsSmartDevice(d)) continue;
        wstring name = !d.customName.empty() ? d.customName
                     : !d.hostname.empty()   ? d.hostname
                     : !d.vendor.empty()     ? d.vendor
                     : L"Unknown Device";
        wstring platform = SmartPlatform(d);
        wstring security = SecurityNote(d);

        LVITEM lvi = {}; lvi.mask = LVIF_TEXT; lvi.iItem = row;
        lvi.pszText = (LPWSTR)d.ip.c_str();
        ListView_InsertItem(_hDeviceList, &lvi);
        ListView_SetItemText(_hDeviceList, row, 1, (LPWSTR)name.c_str());
        ListView_SetItemText(_hDeviceList, row, 2, (LPWSTR)d.deviceType.c_str());
        ListView_SetItemText(_hDeviceList, row, 3, (LPWSTR)platform.c_str());
        ListView_SetItemText(_hDeviceList, row, 4, (LPWSTR)security.c_str());
        ListView_SetItemText(_hDeviceList, row, 5, (LPWSTR)(d.online ? L"Online" : L"Offline"));
        row++;
    }
    PopulateSecurityLog();
}

void TabSmartHome::PopulateSecurityLog() {
    if (!_hSecurityLog || !_mainWnd) return;
    ScanResult r = _mainWnd->GetLastResult();

    wstring log;
    int issues = 0;
    for (auto& d : r.devices) {
        if (!IsSmartDevice(d)) continue;
        wstring note = SecurityNote(d);
        if (note.empty()) continue;
        wstring name = !d.customName.empty() ? d.customName
                     : !d.hostname.empty()   ? d.hostname
                     : d.ip;
        log += name + L"  (" + d.ip + L")  \u2014  " + note + L"\r\n";
        issues++;
    }
    if (log.empty())
        log = issues == 0 && r.devices.empty()
            ? L"Run a network scan to see per-device security notes.\r\nChecks: Telnet, FTP, HTTP-only admin panels, open camera streams."
            : L"\u2714 No security issues found on smart home devices.";
    SetWindowText(_hSecurityLog, log.c_str());
}

// ── Home Assistant ────────────────────────────────────────────────────────────

void TabSmartHome::HaAppendLog(const wstring& text) {
    if (!_hHaLog) return;
    int len = GetWindowTextLength(_hHaLog);
    SendMessage(_hHaLog, EM_SETSEL, len, len);
    SendMessage(_hHaLog, EM_REPLACESEL, FALSE, (LPARAM)(L"\r\n" + text).c_str());
}

void TabSmartHome::HaConnect() {
    wchar_t urlBuf[512] = {}, tokBuf[1024] = {};
    if (_hHaUrl)   GetWindowText(_hHaUrl,   urlBuf, 512);
    if (_hHaToken) GetWindowText(_hHaToken, tokBuf, 1024);

    if (!urlBuf[0]) {
        HaAppendLog(L"[ERROR] Enter the Home Assistant URL (e.g. http://192.168.1.x:8123)");
        return;
    }
    if (!tokBuf[0]) {
        HaAppendLog(L"[ERROR] Paste a Long-Lived Access Token (HA \u2192 Profile \u2192 Security)");
        return;
    }
    _haBaseUrl = urlBuf;
    _haToken   = tokBuf;

    // Strip trailing slash
    if (!_haBaseUrl.empty() && _haBaseUrl.back() == L'/') _haBaseUrl.pop_back();

    HaAppendLog(L"[INFO] Connecting to " + _haBaseUrl + L" ...");
    if (_hHaStatus) SetWindowText(_hHaStatus, L"Status: Connecting...");

    wstring url   = _haBaseUrl + L"/api/";
    wstring token = _haToken;
    HWND hwnd     = _hwnd;
    std::thread([url, token, hwnd]() {
        string resp = HttpGetJson(url, token);
        PostMessage(hwnd, WM_HA_CONNECT, 0, (LPARAM)new wstring(Utf8ToWide(resp)));
    }).detach();
}

void TabSmartHome::HaSync() {
    if (_haBaseUrl.empty() || _haToken.empty()) {
        HaAppendLog(L"[ERROR] Connect to Home Assistant first.");
        return;
    }
    HaAppendLog(L"[INFO] Fetching entity states...");
    wstring url   = _haBaseUrl + L"/api/states";
    wstring token = _haToken;
    HWND hwnd     = _hwnd;
    std::thread([url, token, hwnd]() {
        string resp = HttpGetJson(url, token);
        PostMessage(hwnd, WM_HA_SYNC, 0, (LPARAM)new wstring(Utf8ToWide(resp)));
    }).detach();
}

// ── Philips Hue ───────────────────────────────────────────────────────────────

void TabSmartHome::HueAppendLog(const wstring& text) {
    if (!_hHueLog) return;
    int len = GetWindowTextLength(_hHueLog);
    SendMessage(_hHueLog, EM_SETSEL, len, len);
    SendMessage(_hHueLog, EM_REPLACESEL, FALSE, (LPARAM)(L"\r\n" + text).c_str());
}

void TabSmartHome::HueDiscover() {
    HueAppendLog(L"[INFO] Searching for Hue bridge on network...");
    if (_hHueStatus) SetWindowText(_hHueStatus, L"Status: Searching...");

    if (!_mainWnd) { HueAppendLog(L"[INFO] Run a scan first to see discovered devices."); return; }

    // Look for Hue bridge in scan results (mDNS _hue._tcp or Philips vendor on port 80)
    ScanResult r = _mainWnd->GetLastResult();
    wstring found;
    for (auto& d : r.devices) {
        bool isHue = false;
        for (auto& svc : d.mdnsServices)
            if (svc.find(L"_hue") != wstring::npos) { isHue = true; break; }
        if (!isHue && d.vendor.find(L"Philips") != wstring::npos) {
            for (int p : d.openPorts) if (p == 80) { isHue = true; break; }
        }
        if (isHue) { found = d.ip; break; }
    }

    HWND hwnd = _hwnd;
    if (!found.empty()) {
        PostMessage(hwnd, WM_HUE_DISC, 0, (LPARAM)new wstring(L"found:" + found));
    } else {
        PostMessage(hwnd, WM_HUE_DISC, 0, (LPARAM)new wstring(L"not found"));
    }
}

void TabSmartHome::HuePair() {
    wchar_t ipBuf[64] = {};
    if (_hHueBridgeIp) GetWindowText(_hHueBridgeIp, ipBuf, 64);
    if (!ipBuf[0]) {
        HueAppendLog(L"[ERROR] Enter or discover the bridge IP first.");
        return;
    }
    _hueBridgeIp = ipBuf;
    HueAppendLog(L"[INFO] Pairing with bridge at " + _hueBridgeIp + L" ...");
    HueAppendLog(L"[INFO] Make sure you pressed the link button on the bridge!");

    wstring host = _hueBridgeIp;
    HWND hwnd = _hwnd;
    std::thread([host, hwnd]() {
        string resp = HttpPostLocal(host, 80, L"/api",
                                    "{\"devicetype\":\"transparency#windows\"}");
        PostMessage(hwnd, WM_HUE_PAIR, 0, (LPARAM)new wstring(Utf8ToWide(resp)));
    }).detach();
}

void TabSmartHome::HueSync() {
    if (_hueBridgeIp.empty()) {
        HueAppendLog(L"[ERROR] Discover and pair with a bridge first.");
        return;
    }
    if (_hueUsername.empty()) {
        HueAppendLog(L"[ERROR] Complete pairing first — the bridge username is required.");
        return;
    }
    HueAppendLog(L"[INFO] Fetching lights from bridge " + _hueBridgeIp + L" ...");
    wstring host = _hueBridgeIp, user = _hueUsername;
    HWND hwnd = _hwnd;
    std::thread([host, user, hwnd]() {
        string resp = HttpGetJson(L"http://" + host + L"/api/" + user + L"/lights");
        PostMessage(hwnd, WM_HUE_SYNC, 0, (LPARAM)new wstring(Utf8ToWide(resp)));
    }).detach();
}

// ── Amazon Alexa ──────────────────────────────────────────────────────────────

void TabSmartHome::AlexaRefresh() {
    if (!_hAlexaList || !_mainWnd) return;
    ListView_DeleteAllItems(_hAlexaList);

    ScanResult r = _mainWnd->GetLastResult();
    int row = 0;
    for (auto& d : r.devices) {
        if (!IsAlexaDevice(d)) continue;
        wstring name = !d.customName.empty() ? d.customName
                     : !d.hostname.empty()   ? d.hostname
                     : !d.vendor.empty()     ? d.vendor
                     : L"Amazon Echo";
        wstring model = !d.deviceType.empty() ? d.deviceType : d.vendor;

        LVITEM lvi = {}; lvi.mask = LVIF_TEXT; lvi.iItem = row;
        lvi.pszText = (LPWSTR)d.ip.c_str();
        ListView_InsertItem(_hAlexaList, &lvi);
        ListView_SetItemText(_hAlexaList, row, 1, (LPWSTR)name.c_str());
        ListView_SetItemText(_hAlexaList, row, 2, (LPWSTR)model.c_str());
        ListView_SetItemText(_hAlexaList, row, 3, (LPWSTR)(d.online ? L"Online" : L"Offline"));
        row++;
    }
    if (row == 0) {
        LVITEM lvi = {}; lvi.mask = LVIF_TEXT; lvi.iItem = 0;
        lvi.pszText = (LPWSTR)L"";
        ListView_InsertItem(_hAlexaList, &lvi);
        ListView_SetItemText(_hAlexaList, 0, 1,
            (LPWSTR)L"No Alexa devices found \u2014 run a scan first");
    }
}

// ── Google Home / Cast ────────────────────────────────────────────────────────

void TabSmartHome::GoogleAppendLog(const wstring& text) {
    if (!_hGoogleLog) return;
    int len = GetWindowTextLength(_hGoogleLog);
    SendMessage(_hGoogleLog, EM_SETSEL, len, len);
    SendMessage(_hGoogleLog, EM_REPLACESEL, FALSE, (LPARAM)(L"\r\n" + text).c_str());
}

void TabSmartHome::GoogleSync() {
    if (!_mainWnd) {
        GoogleAppendLog(L"[INFO] Run a scan first to discover Google Cast devices.");
        return;
    }
    ScanResult r = _mainWnd->GetLastResult();
    ListView_DeleteAllItems(_hGoogleList);

    int count = 0;
    HWND hwnd = _hwnd;
    for (auto& d : r.devices) {
        if (!IsGoogleDevice(d)) continue;
        wstring ip = d.ip;
        count++;
        std::thread([ip, hwnd]() {
            string resp = HttpGetJson(L"http://" + ip + L":8008/setup/eureka_info");
            PostMessage(hwnd, WM_GOOGLE_CAST, 0,
                (LPARAM)new std::pair<wstring, string>(ip, resp));
        }).detach();
    }
    if (count == 0)
        GoogleAppendLog(L"[INFO] No Google Cast / Nest devices found. Run a scan first.");
    else
        GoogleAppendLog(L"[INFO] Querying " + std::to_wstring(count) +
                        L" Cast device(s) for local device info...");
}

// ── Command handling ──────────────────────────────────────────────────────────

LRESULT TabSmartHome::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    switch (LOWORD(wp)) {
    case IDC_BTN_HA_CONNECT:      HaConnect();  break;
    case IDC_BTN_HA_SYNC:         HaSync();     break;
    case IDC_BTN_HUE_DISCOVER:    HueDiscover(); break;
    case IDC_BTN_HUE_PAIR:        HuePair();    break;
    case IDC_BTN_HUE_SYNC:        HueSync();    break;

    case IDC_BTN_ALEXA_REFRESH:   AlexaRefresh(); break;
    case IDC_BTN_ALEXA_OPEN:
        ShellExecute(nullptr, L"open", L"https://alexa.amazon.com",
                     nullptr, nullptr, SW_SHOWNORMAL);
        break;
    case IDC_BTN_GOOGLE_SYNC:     GoogleSync();   break;
    case IDC_BTN_GOOGLE_OPEN:
        ShellExecute(nullptr, L"open", L"https://home.google.com",
                     nullptr, nullptr, SW_SHOWNORMAL);
        break;

    case IDC_BTN_SMART_ADD_TRIGGER: {
        if (!_hComboTriggerEvent || !_hComboTriggerAction || !_hTriggerList) break;
        wchar_t evtBuf[128], actBuf[128];
        int ei = (int)SendMessage(_hComboTriggerEvent,  CB_GETCURSEL, 0, 0);
        int ai = (int)SendMessage(_hComboTriggerAction, CB_GETCURSEL, 0, 0);
        SendMessage(_hComboTriggerEvent,  CB_GETLBTEXT, ei, (LPARAM)evtBuf);
        SendMessage(_hComboTriggerAction, CB_GETLBTEXT, ai, (LPARAM)actBuf);
        int idx = ListView_GetItemCount(_hTriggerList);
        LVITEM lvi = {}; lvi.mask = LVIF_TEXT; lvi.iItem = idx; lvi.pszText = evtBuf;
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
    }
    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

LRESULT TabSmartHome::OnNotify(HWND hwnd, NMHDR* hdr) {
    return DefWindowProc(hwnd, WM_NOTIFY, 0, (LPARAM)hdr);
}

void TabSmartHome::RefreshDevices() {
    PopulateSmartDevices();
    AlexaRefresh();
}
