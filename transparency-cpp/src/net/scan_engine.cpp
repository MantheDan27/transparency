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

#include "net/scan_engine.h"
#include "core/oui_data.h"
#include "core/fingerprint.h"
#include "transparency/models.h"

using std::wstring;
using std::string;
using std::vector;
using std::map;
using std::set;

// ─── Port/Service Tables (delegating to core::) ──────────────────────────────

const std::map<int, wstring> ScanEngine::PORT_NAMES = core::PORT_NAMES;
const std::set<int> ScanEngine::RISKY_PORTS = core::RISKY_PORTS;
const std::map<wstring, std::vector<int>> ScanEngine::PORT_PROFILES = core::PORT_PROFILES;

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

// ─── LookupVendor (thin wrapper to core::) ────────────────────────────────────

wstring ScanEngine::LookupVendor(const wstring& mac) {
    return core::lookup_vendor(mac);
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

            ni.ifType = p->IfType;
            result.push_back(std::move(ni));
        }
    }

    return result;
}

// ─── RankNetworkInterfaces ─────────────────────────────────────────────────────

std::vector<ScanEngine::NetworkInterface> ScanEngine::RankNetworkInterfaces() {
    auto nets = GetLocalNetworks();

    for (auto& ni : nets) {
        int score = 0;
        wstring reason;

        // Gateway is the strongest signal
        if (!ni.gateway.empty()) {
            score += 50;
            reason += L"has gateway";
        }

        // Prefix length — /24 is ideal for home/office
        int cidr = _wtoi(ni.cidr.c_str());
        if (cidr >= 20 && cidr <= 24) { score += 30; }
        else if (cidr >= 16 && cidr < 20) { score += 15; }
        else { score += 5; }

        // Adapter type
        if (ni.ifType == 71) { score += 20; if (!reason.empty()) reason += L", "; reason += L"Wi-Fi"; }
        else if (ni.ifType == 6) { score += 15; if (!reason.empty()) reason += L", "; reason += L"Ethernet"; }

        // Penalize virtual adapters
        wstring nameLower = ni.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        if (nameLower.find(L"virtual") != wstring::npos ||
            nameLower.find(L"vmware") != wstring::npos ||
            nameLower.find(L"vethernet") != wstring::npos ||
            nameLower.find(L"hyper-v") != wstring::npos) {
            score -= 40;
            if (!reason.empty()) reason += L", "; reason += L"virtual adapter (penalized)";
        }
        if (nameLower.find(L"vpn") != wstring::npos ||
            nameLower.find(L"tap") != wstring::npos) {
            score -= 20;
            if (!reason.empty()) reason += L", "; reason += L"VPN adapter (penalized)";
        }

        if (!reason.empty()) reason += L", ";
        reason += L"/" + ni.cidr + L" range";

        ni.score = score;
        ni.reason = reason;
    }

    // Sort descending by score
    std::sort(nets.begin(), nets.end(), [](const NetworkInterface& a, const NetworkInterface& b) {
        return a.score > b.score;
    });

    return nets;
}

// ─── PingSingle ───────────────────────────────────────────────────────────────

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

// ─── PingSweep ────────────────────────────────────────────────────────────────

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
    SimpleSemaphore sem(64);
    std::vector<std::future<void>> futures;

    int total = (int)allIPs.size();
    std::atomic<int> done{ 0 };

    for (auto& ipStr : allIPs) {
        if (cancelled) break;
        sem.acquire();
        futures.push_back(std::async(std::launch::async, [&, ip = ipStr]() {
            auto defer = [&sem]() { sem.release(); };
            int lat = PingSingle(ip, 350);
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

// ─── GetIPv6ArpTable ─────────────────────────────────────────────────────────

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

// ─── DiscoverIPv6NDP ─────────────────────────────────────────────────────────

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
    SimpleSemaphore sem(32);

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
                dev.evidence.push_back({L"mac", L"ARP table", dev.mac});
                dev.vendor = LookupVendor(dev.mac);
                if (!dev.vendor.empty())
                    dev.evidence.push_back({L"vendor", L"OUI lookup", dev.vendor});
            }

            // mDNS services
            auto mdnsIt = mdns.find(ipStr);
            if (mdnsIt != mdns.end()) {
                dev.mdnsServices = mdnsIt->second;
                wstring joined;
                for (auto& s : dev.mdnsServices) { if (!joined.empty()) joined += L", "; joined += s; }
                dev.evidence.push_back({L"mdns", L"mDNS discovery", joined});
            }

            // SSDP info
            auto ssdpIt = ssdp.find(ipStr);
            if (ssdpIt != ssdp.end()) {
                dev.ssdpInfo = ssdpIt->second;
                dev.evidence.push_back({L"ssdp", L"SSDP/UPnP", dev.ssdpInfo});
            }

            // Port scan
            if (!portList.empty()) {
                dev.openPorts = ScanPorts(ipStr, portList, gentle, _cancelled);
            }

            // NetBIOS hostname
            if (!_cancelled) {
                dev.netbiosName = LookupNetBIOS(ipStr, 500);
                if (!dev.netbiosName.empty())
                    dev.evidence.push_back({L"netbios", L"NetBIOS query", dev.netbiosName});
            }

            // Reverse DNS
            if (!_cancelled) {
                wstring dnsName = ReverseDNS(ipStr);
                if (!dnsName.empty()) {
                    dev.hostname = dnsName;
                    dev.evidence.push_back({L"hostname", L"Reverse DNS", dnsName});
                } else if (!dev.netbiosName.empty()) {
                    dev.hostname = dev.netbiosName;
                    dev.evidence.push_back({L"hostname", L"NetBIOS fallback", dev.netbiosName});
                }
            }

            // Port evidence
            if (!portList.empty() && !dev.openPorts.empty()) {
                wstring portStr;
                for (int p : dev.openPorts) {
                    if (!portStr.empty()) portStr += L",";
                    portStr += std::to_wstring(p);
                }
                dev.evidence.push_back({L"ports", L"TCP port scan", portStr});
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

            // Banner evidence
            if (!dev.osGuess.empty())
                dev.evidence.push_back({L"os", L"Banner grab", dev.osGuess});

            // Fingerprint + confidence alternatives + classification reason
            dev.deviceType = core::fingerprint_device_type(dev);
            dev.confidence = core::calc_confidence(dev);
            core::build_classification_reason(dev);
            core::fill_confidence_alternatives(dev);

            // IoT behavioral risk profiling
            static const std::set<wstring> IoT_TYPES = {
                L"IoT Device", L"Smart Speaker", L"Smart TV / Streaming",
                L"Smart Home Hub", L"Printer", L"NAS / Storage"
            };
            if (IoT_TYPES.count(dev.deviceType)) {
                dev.iotRiskDetail = core::profile_iot_risk(dev);
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
        auto fMdns = std::async(std::launch::async, [] { return DiscoverMDNS(1500); });
        auto fSsdp = std::async(std::launch::async, [] { return DiscoverSSDP(1500); });
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
