#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

#include "Monitor.h"
#include "Models.h"
#include "Scanner.h"

using std::wstring;

// ─── Helpers ─────────────────────────────────────────────────────────────────

static wstring ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 0) return L"";
    wstring w(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

// ─── Constructor / Destructor ────────────────────────────────────────────────

Monitor::Monitor() = default;

Monitor::~Monitor() {
    Stop();
}

// ─── Start ───────────────────────────────────────────────────────────────────

void Monitor::Start(
    const MonitorConfig& config,
    ScanCallback       onScanComplete,
    InternetCallback   onInternetChange,
    GatewayMacCallback onGatewayMacChange,
    AlertCallback      onAlert,
    DnsCallback        onDnsChange)
{
    Stop(); // Stop any existing worker

    {
        std::lock_guard<std::mutex> lk(_mutex);
        _config             = config;
        _onScanComplete     = onScanComplete;
        _onInternetChange   = onInternetChange;
        _onGatewayMacChange = onGatewayMacChange;
        _onAlert            = onAlert;
        _onDnsChange        = onDnsChange;
        _stopRequested      = false;
    }

    _running = true;
    _workerThread = std::thread(&Monitor::WorkerLoop, this);
}

// ─── Stop ─────────────────────────────────────────────────────────────────────

void Monitor::Stop() {
    _stopRequested = true;
    _scanner.Cancel();
    if (_workerThread.joinable()) _workerThread.join();
    _running = false;
}

// ─── UpdateConfig ─────────────────────────────────────────────────────────────

void Monitor::UpdateConfig(const MonitorConfig& config) {
    std::lock_guard<std::mutex> lk(_mutex);
    _config = config;
}

// ─── Getters ─────────────────────────────────────────────────────────────────

MonitorConfig Monitor::GetConfig() const {
    std::lock_guard<std::mutex> lk(_mutex);
    return _config;
}

InternetStatus Monitor::GetInternetStatus() const {
    std::lock_guard<std::mutex> lk(_mutex);
    return _internetStatus;
}

void Monitor::SetPreviousScan(const ScanResult& sr) {
    std::lock_guard<std::mutex> lk(_mutex);
    _previousScan = sr;
}

// ─── WorkerLoop ──────────────────────────────────────────────────────────────

void Monitor::WorkerLoop() {
    while (!_stopRequested) {
        MonitorConfig cfg;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            cfg = _config;
        }

        if (!cfg.enabled) {
            // Sleep briefly and re-check
            for (int i = 0; i < 20 && !_stopRequested; i++) Sleep(500);
            continue;
        }

        if (IsInQuietHours(cfg)) {
            for (int i = 0; i < 10 && !_stopRequested; i++) Sleep(1000);
            continue;
        }

        PerformChecks();

        // Sleep for interval
        int sleepMs = cfg.intervalMinutes * 60 * 1000;
        int elapsed = 0;
        while (elapsed < sleepMs && !_stopRequested) {
            Sleep(500);
            elapsed += 500;
        }
    }
}

// ─── PerformChecks ────────────────────────────────────────────────────────────

void Monitor::PerformChecks() {
    MonitorConfig cfg;
    ScanResult prevScan;
    {
        std::lock_guard<std::mutex> lk(_mutex);
        cfg = _config;
        prevScan = _previousScan;
    }

    // 1. Internet connectivity
    if (cfg.alertOnOutage || cfg.alertOnHighLatency) {
        auto status = CheckInternetConnectivity();

        InternetStatus oldStatus;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            oldStatus = _internetStatus;
            _internetStatus = status;
        }

        if (_onInternetChange && (status.online != oldStatus.online)) {
            _onInternetChange(status);
        }

        if (!status.online && cfg.alertOnOutage && _onAlert) {
            _onAlert(L"internet_outage", L"Internet connectivity lost. Last check: " + status.lastCheck);
        }

        if (status.online && cfg.alertOnHighLatency &&
            status.latencyMs > cfg.highLatencyThresholdMs && _onAlert) {
            _onAlert(L"high_latency",
                L"High internet latency: " + std::to_wstring(status.latencyMs) + L"ms" +
                L" (threshold: " + std::to_wstring(cfg.highLatencyThresholdMs) + L"ms)");
        }
    }

    // 2. Gateway MAC change
    if (cfg.alertOnGatewayMac) {
        wstring newMac = CheckGatewayMac();
        wstring oldMac;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            oldMac = _lastGatewayMac;
            if (!newMac.empty()) _lastGatewayMac = newMac;
        }

        if (!oldMac.empty() && !newMac.empty() && oldMac != newMac) {
            if (_onGatewayMacChange) _onGatewayMacChange(oldMac, newMac);
            if (_onAlert) {
                _onAlert(L"gateway_mac_changed",
                    L"Gateway MAC changed: " + oldMac + L" -> " + newMac +
                    L". This could indicate ARP poisoning or a router replacement.");
            }
        }
    }

    // 3. DNS change
    if (cfg.alertOnDnsChange) {
        wstring newDns = CheckDnsServers();
        wstring oldDns;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            oldDns = _lastDnsServers;
            if (!newDns.empty()) _lastDnsServers = newDns;
        }

        if (!oldDns.empty() && !newDns.empty() && oldDns != newDns) {
            if (_onDnsChange) _onDnsChange(newDns);
            if (_onAlert) {
                _onAlert(L"dns_changed",
                    L"DNS servers changed: " + oldDns + L" -> " + newDns +
                    L". Verify this was intentional.");
            }
        }
    }

    // 4. Quick scan
    _scanner.Reset();
    auto future = _scanner.QuickScan();
    auto newResult = future.get();

    if (!newResult.devices.empty() || !newResult.anomalies.empty()) {
        // Analyze anomalies
        auto anomalies = ScanEngine::AnalyzeAnomalies(newResult, prevScan);
        newResult.anomalies = anomalies;

        {
            std::lock_guard<std::mutex> lk(_mutex);
            _previousScan = newResult;
        }

        if (_onScanComplete) _onScanComplete(newResult);
    }
}

// ─── CheckInternetConnectivity ────────────────────────────────────────────────

InternetStatus Monitor::CheckInternetConnectivity() {
    InternetStatus status;

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t timeBuf[64];
    swprintf_s(timeBuf, L"%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    status.lastCheck = timeBuf;

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        status.online = false;
        status.latencyMs = -1;
        return status;
    }

    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);

    // Connect to 1.1.1.1:443 (Cloudflare DNS over HTTPS)
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(443);
    inet_pton(AF_INET, "1.1.1.1", &addr.sin_addr);

    DWORD start = GetTickCount();
    connect(s, (sockaddr*)&addr, sizeof(addr));

    fd_set wfds, efds;
    FD_ZERO(&wfds); FD_ZERO(&efds);
    FD_SET(s, &wfds); FD_SET(s, &efds);

    timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    int sel = select(0, nullptr, &wfds, &efds, &tv);
    DWORD elapsed = GetTickCount() - start;

    if (sel > 0 && FD_ISSET(s, &wfds) && !FD_ISSET(s, &efds)) {
        int err = 0, errLen = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &errLen);
        status.online = (err == 0);
        status.latencyMs = status.online ? (int)elapsed : -1;
    } else {
        status.online = false;
        status.latencyMs = -1;
    }

    closesocket(s);
    return status;
}

// ─── GetDefaultGatewayIp ─────────────────────────────────────────────────────

wstring Monitor::GetDefaultGatewayIp() {
    // Use GetIpForwardTable to find default route (dest=0.0.0.0, mask=0.0.0.0)
    ULONG size = 0;
    GetIpForwardTable(nullptr, &size, FALSE);
    if (size == 0) return L"";

    auto buf = std::make_unique<BYTE[]>(size);
    auto* table = reinterpret_cast<MIB_IPFORWARDTABLE*>(buf.get());

    if (GetIpForwardTable(table, &size, FALSE) != NO_ERROR) return L"";

    for (DWORD i = 0; i < table->dwNumEntries; i++) {
        auto& row = table->table[i];
        if (row.dwForwardDest == 0 && row.dwForwardMask == 0) {
            // Default route - get next hop
            struct in_addr gwAddr;
            gwAddr.S_un.S_addr = row.dwForwardNextHop;
            char buf2[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &gwAddr, buf2, sizeof(buf2));
            return ToWide(buf2);
        }
    }
    return L"";
}

// ─── CheckGatewayMac ─────────────────────────────────────────────────────────

wstring Monitor::CheckGatewayMac() {
    wstring gwIp = GetDefaultGatewayIp();
    if (gwIp.empty()) return L"";

    // Look up gateway MAC from ARP table
    ULONG size = 0;
    GetIpNetTable(nullptr, &size, FALSE);
    if (size == 0) return L"";

    auto buf = std::make_unique<BYTE[]>(size);
    auto* table = reinterpret_cast<MIB_IPNETTABLE*>(buf.get());

    if (GetIpNetTable(table, &size, FALSE) != NO_ERROR) return L"";

    std::string gwNarrow;
    {
        int n = WideCharToMultiByte(CP_UTF8, 0, gwIp.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (n > 0) { gwNarrow.resize(n - 1); WideCharToMultiByte(CP_UTF8, 0, gwIp.c_str(), -1, &gwNarrow[0], n, nullptr, nullptr); }
    }

    struct in_addr gwAddr;
    inet_pton(AF_INET, gwNarrow.c_str(), &gwAddr);

    for (DWORD i = 0; i < table->dwNumEntries; i++) {
        auto& row = table->table[i];
        if (row.dwAddr == gwAddr.S_un.S_addr && row.dwPhysAddrLen == 6) {
            wchar_t macStr[24];
            swprintf_s(macStr, L"%02X:%02X:%02X:%02X:%02X:%02X",
                row.bPhysAddr[0], row.bPhysAddr[1], row.bPhysAddr[2],
                row.bPhysAddr[3], row.bPhysAddr[4], row.bPhysAddr[5]);
            {
                std::lock_guard<std::mutex> lk(_mutex);
                _lastGatewayIp = gwIp;
            }
            return macStr;
        }
    }

    // Gateway not in ARP cache - ping it to populate ARP
    // Use IcmpSendEcho
    return L"";
}

// ─── CheckDnsServers ─────────────────────────────────────────────────────────

wstring Monitor::CheckDnsServers() {
    // Use GetNetworkParams
    FIXED_INFO* fi = nullptr;
    ULONG size = 0;
    GetNetworkParams(fi, &size);
    if (size == 0) return L"";

    auto buf = std::make_unique<BYTE[]>(size);
    fi = reinterpret_cast<FIXED_INFO*>(buf.get());
    if (GetNetworkParams(fi, &size) != ERROR_SUCCESS) return L"";

    wstring servers;
    IP_ADDR_STRING* dns = &fi->DnsServerList;
    while (dns) {
        if (dns->IpAddress.String[0] != '\0') {
            if (!servers.empty()) servers += L",";
            servers += ToWide(dns->IpAddress.String);
        }
        dns = dns->Next;
    }
    return servers;
}

// ─── IsInQuietHours ──────────────────────────────────────────────────────────

bool Monitor::IsInQuietHours(const MonitorConfig& config) {
    if (config.quietHoursStart.empty() || config.quietHoursEnd.empty()) return false;

    wstring current = GetCurrentTimeHHMM();

    // Parse HH:MM
    auto parseHHMM = [](const wstring& s) -> int {
        if (s.size() < 5 || s[2] != L':') return -1;
        int h = _wtoi(s.substr(0, 2).c_str());
        int m = _wtoi(s.substr(3, 2).c_str());
        return h * 60 + m;
    };

    int start = parseHHMM(config.quietHoursStart);
    int end   = parseHHMM(config.quietHoursEnd);
    int now   = parseHHMM(current);

    if (start < 0 || end < 0 || now < 0) return false;

    if (start <= end) {
        return now >= start && now < end;
    } else {
        // Overnight: e.g. 22:00 - 07:00
        return now >= start || now < end;
    }
}

wstring Monitor::GetCurrentTimeHHMM() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t buf[8];
    swprintf_s(buf, L"%02d:%02d", st.wHour, st.wMinute);
    return buf;
}

// ─── FireWebhook ─────────────────────────────────────────────────────────────

void Monitor::FireWebhook(const wstring& url, const wstring& payload) {
    // Simplified webhook via WinHTTP would go here
    // For now, just log - actual implementation would use WinHTTP
    (void)url;
    (void)payload;
}
