#pragma once
#include "models.h"
#include <functional>
#include <future>
#include <atomic>

using ProgressCallback = std::function<void(int pct, const std::string& msg)>;

class ScanEngine {
public:
    std::future<ScanResult> QuickScan(ProgressCallback cb = nullptr);
    std::future<ScanResult> StandardScan(ProgressCallback cb = nullptr);
    std::future<ScanResult> DeepScan(ProgressCallback cb = nullptr);
    void Cancel();

    static const std::map<int, std::string> PORT_NAMES;

private:
    std::atomic<bool> _cancelled{false};

    struct NetworkInfo {
        std::string ip;
        std::string netmask;
        std::string gateway;
        std::string iface;
        int prefix = 24;
    };

    std::vector<NetworkInfo> getLocalNetworks();
    std::string getDefaultGateway();
    std::map<std::string, std::string> readArpTable();
    std::vector<std::string> generateSubnetIPs(const std::string& ip, int prefix);
    bool pingHost(const std::string& ip, int timeoutMs = 1000);
    int measureLatency(const std::string& ip);
    bool probePort(const std::string& ip, int port, int timeoutMs = 1000);
    std::vector<int> scanPorts(const std::string& ip, const std::vector<int>& ports, int timeoutMs = 1000);
    std::string resolveHostname(const std::string& ip);
    std::string grabBanner(const std::string& ip, int port);
    std::string lookupVendor(const std::string& mac);
    void fingerprintDevice(Device& dev);
    void detectVM(Device& dev);
    void assessIoTRisk(Device& dev);
    void generateAnomalies(ScanResult& result, const std::vector<Device>& prev);
    ScanResult doScan(const std::string& mode, const std::vector<int>& ports, ProgressCallback cb);
};
