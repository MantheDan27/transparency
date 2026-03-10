#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <future>
#include <functional>
#include <atomic>
#include <mutex>
#include "Models.h"

class ScanEngine {
public:
    struct NetworkInterface {
        std::wstring name;
        std::wstring localIp;
        std::wstring baseIp;
        std::wstring cidr;
        std::wstring gateway;
        std::wstring mask;
        int    score  = 0;      // NIC ranking score (higher = better)
        std::wstring reason;    // human-readable explanation of score
        int    ifType = 0;      // adapter type (6=Ethernet, 71=Wi-Fi)
    };

    ScanEngine();
    ~ScanEngine();

    // Network discovery
    static std::vector<NetworkInterface> GetLocalNetworks();
    static std::vector<NetworkInterface> RankNetworkInterfaces();
    static std::wstring LookupVendor(const std::wstring& mac);
    static std::wstring FingerprintDeviceType(const Device& d);
    static void BuildClassificationReason(Device& d);
    static int CalcConfidence(const Device& d);
    static std::wstring GetCurrentTimestamp();

    // Async scan entry points
    std::future<ScanResult> QuickScan(
        std::function<void(int, std::wstring)> progressCb = nullptr);
    std::future<ScanResult> StandardScan(
        std::function<void(int, std::wstring)> progressCb = nullptr);
    std::future<ScanResult> DeepScan(
        std::function<void(int, std::wstring)> progressCb = nullptr);

    // Cancel
    void Cancel() { _cancelled = true; }
    bool IsCancelled() const { return _cancelled; }
    void Reset() { _cancelled = false; }

    // Anomaly analysis
    static std::vector<Anomaly> AnalyzeAnomalies(
        const ScanResult& current,
        const ScanResult& previous);

    // IoT behavioral risk profiling — returns plain-language risk summary or empty
    static std::wstring ProfileIoTRisk(const Device& d);

    // Fill top-2 confidence alternative device types on a device
    static void FillConfidenceAlternatives(Device& d);

    // Port/service data
    static const std::map<int, std::wstring> PORT_NAMES;
    static const std::set<int> RISKY_PORTS;
    static const std::map<std::wstring, std::vector<int>> PORT_PROFILES;

private:
    std::atomic<bool> _cancelled{ false };
    mutable std::mutex _mutex;

    // Low-level probes
    static std::vector<std::wstring> PingSweep(
        const std::vector<NetworkInterface>& nets,
        std::function<void(int, std::wstring)> progressCb,
        const std::atomic<bool>& cancelled);

    static std::map<std::wstring, std::wstring> GetArpTable();
    static std::map<std::wstring, std::wstring> GetIPv6ArpTable();

    static bool TcpProbe(const std::wstring& ip, int port, int timeoutMs);

    static std::vector<int> ScanPorts(
        const std::wstring& ip,
        const std::vector<int>& ports,
        bool gentle,
        const std::atomic<bool>& cancelled);

    static std::map<std::wstring, std::vector<std::wstring>> DiscoverMDNS(int timeoutMs);
    static std::map<std::wstring, std::wstring> DiscoverSSDP(int timeoutMs);
    static std::wstring LookupNetBIOS(const std::wstring& ip, int timeoutMs);
    static std::map<std::wstring, std::wstring> DiscoverIPv6NDP();
    static std::wstring GrabBanner(const std::wstring& ip, int port, int timeoutMs);

    // DNS reverse lookup
    static std::wstring ReverseDNS(const std::wstring& ip);

    // Ping single host, return latency or -1
    static int PingSingle(const std::wstring& ip, int timeoutMs);

    // Build a ScanResult from raw data
    ScanResult BuildResult(
        const std::vector<std::wstring>& liveIPs,
        const std::map<std::wstring, std::wstring>& arpTable,
        const std::map<std::wstring, std::vector<std::wstring>>& mdns,
        const std::map<std::wstring, std::wstring>& ssdp,
        const std::string& mode,
        const std::vector<int>& portList,
        bool gentle,
        bool grabBanners,
        std::function<void(int, std::wstring)> progressCb);
};
