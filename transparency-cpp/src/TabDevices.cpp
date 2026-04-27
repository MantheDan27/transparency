#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <cwctype>
#include <sstream>
#include <vector>
#include <algorithm>
#include <mutex>

#include "TabDevices.h"
#include "MainWindow.h"
#include "Theme.h"
#include "Resource.h"
#include "Scanner.h"
#include <shellapi.h>
#include <ws2tcpip.h>

using std::wstring;



const wchar_t* TabDevices::s_className = L"TransparencyTabDevices";

static const wchar_t* FILTER_LABELS[] = {
    L"All", L"Online", L"Unknown", L"Watchlist", L"Owned", L"Changed"
};

// ── Glossary popup ────────────────────────────────────────────────────────────

static HWND s_glossaryPopup = nullptr;

struct PopupData { wstring title; wstring body; };

struct AcronymEntry { const wchar_t* key; const wchar_t* explanation; };
static const AcronymEntry ACRONYM_DB[] = {
    { L"ARP",
      L"Address Resolution Protocol — How your network maps IP addresses to physical device addresses. "
      L"When your router needs to reach 192.168.1.5, it broadcasts 'Who has this IP?' and the device "
      L"replies with its hardware (MAC) address." },
    { L"MAC",
      L"Media Access Control Address — A unique 12-character identifier (e.g. AA:BB:CC:DD:EE:FF) permanently "
      L"burned into every network adapter by its manufacturer. The first 6 characters identify the maker (OUI); "
      L"the last 6 are unique to the device. Unlike an IP address, a MAC address does not change between networks." },
    { L"mDNS",
      L"Multicast DNS — Lets devices on your local network announce and find each other by name without a "
      L"central server. Your Mac broadcasts '_http._tcp.local' so other devices can discover its web service "
      L"automatically. Commonly used by Apple, Chromecast, and smart home devices." },
    { L"MDNS", L"See mDNS — Multicast DNS. Lets devices discover each other by name on a local network without a central server." },
    { L"DHCP",
      L"Dynamic Host Configuration Protocol — The service (usually on your router) that automatically hands out "
      L"IP addresses when devices join your network. Without DHCP you would need to set a unique IP on every device "
      L"manually. If DHCP fails, a device may fall back to a self-assigned 169.254.x.x (APIPA) address." },
    { L"DNS",
      L"Domain Name System — The internet's address book. Translates human-readable names like 'google.com' "
      L"into numeric IP addresses like '142.250.80.78' that computers use for routing. Your DNS server is "
      L"usually your router or a public resolver such as 8.8.8.8 (Google) or 1.1.1.1 (Cloudflare)." },
    { L"TCP",
      L"Transmission Control Protocol — The reliable delivery layer used by most internet services (web, email, SSH). "
      L"It guarantees data arrives complete and in order by re-sending lost packets. Slower than UDP, "
      L"but essential when accuracy matters." },
    { L"UDP",
      L"User Datagram Protocol — A faster but 'fire-and-forget' alternative to TCP. Used for video streaming, "
      L"online games, and DNS lookups where speed matters more than guaranteed delivery. "
      L"Lost UDP packets are not re-sent." },
    { L"OUI",
      L"Organizationally Unique Identifier — The first three bytes of a MAC address, assigned by the IEEE to "
      L"identify the chip manufacturer. For example, 'B8:27:EB' is a Raspberry Pi, 'A4:CF:12' is a Nest device. "
      L"This is how Transparency identifies vendors without actively scanning the device." },
    { L"APIPA",
      L"Automatic Private IP Addressing — When a device cannot reach a DHCP server it assigns itself a "
      L"169.254.x.x address. The device can still talk to other APIPA devices on the same cable/Wi-Fi, "
      L"but cannot access the internet or other subnets. Usually signals a network configuration problem." },
    { L"NAT",
      L"Network Address Translation — How your router lets many private-network devices share a single public "
      L"internet IP address. Outgoing traffic gets its source IP rewritten to your router's public IP; "
      L"replies are rewritten back. This is why all home devices appear to have the same internet-facing IP." },
    { L"ICMP",
      L"Internet Control Message Protocol — The protocol behind 'ping'. Used to test whether a device is "
      L"reachable and measure round-trip time. It also carries error messages like 'Destination Unreachable'. "
      L"ICMP does not carry application data — only control and diagnostic information." },
    { L"NDP",
      L"Neighbor Discovery Protocol — The IPv6 replacement for ARP. Allows IPv6 devices to discover each "
      L"other's MAC addresses, find routers, and detect duplicate addresses, all via multicast messages "
      L"rather than the broadcast packets ARP uses in IPv4." },
    { L"NetBIOS",
      L"Network Basic Input/Output System — An older Microsoft protocol for local network name resolution "
      L"and file/printer sharing, common before DNS was widely used in LANs. Still present in Windows as "
      L"part of SMB (file sharing). Devices respond to NetBIOS queries with their computer name." },
    { L"SSDP",
      L"Simple Service Discovery Protocol — The announcement layer used by UPnP devices (smart TVs, "
      L"game consoles, printers). Devices send periodic SSDP multicast messages to advertise their services. "
      L"This is how Windows automatically finds your network printer or TV." },
    { L"UPnP",
      L"Universal Plug and Play — Protocols that let devices configure themselves and discover each other "
      L"automatically. Commonly used by game consoles, streaming sticks, and smart home gear. "
      L"Can be a security risk if your router allows UPnP to open ports exposed to the internet." },
    { L"WOL",
      L"Wake-on-LAN — A feature that lets you remotely power on a computer by sending a 'magic packet' "
      L"containing the target's MAC address over the network. The target must support WOL and have it "
      L"enabled in its BIOS/UEFI settings." },
    { L"IPv6",
      L"Internet Protocol version 6 — The modern addressing system replacing IPv4. Uses 128-bit addresses "
      L"(e.g. 2001:db8::1) to support the enormous number of internet-connected devices. "
      L"A device often has both an IPv4 and an IPv6 address at the same time." },
    { L"BSSID",
      L"Basic Service Set Identifier — The MAC address of the specific Wi-Fi access point you are connected to. "
      L"If your router broadcasts on both 2.4 GHz and 5 GHz, each radio has a different BSSID "
      L"even though they share the same SSID (network name)." },
    { L"SSID",
      L"Service Set Identifier — The human-readable name of a Wi-Fi network (what you see when scanning "
      L"for Wi-Fi on your phone). Multiple access points can share the same SSID to form a seamless "
      L"roaming network while each has a unique BSSID." },
    { L"RFC",
      L"Request for Comments — Official internet standards documents published by the IETF. "
      L"Despite the name, most RFCs are finalized standards. 'RFC 1918' defines the private IP ranges "
      L"(10.x.x.x, 172.16-31.x.x, 192.168.x.x) reserved for local networks and not routable on the internet." },
    { L"IANA",
      L"Internet Assigned Numbers Authority — The global body that coordinates IP address allocation, "
      L"DNS root zones, and protocol registries. IANA allocates large address blocks to regional bodies "
      L"(like ARIN for North America) which then distribute them to ISPs and organizations." },
    { L"ISP",
      L"Internet Service Provider — The company that sells you internet access (e.g. Comcast, AT&T, BT). "
      L"Your ISP assigns your router a public IP address and connects your network to the broader internet. "
      L"Some ISPs use carrier-grade NAT to share one public IP among many customers." },
};

static const wchar_t* LookupAcronym(const wstring& key) {
    for (auto& e : ACRONYM_DB) {
        if (key == e.key) return e.explanation;
    }
    return nullptr;
}

static wstring EscapeForLink(const wstring& s) {
    wstring out;
    out.reserve(s.size() + 8);
    for (wchar_t c : s) {
        if      (c == L'&') out += L"&amp;";
        else if (c == L'<') out += L"&lt;";
        else if (c == L'>') out += L"&gt;";
        else                out += c;
    }
    return out;
}

static LRESULT CALLBACK GlossaryPopupProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* data = reinterpret_cast<PopupData*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        auto* d  = reinterpret_cast<PopupData*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d));

        HINSTANCE hInst = GetModuleHandle(nullptr);

        // Body text static (multi-line, word-wrap)
        HWND hBody = CreateWindowEx(0, L"STATIC", d->body.c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            12, 44, 336, 138, hwnd, nullptr, hInst, nullptr);
        SendMessage(hBody, WM_SETFONT, (WPARAM)Theme::FontBodySm(), TRUE);

        // Close button
        HWND hClose = CreateWindowEx(0, L"BUTTON", L"Close",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            130, 190, 100, 26, hwnd, (HMENU)IDCANCEL, hInst, nullptr);
        SendMessage(hClose, WM_SETFONT, (WPARAM)Theme::FontBodySm(), TRUE);

        // × button in header
        HWND hX = CreateWindowEx(0, L"BUTTON", L"\xD7",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            330, 7, 22, 22, hwnd, (HMENU)IDCANCEL, hInst, nullptr);
        SendMessage(hX, WM_SETFONT, (WPARAM)Theme::FontBodySm(), TRUE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        FillRect(hdc, &rc, Theme::BrushElevated());

        RECT hdr = { 0, 0, rc.right, 36 };
        FillRect(hdc, &hdr, Theme::BrushOverlay());

        if (data) {
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, Theme::ACCENT_BLUE);
            HFONT oldFont = (HFONT)SelectObject(hdc, Theme::FontBold());
            RECT titleRc = { 12, 6, rc.right - 32, 30 };
            DrawText(hdc, data->title.c_str(), -1, &titleRc,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            SelectObject(hdc, oldFont);
        }

        RECT sep = { 0, 36, rc.right, 37 };
        FillRect(hdc, &sep, Theme::BrushBorderDefault());

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, Theme::TEXT_SECONDARY);
        SetBkColor(hdc, Theme::BG_ELEVATED);
        return (LRESULT)Theme::BrushElevated();
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDCANCEL) DestroyWindow(hwnd);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        delete data;
        if (s_glossaryPopup == hwnd) s_glossaryPopup = nullptr;
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static void ShowGlossaryPopup(HWND rootWnd, const wstring& title, const wstring& body) {
    if (s_glossaryPopup) {
        DestroyWindow(s_glossaryPopup);
        s_glossaryPopup = nullptr;
    }
    auto* data = new PopupData{ title, body };

    POINT pt; GetCursorPos(&pt);
    const int PW = 360, PH = 224;
    HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {}; mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMon, &mi);
    int x = pt.x + 12;
    int y = pt.y + 12;
    if (x + PW > mi.rcWork.right)  x = mi.rcWork.right  - PW - 4;
    if (y + PH > mi.rcWork.bottom) y = mi.rcWork.bottom - PH - 4;
    if (x < mi.rcWork.left) x = mi.rcWork.left + 4;
    if (y < mi.rcWork.top)  y = mi.rcWork.top  + 4;

    s_glossaryPopup = CreateWindowEx(
        WS_EX_TOPMOST,
        L"TranspGlossaryPopup", nullptr,
        WS_POPUP | WS_BORDER,
        x, y, PW, PH,
        rootWnd, nullptr, GetModuleHandle(nullptr), data);

    if (s_glossaryPopup) ShowWindow(s_glossaryPopup, SW_SHOWNOACTIVATE);
    else delete data;
}

// Subclass proc that fills WC_LINK background with the panel's beige surface color
static LRESULT CALLBACK LinkSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                          UINT_PTR uid, DWORD_PTR) {
    if (msg == WM_ERASEBKGND) {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, Theme::BrushSurface());
        return 1;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// Subclass proc for detail scroll panel
static LRESULT CALLBACK DetailPanelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR dwData) {
    if (msg == WM_COMMAND)  return SendMessage(GetParent(hwnd), msg, wp, lp);
    if (msg == WM_NOTIFY)   return SendMessage(GetParent(hwnd), msg, wp, lp);
    if (msg == WM_DRAWITEM) return SendMessage(GetParent(hwnd), msg, wp, lp);
    if (msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORBTN) {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SetBkColor(hdc, Theme::BG_SURFACE);
        return (LRESULT)Theme::BrushSurface();
    }
    auto* self = reinterpret_cast<TabDevices*>(dwData);
    if (msg == WM_VSCROLL    && self) return self->OnDetailScroll(hwnd, wp);
    if (msg == WM_MOUSEWHEEL && self) return self->OnDetailMouseWheel(hwnd, wp);
    if (msg == WM_ERASEBKGND) {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wp, &rc, Theme::BrushSurface());
        return 1;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

bool TabDevices::Create(HWND parent, int x, int y, int w, int h, MainWindow* mainWnd) {
    _mainWnd = mainWnd;

    static bool s_classesRegistered = false;
    if (!s_classesRegistered) {
        WNDCLASSEX wc = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = WndProc;
        wc.hInstance     = GetModuleHandle(nullptr);
        wc.hbrBackground = Theme::BrushSurface();
        wc.lpszClassName = s_className;
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        RegisterClassEx(&wc);

        // Custom class for the scrollable detail panel
        WNDCLASSEX wc2 = {};
        wc2.cbSize        = sizeof(wc2);
        wc2.lpfnWndProc   = DefWindowProc;
        wc2.hInstance     = GetModuleHandle(nullptr);
        wc2.hbrBackground = Theme::BrushSurface();
        wc2.lpszClassName = L"TranspDetailScroll";
        RegisterClassEx(&wc2);

        // Glossary popup window class
        WNDCLASSEX wc3 = {};
        wc3.cbSize        = sizeof(wc3);
        wc3.lpfnWndProc   = GlossaryPopupProc;
        wc3.hInstance     = GetModuleHandle(nullptr);
        wc3.hbrBackground = Theme::BrushElevated();
        wc3.lpszClassName = L"TranspGlossaryPopup";
        RegisterClassEx(&wc3);

        s_classesRegistered = true;
    }

    _hwnd = CreateWindowEx(0, s_className, nullptr,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        x, y, w, h, parent, nullptr, GetModuleHandle(nullptr), this);

    return _hwnd != nullptr;
}

LRESULT CALLBACK TabDevices::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TabDevices* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<TabDevices*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        self->_hwnd = hwnd;
    } else {
        self = reinterpret_cast<TabDevices*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (!self) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CREATE:     return self->OnCreate(hwnd, reinterpret_cast<LPCREATESTRUCT>(lp));
    case WM_SIZE:       self->OnSize(hwnd, LOWORD(lp), HIWORD(lp)); return 0;
    case WM_PAINT:      return self->OnPaint(hwnd);
    case WM_ERASEBKGND: return 1;  // Suppress — OnPaint handles all drawing
    case WM_COMMAND:    return self->OnCommand(hwnd, wp, lp);
    case WM_NOTIFY:     return self->OnNotify(hwnd, reinterpret_cast<NMHDR*>(lp));
    case WM_DRAWITEM: {
        auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        if (dis && dis->CtlID >= IDC_BTN_FILTER_ALL && dis->CtlID <= IDC_BTN_FILTER_CHANGED) {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            bool pressed = (dis->itemState & ODS_SELECTED) != 0;
            int filterIdx = dis->CtlID - IDC_BTN_FILTER_ALL;
            bool active = (filterIdx == self->_filterMode);

            Theme::DrawGlassPill(hdc, rc, 15, active, pressed);

            wchar_t text[32] = {};
            GetWindowText(dis->hwndItem, text, 32);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, active ? Theme::ACCENT_BLUE : Theme::TEXT_SECONDARY);
            HFONT old = (HFONT)SelectObject(hdc, Theme::FontCaption());
            DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, old);
            return TRUE;
        }
        // Save / Close / Pause buttons
        if (dis && (dis->CtlID == IDC_BTN_DEVICE_SAVE || dis->CtlID == 9500 ||
                    dis->CtlID == IDC_BTN_DEVICE_PAUSE)) {
            HDC hdc = dis->hDC;
            RECT rc = dis->rcItem;
            bool pressed = (dis->itemState & ODS_SELECTED) != 0;
            bool focused = (dis->itemState & ODS_FOCUS) != 0;

            int variant = 1;
            COLORREF textColor = Theme::TEXT_PRIMARY;

            if (dis->CtlID == IDC_BTN_DEVICE_SAVE) {
                variant = 0;
                textColor = RGB(255, 255, 255);
            } else if (dis->CtlID == IDC_BTN_DEVICE_PAUSE) {
                wchar_t btnText[32] = {};
                GetWindowText(dis->hwndItem, btnText, 32);
                bool isPaused = (wcsncmp(btnText, L"Resume", 6) == 0);
                variant = isPaused ? 2 : 1;
                textColor = isPaused ? Theme::ACCENT_RED : Theme::ACCENT_AMBER;
            }

            Theme::DrawGlassButton(hdc, rc, Theme::RADIUS_MD, pressed, variant, focused);

            wchar_t text[64] = {};
            GetWindowText(dis->hwndItem, text, 64);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, textColor);
            HFONT old = (HFONT)SelectObject(hdc, Theme::FontNavActive());
            DrawText(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, old);
            return TRUE;
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, Theme::TEXT_PRIMARY);
        SetBkColor(hdc, Theme::BG_SURFACE);
        return (LRESULT)Theme::BrushSurface();
    }
    case WM_SCAN_COMPLETE: return self->OnScanComplete(hwnd);
    case WM_MOUSEWHEEL: {
        // Forward wheel to detail panel when cursor is over it
        if (self->_detailVisible && self->_hDetailPanel) {
            POINT pt; GetCursorPos(&pt);
            RECT panelRc; GetWindowRect(self->_hDetailPanel, &panelRc);
            if (PtInRect(&panelRc, pt))
                return SendMessage(self->_hDetailPanel, WM_MOUSEWHEEL, wp, lp);
        }
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    default: return DefWindowProc(hwnd, msg, wp, lp);
    }
}

LRESULT TabDevices::OnCreate(HWND hwnd, LPCREATESTRUCT cs) {
    RECT rc; GetClientRect(hwnd, &rc);
    CreateControls(hwnd, rc.right, rc.bottom);
    return 0;
}

void TabDevices::CreateControls(HWND hwnd, int cx, int cy) {
    HINSTANCE hInst = GetModuleHandle(nullptr);

    // Search box
    _hSearch = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_LEFT | ES_AUTOHSCROLL,
        16, 12, 260, 26, hwnd, (HMENU)IDC_EDIT_SEARCH, hInst, nullptr);
    SendMessage(_hSearch, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    SendMessage(_hSearch, EM_SETCUEBANNER, FALSE, (LPARAM)L"Search devices...");
    Theme::ApplyDarkEdit(_hSearch);

    // Filter buttons — owner-drawn pill style
    int btnX = 290;
    for (int i = 0; i < 6; i++) {
        _hFilterBtns[i] = CreateWindowEx(0, L"BUTTON", FILTER_LABELS[i],
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            btnX, 10, 72, 30, hwnd, (HMENU)(IDC_BTN_FILTER_ALL + i), hInst, nullptr);
        btnX += 76;
    }

    // List view
    int listW = _detailVisible ? cx - DETAIL_WIDTH - 32 : cx - 32;
    _hList = CreateWindowEx(
        WS_EX_CLIENTEDGE, WC_LISTVIEW, nullptr,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS |
        LVS_SINGLESEL | LVS_NOSORTHEADER | WS_VSCROLL | WS_HSCROLL,
        16, 48, listW, cy - 64,
        hwnd, (HMENU)IDC_LIST_DEVICES, hInst, nullptr);

    SendMessage(_hList, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    ListView_SetExtendedListViewStyle(_hList,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP);

    Theme::ApplyDarkScrollbar(_hList);

    // Columns
    struct ColDef { const wchar_t* name; int width; int fmt; };
    static const ColDef COLS[] = {
        { L"",           16,  LVCFMT_CENTER }, // Status dot
        { L"Name",       170, LVCFMT_LEFT   },
        { L"IP Address", 120, LVCFMT_LEFT   },
        { L"MAC",        130, LVCFMT_LEFT   },
        { L"Vendor",     110, LVCFMT_LEFT   },
        { L"Type",       140, LVCFMT_LEFT   },  // now shows confidence %
        { L"Trust",       80, LVCFMT_LEFT   },
        { L"Open Ports", 130, LVCFMT_LEFT   },
        { L"Seen",        50, LVCFMT_CENTER },  // sighting count
        { L"Last Seen",  110, LVCFMT_LEFT   },
    };

    LVCOLUMN col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    for (int i = 0; i < (int)(sizeof(COLS)/sizeof(COLS[0])); i++) {
        col.cx = COLS[i].width;
        col.pszText = (LPWSTR)COLS[i].name;
        col.fmt = COLS[i].fmt;
        ListView_InsertColumn(_hList, i, &col);
    }

    // Detail panel — scrollable custom class
    _hDetailPanel = CreateWindowEx(WS_EX_STATICEDGE, L"TranspDetailScroll", nullptr,
        WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        cx - DETAIL_WIDTH - 16, 48, DETAIL_WIDTH, cy - 64,
        hwnd, nullptr, hInst, nullptr);

    SetWindowSubclass(_hDetailPanel, DetailPanelProc, 0, (DWORD_PTR)this);

    // Layout constants — sized for actual font metrics
    // FontBody=16px → needs 20px, FontBodySm/Mono=14px → needs 18px
    const int PAD = 10;
    const int LW  = DETAIL_WIDTH - 2 * PAD;  // label width

    // Helper lambdas to create labels inside the detail panel
    auto makeBodyLbl = [&](const wchar_t* text, int y, int h) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", text,
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            PAD, y, LW, h, _hDetailPanel, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
        return hw;
    };
    auto makeSmLbl = [&](const wchar_t* text, int y, int h) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", text,
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            PAD, y, LW, h, _hDetailPanel, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontBodySm(), TRUE);
        return hw;
    };
    auto makeMonoLbl = [&](const wchar_t* text, int y, int h) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", text,
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            PAD, y, LW, h, _hDetailPanel, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontMono(), TRUE);
        return hw;
    };
    auto makeCaptionLbl = [&](const wchar_t* text, int y) -> HWND {
        HWND hw = CreateWindowEx(0, L"STATIC", text,
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            PAD, y, LW, 16, _hDetailPanel, nullptr, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)Theme::FontCaption(), TRUE);
        return hw;
    };

    // WC_LINK (SysLink) — used for clickable acronym and IP labels
    auto makeLinkLbl = [&](int id, const wchar_t* text, int y, int h,
                           HFONT font = nullptr) -> HWND {
        HWND hw = CreateWindowEx(0, WC_LINK, text,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            PAD, y, LW, h, _hDetailPanel, (HMENU)(INT_PTR)id, hInst, nullptr);
        SendMessage(hw, WM_SETFONT, (WPARAM)(font ? font : Theme::FontBodySm()), TRUE);
        SetWindowSubclass(hw, LinkSubclassProc, 0, 0);
        return hw;
    };

    int dy = 10;

    // ── Custom Name ──────────────────────────────────────────────────────────
    makeCaptionLbl(L"CUSTOM NAME", dy); dy += 18;
    _hDetailCustomName = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        PAD, dy, LW, 24, _hDetailPanel, (HMENU)IDC_EDIT_DEVICE_NAME, hInst, nullptr);
    SendMessage(_hDetailCustomName, WM_SETFONT, (WPARAM)Theme::FontBody(), TRUE);
    Theme::ApplyDarkEdit(_hDetailCustomName);
    dy += 30;

    // ── Device Identity ──────────────────────────────────────────────────────
    dy += 4;  // section gap
    _hDetailName     = makeLinkLbl(IDC_LINK_NAME_SRC, L"", dy, 20, Theme::FontBody()); dy += 24;
    _hDetailType     = makeSmLbl  (L"", dy, 18); dy += 22;   // type + confidence
    _hDetailEvidence = makeSmLbl  (L"", dy, 34); dy += 38;   // evidence (2 lines)
    _hDetailAlt      = makeSmLbl  (L"", dy, 30); dy += 34;   // alternatives (2 lines)

    // ── Network Info ─────────────────────────────────────────────────────────
    dy += 4;
    _hDetailVendor    = makeLinkLbl(IDC_LINK_VENDOR_SRC, L"", dy, 18); dy += 22;
    _hDetailIp        = makeLinkLbl(IDC_LINK_IP_ADDR,    L"", dy, 18, Theme::FontMono()); dy += 22;
    _hDetailMac       = makeLinkLbl(IDC_LINK_MAC_LABEL,  L"", dy, 18, Theme::FontMono()); dy += 22;
    _hDetailSubnet    = makeMonoLbl(L"", dy, 18); dy += 22;
    _hDetailIpHistory = makeSmLbl  (L"", dy, 18); dy += 22;

    // ── Timing ───────────────────────────────────────────────────────────────
    dy += 4;
    _hDetailFirstSeen  = makeSmLbl(L"", dy, 18); dy += 22;
    _hDetailLastSeen   = makeSmLbl(L"", dy, 18); dy += 22;
    _hDetailSightings  = makeSmLbl(L"", dy, 18); dy += 22;

    // ── Ports & Services ─────────────────────────────────────────────────────
    dy += 4;
    _hDetailPorts = makeSmLbl(L"", dy, 36); dy += 40;   // up to 2 wrapped lines
    _hDetailMdns  = makeLinkLbl(IDC_LINK_MDNS_SRC, L"", dy, 18); dy += 22;

    // IoT risk box (hidden when not applicable)
    _hDetailIotRisk = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        PAD, dy, LW, 60, _hDetailPanel, nullptr, hInst, nullptr);
    SendMessage(_hDetailIotRisk, WM_SETFONT, (WPARAM)Theme::FontBodySm(), TRUE);
    Theme::ApplyDarkEdit(_hDetailIotRisk);
    dy += 66;

    // ── Alerts ───────────────────────────────────────────────────────────────
    dy += 4;
    _hDetailAnoms = makeSmLbl(L"", dy, 50); dy += 56;

    // ── Notes ────────────────────────────────────────────────────────────────
    dy += 4;
    makeCaptionLbl(L"NOTES", dy); dy += 18;
    _hDetailNotes = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        PAD, dy, LW, 56, _hDetailPanel, (HMENU)IDC_EDIT_DEVICE_NOTES, hInst, nullptr);
    SendMessage(_hDetailNotes, WM_SETFONT, (WPARAM)Theme::FontBodySm(), TRUE);
    Theme::ApplyDarkEdit(_hDetailNotes);
    dy += 62;

    // ── Trust ────────────────────────────────────────────────────────────────
    dy += 4;
    makeCaptionLbl(L"TRUST", dy); dy += 18;
    _hDetailTrust = CreateWindowEx(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        PAD, dy, LW, 120, _hDetailPanel, (HMENU)IDC_COMBO_TRUST, hInst, nullptr);
    SendMessage(_hDetailTrust, WM_SETFONT, (WPARAM)Theme::FontBodySm(), TRUE);
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"unknown");
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"owned");
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"watchlist");
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"guest");
    SendMessage(_hDetailTrust, CB_ADDSTRING, 0, (LPARAM)L"blocked");
    dy += 30;

    // ── Network Pause ────────────────────────────────────────────────────────
    dy += 8;
    _hDetailPause = CreateWindowEx(0, L"BUTTON", L"Pause Network",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        PAD, dy, LW, 32, _hDetailPanel, (HMENU)IDC_BTN_DEVICE_PAUSE, hInst, nullptr);
    dy += 38;

    // ── Save + Close ─────────────────────────────────────────────────────────
    dy += 4;
    _hDetailSave = CreateWindowEx(0, L"BUTTON", L"Save",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        PAD, dy, (LW / 2) - 4, 36, _hDetailPanel, (HMENU)IDC_BTN_DEVICE_SAVE, hInst, nullptr);

    _hDetailClose = CreateWindowEx(0, L"BUTTON", L"Close",
        WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        PAD + (LW / 2) + 4, dy, (LW / 2) - 4, 36, _hDetailPanel, (HMENU)9500, hInst, nullptr);

    _detailContentH = dy + 36 + 10;  // total scrollable content height
}

void TabDevices::LayoutControls(int cx, int cy) {
    int listW = _detailVisible ? cx - DETAIL_WIDTH - 32 : cx - 32;
    if (_hList) SetWindowPos(_hList, nullptr, 16, 48, listW, cy - 64, SWP_NOZORDER);

    if (_hDetailPanel) {
        if (_detailVisible)
            SetWindowPos(_hDetailPanel, nullptr, cx - DETAIL_WIDTH - 16, 48, DETAIL_WIDTH, cy - 64, SWP_NOZORDER | SWP_SHOWWINDOW);
        else
            ShowWindow(_hDetailPanel, SW_HIDE);
    }

    // Reposition filter buttons
    int btnX = 290;
    for (int i = 0; i < 6; i++) {
        if (_hFilterBtns[i]) SetWindowPos(_hFilterBtns[i], nullptr, btnX, 12, 72, 26, SWP_NOZORDER);
        btnX += 76;
    }
}

LRESULT TabDevices::OnSize(HWND hwnd, int cx, int cy) {
    LayoutControls(cx, cy);
    return 0;
}

LRESULT TabDevices::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, Theme::BrushSurface());

    // Section separator under toolbar
    RECT sep = { 16, 44, rc.right - 16, 45 };
    FillRect(hdc, &sep, Theme::BrushBorderSubtle());

    // Section label — caption style
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, Theme::TEXT_TERTIARY);
    HFONT old = (HFONT)SelectObject(hdc, Theme::FontCaption());
    RECT hdr = { 16, 34, 200, 46 };
    DrawText(hdc, L"DEVICE LIST", -1, &hdr, DT_LEFT | DT_SINGLELINE);
    SelectObject(hdc, old);

    EndPaint(hwnd, &ps);
    return 0;
}

LRESULT TabDevices::OnCommand(HWND hwnd, WPARAM wp, LPARAM lp) {
    int id = LOWORD(wp);

    if (id >= IDC_BTN_FILTER_ALL && id <= IDC_BTN_FILTER_CHANGED) {
        _filterMode = id - IDC_BTN_FILTER_ALL;
        ApplyFilter();
        return 0;
    }

    if (id == 9500) { // Close detail
        HideDetailPanel();
        return 0;
    }

    if (id == IDC_BTN_DEVICE_SAVE) { // Save name/notes/trust back to device
        if (_mainWnd && !_detailDeviceIp.empty()) {
            wchar_t nameBuf[256] = {}, notesBuf[1024] = {};
            if (_hDetailCustomName) GetWindowText(_hDetailCustomName, nameBuf, 256);
            if (_hDetailNotes) GetWindowText(_hDetailNotes, notesBuf, 1024);

            int trustSel = _hDetailTrust ? (int)SendMessage(_hDetailTrust, CB_GETCURSEL, 0, 0) : 0;
            static const wchar_t* trustOpts[] = { L"unknown", L"owned", L"watchlist", L"guest", L"blocked" };
            wstring trust = (trustSel >= 0 && trustSel < 5) ? trustOpts[trustSel] : L"unknown";

            {
                std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
                for (auto& d : _mainWnd->_lastResult.devices) {
                    if (d.ip == _detailDeviceIp) {
                        d.customName = nameBuf;
                        d.notes = notesBuf;
                        d.trustState = trust;
                        break;
                    }
                }
            } // release lock before calling ApplyFilter/HideDetailPanel
            ApplyFilter();
            HideDetailPanel();
        }
        return 0;
    }

    if (id == IDC_BTN_DEVICE_PAUSE) {
        if (_mainWnd && !_detailDeviceIp.empty()) {
            bool newPaused = false;
            {
                std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
                for (auto& d : _mainWnd->_lastResult.devices) {
                    if (d.ip == _detailDeviceIp) {
                        d.paused = !d.paused;
                        newPaused = d.paused;
                        break;
                    }
                }
            }
            PauseDevice(_detailDeviceIp, newPaused);
            // Refresh the button state
            ScanResult r = _mainWnd->GetLastResult();
            for (const auto& d : r.devices) {
                if (d.ip == _detailDeviceIp) { UpdateDetailPanel(d); break; }
            }
        }
        return 0;
    }

    if (id == IDC_EDIT_SEARCH && HIWORD(wp) == EN_CHANGE) {
        ApplyFilter();
    }

    return DefWindowProc(hwnd, WM_COMMAND, wp, lp);
}

LRESULT TabDevices::OnNotify(HWND hwnd, NMHDR* hdr) {
    if (!hdr) return 0;

    // Glossary / IP-info popups triggered by WC_LINK clicks in the detail panel
    if (hdr->code == NM_CLICK || hdr->code == NM_RETURN) {
        if (hdr->idFrom == IDC_LINK_VENDOR_SRC ||
            hdr->idFrom == IDC_LINK_IP_ADDR     ||
            hdr->idFrom == IDC_LINK_MDNS_SRC    ||
            hdr->idFrom == IDC_LINK_MAC_LABEL   ||
            hdr->idFrom == IDC_LINK_NAME_SRC) {

            auto* nmLink = reinterpret_cast<NMLINK*>(hdr);
            wstring href(nmLink->item.szUrl);
            HWND rootWnd = GetAncestor(_hwnd, GA_ROOT);

            if (href == L"IP") {
                wstring desc = GetIpDescription(_detailDeviceIp);
                ShowGlossaryPopup(rootWnd, _detailDeviceIp + L"  —  IP Address", desc);
            } else {
                const wchar_t* exp = LookupAcronym(href);
                if (exp) ShowGlossaryPopup(rootWnd, href, exp);
            }
            return 1;
        }
    }

    if (hdr->idFrom == IDC_LIST_DEVICES) {
        switch (hdr->code) {
        case NM_RCLICK: {
            NMITEMACTIVATE* nia = reinterpret_cast<NMITEMACTIVATE*>(hdr);
            if (nia->iItem >= 0 && nia->iItem < (int)_filteredIndices.size()) {
                POINT pt;
                GetCursorPos(&pt);
                ShowDeviceContextMenu(hwnd, pt.x, pt.y, _filteredIndices[nia->iItem]);
            }
            return 0;
        }
        case NM_CLICK:
        case NM_DBLCLK: {
            NMITEMACTIVATE* nm = (NMITEMACTIVATE*)hdr;
            if (nm->iItem >= 0 && nm->iItem < (int)_filteredIndices.size()) {
                _selectedDevice = _filteredIndices[nm->iItem];
                ShowDetailPanel(_selectedDevice);
            }
            break;
        }
        case LVN_COLUMNCLICK: {
            NMLISTVIEW* nm = (NMLISTVIEW*)hdr;
            if (nm->iSubItem == _sortCol) _sortAsc = !_sortAsc;
            else { _sortCol = nm->iSubItem; _sortAsc = true; }
            ApplyFilter();
            break;
        }
        case NM_CUSTOMDRAW: {
            NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)hdr;
            switch (cd->nmcd.dwDrawStage) {
            case CDDS_PREPAINT:
                return CDRF_NOTIFYITEMDRAW;
            case CDDS_ITEMPREPAINT: {
                int row = (int)cd->nmcd.dwItemSpec;
                bool sel = (cd->nmcd.uItemState & CDIS_SELECTED) != 0;
                COLORREF bg;
                if (sel)
                    bg = Theme::BG_ROW_SEL;
                else if (row % 2 == 1)
                    bg = Theme::BG_ROW_ALT;
                else
                    bg = Theme::BG_SURFACE;

                cd->clrTextBk = bg;
                cd->clrText   = Theme::TEXT_PRIMARY;
                return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
            }
            case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
                // Status dot column (col 0)
                if (cd->iSubItem == 0) {
                    int row = (int)cd->nmcd.dwItemSpec;
                    bool online = true;
                    if (row < (int)_filteredIndices.size()) {
                        int idx = _filteredIndices[row];
                        if (idx < (int)_paintCache.devices.size())
                            online = _paintCache.devices[idx].online;
                    }
                    cd->clrText = online ? Theme::SUCCESS : Theme::TEXT_MUTED;
                    return CDRF_NEWFONT;
                }
                // Monospace for IP (col 2), MAC (col 3), Ports (col 7)
                if (cd->iSubItem == 2 || cd->iSubItem == 3 || cd->iSubItem == 7) {
                    SelectObject(cd->nmcd.hdc, Theme::FontMono());
                    cd->clrText = Theme::TEXT_SECONDARY;
                    return CDRF_NEWFONT;
                }
                // Trust column color-coded (col 6)
                if (cd->iSubItem == 6) {
                    int row = (int)cd->nmcd.dwItemSpec;
                    if (row < (int)_filteredIndices.size()) {
                        int idx = _filteredIndices[row];
                        if (idx < (int)_paintCache.devices.size()) {
                            auto& trust = _paintCache.devices[idx].trustState;
                            if (trust == L"owned")          cd->clrText = Theme::ACCENT_GREEN;
                            else if (trust == L"known")     cd->clrText = Theme::ACCENT_BLUE;
                            else if (trust == L"guest")     cd->clrText = Theme::ACCENT_AMBER;
                            else if (trust == L"blocked")   cd->clrText = Theme::ACCENT_RED;
                            else if (trust == L"watchlist") cd->clrText = Theme::ACCENT_PURPLE;
                            else                            cd->clrText = Theme::TEXT_TERTIARY;
                            return CDRF_NEWFONT;
                        }
                    }
                }
                return CDRF_DODEFAULT;
            }
            default:
                return CDRF_DODEFAULT;
            }
        }
        }
    }

    return CDRF_DODEFAULT;
}

LRESULT TabDevices::OnScanComplete(HWND hwnd) {
    ApplyFilter();
    return 0;
}

void TabDevices::ApplyFilter() {
    if (!_mainWnd || !_hList) return;

    // Single lock + copy for the entire filter+populate cycle
    _paintCache = _mainWnd->GetLastResult();
    const ScanResult& r = _paintCache;

    // Get search text
    wchar_t searchBuf[256] = {};
    if (_hSearch) GetWindowText(_hSearch, searchBuf, 256);
    wstring search = searchBuf;
    std::transform(search.begin(), search.end(), search.begin(), ::tolower);

    _filteredIndices.clear();
    for (int i = 0; i < (int)r.devices.size(); i++) {
        const Device& d = r.devices[i];

        // Apply filter mode
        switch (_filterMode) {
        case 1: if (!d.online) continue; break;
        case 2: if (d.trustState != L"unknown") continue; break;
        case 3: if (d.trustState != L"watchlist") continue; break;
        case 4: if (d.trustState != L"owned") continue; break;
        case 5: if (d.prevPorts == d.openPorts) continue; break;
        }

        // Apply search
        if (!search.empty()) {
            wstring ip = d.ip, mac = d.mac, name = d.hostname, vendor = d.vendor;
            std::transform(ip.begin(), ip.end(), ip.begin(), ::tolower);
            std::transform(mac.begin(), mac.end(), mac.begin(), ::tolower);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::tolower);

            if (ip.find(search) == wstring::npos &&
                mac.find(search) == wstring::npos &&
                name.find(search) == wstring::npos &&
                vendor.find(search) == wstring::npos)
                continue;
        }

        _filteredIndices.push_back(i);
    }

    PopulateList();
}

void TabDevices::PopulateList() {
    if (!_hList || !_mainWnd) return;

    // _paintCache was set by ApplyFilter() before this call — no extra lock needed.
    const ScanResult& r = _paintCache;

    ListView_DeleteAllItems(_hList);

    for (int row = 0; row < (int)_filteredIndices.size(); row++) {
        int idx = _filteredIndices[row];
        if (idx >= (int)r.devices.size()) continue;
        const Device& d = r.devices[idx];

        LVITEM item = {};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.iSubItem = 0;
        item.pszText = (LPWSTR)(d.online ? L"\u25CF" : L"\u25CB"); // filled/empty circle
        ListView_InsertItem(_hList, &item);

        // Name
        wstring name = d.customName.empty() ? d.hostname : d.customName;
        if (name.empty()) name = d.ip;
        ListView_SetItemText(_hList, row, 1, (LPWSTR)name.c_str());

        // IP (+ IPv6 badge)
        wstring ip = d.ip;
        if (!d.ipv6Address.empty()) ip += L" [v6]";
        ListView_SetItemText(_hList, row, 2, (LPWSTR)ip.c_str());

        ListView_SetItemText(_hList, row, 3, (LPWSTR)d.mac.c_str());
        ListView_SetItemText(_hList, row, 4, (LPWSTR)d.vendor.c_str());

        // Type + confidence in one column
        wstring typeConf = d.deviceType + L" (" + std::to_wstring(d.confidence) + L"%)";
        ListView_SetItemText(_hList, row, 5, (LPWSTR)typeConf.c_str());

        ListView_SetItemText(_hList, row, 6, (LPWSTR)d.trustState.c_str());

        // Ports
        wstring ports = GetPortSummary(d);
        ListView_SetItemText(_hList, row, 7, (LPWSTR)ports.c_str());

        // Sighting count
        wstring seen = std::to_wstring(d.sightingCount);
        ListView_SetItemText(_hList, row, 8, (LPWSTR)seen.c_str());

        ListView_SetItemText(_hList, row, 9, (LPWSTR)d.lastSeen.c_str());
    }
}

wstring TabDevices::GetPortSummary(const Device& dev) {
    if (dev.openPorts.empty()) return L"None";

    wstring s;
    int count = std::min((int)dev.openPorts.size(), 4);
    for (int i = 0; i < count; i++) {
        int port = dev.openPorts[i];
        auto it = ScanEngine::PORT_NAMES.find(port);
        if (it != ScanEngine::PORT_NAMES.end())
            s += std::to_wstring(port) + L"(" + it->second + L") ";
        else
            s += std::to_wstring(port) + L" ";
    }
    if ((int)dev.openPorts.size() > 4)
        s += L"+" + std::to_wstring(dev.openPorts.size() - 4) + L" more";

    return s;
}

void TabDevices::ShowDetailPanel(int idx) {
    if (!_mainWnd) return;
    ScanResult r = _mainWnd->GetLastResult();
    if (idx < 0 || idx >= (int)r.devices.size()) return;

    // Reset scroll to top before populating (scroll children back up)
    if (_detailScrollPos > 0 && _hDetailPanel)
        ScrollWindowEx(_hDetailPanel, 0, _detailScrollPos, nullptr, nullptr, nullptr, nullptr, SW_SCROLLCHILDREN);
    _detailScrollPos = 0;

    _detailVisible = true;
    UpdateDetailPanel(r.devices[idx]);

    RECT rc; GetClientRect(_hwnd, &rc);
    LayoutControls(rc.right, rc.bottom);
    ShowWindow(_hDetailPanel, SW_SHOW);

    UpdateDetailScrollInfo();
    InvalidateRect(_hwnd, nullptr, FALSE);
}

void TabDevices::UpdateDetailScrollInfo() {
    if (!_hDetailPanel) return;
    RECT rc; GetClientRect(_hDetailPanel, &rc);
    int pageH = rc.bottom - rc.top;
    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin   = 0;
    si.nMax   = std::max(0, _detailContentH - 1);
    si.nPage  = (UINT)pageH;
    si.nPos   = _detailScrollPos;
    SetScrollInfo(_hDetailPanel, SB_VERT, &si, TRUE);
}

LRESULT TabDevices::OnDetailScroll(HWND hwnd, WPARAM wp) {
    RECT rc; GetClientRect(hwnd, &rc);
    int pageH  = rc.bottom - rc.top;
    int maxPos = std::max(0, _detailContentH - pageH);
    int newPos = _detailScrollPos;

    switch (LOWORD(wp)) {
    case SB_LINEUP:        newPos = std::max(0,      newPos - 20);    break;
    case SB_LINEDOWN:      newPos = std::min(maxPos, newPos + 20);    break;
    case SB_PAGEUP:        newPos = std::max(0,      newPos - pageH); break;
    case SB_PAGEDOWN:      newPos = std::min(maxPos, newPos + pageH); break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION: newPos = (int)(short)HIWORD(wp);           break;
    case SB_TOP:           newPos = 0;       break;
    case SB_BOTTOM:        newPos = maxPos;  break;
    }
    newPos = std::max(0, std::min(maxPos, newPos));
    if (newPos == _detailScrollPos) return 0;

    int delta = _detailScrollPos - newPos;
    _detailScrollPos = newPos;
    ScrollWindowEx(hwnd, 0, delta, nullptr, nullptr, nullptr, nullptr,
                   SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    UpdateWindow(hwnd);

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_POS;
    si.nPos   = _detailScrollPos;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    return 0;
}

LRESULT TabDevices::OnDetailMouseWheel(HWND hwnd, WPARAM wp) {
    int zDelta = GET_WHEEL_DELTA_WPARAM(wp);
    // Compute total pixel delta (3 lines per notch, 20px per line) and scroll once.
    int pixelDelta = (zDelta * 3 * 20) / WHEEL_DELTA;

    RECT rc; GetClientRect(hwnd, &rc);
    int pageH  = rc.bottom - rc.top;
    int maxPos = std::max(0, _detailContentH - pageH);
    int newPos = std::max(0, std::min(maxPos, _detailScrollPos - pixelDelta));

    if (newPos == _detailScrollPos) return 0;

    int delta = _detailScrollPos - newPos;
    _detailScrollPos = newPos;
    ScrollWindowEx(hwnd, 0, delta, nullptr, nullptr, nullptr, nullptr,
                   SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    UpdateWindow(hwnd);

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask  = SIF_POS;
    si.nPos   = _detailScrollPos;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
    return 0;
}

void TabDevices::HideDetailPanel() {
    _detailVisible = false;
    ShowWindow(_hDetailPanel, SW_HIDE);

    RECT rc; GetClientRect(_hwnd, &rc);
    LayoutControls(rc.right, rc.bottom);
    InvalidateRect(_hwnd, nullptr, FALSE);
}

static wstring GetIpDescription(const wstring& ip) {
    int o1 = 0, o2 = 0, o3 = 0, o4 = 0;
    swscanf_s(ip.c_str(), L"%d.%d.%d.%d", &o1, &o2, &o3, &o4);

    if (o1 == 10)
        return L"This is an RFC 1918 private address in the 10.0.0.0/8 range, commonly used in home and enterprise networks. It is not routable on the public internet and can only be reached within its local network.";
    if (o1 == 172 && o2 >= 16 && o2 <= 31)
        return L"This is an RFC 1918 private address in the 172.16.0.0/12 range, often assigned in corporate or cloud virtual networks. It cannot be reached from the public internet and is confined to its local subnet.";
    if (o1 == 192 && o2 == 168)
        return L"This is an RFC 1918 private address in the 192.168.0.0/16 range, the most common choice for home and small-office networks. It is not reachable from the public internet and requires NAT to access external services.";
    if (o1 == 169 && o2 == 254)
        return L"This is an APIPA link-local address, self-assigned by the device when no DHCP server responded to its request. It typically indicates a network misconfiguration and the device may be unable to reach other subnets.";
    if (o1 == 127)
        return L"This is a loopback address in the 127.0.0.0/8 range that always refers to the local device itself. Seeing this appear on a network scan is unusual and may indicate a software or configuration anomaly.";
    if (o1 == 100 && o2 >= 64 && o2 <= 127)
        return L"This address falls in the RFC 6598 shared address space (100.64.0.0/10), used by ISPs for carrier-grade NAT between the provider and customer. You may be behind a double NAT, which can affect port forwarding and peer-to-peer connectivity.";
    if (o1 >= 224 && o1 <= 239)
        return L"This is a multicast address used for group communication protocols such as mDNS and UPnP discovery. It does not correspond to a single physical device and cannot be directly addressed with unicast traffic.";
    if (o1 >= 240)
        return L"This address falls in the reserved Class E range (240.0.0.0/4), set aside by IANA and not used in normal network operations. Seeing this on a scan is highly unusual and may suggest a spoofed or misconfigured packet.";
    return L"This appears to be a public IP address, meaning the device may be directly reachable from the internet without NAT. Verify your firewall rules and ensure this device is intentionally exposed if it appears on your local network.";
}

void TabDevices::UpdateDetailPanel(const Device& dev) {
    if (!_hDetailPanel) return;

    // Custom name field
    if (_hDetailCustomName) SetWindowText(_hDetailCustomName, dev.customName.c_str());

    // Display name (hostname or IP) with clickable source tag (WC_LINK)
    wstring displayName = dev.customName.empty() ? dev.hostname : dev.customName;
    if (displayName.empty()) displayName = dev.ip;
    wstring nameSrc;
    for (auto& e : dev.evidence) {
        if (e.field == L"hostname") { nameSrc = e.source; break; }
    }
    if (_hDetailName) {
        wstring nameText = EscapeForLink(displayName);
        if (!nameSrc.empty() && dev.customName.empty())
            nameText += L"  <a href=\"" + nameSrc + L"\">[" + nameSrc + L"]</a>";
        SetWindowText(_hDetailName, nameText.c_str());
    }

    // Type + confidence
    if (_hDetailType) SetWindowText(_hDetailType,
        (dev.deviceType + L"  (" + std::to_wstring(dev.confidence) + L"% confidence)").c_str());

    // Classification evidence
    if (_hDetailEvidence) {
        wstring ev = dev.classificationReason;
        if (ev.empty()) ev = L"No evidence — run a deeper scan";
        SetWindowText(_hDetailEvidence, (L"Evidence: " + ev).c_str());
    }

    // Confidence alternatives
    wstring altStr;
    if (!dev.altType1.empty())
        altStr += L"Alt: " + dev.altType1 + L" (" + std::to_wstring(dev.altConf1) + L"%)";
    if (!dev.altType2.empty())
        altStr += L"  |  " + dev.altType2 + L" (" + std::to_wstring(dev.altConf2) + L"%)";
    if (altStr.empty()) altStr = L"No alternatives — run Deep scan";
    if (_hDetailAlt) SetWindowText(_hDetailAlt, altStr.c_str());

    // Vendor with clickable evidence source tag (WC_LINK)
    if (_hDetailVendor) {
        wstring vendorName = dev.vendor.empty() ? L"Unknown" : EscapeForLink(dev.vendor);
        wstring vendorStr  = L"Vendor: " + vendorName;
        for (auto& e : dev.evidence) {
            if (e.field == L"vendor") {
                vendorStr += L"  <a href=\"" + e.source + L"\">[" + e.source + L"]</a>";
                break;
            }
        }
        SetWindowText(_hDetailVendor, vendorStr.c_str());
    }

    // IP address as clickable link — click opens range explanation popup (WC_LINK)
    if (_hDetailIp) {
        wstring ipStr = L"IP:  <a href=\"IP\">" + EscapeForLink(dev.ip) + L"</a>";
        if (!dev.ipv6Address.empty()) ipStr += L"  [IPv6]";
        SetWindowText(_hDetailIp, ipStr.c_str());
    }

    // MAC as clickable acronym label (WC_LINK)
    if (_hDetailMac) {
        wstring macStr = L"<a href=\"MAC\">MAC</a>: " + EscapeForLink(dev.mac);
        if (dev.latencyMs >= 0)
            macStr += L"   " + std::to_wstring(dev.latencyMs) + L"ms";
        SetWindowText(_hDetailMac, macStr.c_str());
    }

    // Subnet
    if (_hDetailSubnet) {
        wstring sub = dev.subnet.empty() ? L"Subnet: unknown" : L"Subnet: " + dev.subnet;
        SetWindowText(_hDetailSubnet, sub.c_str());
    }

    // First seen / Last seen / Sightings
    if (_hDetailFirstSeen)
        SetWindowText(_hDetailFirstSeen, (L"First seen: " + (dev.firstSeen.empty() ? L"this scan" : dev.firstSeen)).c_str());
    if (_hDetailLastSeen)
        SetWindowText(_hDetailLastSeen, (L"Last seen: " + dev.lastSeen).c_str());
    if (_hDetailSightings)
        SetWindowText(_hDetailSightings, (L"Sightings: " + std::to_wstring(dev.sightingCount) + L" scan(s)").c_str());

    // IP history
    if (_hDetailIpHistory) {
        if (dev.ipHistory.empty()) {
            SetWindowText(_hDetailIpHistory, L"No IP changes recorded");
        } else {
            wstring hist = L"Prior IPs: ";
            for (size_t i = 0; i < dev.ipHistory.size(); i++) {
                if (i > 0) hist += L", ";
                hist += dev.ipHistory[i];
            }
            SetWindowText(_hDetailIpHistory, hist.c_str());
        }
    }

    // Ports
    wstring portStr;
    if (dev.openPorts.empty()) portStr = L"No open ports";
    else for (int p : dev.openPorts) {
        auto it = ScanEngine::PORT_NAMES.find(p);
        portStr += std::to_wstring(p);
        if (it != ScanEngine::PORT_NAMES.end()) portStr += L"/" + it->second;
        portStr += L"  ";
    }
    if (_hDetailPorts) SetWindowText(_hDetailPorts, portStr.c_str());

    // mDNS with clickable [mDNS] prefix (WC_LINK)
    if (_hDetailMdns) {
        wstring mdns;
        for (auto& s : dev.mdnsServices) mdns += s + L"  ";
        wstring mdnsStr;
        if (mdns.empty())
            mdnsStr = L"No mDNS services";
        else
            mdnsStr = L"<a href=\"mDNS\">[mDNS]</a> " + EscapeForLink(mdns);
        SetWindowText(_hDetailMdns, mdnsStr.c_str());
    }

    // IoT risk
    if (_hDetailIotRisk) {
        if (dev.iotRisk && !dev.iotRiskDetail.empty()) {
            SetWindowText(_hDetailIotRisk, dev.iotRiskDetail.c_str());
            ShowWindow(_hDetailIotRisk, SW_SHOW);
        } else {
            ShowWindow(_hDetailIotRisk, SW_HIDE);
        }
    }

    // Anomalies for this device
    wstring anoms;
    if (_mainWnd) {
        ScanResult r = _mainWnd->GetLastResult();
        for (auto& a : r.anomalies) {
            if (a.deviceIp == dev.ip)
                anoms += L"[" + a.severity + L"] " + a.description + L"\r\n";
        }
    }
    if (anoms.empty()) anoms = L"No alerts for this device";
    if (_hDetailAnoms) SetWindowText(_hDetailAnoms, anoms.c_str());

    // Notes
    if (_hDetailNotes) SetWindowText(_hDetailNotes, dev.notes.c_str());

    // Pause button
    if (_hDetailPause) {
        SetWindowText(_hDetailPause, dev.paused ? L"Resume Network" : L"Pause Network");
        InvalidateRect(_hDetailPause, nullptr, FALSE);
    }

    // Trust
    if (_hDetailTrust) {
        static const wchar_t* trustOpts[] = { L"unknown", L"owned", L"watchlist", L"guest", L"blocked" };
        for (int i = 0; i < 5; i++) {
            if (dev.trustState == trustOpts[i]) {
                SendMessage(_hDetailTrust, CB_SETCURSEL, i, 0);
                break;
            }
        }
    }

    _detailDeviceIp = dev.ip;
}

void TabDevices::PauseDevice(const wstring& ip, bool pause) {
    if (!ScanEngine::IsSafeIP(ip)) return;

    wstring ruleName = L"Transparency_Block_" + ip;
    wstring args;
    if (pause) {
        args = L"/c netsh advfirewall firewall add rule name=\"" + ruleName +
               L"\" dir=in action=block remoteip=" + ip +
               L" & netsh advfirewall firewall add rule name=\"" + ruleName +
               L"\" dir=out action=block remoteip=" + ip;
    } else {
        args = L"/c netsh advfirewall firewall delete rule name=\"" + ruleName + L"\"";
    }

    // Run netsh elevated — user will see UAC prompt
    ShellExecute(_hwnd, L"runas", L"cmd.exe", args.c_str(), nullptr, SW_HIDE);
}

void TabDevices::RefreshList() {
    ApplyFilter();
}

void TabDevices::SelectAndShowDevice(int rawDeviceIndex) {
    if (!_hList) return;

    // Find the list row that corresponds to this raw device index
    int listRow = -1;
    for (int i = 0; i < (int)_filteredIndices.size(); i++) {
        if (_filteredIndices[i] == rawDeviceIndex) { listRow = i; break; }
    }

    // If not in current filtered view, reset filter to "All" and re-search
    if (listRow < 0) {
        _filterMode = 0;
        if (_hSearch) SetWindowText(_hSearch, L"");
        ApplyFilter();
        for (int i = 0; i < (int)_filteredIndices.size(); i++) {
            if (_filteredIndices[i] == rawDeviceIndex) { listRow = i; break; }
        }
    }

    if (listRow < 0) return;

    // Highlight the row in the list
    ListView_SetItemState(_hList, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(_hList, listRow, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(_hList, listRow, FALSE);

    // Open the detail panel
    _selectedDevice = rawDeviceIndex;
    ShowDetailPanel(rawDeviceIndex);
}

void TabDevices::ShowDeviceContextMenu(HWND hwnd, int x, int y, int deviceIdx) {
    if (!_mainWnd) return;
    ScanResult r = _mainWnd->GetLastResult();
    if (deviceIdx < 0 || deviceIdx >= (int)r.devices.size()) return;
    const Device& dev = r.devices[deviceIdx];

    HMENU hMenu = CreatePopupMenu();

    // Basic actions always available
    AppendMenu(hMenu, MF_STRING, 12001, L"Ping Device");
    AppendMenu(hMenu, MF_STRING, 12002, L"Traceroute");
    AppendMenu(hMenu, MF_STRING, 12003, L"Port Scan");
    AppendMenu(hMenu, MF_STRING, 12004, L"Copy IP Address");
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);

    // Trust state options
    AppendMenu(hMenu, MF_STRING, 12010, L"Mark as Owned");
    AppendMenu(hMenu, MF_STRING, 12011, L"Mark as Guest");
    AppendMenu(hMenu, MF_STRING, 12012, L"Add to Watchlist");
    AppendMenu(hMenu, MF_STRING, 12013, L"Block Device");
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);

    // Connection-based options
    bool hasWeb = false, hasSsh = false, hasRdp = false, hasFtp = false, hasSamba = false;
    for (int p : dev.openPorts) {
        if (p == 80 || p == 443 || p == 8080 || p == 8443) hasWeb = true;
        if (p == 22) hasSsh = true;
        if (p == 3389) hasRdp = true;
        if (p == 21) hasFtp = true;
        if (p == 445 || p == 139) hasSamba = true;
    }

    if (hasWeb)   AppendMenu(hMenu, MF_STRING, 12020, L"Open in Browser");
    if (hasSsh)   AppendMenu(hMenu, MF_STRING, 12021, L"Connect via SSH");
    if (hasRdp)   AppendMenu(hMenu, MF_STRING, 12022, L"Remote Desktop (RDP)");
    if (hasFtp)   AppendMenu(hMenu, MF_STRING, 12023, L"Open FTP Connection");
    if (hasSamba) AppendMenu(hMenu, MF_STRING, 12024, L"Browse Network Share");

    if (hasWeb || hasSsh || hasRdp || hasFtp || hasSamba)
        AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);

    // Wake-on-LAN (if offline and has MAC)
    if (!dev.online && !dev.mac.empty())
        AppendMenu(hMenu, MF_STRING, 12030, L"Wake-on-LAN");

    // DNS lookup
    AppendMenu(hMenu, MF_STRING, 12031, L"Reverse DNS Lookup");

    // Pause / resume
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hMenu, MF_STRING, 12050, dev.paused ? L"Resume Network Access" : L"Pause Network Access");

    // Detail panel
    AppendMenu(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenu(hMenu, MF_STRING, 12040, L"View Details");

    // Store device IP for command handler
    _detailDeviceIp = dev.ip;

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 0) return;



    switch (cmd) {
    case 12001: { // Ping
        if (!ScanEngine::IsSafeIP(dev.ip)) {
            MessageBox(hwnd, L"Invalid IP address format.", L"Security Error", MB_OK | MB_ICONERROR);
            break;
        }
        wstring cmdLine = L"cmd /c start cmd /k ping " + dev.ip;
        _wsystem(cmdLine.c_str());
        break;
    }
    case 12002: { // Traceroute
        if (!ScanEngine::IsSafeIP(dev.ip)) {
            MessageBox(hwnd, L"Invalid IP address format.", L"Security Error", MB_OK | MB_ICONERROR);
            break;
        }
        wstring cmdLine = L"cmd /c start cmd /k tracert " + dev.ip;
        _wsystem(cmdLine.c_str());
        break;
    }
    case 12003: { // Port Scan — switch to tools tab
        if (_mainWnd) _mainWnd->SwitchTab(Tab::Tools);
        break;
    }
    case 12004: { // Copy IP
        if (OpenClipboard(hwnd)) {
            EmptyClipboard();
            size_t len = (dev.ip.size() + 1) * sizeof(wchar_t);
            HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, len);
            if (hg) {
                memcpy(GlobalLock(hg), dev.ip.c_str(), len);
                GlobalUnlock(hg);
                SetClipboardData(CF_UNICODETEXT, hg);
            }
            CloseClipboard();
        }
        break;
    }
    case 12010: { // Mark Owned
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        for (auto& d : _mainWnd->_lastResult.devices) {
            if (d.ip == dev.ip) { d.trustState = L"owned"; break; }
        }
        ApplyFilter();
        break;
    }
    case 12011: { // Mark Guest
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        for (auto& d : _mainWnd->_lastResult.devices) {
            if (d.ip == dev.ip) { d.trustState = L"guest"; break; }
        }
        ApplyFilter();
        break;
    }
    case 12012: { // Watchlist
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        for (auto& d : _mainWnd->_lastResult.devices) {
            if (d.ip == dev.ip) { d.trustState = L"watchlist"; break; }
        }
        ApplyFilter();
        break;
    }
    case 12013: { // Block
        std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
        for (auto& d : _mainWnd->_lastResult.devices) {
            if (d.ip == dev.ip) { d.trustState = L"blocked"; break; }
        }
        ApplyFilter();
        break;
    }
    case 12020: { // Open in Browser
        if (!ScanEngine::IsSafeIP(dev.ip)) { MessageBox(hwnd, L"Invalid IP address format.", L"Security Error", MB_OK | MB_ICONERROR); break; }
        wstring url = L"http://" + dev.ip;
        for (int p : dev.openPorts) {
            if (p == 443 || p == 8443) { url = L"https://" + dev.ip; break; }
            if (p == 8080) { url = L"http://" + dev.ip + L":8080"; break; }
        }
        ShellExecute(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOW);
        break;
    }
    case 12021: { // SSH
        if (!ScanEngine::IsSafeIP(dev.ip)) {
            MessageBox(hwnd, L"Invalid IP address format.", L"Security Error", MB_OK | MB_ICONERROR);
            break;
        }
        wstring cmdLine = L"cmd /c start cmd /k ssh " + dev.ip;
        _wsystem(cmdLine.c_str());
        break;
    }
    case 12022: { // RDP
        if (!ScanEngine::IsSafeIP(dev.ip)) { MessageBox(hwnd, L"Invalid IP address format.", L"Security Error", MB_OK | MB_ICONERROR); break; }
        ShellExecute(nullptr, L"open", L"mstsc.exe", (L"/v:" + dev.ip).c_str(), nullptr, SW_SHOW);
        break;
    }
    case 12023: { // FTP
        if (!ScanEngine::IsSafeIP(dev.ip)) { MessageBox(hwnd, L"Invalid IP address format.", L"Security Error", MB_OK | MB_ICONERROR); break; }
        wstring url = L"ftp://" + dev.ip;
        ShellExecute(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOW);
        break;
    }
    case 12024: { // Network share
        if (!ScanEngine::IsSafeIP(dev.ip)) { MessageBox(hwnd, L"Invalid IP address format.", L"Security Error", MB_OK | MB_ICONERROR); break; }
        wstring path = L"\\\\" + dev.ip;
        ShellExecute(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOW);
        break;
    }
    case 12030: { // Wake-on-LAN
        MessageBox(hwnd, (L"Wake-on-LAN sent to " + dev.mac).c_str(), L"WOL", MB_OK | MB_ICONINFORMATION);
        break;
    }
    case 12031: { // Reverse DNS
        if (!ScanEngine::IsSafeIP(dev.ip)) {
            MessageBox(hwnd, L"Invalid IP address format.", L"Security Error", MB_OK | MB_ICONERROR);
            break;
        }
        wstring cmdLine = L"cmd /c start cmd /k nslookup " + dev.ip;
        _wsystem(cmdLine.c_str());
        break;
    }
    case 12040: { // View Details
        for (int i = 0; i < (int)_filteredIndices.size(); i++) {
            if (_filteredIndices[i] == deviceIdx) {
                ShowDetailPanel(i);
                break;
            }
        }
        break;
    }
    case 12050: { // Pause / Resume network access
        bool newPaused = false;
        {
            std::lock_guard<std::mutex> lk(_mainWnd->_dataMutex);
            for (auto& d : _mainWnd->_lastResult.devices) {
                if (d.ip == dev.ip) {
                    d.paused = !d.paused;
                    newPaused = d.paused;
                    break;
                }
            }
        }
        PauseDevice(dev.ip, newPaused);
        ApplyFilter();
        break;
    }
    }
}
