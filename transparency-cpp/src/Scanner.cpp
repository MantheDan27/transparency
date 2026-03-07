#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <windns.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <future>
#include <thread>
#include <mutex>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <functional>
#include <memory>
#include <chrono>
#include <condition_variable>

// C++17 compatible semaphore using mutex + condition_variable
class SimpleSemaphore {
public:
    explicit SimpleSemaphore(int count) : _count(count) {}
    void acquire() {
        std::unique_lock<std::mutex> lock(_mtx);
        _cv.wait(lock, [this] { return _count > 0; });
        --_count;
    }
    void release() {
        std::unique_lock<std::mutex> lock(_mtx);
        ++_count;
        _cv.notify_one();
    }
private:
    std::mutex _mtx;
    std::condition_variable _cv;
    int _count;
};

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "dnsapi.lib")

#include "Scanner.h"
#include "OuiData.h"
#include "Models.h"

using std::wstring;
using std::string;
using std::vector;
using std::map;
using std::set;

// ─── Port/Service Tables ──────────────────────────────────────────────────────

const std::map<int, wstring> ScanEngine::PORT_NAMES = {
    {20,   L"FTP-Data"},      {21,   L"FTP"},
    {22,   L"SSH"},           {23,   L"Telnet"},
    {25,   L"SMTP"},          {53,   L"DNS"},
    {67,   L"DHCP-Server"},   {68,   L"DHCP-Client"},
    {69,   L"TFTP"},          {80,   L"HTTP"},
    {110,  L"POP3"},          {119,  L"NNTP"},
    {123,  L"NTP"},           {135,  L"MSRPC"},
    {137,  L"NetBIOS-NS"},    {138,  L"NetBIOS-DG"},
    {139,  L"NetBIOS-SSN"},   {143,  L"IMAP"},
    {161,  L"SNMP"},          {162,  L"SNMP-Trap"},
    {389,  L"LDAP"},          {443,  L"HTTPS"},
    {445,  L"SMB"},           {465,  L"SMTPS"},
    {514,  L"Syslog"},        {515,  L"LPD/LPR"},
    {548,  L"AFP"},           {587,  L"SMTP-Submit"},
    {631,  L"IPP"},           {636,  L"LDAPS"},
    {873,  L"rsync"},         {993,  L"IMAPS"},
    {995,  L"POP3S"},         {1080, L"SOCKS"},
    {1194, L"OpenVPN"},       {1433, L"MSSQL"},
    {1521, L"Oracle-DB"},     {1723, L"PPTP"},
    {1883, L"MQTT"},          {2049, L"NFS"},
    {2082, L"cPanel"},        {2083, L"cPanel-SSL"},
    {2375, L"Docker"},        {2376, L"Docker-SSL"},
    {3000, L"Dev-Server"},    {3268, L"LDAP-GC"},
    {3306, L"MySQL"},         {3389, L"RDP"},
    {3690, L"SVN"},           {4000, L"ICQ"},
    {4443, L"HTTPS-Alt"},     {4500, L"IKE-NAT"},
    {5000, L"Synology-HTTP"}, {5001, L"Synology-HTTPS"},
    {5060, L"SIP"},           {5061, L"SIP-TLS"},
    {5353, L"mDNS"},          {5432, L"PostgreSQL"},
    {5900, L"VNC"},           {5985, L"WinRM-HTTP"},
    {5986, L"WinRM-HTTPS"},   {6379, L"Redis"},
    {7070, L"AJP"},           {7443, L"HTTPS-Alt"},
    {8008, L"Chromecast"},    {8009, L"AJP-Tomcat"},
    {8080, L"HTTP-Alt"},      {8083, L"MQTT-WS"},
    {8086, L"InfluxDB"},      {8088, L"HTTP-Alt2"},
    {8123, L"Home-Assistant"},{8443, L"HTTPS-Alt"},
    {8883, L"MQTT-SSL"},      {9000, L"Portainer"},
    {9090, L"Prometheus"},    {9091, L"Transmission"},
    {9100, L"Printer-PDL"},   {9123, L"Smart-Home"},
    {9200, L"Elasticsearch"}, {10000, L"Webmin"},
    {27017, L"MongoDB"},      {32400, L"Plex-Media"},
    {51413, L"BitTorrent"},
};

const std::set<int> ScanEngine::RISKY_PORTS = {
    21, 23, 135, 137, 138, 139, 445,
    1433, 1521, 1723, 3306, 3389, 4443,
    5432, 5900, 2375, 27017, 6379
};

const std::map<wstring, std::vector<int>> ScanEngine::PORT_PROFILES = {
    { L"common",   { 21,22,23,25,53,80,110,135,139,443,445,
                     3306,3389,5900,8080,8443 } },
    { L"iot",      { 80,443,1883,8008,8009,8080,8443,9123 } },
    { L"nas",      { 21,22,80,139,443,445,548,873,2049,
                     5000,5001,8080,8443 } },
    { L"security", { 22,80,443,8080,8443,10000 } },
    { L"full",     [] {
        std::vector<int> v;
        for (int i = 1; i <= 1024; i++) v.push_back(i);
        return v;
    }() },
};

// ─── Helpers ──────────────────────────────────────────────────────────────────

static wstring ToWide(const string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return L"";
    wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

static string ToNarrow(const wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

static wstring IpDwordToWstr(DWORD ip) {
    struct in_addr addr;
    addr.S_un.S_addr = ip;
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, buf, sizeof(buf));
    return ToWide(buf);
}

static wstring MacBytesToWstr(const BYTE* mac, int len) {
    wchar_t buf[24] = {};
    swprintf_s(buf, L"%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}

static wstring GetCurrentTimestampImpl() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[64];
    swprintf_s(buf, L"%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
    return buf;
}

wstring ScanEngine::GetCurrentTimestamp() {
    return GetCurrentTimestampImpl();
}

// ─── Constructor / Destructor ────────────────────────────────────────────────

ScanEngine::ScanEngine() {
    WSADATA wd;
    WSAStartup(MAKEWORD(2, 2), &wd);
}

ScanEngine::~ScanEngine() {
    // WSACleanup is managed globally in main
}

// ─── GetLocalNetworks ─────────────────────────────────────────────────────────

std::vector<ScanEngine::NetworkInterface> ScanEngine::GetLocalNetworks() {
    std::vector<NetworkInterface> result;

    ULONG bufLen = 15000;
    auto buf = std::make_unique<BYTE[]>(bufLen);
    auto pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.get());

    ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_PREFIX;
    ULONG ret = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &bufLen);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        buf = std::make_unique<BYTE[]>(bufLen);
        pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.get());
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr, pAddresses, &bufLen);
    }
    if (ret != NO_ERROR) return result;

    for (auto* p = pAddresses; p; p = p->Next) {
        if (p->OperStatus != IfOperStatusUp) continue;
        if (p->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (p->IfType == IF_TYPE_TUNNEL) continue;

        for (auto* ua = p->FirstUnicastAddress; ua; ua = ua->Next) {
            if (ua->Address.lpSockaddr->sa_family != AF_INET) continue;

            auto* sin = reinterpret_cast<sockaddr_in*>(ua->Address.lpSockaddr);
            DWORD ipDword = sin->sin_addr.S_un.S_addr;
            DWORD prefixLen = ua->OnLinkPrefixLength;
            DWORD maskDword = prefixLen == 0 ? 0 : (~0u) << (32 - prefixLen);
            maskDword = _byteswap_ulong(maskDword);

            wchar_t ipStr[INET_ADDRSTRLEN], maskStr[INET_ADDRSTRLEN];
            InetNtop(AF_INET, &sin->sin_addr, ipStr, INET_ADDRSTRLEN);

            struct in_addr maskAddr;
            maskAddr.S_un.S_addr = maskDword;
            InetNtop(AF_INET, &maskAddr, maskStr, INET_ADDRSTRLEN);

            DWORD netDword = ipDword & maskDword;
            struct in_addr netAddr;
            netAddr.S_un.S_addr = netDword;
            wchar_t netStr[INET_ADDRSTRLEN];
            InetNtop(AF_INET, &netAddr, netStr, INET_ADDRSTRLEN);

            NetworkInterface ni;
            ni.name = p->FriendlyName ? wstring(p->FriendlyName) : L"";
            ni.localIp = ipStr;
            ni.baseIp = netStr;
            ni.cidr = std::to_wstring(prefixLen);
            ni.mask = maskStr;

            // Get gateway
            for (auto* gw = p->FirstGatewayAddress; gw; gw = gw->Next) {
                if (gw->Address.lpSockaddr->sa_family == AF_INET) {
                    auto* gwSin = reinterpret_cast<sockaddr_in*>(gw->Address.lpSockaddr);
                    wchar_t gwStr[INET_ADDRSTRLEN];
                    InetNtop(AF_INET, &gwSin->sin_addr, gwStr, INET_ADDRSTRLEN);
                    ni.gateway = gwStr;
                    break;
                }
            }

            result.push_back(std::move(ni));
        }
    }

    return result;
}

// ─── LookupVendor ─────────────────────────────────────────────────────────────

wstring ScanEngine::LookupVendor(const wstring& mac) {
    if (mac.size() < 8) return L"";
    // Convert first 8 chars to uppercase narrow
    string prefix = ToNarrow(mac.substr(0, 8));
    for (char& c : prefix) c = (char)toupper((unsigned char)c);
    const char* v = LookupOuiVendor(prefix.c_str());
    if (v) return ToWide(v);
    return L"";
}

// ─── PingSweep ────────────────────────────────────────────────────────────────

int ScanEngine::PingSingle(const wstring& ip, int timeoutMs) {
    HANDLE hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) return -1;

    string ipNarrow = ToNarrow(ip);
    struct in_addr addr;
    inet_pton(AF_INET, ipNarrow.c_str(), &addr);

    char sendData[32] = "TransparencyPing";
    DWORD repBufSize = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8;
    auto repBuf = std::make_unique<BYTE[]>(repBufSize);

    DWORD start = GetTickCount();
    DWORD res = IcmpSendEcho(hIcmp, addr.S_un.S_addr, sendData, sizeof(sendData),
        nullptr, repBuf.get(), repBufSize, timeoutMs);
    DWORD elapsed = GetTickCount() - start;

    IcmpCloseHandle(hIcmp);

    if (res > 0) {
        auto* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(repBuf.get());
        if (reply->Status == IP_SUCCESS) return (int)elapsed;
    }
    return -1;
}

std::vector<wstring> ScanEngine::PingSweep(
    const std::vector<NetworkInterface>& nets,
    std::function<void(int, wstring)> progressCb,
    const std::atomic<bool>& cancelled)
{
    std::vector<wstring> liveIPs;
    std::mutex liveMtx;

    // Collect all IPs to scan
    std::vector<wstring> allIPs;
    for (auto& ni : nets) {
        // Parse base IP and mask
        string baseStr = ToNarrow(ni.baseIp);
        int cidr = _wtoi(ni.cidr.c_str());
        if (cidr < 8 || cidr > 30) continue; // Skip unusual subnets

        struct in_addr baseAddr;
        inet_pton(AF_INET, baseStr.c_str(), &baseAddr);
        DWORD baseIpDw = _byteswap_ulong(baseAddr.S_un.S_addr);
        DWORD mask = cidr == 0 ? 0 : (~0u) << (32 - cidr);
        DWORD count = ~mask; // host count (including broadcast)
        if (count > 512) count = 512; // Cap for safety

        for (DWORD i = 1; i < count; i++) {
            DWORD hostDw = _byteswap_ulong((baseIpDw & mask) | i);
            struct in_addr hostAddr;
            hostAddr.S_un.S_addr = hostDw;
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &hostAddr, buf, sizeof(buf));
            allIPs.push_back(ToWide(buf));
        }
    }

    if (allIPs.empty()) return liveIPs;

    // Semaphore for max 40 concurrent pings
    SimpleSemaphore sem(40);
    std::vector<std::future<void>> futures;

    int total = (int)allIPs.size();
    std::atomic<int> done{ 0 };

    for (auto& ipStr : allIPs) {
        if (cancelled) break;
        sem.acquire();
        futures.push_back(std::async(std::launch::async, [&, ip = ipStr]() {
            auto defer = [&sem]() { sem.release(); };
            int lat = PingSingle(ip, 600);
            if (lat >= 0) {
                std::lock_guard<std::mutex> lock(liveMtx);
                liveIPs.push_back(ip);
            }
            int d = ++done;
            if (progressCb && (d % 10 == 0 || d == total)) {
                int pct = (d * 40) / total; // 0-40% for ping phase
                progressCb(pct, L"Scanning " + ip + L"...");
            }
            defer();
        }));
    }

    for (auto& f : futures) f.wait();
    return liveIPs;
}

// ─── GetArpTable ─────────────────────────────────────────────────────────────

std::map<wstring, wstring> ScanEngine::GetArpTable() {
    std::map<wstring, wstring> result;

    ULONG size = 0;
    GetIpNetTable(nullptr, &size, FALSE);
    if (size == 0) return result;

    auto buf = std::make_unique<BYTE[]>(size);
    auto* table = reinterpret_cast<MIB_IPNETTABLE*>(buf.get());

    if (GetIpNetTable(table, &size, FALSE) != NO_ERROR) return result;

    for (DWORD i = 0; i < table->dwNumEntries; i++) {
        auto& row = table->table[i];
        if (row.dwType == MIB_IPNET_TYPE_INVALID) continue;
        if (row.dwPhysAddrLen != 6) continue;

        wstring ip = IpDwordToWstr(row.dwAddr);
        wstring mac = MacBytesToWstr(row.bPhysAddr, row.dwPhysAddrLen);
        result[ip] = mac;
    }

    return result;
}

std::map<wstring, wstring> ScanEngine::GetIPv6ArpTable() {
    std::map<wstring, wstring> result;

    MIB_IPNET_TABLE2* table = nullptr;
    if (GetIpNetTable2(AF_INET6, &table) != NO_ERROR) return result;

    for (ULONG i = 0; i < table->NumEntries; i++) {
        auto& row = table->Table[i];
        if (row.PhysicalAddressLength != 6) continue;

        wchar_t ipStr[INET6_ADDRSTRLEN];
        InetNtop(AF_INET6, &row.Address.Ipv6.sin6_addr, ipStr, INET6_ADDRSTRLEN);
        wstring ip = ipStr;

        // Skip link-local
        if (ip.substr(0, 4) == L"fe80") continue;
        // Skip loopback
        if (ip == L"::1") continue;

        wstring mac = MacBytesToWstr(row.PhysicalAddress, row.PhysicalAddressLength);
        result[ip] = mac;
    }

    FreeMibTable(table);
    return result;
}

std::map<wstring, wstring> ScanEngine::DiscoverIPv6NDP() {
    return GetIPv6ArpTable();
}

// ─── TcpProbe ────────────────────────────────────────────────────────────────

bool ScanEngine::TcpProbe(const wstring& ip, int port, int timeoutMs) {
    string ipNarrow = ToNarrow(ip);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;

    // Non-blocking
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    inet_pton(AF_INET, ipNarrow.c_str(), &addr.sin_addr);

    connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    fd_set writefds, exceptfds;
    FD_ZERO(&writefds);
    FD_ZERO(&exceptfds);
    FD_SET(s, &writefds);
    FD_SET(s, &exceptfds);

    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int sel = select(0, nullptr, &writefds, &exceptfds, &tv);
    bool connected = false;

    if (sel > 0 && FD_ISSET(s, &writefds) && !FD_ISSET(s, &exceptfds)) {
        // Verify connection
        int err = 0;
        int errLen = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &errLen);
        connected = (err == 0);
    }

    closesocket(s);
    return connected;
}

// ─── ScanPorts ───────────────────────────────────────────────────────────────

std::vector<int> ScanEngine::ScanPorts(
    const wstring& ip,
    const std::vector<int>& ports,
    bool gentle,
    const std::atomic<bool>& cancelled)
{
    std::vector<int> openPorts;
    std::mutex mtx;

    int timeout = gentle ? 1500 : 500;

    int num_threads = std::min(30, (int)ports.size());
    std::vector<std::thread> threads;
    std::atomic<size_t> idx{0};

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            while (!cancelled) {
                size_t current_idx = idx.fetch_add(1);
                if (current_idx >= ports.size()) break;

                int p = ports[current_idx];
                if (TcpProbe(ip, p, timeout)) {
                    std::lock_guard<std::mutex> lk(mtx);
                    openPorts.push_back(p);
                }
            }
        });
    }

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    std::sort(openPorts.begin(), openPorts.end());
    return openPorts;
}

// ─── DiscoverMDNS ────────────────────────────────────────────────────────────

std::map<wstring, std::vector<wstring>> ScanEngine::DiscoverMDNS(int timeoutMs) {
    std::map<wstring, std::vector<wstring>> result;

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return result;

    // Allow address reuse
    BOOL reuse = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    sockaddr_in bindAddr{};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(5353);
    bindAddr.sin_addr.S_un.S_addr = INADDR_ANY;
    bind(s, (sockaddr*)&bindAddr, sizeof(bindAddr));

    // Join mDNS multicast group
    ip_mreq mreq{};
    inet_pton(AF_INET, "224.0.0.251", &mreq.imr_multiaddr);
    mreq.imr_interface.S_un.S_addr = INADDR_ANY;
    setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));

    // Set TTL for multicast
    DWORD ttl = 255;
    setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));

    // Service types to query
    static const char* serviceTypes[] = {
        "_http._tcp.local",
        "_https._tcp.local",
        "_ssh._tcp.local",
        "_ftp._tcp.local",
        "_sftp-ssh._tcp.local",
        "_smb._tcp.local",
        "_afpovertcp._tcp.local",
        "_nfs._tcp.local",
        "_printer._tcp.local",
        "_ipp._tcp.local",
        "_ipp-tls._tcp.local",
        "_ipps._tcp.local",
        "_workstation._tcp.local",
        "_device-info._tcp.local",
        "_airplay._tcp.local",
        "_googlecast._tcp.local",
        "_homekit._tcp.local",
        "_matter._tcp.local",
        "_sonos._tcp.local",
        "_spotify-connect._tcp.local",
        "_raop._tcp.local",
        "_appletv._tcp.local",
        "_rdp._tcp.local",
        "_nvstream._tcp.local",
        "_daap._tcp.local",
        nullptr
    };

    sockaddr_in mcastAddr{};
    mcastAddr.sin_family = AF_INET;
    mcastAddr.sin_port = htons(5353);
    inet_pton(AF_INET, "224.0.0.251", &mcastAddr.sin_addr);

    // Build and send PTR query for each service type
    for (int i = 0; serviceTypes[i]; i++) {
        const char* svc = serviceTypes[i];
        // Build minimal DNS query packet
        // DNS header
        BYTE pkt[512] = {};
        int pos = 0;

        // ID = 0x0000, QR=0 (query), OPCODE=0, AA=0, TC=0, RD=0, RA=0, Z=0
        pkt[pos++] = 0x00; pkt[pos++] = 0x00; // ID
        pkt[pos++] = 0x00; pkt[pos++] = 0x00; // Flags
        pkt[pos++] = 0x00; pkt[pos++] = 0x01; // QDCOUNT = 1
        pkt[pos++] = 0x00; pkt[pos++] = 0x00; // ANCOUNT = 0
        pkt[pos++] = 0x00; pkt[pos++] = 0x00; // NSCOUNT = 0
        pkt[pos++] = 0x00; pkt[pos++] = 0x00; // ARCOUNT = 0

        // Encode service name as DNS labels
        // e.g. "_http._tcp.local" -> \x05_http\x04_tcp\x05local\x00
        string svcStr = svc;
        size_t start = 0, end = 0;
        while ((end = svcStr.find('.', start)) != string::npos) {
            string label = svcStr.substr(start, end - start);
            pkt[pos++] = (BYTE)label.size();
            memcpy(pkt + pos, label.c_str(), label.size());
            pos += (int)label.size();
            start = end + 1;
        }
        // Last label
        string lastLabel = svcStr.substr(start);
        pkt[pos++] = (BYTE)lastLabel.size();
        memcpy(pkt + pos, lastLabel.c_str(), lastLabel.size());
        pos += (int)lastLabel.size();
        pkt[pos++] = 0x00; // Root label

        // QTYPE = PTR (0x000C), QCLASS = IN (0x0001)
        pkt[pos++] = 0x00; pkt[pos++] = 0x0C;
        pkt[pos++] = 0x00; pkt[pos++] = 0x01;

        sendto(s, (char*)pkt, pos, 0, (sockaddr*)&mcastAddr, sizeof(mcastAddr));
        Sleep(20);
    }

    // Receive responses
    WSAEVENT evt = WSACreateEvent();
    WSAEventSelect(s, evt, FD_READ);

    DWORD deadline = GetTickCount() + (DWORD)timeoutMs;
    BYTE recvBuf[4096];

    while (GetTickCount() < deadline) {
        DWORD remaining = deadline - GetTickCount();
        if (remaining == 0) break;
        DWORD wait = WSAWaitForMultipleEvents(1, &evt, FALSE, std::min(remaining, (DWORD)200), FALSE);
        WSAResetEvent(evt);

        if (wait == WSA_WAIT_TIMEOUT) continue;

        sockaddr_in fromAddr{};
        int fromLen = sizeof(fromAddr);
        int recvd = recvfrom(s, (char*)recvBuf, sizeof(recvBuf), 0, (sockaddr*)&fromAddr, &fromLen);
        if (recvd <= 0) continue;

        wchar_t ipStr[INET_ADDRSTRLEN];
        InetNtop(AF_INET, &fromAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
        wstring ipWstr = ipStr;

        // Minimal DNS parsing - extract service type from answer name
        // Just note the IP and check answer section for PTR records
        if (recvd < 12) continue;

        // Parse header
        int anCount = (recvBuf[6] << 8) | recvBuf[7];
        if (anCount <= 0) continue;

        // Find service type in answer - simplified: look for known service strings
        string response(reinterpret_cast<char*>(recvBuf), recvd);

        auto findService = [&](const char* svcName) -> bool {
            return response.find(svcName) != string::npos;
        };

        std::vector<wstring> services;
        if (findService("_http._tcp"))      services.push_back(L"_http._tcp");
        if (findService("_https._tcp"))     services.push_back(L"_https._tcp");
        if (findService("_ssh._tcp"))       services.push_back(L"_ssh._tcp");
        if (findService("_smb._tcp"))       services.push_back(L"_smb._tcp");
        if (findService("_afp"))            services.push_back(L"_afpovertcp._tcp");
        if (findService("_printer._tcp"))   services.push_back(L"_printer._tcp");
        if (findService("_ipp._tcp"))       services.push_back(L"_ipp._tcp");
        if (findService("_airplay"))        services.push_back(L"_airplay._tcp");
        if (findService("_googlecast"))     services.push_back(L"_googlecast._tcp");
        if (findService("_homekit"))        services.push_back(L"_homekit._tcp");
        if (findService("_sonos"))          services.push_back(L"_sonos._tcp");
        if (findService("_raop"))           services.push_back(L"_raop._tcp");
        if (findService("_workstation"))    services.push_back(L"_workstation._tcp");
        if (findService("_device-info"))    services.push_back(L"_device-info._tcp");
        if (findService("_matter"))         services.push_back(L"_matter._tcp");

        if (!services.empty()) {
            auto& existing = result[ipWstr];
            for (auto& sv : services) {
                if (std::find(existing.begin(), existing.end(), sv) == existing.end())
                    existing.push_back(sv);
            }
        } else if (result.find(ipWstr) == result.end()) {
            result[ipWstr] = {}; // seen but no known service
        }
    }

    WSACloseEvent(evt);
    closesocket(s);
    return result;
}

// ─── DiscoverSSDP ────────────────────────────────────────────────────────────

std::map<wstring, wstring> ScanEngine::DiscoverSSDP(int timeoutMs) {
    std::map<wstring, wstring> result;

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return result;

    BOOL reuse = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

    const char* msearch =
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "MX: 2\r\n"
        "ST: ssdp:all\r\n"
        "\r\n";

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(1900);
    inet_pton(AF_INET, "239.255.255.250", &dest.sin_addr);

    sendto(s, msearch, (int)strlen(msearch), 0, (sockaddr*)&dest, sizeof(dest));

    // Also send unicast to broadcast
    sockaddr_in bcast{};
    bcast.sin_family = AF_INET;
    bcast.sin_port = htons(1900);
    bcast.sin_addr.S_un.S_addr = INADDR_BROADCAST;
    BOOL optBcast = TRUE;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char*)&optBcast, sizeof(optBcast));
    sendto(s, msearch, (int)strlen(msearch), 0, (sockaddr*)&bcast, sizeof(bcast));

    WSAEVENT evt = WSACreateEvent();
    WSAEventSelect(s, evt, FD_READ);

    DWORD deadline = GetTickCount() + (DWORD)timeoutMs;
    char recvBuf[4096];

    while (GetTickCount() < deadline) {
        DWORD remaining = deadline - GetTickCount();
        if (remaining == 0) break;
        DWORD wait = WSAWaitForMultipleEvents(1, &evt, FALSE, std::min(remaining, (DWORD)200), FALSE);
        WSAResetEvent(evt);
        if (wait == WSA_WAIT_TIMEOUT) continue;

        sockaddr_in fromAddr{};
        int fromLen = sizeof(fromAddr);
        int recvd = recvfrom(s, recvBuf, sizeof(recvBuf) - 1, 0, (sockaddr*)&fromAddr, &fromLen);
        if (recvd <= 0) continue;
        recvBuf[recvd] = '\0';

        wchar_t ipStr[INET_ADDRSTRLEN];
        InetNtop(AF_INET, &fromAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
        wstring ip = ipStr;

        string resp = recvBuf;

        // Extract SERVER or USN or ST header
        auto extractHeader = [&](const string& hdr) -> string {
            string search = hdr + ":";
            size_t pos = resp.find(search);
            if (pos == string::npos) {
                // Case insensitive search
                string respLower = resp;
                string hdrLower = search;
                std::transform(respLower.begin(), respLower.end(), respLower.begin(), ::tolower);
                std::transform(hdrLower.begin(), hdrLower.end(), hdrLower.begin(), ::tolower);
                pos = respLower.find(hdrLower);
                if (pos == string::npos) return "";
            }
            pos += search.size();
            while (pos < resp.size() && resp[pos] == ' ') pos++;
            size_t end = resp.find("\r\n", pos);
            if (end == string::npos) end = resp.size();
            return resp.substr(pos, end - pos);
        };

        string server = extractHeader("SERVER");
        string usn = extractHeader("USN");
        string st = extractHeader("ST");

        string info = server.empty() ? (usn.empty() ? st : usn) : server;

        if (result.find(ip) == result.end()) {
            result[ip] = ToWide(info);
        } else if (!info.empty()) {
            result[ip] += L" | " + ToWide(info);
        }
    }

    WSACloseEvent(evt);
    closesocket(s);
    return result;
}

// ─── LookupNetBIOS ────────────────────────────────────────────────────────────

wstring ScanEngine::LookupNetBIOS(const wstring& ip, int timeoutMs) {
    string ipNarrow = ToNarrow(ip);

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return L"";

    // NetBIOS Name Status Request
    BYTE req[] = {
        0xAB, 0xCD,       // Transaction ID
        0x00, 0x00,       // Flags (Query)
        0x00, 0x01,       // QDCOUNT = 1
        0x00, 0x00,       // ANCOUNT = 0
        0x00, 0x00,       // NSCOUNT = 0
        0x00, 0x00,       // ARCOUNT = 0
        // QNAME = "*\0" encoded as NetBIOS
        0x20,             // label length = 32
        'C','K','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        'A','A','A','A','A','A','A','A','A','A','A','A','A','A','A','A',
        0x00,             // null terminator
        0x00, 0x21,       // QTYPE = NBSTAT (33)
        0x00, 0x01        // QCLASS = IN
    };

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(137);
    inet_pton(AF_INET, ipNarrow.c_str(), &dest.sin_addr);

    // Set timeout
    DWORD to = (DWORD)timeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&to, sizeof(to));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char*)&to, sizeof(to));

    sendto(s, (char*)req, sizeof(req), 0, (sockaddr*)&dest, sizeof(dest));

    BYTE resp[1024] = {};
    sockaddr_in fromAddr{};
    int fromLen = sizeof(fromAddr);
    int recvd = recvfrom(s, (char*)resp, sizeof(resp), 0, (sockaddr*)&fromAddr, &fromLen);
    closesocket(s);

    if (recvd < 57) return L""; // Too short

    // Parse NetBIOS Name Status Response
    // Header is 12 bytes, then answer name (variable), then data
    // The actual name table starts after the answer
    // Skip header (12) + answer name + type/class/ttl/rdlength
    // Minimal parse: find the number of names after RDATA
    // At offset 56: number of names (1 byte)
    int numNames = resp[56];
    if (numNames <= 0 || recvd < 57 + numNames * 18) return L"";

    // Each name entry is 18 bytes: 15 chars + 1 type + 2 flags
    string firstName;
    for (int i = 0; i < numNames; i++) {
        int offset = 57 + i * 18;
        // NetBIOS name is 15 chars + 1 suffix byte
        char name[16] = {};
        memcpy(name, resp + offset, 15);
        // Trim trailing spaces
        for (int j = 14; j >= 0; j--) {
            if (name[j] == ' ' || name[j] == '\0') name[j] = '\0';
            else break;
        }
        if (name[0] != '\0' && firstName.empty()) {
            firstName = name;
        }
    }

    return ToWide(firstName);
}

// ─── GrabBanner ──────────────────────────────────────────────────────────────

wstring ScanEngine::GrabBanner(const wstring& ip, int port, int timeoutMs) {
    string ipNarrow = ToNarrow(ip);

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return L"";

    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    inet_pton(AF_INET, ipNarrow.c_str(), &addr.sin_addr);

    connect(s, (sockaddr*)&addr, sizeof(addr));

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(s, &wfds);

    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    if (select(0, nullptr, &wfds, nullptr, &tv) <= 0) {
        closesocket(s);
        return L"";
    }

    // Send HTTP GET for port 80/443/8080 etc
    if (port == 80 || port == 8080 || port == 8008 || port == 8081) {
        const char* req = "GET / HTTP/1.0\r\nHost: localhost\r\n\r\n";
        send(s, req, (int)strlen(req), 0);
    }

    // Read banner
    char buf[256] = {};
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(s, &rfds);

    mode = 0;
    ioctlsocket(s, FIONBIO, &mode);
    DWORD to2 = 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&to2, sizeof(to2));

    int recvd = recv(s, buf, sizeof(buf) - 1, 0);
    closesocket(s);

    if (recvd <= 0) return L"";

    // Clean non-printable chars
    for (int i = 0; i < recvd; i++) {
        if ((BYTE)buf[i] < 32 && buf[i] != '\n' && buf[i] != '\r') buf[i] = '.';
    }
    buf[recvd] = '\0';

    return ToWide(string(buf));
}

// ─── ReverseDNS ──────────────────────────────────────────────────────────────

wstring ScanEngine::ReverseDNS(const wstring& ip) {
    string ipNarrow = ToNarrow(ip);
    struct in_addr addr;
    if (inet_pton(AF_INET, ipNarrow.c_str(), &addr) != 1) return L"";

    char host[NI_MAXHOST];
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_addr = addr;

    if (getnameinfo((sockaddr*)&sa, sizeof(sa), host, NI_MAXHOST, nullptr, 0, NI_NAMEREQD) == 0) {
        return ToWide(host);
    }
    return L"";
}

// ─── FingerprintDeviceType ────────────────────────────────────────────────────

wstring ScanEngine::FingerprintDeviceType(const Device& d) {
    wstring vendor = d.vendor;
    std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::tolower);
    wstring hostname = d.hostname;
    std::transform(hostname.begin(), hostname.end(), hostname.begin(), ::tolower);
    wstring ssdp = d.ssdpInfo;
    std::transform(ssdp.begin(), ssdp.end(), ssdp.begin(), ::tolower);

    auto hasPort = [&](int p) {
        return std::find(d.openPorts.begin(), d.openPorts.end(), p) != d.openPorts.end();
    };
    auto hasMdns = [&](const wchar_t* svc) {
        for (auto& s : d.mdnsServices)
            if (s.find(svc) != wstring::npos) return true;
        return false;
    };
    auto vendorContains = [&](const wchar_t* v) {
        return vendor.find(v) != wstring::npos;
    };
    auto ssdpContains = [&](const wchar_t* v) {
        return ssdp.find(v) != wstring::npos;
    };
    auto hostContains = [&](const wchar_t* v) {
        return hostname.find(v) != wstring::npos;
    };

    // Network equipment
    if (vendorContains(L"cisco") || vendorContains(L"netgear") ||
        vendorContains(L"ubiquiti") || vendorContains(L"linksys") ||
        ssdpContains(L"router") || ssdpContains(L"gateway") ||
        hostContains(L"router") || hostContains(L"gateway"))
        return L"Router/Switch";

    // NAS
    if (vendorContains(L"synology") || vendorContains(L"qnap") ||
        (hasPort(5000) && hasPort(5001)) || hasPort(873) ||
        ssdpContains(L"nas") || hostContains(L"nas") || hostContains(L"diskstation"))
        return L"NAS / Storage";

    // Printer
    if (vendorContains(L"epson") || vendorContains(L"canon") || vendorContains(L"brother") ||
        vendorContains(L"hp") ||
        hasMdns(L"_ipp") || hasMdns(L"_printer") || hasPort(9100) || hasPort(631) ||
        ssdpContains(L"printer") || hostContains(L"printer"))
        return L"Printer";

    // Smart TV / Streaming
    if (vendorContains(L"roku") || hasMdns(L"_googlecast") || hasMdns(L"_airplay") ||
        hasMdns(L"_raop") || ssdpContains(L"tv") || ssdpContains(L"roku") ||
        ssdpContains(L"chromecast") || hostContains(L"appletv") || hostContains(L"roku"))
        return L"Smart TV / Streaming";

    // Smart Speaker
    if (vendorContains(L"sonos") || hasMdns(L"_sonos") || ssdpContains(L"sonos") ||
        vendorContains(L"bose") || vendorContains(L"google") && hasPort(8008) ||
        vendorContains(L"amazon") || ssdpContains(L"echo") || ssdpContains(L"alexa"))
        return L"Smart Speaker";

    // Smart Home Hub
    if (vendorContains(L"nest") || vendorContains(L"philips hue") ||
        hasMdns(L"_homekit") || hasMdns(L"_matter") ||
        ssdpContains(L"homekit") || ssdpContains(L"hue bridge") ||
        hostContains(L"homehub") || hostContains(L"home assistant") || hasPort(8123))
        return L"Smart Home Hub";

    // IoT device
    if (hasPort(1883) || hasPort(8883) || ssdpContains(L"iot") ||
        hostContains(L"iot") || hostContains(L"sensor") || hostContains(L"esp"))
        return L"IoT Device";

    // Raspberry Pi
    if (vendorContains(L"raspberry"))
        return L"Single-Board Computer";

    // Desktop PC / Windows
    if (hasPort(3389) || hasPort(445) || hasPort(139) || hasPort(135))
        return L"Windows PC";

    // Linux/Unix server
    if (hasPort(22) && (hasPort(80) || hasPort(443) || hasPort(8080)))
        return L"Linux Server";

    // macOS
    if (vendorContains(L"apple") && (hasMdns(L"_workstation") || hasPort(548)))
        return L"Mac";

    // Mobile/Phone
    if (vendorContains(L"apple") || vendorContains(L"samsung") ||
        vendorContains(L"xiaomi") || vendorContains(L"huawei"))
        return L"Mobile Device";

    // Generic computer
    if (vendorContains(L"dell") || vendorContains(L"hp") || vendorContains(L"lenovo") ||
        vendorContains(L"intel") || vendorContains(L"microsoft"))
        return L"Computer";

    return L"Unknown Device";
}

// ─── CalcConfidence ────────────────────────────────────────────────────────────

int ScanEngine::CalcConfidence(const Device& d) {
    int score = 0;

    if (!d.vendor.empty()) score += 20;
    if (!d.hostname.empty()) score += 15;
    if (!d.openPorts.empty()) score += 20;
    if (!d.mdnsServices.empty()) score += 25;
    if (!d.ssdpInfo.empty()) score += 15;
    if (!d.netbiosName.empty()) score += 10;
    if (d.latencyMs >= 0) score += 5;

    return std::min(score, 100);
}

// ─── IoT Risk Profiling ────────────────────────────────────────────────────────
// Returns a plain-language risk summary for IoT devices, or empty string if no risk.

wstring ScanEngine::ProfileIoTRisk(const Device& d) {
    auto hasPort = [&](int p) {
        return std::find(d.openPorts.begin(), d.openPorts.end(), p) != d.openPorts.end();
    };

    wstring risk;
    bool hasHttp  = hasPort(80);
    bool hasHttps = hasPort(443);
    bool hasTelnet = hasPort(23);
    bool hasFtp    = hasPort(21);
    bool hasUpnp   = hasPort(1900);
    bool hasSsh    = hasPort(22);
    bool hasRtsp   = hasPort(554);
    bool hasOnvif  = hasPort(2020) || hasPort(8899) || hasPort(37777);

    // Telnet on any device is critical
    if (hasTelnet) {
        risk += L"CRITICAL: Telnet (port 23) is open. "
                L"Credentials transmitted in cleartext — any device on the network can intercept them. "
                L"Disable Telnet immediately and enable SSH if remote access is needed.\r\n";
    }

    // FTP is high risk
    if (hasFtp) {
        risk += L"HIGH: FTP (port 21) is open. "
                L"Unencrypted file transfer — passwords and data are visible in plain text on the wire. "
                L"Disable FTP or replace with SFTP.\r\n";
    }

    // HTTP admin panel without HTTPS
    if (hasHttp && !hasHttps) {
        risk += L"MEDIUM: HTTP admin panel (port 80) without HTTPS. "
                L"Logging into this device over HTTP exposes your credentials on the local network. "
                L"Enable HTTPS in device settings, or access only from a trusted wired connection.\r\n";
    }

    // UPnP exposed
    if (hasUpnp) {
        risk += L"MEDIUM: UPnP (port 1900) is exposed. "
                L"UPnP has a history of critical vulnerabilities (CallStranger, NAT injection). "
                L"Disable UPnP in device and router settings if not required.\r\n";
    }

    // RTSP/camera stream exposed
    if (hasRtsp || hasOnvif) {
        risk += L"HIGH: Camera stream port (RTSP/ONVIF) is open. "
                L"Verify this camera requires authentication before connecting. "
                L"Many IP cameras have default or blank credentials. "
                L"Change the admin password and disable ONVIF if unused.\r\n";
    }

    // SSH without HTTPS (unusual for IoT — may indicate compromised device)
    if (hasSsh && !hasHttp && d.deviceType.find(L"IoT") != wstring::npos) {
        risk += L"INFO: SSH (port 22) open on what appears to be an IoT device. "
                L"Verify this is expected — some IoT devices include SSH backdoors. "
                L"Check that SSH uses key-based auth, not a default password.\r\n";
    }

    return risk;
}

// ─── FillConfidenceAlternatives ───────────────────────────────────────────────
// Populates altType1/altConf1/altType2/altConf2 on a device after fingerprinting.

void ScanEngine::FillConfidenceAlternatives(Device& d) {
    // Score each type category independently
    struct TypeScore { wstring type; int score; };
    std::vector<TypeScore> scores;

    auto hasPort = [&](int p) {
        return std::find(d.openPorts.begin(), d.openPorts.end(), p) != d.openPorts.end();
    };
    auto hasMdns = [&](const wchar_t* svc) {
        for (auto& s : d.mdnsServices)
            if (s.find(svc) != wstring::npos) return true;
        return false;
    };
    wstring vendor = d.vendor; std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::tolower);
    wstring ssdp   = d.ssdpInfo; std::transform(ssdp.begin(), ssdp.end(), ssdp.begin(), ::tolower);
    wstring host   = d.hostname; std::transform(host.begin(), host.end(), host.begin(), ::tolower);

    // Helper to do a case-insensitive match against a lowercased input string,
    // where 'v' must already be lowercased before calling this lambda.
    auto vendorHas = [&](const wchar_t* v) { return vendor.find(v) != wstring::npos; };
    auto ssdpHas   = [&](const wchar_t* v) { return ssdp.find(v) != wstring::npos; };
    auto hostHas   = [&](const wchar_t* v) { return host.find(v) != wstring::npos; };

    auto add = [&](const wchar_t* type, int sc) { scores.push_back({ type, sc }); };

    add(L"Router/Switch",      (vendorHas(L"cisco")||vendorHas(L"netgear")||vendorHas(L"ubiquiti")||ssdpHas(L"router")||hostHas(L"router")) ? 80 : 5);
    add(L"NAS / Storage",      (vendorHas(L"synology")||vendorHas(L"qnap")||(hasPort(5000)&&hasPort(5001))||hostHas(L"nas")) ? 80 : 5);
    add(L"Printer",            (hasMdns(L"_ipp")||hasMdns(L"_printer")||hasPort(9100)||hasPort(631)||vendorHas(L"epson")||vendorHas(L"canon")||vendorHas(L"brother")) ? 80 : 5);
    add(L"Smart TV / Streaming",(hasMdns(L"_googlecast")||hasMdns(L"_airplay")||vendorHas(L"roku")||ssdpHas(L"tv")) ? 80 : 5);
    add(L"Smart Speaker",      (hasMdns(L"_sonos")||vendorHas(L"sonos")||vendorHas(L"amazon")||ssdpHas(L"echo")) ? 80 : 5);
    add(L"Smart Home Hub",     (hasMdns(L"_homekit")||hasMdns(L"_matter")||vendorHas(L"philips hue")||hasPort(8123)) ? 80 : 5);
    add(L"IoT Device",         (hasPort(1883)||hasPort(8883)||hostHas(L"esp")||hostHas(L"sensor")) ? 75 : 5);
    add(L"Windows PC",         (hasPort(3389)||hasPort(445)||(hasPort(139)&&hasPort(135))) ? 80 : 5);
    add(L"Linux Server",       (hasPort(22)&&(hasPort(80)||hasPort(443)||hasPort(8080))) ? 75 : 5);
    add(L"Mac",                (vendorHas(L"apple")&&(hasMdns(L"_workstation")||hasPort(548))) ? 80 : 5);
    add(L"Mobile Device",      (vendorHas(L"apple")||vendorHas(L"samsung")||vendorHas(L"xiaomi")) ? 50 : 5);
    add(L"Computer",           (vendorHas(L"dell")||vendorHas(L"hp")||vendorHas(L"lenovo")||vendorHas(L"intel")) ? 60 : 5);
    add(L"Unknown Device",     15);

    // Sort descending
    std::sort(scores.begin(), scores.end(), [](const TypeScore& a, const TypeScore& b) {
        return a.score > b.score;
    });

    // Remove primary type from alternatives
    int altIdx = 0;
    for (auto& ts : scores) {
        if (ts.type == d.deviceType) continue;
        if (altIdx == 0) { d.altType1 = ts.type; d.altConf1 = ts.score; altIdx++; }
        else if (altIdx == 1) { d.altType2 = ts.type; d.altConf2 = ts.score; break; }
    }
}

// ─── AnalyzeAnomalies ────────────────────────────────────────────────────────

std::vector<Anomaly> ScanEngine::AnalyzeAnomalies(
    const ScanResult& current,
    const ScanResult& previous)
{
    std::vector<Anomaly> anomalies;

    // Build previous device lookup by MAC
    std::map<wstring, const Device*> prevByMac;
    std::map<wstring, const Device*> prevByIp;
    for (auto& d : previous.devices) {
        if (!d.mac.empty()) prevByMac[d.mac] = &d;
        prevByIp[d.ip] = &d;
    }

    std::set<wstring> currentMacs;
    std::set<wstring> currentIps;
    for (auto& d : current.devices) {
        if (!d.mac.empty()) currentMacs.insert(d.mac);
        currentIps.insert(d.ip);
    }

    // Per-device anomalies
    for (auto& dev : current.devices) {
        // New device
        bool isNew = (prevByMac.find(dev.mac) == prevByMac.end() &&
                      prevByIp.find(dev.ip) == prevByIp.end());
        if (isNew && !previous.devices.empty()) {
            Anomaly a;
            a.type = L"new_device";
            a.severity = L"medium";
            a.deviceIp = dev.ip;
            a.description = L"New device detected: " + dev.ip +
                            (dev.hostname.empty() ? L"" : L" (" + dev.hostname + L")");
            a.explanation = L"A device was found on your network that was not present in the previous scan. "
                            L"This could be a new device you added, a guest, or an unauthorized device. "
                            L"Vendor: " + (dev.vendor.empty() ? L"Unknown" : dev.vendor) + L".";
            a.remediation = L"1. Identify the device by its MAC address and vendor. "
                            L"2. If authorized, mark it as 'Known' in the device list. "
                            L"3. If unrecognized, consider blocking it on your router's MAC filter. "
                            L"4. Enable router logs to track when it connected.";
            a.traceSource = L"ArpTable+PingSweep";
            anomalies.push_back(a);
        }

        // Risky ports
        for (int port : dev.openPorts) {
            if (RISKY_PORTS.count(port) == 0) continue;

            Anomaly a;
            a.type = L"risky_port";
            a.deviceIp = dev.ip;
            a.affectedPorts.push_back(port);

            switch (port) {
            case 21:
                a.severity = L"high";
                a.description = L"FTP (port 21) is open on " + dev.ip;
                a.explanation = L"FTP transmits data and credentials in plaintext, making it vulnerable to "
                                L"man-in-the-middle attacks and packet sniffing. Modern replacements (SFTP/FTPS) "
                                L"should be used instead.";
                a.remediation = L"1. Disable FTP if not actively used. "
                                L"2. Replace with SFTP (port 22) or FTPS (port 990). "
                                L"3. If FTP is required, restrict access with a firewall to known IPs only. "
                                L"4. Enable FTP over TLS (FTPS) at minimum.";
                break;
            case 23:
                a.severity = L"critical";
                a.description = L"Telnet (port 23) is open on " + dev.ip;
                a.explanation = L"Telnet transmits everything - including usernames and passwords - in plaintext. "
                                L"It has been deprecated for decades and should never be used on a network. "
                                L"This is a serious security risk as any attacker who can see network traffic "
                                L"can capture credentials.";
                a.remediation = L"1. Immediately disable Telnet on this device. "
                                L"2. Enable SSH instead (port 22). "
                                L"3. If the device is a router or switch, access it via the web interface or SSH. "
                                L"4. If this is an IoT device, check for a firmware update that adds SSH.";
                break;
            case 135:
                a.severity = L"high";
                a.description = L"MSRPC (port 135) exposed on " + dev.ip;
                a.explanation = L"Windows Remote Procedure Call (MSRPC) on port 135 is used for DCOM and WMI. "
                                L"This port has been exploited by numerous worms (MS03-026, Blaster) and should "
                                L"not be accessible across network segments. Exposure increases lateral movement risk.";
                a.remediation = L"1. Block port 135 at the firewall/router. "
                                L"2. Ensure Windows Firewall is enabled on this device. "
                                L"3. Apply all Windows security updates. "
                                L"4. Consider network segmentation to isolate Windows hosts.";
                break;
            case 139:
                a.severity = L"high";
                a.description = L"NetBIOS Session Service (port 139) on " + dev.ip;
                a.explanation = L"NetBIOS over TCP/IP (port 139) is an older Windows file sharing protocol. "
                                L"Combined with SMB, it can expose file shares and be exploited for credential "
                                L"attacks (Pass-the-Hash). EternalBlue/WannaCry also targeted this.";
                a.remediation = L"1. Disable NetBIOS over TCP/IP if not needed. "
                                L"2. Block ports 137-139 at the firewall. "
                                L"3. Use SMB2/SMB3 (port 445) instead if file sharing is needed. "
                                L"4. Ensure SMBv1 is disabled on all Windows hosts.";
                break;
            case 445:
                a.severity = L"high";
                a.description = L"SMB (port 445) exposed on " + dev.ip;
                a.explanation = L"Server Message Block (SMB) is used for Windows file sharing. "
                                L"This port was targeted by EternalBlue (NSA exploit) used in the WannaCry "
                                L"and NotPetya ransomware attacks. Exposure to the internet or untrusted "
                                L"networks is extremely dangerous. Even on local networks, SMBv1 has severe vulnerabilities.";
                a.remediation = L"1. Block SMB (445) at the network perimeter - NEVER expose to internet. "
                                L"2. Disable SMBv1 on all Windows machines. "
                                L"3. Apply MS17-010 patch if not already done. "
                                L"4. Consider requiring authentication before accessing shares. "
                                L"5. Use Windows Defender Firewall to restrict SMB to known hosts.";
                break;
            case 1433:
                a.severity = L"high";
                a.description = L"SQL Server (port 1433) exposed on " + dev.ip;
                a.explanation = L"Microsoft SQL Server listening on port 1433 is a target for SQL injection, "
                                L"brute force, and exploitation. Direct database access from the network "
                                L"bypasses application-layer security controls and can lead to data theft.";
                a.remediation = L"1. Restrict SQL Server access to the application server only via firewall. "
                                L"2. Disable the SA account or ensure it has a strong password. "
                                L"3. Enable SQL Server auditing. "
                                L"4. Use Windows Authentication instead of SQL Authentication where possible. "
                                L"5. Apply all SQL Server security updates.";
                break;
            case 1723:
                a.severity = L"medium";
                a.description = L"PPTP VPN (port 1723) on " + dev.ip;
                a.explanation = L"PPTP (Point-to-Point Tunneling Protocol) is a deprecated VPN protocol "
                                L"with known cryptographic weaknesses. MS-CHAPv2, used by PPTP, can be "
                                L"cracked in under a day with modern hardware. PPTP should be considered broken.";
                a.remediation = L"1. Replace PPTP with OpenVPN, WireGuard, or IKEv2/IPSec. "
                                L"2. If PPTP must be used temporarily, ensure strong passwords. "
                                L"3. Consider limiting PPTP access to specific client IPs at the firewall.";
                break;
            case 3306:
                a.severity = L"high";
                a.description = L"MySQL (port 3306) exposed on " + dev.ip;
                a.explanation = L"MySQL/MariaDB database server directly accessible on the network. "
                                L"Databases should never be directly accessible across the network unless "
                                L"absolutely necessary. Attackers can attempt brute force, exploit known "
                                L"vulnerabilities, or steal data if they gain access.";
                a.remediation = L"1. Bind MySQL to 127.0.0.1 if only local access is needed. "
                                L"2. Use a firewall to restrict port 3306 to the application server IP only. "
                                L"3. Disable the root user's remote login capability. "
                                L"4. Use strong passwords for all database accounts. "
                                L"5. Apply all MySQL/MariaDB security patches.";
                break;
            case 3389:
                a.severity = L"high";
                a.description = L"RDP (port 3389) exposed on " + dev.ip;
                a.explanation = L"Remote Desktop Protocol (RDP) is a frequent target for ransomware groups. "
                                L"BlueKeep (CVE-2019-0708) and DejaBlue are critical RDP vulnerabilities. "
                                L"Brute force attacks against RDP are extremely common. Over 4 million "
                                L"RDP servers are scanned daily by attackers on the internet.";
                a.remediation = L"1. Do NOT expose RDP directly to the internet. "
                                L"2. Use a VPN to access RDP internally. "
                                L"3. Enable Network Level Authentication (NLA). "
                                L"4. Change RDP port from 3389 to a non-standard port. "
                                L"5. Enable account lockout policies. "
                                L"6. Use RD Gateway if external access is required. "
                                L"7. Apply all BlueKeep/DejaBlue patches.";
                break;
            case 5900:
                a.severity = L"high";
                a.description = L"VNC (port 5900) exposed on " + dev.ip;
                a.explanation = L"VNC (Virtual Network Computing) provides remote desktop access. "
                                L"VNC often uses weak authentication (just a password, no username) "
                                L"and many implementations have been found to have critical vulnerabilities. "
                                L"If exposed to the internet, VNC servers are quickly found and attacked.";
                a.remediation = L"1. Never expose VNC directly to the internet. "
                                L"2. Tunnel VNC over SSH (ssh -L 5900:localhost:5900). "
                                L"3. Use a firewall to restrict port 5900 to trusted IPs only. "
                                L"4. Use a strong VNC password (8+ chars). "
                                L"5. Consider replacing VNC with RDP or a commercial solution with MFA.";
                break;
            default:
                a.severity = L"medium";
                a.description = L"Potentially risky port " + std::to_wstring(port) + L" open on " + dev.ip;
                a.explanation = L"Port " + std::to_wstring(port) + L" is flagged as potentially risky.";
                a.remediation = L"Review whether this service needs to be accessible from the network. "
                                L"If not, disable the service or firewall the port.";
            }

            a.traceSource = L"PortScan";
            anomalies.push_back(a);
        }

        // Port changes
        if (!dev.prevPorts.empty() && dev.prevPorts != dev.openPorts) {
            std::vector<int> newPorts, closedPorts;
            for (int p : dev.openPorts) {
                if (std::find(dev.prevPorts.begin(), dev.prevPorts.end(), p) == dev.prevPorts.end())
                    newPorts.push_back(p);
            }
            for (int p : dev.prevPorts) {
                if (std::find(dev.openPorts.begin(), dev.openPorts.end(), p) == dev.openPorts.end())
                    closedPorts.push_back(p);
            }

            if (!newPorts.empty()) {
                Anomaly a;
                a.type = L"port_changed";
                a.severity = L"medium";
                a.deviceIp = dev.ip;
                a.affectedPorts = newPorts;

                wstring portList;
                for (int p : newPorts) {
                    auto it = PORT_NAMES.find(p);
                    portList += std::to_wstring(p);
                    if (it != PORT_NAMES.end()) portList += L" (" + it->second + L")";
                    portList += L", ";
                }
                if (!portList.empty()) portList.pop_back(), portList.pop_back();

                a.description = L"New ports opened on " + dev.ip + L": " + portList;
                a.explanation = L"Services that were not running in the previous scan are now active. "
                                L"This could indicate new software was installed, a service was started, "
                                L"or a potential compromise installing a backdoor.";
                a.remediation = L"1. Verify the new service is intentional. "
                                L"2. Check which process is listening on these ports (netstat -ano). "
                                L"3. If unexpected, investigate for signs of compromise. "
                                L"4. Review installed software and startup items.";
                a.traceSource = L"PortScan+Comparison";
                anomalies.push_back(a);
            }
        }

        // IP changed for same MAC
        if (!dev.mac.empty()) {
            auto it = prevByMac.find(dev.mac);
            if (it != prevByMac.end() && it->second->ip != dev.ip) {
                Anomaly a;
                a.type = L"ip_changed";
                a.severity = L"low";
                a.deviceIp = dev.ip;
                a.description = L"IP address changed for " + dev.mac +
                                L": was " + it->second->ip + L", now " + dev.ip;
                a.explanation = L"A device's IP address changed between scans. This is common when DHCP "
                                L"leases expire and the device gets a new address. However, it can also "
                                L"indicate IP spoofing or ARP poisoning in rare cases.";
                a.remediation = L"1. If this is unexpected, verify the device's MAC address physically. "
                                L"2. Consider assigning a static IP or DHCP reservation to this device. "
                                L"3. Monitor for further IP changes.";
                a.traceSource = L"ArpTable+Comparison";
                anomalies.push_back(a);
            }
        }
    }

    // Device offline
    for (auto& prev : previous.devices) {
        if (currentIps.find(prev.ip) == currentIps.end() &&
            (prev.mac.empty() || currentMacs.find(prev.mac) == currentMacs.end())) {
            Anomaly a;
            a.type = L"device_offline";
            a.severity = L"low";
            a.deviceIp = prev.ip;
            a.description = L"Device " + prev.ip +
                            (prev.hostname.empty() ? L"" : L" (" + prev.hostname + L")") +
                            L" is no longer reachable";
            a.explanation = L"A device that was present in the previous scan is no longer responding. "
                            L"This could mean the device was turned off, unplugged, or left the network. "
                            L"It could also indicate a connectivity issue.";
            a.remediation = L"1. Check if the device was intentionally disconnected. "
                            L"2. Verify the device is powered on. "
                            L"3. Check for network issues if multiple devices are offline.";
            a.traceSource = L"PingSweep+Comparison";
            anomalies.push_back(a);
        }
    }

    return anomalies;
}

// ─── BuildResult ─────────────────────────────────────────────────────────────

ScanResult ScanEngine::BuildResult(
    const std::vector<wstring>& liveIPs,
    const std::map<wstring, wstring>& arpTable,
    const std::map<wstring, std::vector<wstring>>& mdns,
    const std::map<wstring, wstring>& ssdp,
    const std::string& mode,
    const std::vector<int>& portList,
    bool gentle,
    bool grabBanners,
    std::function<void(int, wstring)> progressCb)
{
    ScanResult result;
    result.mode = ToWide(mode);
    result.scannedAt = GetCurrentTimestampImpl();

    std::mutex devMtx;
    int total = (int)liveIPs.size();
    std::atomic<int> done{ 0 };

    // Process each live IP
    std::vector<std::future<Device>> futures;
    SimpleSemaphore sem(20);

    for (auto& ip : liveIPs) {
        if (_cancelled) break;
        sem.acquire();
        futures.push_back(std::async(std::launch::async, [&, ipStr = ip]() -> Device {
            auto defer = [&sem]() { sem.release(); };

            Device dev;
            dev.ip = ipStr;
            dev.online = true;
            dev.firstSeen = GetCurrentTimestampImpl();
            dev.lastSeen = GetCurrentTimestampImpl();

            // MAC from ARP
            auto arpIt = arpTable.find(ipStr);
            if (arpIt != arpTable.end()) {
                dev.mac = arpIt->second;
                dev.vendor = LookupVendor(dev.mac);
            }

            // mDNS services
            auto mdnsIt = mdns.find(ipStr);
            if (mdnsIt != mdns.end()) {
                dev.mdnsServices = mdnsIt->second;
            }

            // SSDP info
            auto ssdpIt = ssdp.find(ipStr);
            if (ssdpIt != ssdp.end()) {
                dev.ssdpInfo = ssdpIt->second;
            }

            // Port scan
            if (!portList.empty()) {
                dev.openPorts = ScanPorts(ipStr, portList, gentle, _cancelled);
            }

            // NetBIOS hostname
            if (!_cancelled) {
                dev.netbiosName = LookupNetBIOS(ipStr, 500);
            }

            // Reverse DNS
            if (!_cancelled) {
                dev.hostname = ReverseDNS(ipStr);
                if (dev.hostname.empty()) dev.hostname = dev.netbiosName;
            }

            // Banner grabbing
            if (grabBanners && !_cancelled) {
                for (int port : dev.openPorts) {
                    if (_cancelled) break;
                    wstring banner = GrabBanner(ipStr, port, 1500);
                    if (!banner.empty()) {
                        // Extract useful info from banner
                        if (dev.osGuess.empty() && banner.find(L"Server:") != wstring::npos) {
                            size_t pos = banner.find(L"Server:");
                            if (pos != wstring::npos) {
                                size_t end = banner.find(L"\r\n", pos);
                                if (end != wstring::npos)
                                    dev.osGuess = banner.substr(pos + 8, end - pos - 8);
                            }
                        }
                    }
                }
            }

            // Latency
            dev.latencyMs = PingSingle(ipStr, 300);

            // Fingerprint + confidence alternatives
            dev.deviceType = FingerprintDeviceType(dev);
            dev.confidence = CalcConfidence(dev);
            FillConfidenceAlternatives(dev);

            // IoT behavioral risk profiling
            static const std::set<wstring> IoT_TYPES = {
                L"IoT Device", L"Smart Speaker", L"Smart TV / Streaming",
                L"Smart Home Hub", L"Printer", L"NAS / Storage"
            };
            if (IoT_TYPES.count(dev.deviceType)) {
                dev.iotRiskDetail = ProfileIoTRisk(dev);
                dev.iotRisk = !dev.iotRiskDetail.empty();
            }

            int d = ++done;
            if (progressCb) {
                int pct = 50 + (d * 45) / std::max(total, 1);
                progressCb(pct, L"Analyzing " + ipStr);
            }

            defer();
            return dev;
        }));
    }

    for (auto& f : futures) {
        result.devices.push_back(f.get());
    }

    if (progressCb) progressCb(100, L"Scan complete");
    return result;
}

// ─── QuickScan ───────────────────────────────────────────────────────────────

std::future<ScanResult> ScanEngine::QuickScan(
    std::function<void(int, wstring)> progressCb)
{
    return std::async(std::launch::async, [this, progressCb]() -> ScanResult {
        Reset();
        if (progressCb) progressCb(0, L"Starting Quick Scan...");

        auto nets = GetLocalNetworks();
        if (nets.empty()) return ScanResult{};

        // Run mDNS, SSDP, IPv6 NDP in parallel
        auto fMdns = std::async(std::launch::async, [] { return DiscoverMDNS(2500); });
        auto fSsdp = std::async(std::launch::async, [] { return DiscoverSSDP(2500); });
        auto fNdp  = std::async(std::launch::async, [] { return DiscoverIPv6NDP(); });

        if (progressCb) progressCb(5, L"Discovering services (mDNS/SSDP)...");

        // Ping sweep
        auto liveIPs = PingSweep(nets, progressCb, _cancelled);
        if (progressCb) progressCb(45, L"Processing results...");

        auto mdns = fMdns.get();
        auto ssdp = fSsdp.get();

        // Add IPs discovered via mDNS/SSDP but not in ping sweep
        std::unordered_set<std::wstring> seenIPs(liveIPs.begin(), liveIPs.end());
        for (auto& kv : mdns) {
            if (seenIPs.insert(kv.first).second)
                liveIPs.push_back(kv.first);
        }
        for (auto& kv : ssdp) {
            if (seenIPs.insert(kv.first).second)
                liveIPs.push_back(kv.first);
        }

        auto arp = GetArpTable();
        return BuildResult(liveIPs, arp, mdns, ssdp, "quick", {}, false, false, progressCb);
    });
}

// ─── StandardScan ────────────────────────────────────────────────────────────

std::future<ScanResult> ScanEngine::StandardScan(
    std::function<void(int, wstring)> progressCb)
{
    return std::async(std::launch::async, [this, progressCb]() -> ScanResult {
        Reset();
        if (progressCb) progressCb(0, L"Starting Standard Scan...");

        auto nets = GetLocalNetworks();
        if (nets.empty()) return ScanResult{};

        auto fMdns = std::async(std::launch::async, [] { return DiscoverMDNS(3000); });
        auto fSsdp = std::async(std::launch::async, [] { return DiscoverSSDP(3000); });

        if (progressCb) progressCb(3, L"Discovering mDNS/SSDP services...");
        auto liveIPs = PingSweep(nets, progressCb, _cancelled);
        if (progressCb) progressCb(45, L"Fetching ARP table...");

        auto arp = GetArpTable();
        auto mdns = fMdns.get();
        auto ssdp = fSsdp.get();

        // Merge
        std::unordered_set<std::wstring> seenIPs(liveIPs.begin(), liveIPs.end());
        for (auto& kv : mdns) {
            if (seenIPs.insert(kv.first).second)
                liveIPs.push_back(kv.first);
        }
        for (auto& kv : ssdp) {
            if (seenIPs.insert(kv.first).second)
                liveIPs.push_back(kv.first);
        }

        auto& ports = PORT_PROFILES.at(L"common");
        return BuildResult(liveIPs, arp, mdns, ssdp, "standard", ports, false, false, progressCb);
    });
}

// ─── DeepScan ────────────────────────────────────────────────────────────────

std::future<ScanResult> ScanEngine::DeepScan(
    std::function<void(int, wstring)> progressCb)
{
    return std::async(std::launch::async, [this, progressCb]() -> ScanResult {
        Reset();
        if (progressCb) progressCb(0, L"Starting Deep Scan...");

        auto nets = GetLocalNetworks();
        if (nets.empty()) return ScanResult{};

        auto fMdns = std::async(std::launch::async, [] { return DiscoverMDNS(5000); });
        auto fSsdp = std::async(std::launch::async, [] { return DiscoverSSDP(5000); });

        if (progressCb) progressCb(2, L"Discovering services...");
        auto liveIPs = PingSweep(nets, progressCb, _cancelled);
        if (progressCb) progressCb(40, L"Fetching ARP table...");

        auto arp = GetArpTable();
        auto mdns = fMdns.get();
        auto ssdp = fSsdp.get();

        std::unordered_set<std::wstring> seenIPs(liveIPs.begin(), liveIPs.end());
        for (auto& kv : mdns) {
            if (seenIPs.insert(kv.first).second)
                liveIPs.push_back(kv.first);
        }
        for (auto& kv : ssdp) {
            if (seenIPs.insert(kv.first).second)
                liveIPs.push_back(kv.first);
        }

        // Extended port list (common + IoT + NAS extra ports)
        std::vector<int> ports;
        for (int p : PORT_PROFILES.at(L"common")) ports.push_back(p);
        for (int p : {548, 873, 2049, 5000, 5001, 1883, 8008, 8009, 8123, 9100, 32400}) {
            if (std::find(ports.begin(), ports.end(), p) == ports.end())
                ports.push_back(p);
        }
        std::sort(ports.begin(), ports.end());

        return BuildResult(liveIPs, arp, mdns, ssdp, "deep", ports, false, true, progressCb);
    });
}
