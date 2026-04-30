#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <string>
#include "GlossaryPopup.h"
#include "Theme.h"

using std::wstring;

// ── Glossary database ─────────────────────────────────────────────────────────

struct AcronymEntry { const wchar_t* key; const wchar_t* explanation; };

static const AcronymEntry ACRONYM_DB[] = {
    { L"ARP",
      L"Address Resolution Protocol — How your network maps IP addresses to the physical hardware "
      L"addresses of devices. When your router needs to contact 192.168.1.5, it broadcasts 'Who has "
      L"this IP?' and the device replies with its MAC address so traffic can be delivered." },

    { L"MAC",
      L"Media Access Control Address — A unique 12-character identifier (e.g. AA:BB:CC:DD:EE:FF) "
      L"permanently burned into every network adapter by its manufacturer. The first 6 characters "
      L"identify the maker (OUI); the last 6 are unique to the device. Unlike an IP address, a MAC "
      L"address does not change when you move between networks." },

    { L"mDNS",
      L"Multicast DNS — Lets devices on your local network announce and find each other by name "
      L"without a central server. Your Mac broadcasts '_http._tcp.local' so other devices can discover "
      L"its web service automatically. Commonly used by Apple, Chromecast, and smart home devices." },

    { L"MDNS", L"See mDNS — Multicast DNS. Lets devices discover each other by name on a local "
      L"network without needing a central server." },

    { L"DHCP",
      L"Dynamic Host Configuration Protocol — The service (usually on your router) that automatically "
      L"hands out IP addresses when devices join your network. Without DHCP you would need to set a "
      L"unique IP on every device manually. If DHCP fails, a device may fall back to a self-assigned "
      L"169.254.x.x address (APIPA)." },

    { L"DNS",
      L"Domain Name System — The internet's address book. Translates human-readable names like "
      L"'google.com' into the numeric IP addresses computers use for routing. Your DNS server is "
      L"usually your router or a public resolver such as 8.8.8.8 (Google) or 1.1.1.1 (Cloudflare)." },

    { L"TCP",
      L"Transmission Control Protocol — The reliable delivery layer used by most internet services "
      L"(web, email, SSH). It guarantees data arrives complete and in order by re-sending lost packets. "
      L"Slower than UDP but essential when accuracy matters." },

    { L"UDP",
      L"User Datagram Protocol — A faster but 'fire-and-forget' alternative to TCP. Used for video "
      L"streaming, online games, and DNS lookups where speed matters more than guaranteed delivery. "
      L"Lost UDP packets are not re-sent." },

    { L"OUI",
      L"Organizationally Unique Identifier — The first three bytes of a MAC address, assigned by the "
      L"IEEE to identify the chip manufacturer. For example, 'B8:27:EB' is a Raspberry Pi and "
      L"'A4:CF:12' is a Nest device. This is how Transparencii identifies vendors without scanning." },

    { L"APIPA",
      L"Automatic Private IP Addressing — When a device cannot reach a DHCP server it assigns itself "
      L"a 169.254.x.x address. The device can still talk to other APIPA devices nearby, but cannot "
      L"access the internet or other subnets. Usually signals a network configuration problem." },

    { L"NAT",
      L"Network Address Translation — How your router lets many private-network devices share a single "
      L"public internet IP address. Outgoing traffic gets its source IP rewritten to your router's "
      L"public IP; replies are rewritten back. This is why all home devices appear to share one IP." },

    { L"ICMP",
      L"Internet Control Message Protocol — The protocol behind 'ping'. Used to test whether a device "
      L"is reachable and measure round-trip time. It also carries error messages like 'Destination "
      L"Unreachable'. ICMP carries control information, not application data." },

    { L"NDP",
      L"Neighbor Discovery Protocol — The IPv6 replacement for ARP. Allows IPv6 devices to discover "
      L"each other's MAC addresses and find routers, all via multicast messages rather than the "
      L"broadcast packets ARP uses in IPv4." },

    { L"NetBIOS",
      L"Network Basic Input/Output System — An older Microsoft protocol for local network name "
      L"resolution and file/printer sharing, common before DNS was widespread in local networks. "
      L"Still present in Windows as part of file sharing (SMB). Devices respond to NetBIOS queries "
      L"with their computer name." },

    { L"SSDP",
      L"Simple Service Discovery Protocol — The announcement layer used by UPnP devices (smart TVs, "
      L"game consoles, printers). Devices send SSDP multicast messages to advertise their services. "
      L"This is how Windows automatically finds your network printer or smart TV." },

    { L"UPnP",
      L"Universal Plug and Play — Protocols that let devices configure themselves and discover each "
      L"other automatically. Commonly used by game consoles, streaming sticks, and smart home gear. "
      L"Can be a security risk if your router allows UPnP to open ports exposed to the internet." },

    { L"WOL",
      L"Wake-on-LAN — A feature that lets you remotely power on a computer by sending a 'magic "
      L"packet' containing its MAC address over the network. The target computer must support WOL "
      L"and have it enabled in its BIOS or firmware settings." },

    { L"IPv6",
      L"Internet Protocol version 6 — The modern addressing system replacing IPv4. Uses 128-bit "
      L"addresses (e.g. 2001:db8::1) to support the enormous number of internet-connected devices. "
      L"A device often has both an IPv4 and an IPv6 address active at the same time." },

    { L"BSSID",
      L"Basic Service Set Identifier — The MAC address of the specific Wi-Fi access point radio you "
      L"are connected to. If your router broadcasts on 2.4 GHz and 5 GHz, each band has a different "
      L"BSSID even though they share the same network name (SSID)." },

    { L"SSID",
      L"Service Set Identifier — The human-readable name of a Wi-Fi network (what you see when "
      L"scanning for Wi-Fi on your phone). Multiple access points can share the same SSID to form a "
      L"seamless roaming network while each has its own unique BSSID." },

    { L"RFC",
      L"Request for Comments — Official internet standards documents published by the IETF. Despite "
      L"the name, most RFCs are finalized standards. 'RFC 1918' defines the private IP address ranges "
      L"(10.x.x.x, 172.16-31.x.x, 192.168.x.x) reserved for local networks and not usable on the "
      L"public internet." },

    { L"IANA",
      L"Internet Assigned Numbers Authority — The global body that coordinates IP address allocation, "
      L"DNS root zones, and protocol registries. IANA allocates large blocks to regional bodies (like "
      L"ARIN for North America) which then distribute them to ISPs and organizations." },

    { L"ISP",
      L"Internet Service Provider — The company that sells you internet access (e.g. Comcast, AT&T, "
      L"BT). Your ISP assigns your router a public IP address and connects your network to the "
      L"internet. Some ISPs use carrier-grade NAT, meaning many customers share one public IP." },

    { L"NIC",
      L"Network Interface Card — The hardware (or virtual) adapter your computer uses to connect to "
      L"a network. You may have several: one for Wi-Fi, one for Ethernet, and additional virtual ones "
      L"created by software like VirtualBox, VMware, or VPN clients. Transparencii scans from the "
      L"highest-ranked physical adapter by default." },

    { L"GATEWAY",
      L"The device (almost always your router) that connects your local network to the internet. "
      L"All outbound traffic from your devices is sent through the gateway. It also manages your "
      L"local IP addresses via DHCP. If the gateway is unreachable, devices on your network lose "
      L"internet access." },

    { L"MONITOR",
      L"Continuous Monitoring — Automatically re-scans your network at a regular interval (e.g. "
      L"every 5 minutes). If a new device joins, a known device disappears, or anything changes "
      L"(IP, ports, hostname), an alert is raised. Useful for catching unauthorized devices without "
      L"having to run scans manually." },

    { L"SUBNET",
      L"A subdivision of an IP network. Devices on the same subnet can talk to each other directly; "
      L"traffic between subnets must pass through a router. Home networks typically use a single "
      L"subnet (e.g. 192.168.1.0/24), which can hold up to 254 devices." },

    { L"CIDR",
      L"Classless Inter-Domain Routing notation — A compact way to write an IP address range. The "
      L"number after the slash (e.g. /24) says how many bits are the 'network' part. /24 means the "
      L"first 24 bits are fixed and the last 8 bits vary, giving 254 usable device addresses. "
      L"Common home ranges: /24 (254 devices), /16 (65,534 devices)." },

    { L"PING",
      L"A basic network test that sends a small packet to a device and waits for a reply, measuring "
      L"how long the round trip takes (in milliseconds). If a device does not reply it may be "
      L"offline, blocking ICMP, or out of range. The name comes from submarine sonar terminology." },

    // Scan modes
    { L"QUICK",
      L"Quick Scan — The fastest option. Sends one ping to each possible address in your network and "
      L"checks the system ARP table. Finishes in under 30 seconds. Best for a rapid head-count of "
      L"online devices. May miss devices that block ping or have just joined the network." },

    { L"STANDARD",
      L"Standard Scan — The recommended choice for everyday use. Combines ping sweeps with mDNS and "
      L"SSDP service discovery and checks the ARP table. Finds most devices reliably within about "
      L"60 seconds. Balances thoroughness with speed and is suitable for home and office networks." },

    { L"DEEP",
      L"Deep Scan — The most thorough option. Adds port scanning (checking which network services "
      L"are open on each device), banner grabbing, and NetBIOS name resolution on top of everything "
      L"Standard Scan does. Takes 2-5 minutes but gives you the fullest picture of every device and "
      L"service on your network." },

    { L"GENTLE",
      L"Gentle Mode — Slows down port scanning by using longer timeouts between probes. Use this if "
      L"the scan seems to upset your router, trigger security alerts on your network gear, or disrupt "
      L"sensitive devices such as medical equipment or industrial controllers. Scans take longer but "
      L"are much less likely to overwhelm any device." },

    { L"IP",
      L"Internet Protocol Address — A numerical label (e.g. 192.168.1.5) assigned to every device "
      L"on a network so that traffic can be routed to it. Think of it like a postal address: your "
      L"router hands one out to each device via DHCP. Addresses starting with 10., 172.16-31., or "
      L"192.168. are private (local network only) and are not directly reachable from the internet." },
};

static const wchar_t* LookupAcronymImpl(const wstring& key) {
    for (auto& e : ACRONYM_DB)
        if (key == e.key) return e.explanation;
    return nullptr;
}

// ── Shared popup state ────────────────────────────────────────────────────────

static HWND s_glossaryPopup = nullptr;

struct PopupData { wstring title; wstring body; };

// ── Window subclass for WC_LINK background ────────────────────────────────────

static LRESULT CALLBACK LinkSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                          UINT_PTR, DWORD_PTR) {
    if (msg == WM_ERASEBKGND) {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc, Theme::BrushSurface());
        return 1;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// ── Popup window procedure ────────────────────────────────────────────────────

static LRESULT CALLBACK GlossaryPopupProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* data = reinterpret_cast<PopupData*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        auto* d  = reinterpret_cast<PopupData*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(d));

        HINSTANCE hInst = GetModuleHandle(nullptr);

        HWND hBody = CreateWindowEx(0, L"STATIC", d->body.c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
            12, 44, 336, 142, hwnd, nullptr, hInst, nullptr);
        SendMessage(hBody, WM_SETFONT, (WPARAM)Theme::FontBodySm(), TRUE);

        HWND hClose = CreateWindowEx(0, L"BUTTON", L"Close",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            130, 194, 100, 26, hwnd, (HMENU)IDCANCEL, hInst, nullptr);
        SendMessage(hClose, WM_SETFONT, (WPARAM)Theme::FontBodySm(), TRUE);

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

// ── Public API ────────────────────────────────────────────────────────────────

void RegisterGlossaryPopupClass(HINSTANCE hInst) {
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = GlossaryPopupProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = Theme::BrushElevated();
    wc.lpszClassName = L"TranspGlossaryPopup";
    RegisterClassEx(&wc);
}

void ShowGlossaryPopup(HWND rootWnd, const wstring& title, const wstring& body) {
    if (s_glossaryPopup) {
        DestroyWindow(s_glossaryPopup);
        s_glossaryPopup = nullptr;
    }
    auto* data = new PopupData{ title, body };

    POINT pt; GetCursorPos(&pt);
    const int PW = 360, PH = 228;
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

    if (s_glossaryPopup)
        ShowWindow(s_glossaryPopup, SW_SHOWNOACTIVATE);
    else
        delete data;
}

const wchar_t* LookupAcronym(const wstring& key) {
    return LookupAcronymImpl(key);
}

wstring EscapeForLink(const wstring& s) {
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

void SubclassLinkCtrl(HWND hwnd) {
    SetWindowSubclass(hwnd, LinkSubclassProc, 0, 0);
}
