#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include "transparency/models.h"
#include "net/scan_engine.h"

class Monitor {
public:
    Monitor();
    ~Monitor();

    using ScanCallback      = std::function<void(ScanResult)>;
    using InternetCallback  = std::function<void(InternetStatus)>;
    using GatewayMacCallback= std::function<void(std::wstring, std::wstring)>; // old, new
    using AlertCallback     = std::function<void(std::wstring, std::wstring)>; // type, message
    using DnsCallback       = std::function<void(std::wstring)>;                // new servers

    void Start(
        const MonitorConfig& config,
        ScanCallback       onScanComplete,
        InternetCallback   onInternetChange   = nullptr,
        GatewayMacCallback onGatewayMacChange = nullptr,
        AlertCallback      onAlert            = nullptr,
        DnsCallback        onDnsChange        = nullptr);

    void Stop();
    void UpdateConfig(const MonitorConfig& config);

    MonitorConfig   GetConfig() const;
    InternetStatus  GetInternetStatus() const;
    bool            IsRunning() const { return _running; }

    // Set the previous scan result to compare against
    void SetPreviousScan(const ScanResult& sr);

private:
    mutable std::mutex  _mutex;
    std::atomic<bool>   _running{ false };
    std::atomic<bool>   _stopRequested{ false };
    std::thread         _workerThread;

    MonitorConfig   _config;
    InternetStatus  _internetStatus;
    ScanResult      _previousScan;

    std::wstring _lastGatewayMac;
    std::wstring _lastDnsServers;
    std::wstring _lastGatewayIp;

    ScanCallback        _onScanComplete;
    InternetCallback    _onInternetChange;
    GatewayMacCallback  _onGatewayMacChange;
    AlertCallback       _onAlert;
    DnsCallback         _onDnsChange;

    ScanEngine          _scanner;

    void WorkerLoop();
    void PerformChecks();

    InternetStatus  CheckInternetConnectivity();
    std::wstring    CheckGatewayMac();
    std::wstring    CheckDnsServers();
    std::wstring    GetDefaultGatewayIp();

    static bool IsInQuietHours(const MonitorConfig& config);
    static std::wstring GetCurrentTimeHHMM();

    static void FireWebhook(const std::wstring& url,
                            const std::wstring& payload);
};
