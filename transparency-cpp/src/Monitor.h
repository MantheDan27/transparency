#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "Models.h"
#include "Scanner.h"

/**
 * Start monitoring using the provided configuration and register callbacks.
 * @param config Monitoring configuration to apply.
 * @param onScanComplete Callback invoked with each scan's result.
 * @param onInternetChange Optional callback invoked when internet connectivity changes.
 * @param onGatewayMacChange Optional callback invoked with the previous and new gateway MAC values.
 * @param onAlert Optional callback invoked with an alert type and message.
 * @param onDnsChange Optional callback invoked when DNS server information changes.
 */
/**
 * Stop monitoring and terminate the background worker thread.
 */
/**
 * Update the active monitoring configuration; the worker thread will observe the change and adapt.
 * @param config New monitoring configuration to apply.
 */
/**
 * Retrieve the current monitoring configuration.
 * @return The active MonitorConfig.
 */
/**
 * Retrieve the most recently observed internet connectivity status.
 * @return The last recorded InternetStatus.
 */
/**
 * Store a previous scan result for use when comparing against future scans.
 * @param sr ScanResult to record as the previous scan.
 */
/**
 * Main loop executed by the worker thread; runs monitoring cycles until stop is requested.
 */
/**
 * Execute a single cycle of monitoring checks and dispatch callbacks for detected changes.
 */
/**
 * Check current internet connectivity and return the resulting status.
 * @return Updated InternetStatus after performing the check.
 */
/**
 * Determine the current gateway MAC address as observed by the system.
 * @return Gateway MAC address as a wide string.
 */
/**
 * Determine the current DNS server configuration as observed by the system.
 * @return DNS server information as a wide string.
 */
/**
 * Obtain the system's default gateway IP address.
 * @return Default gateway IP as a wide string.
 */
/**
 * Determine whether current time falls within configured quiet hours.
 * @param config Configuration whose quiet-hours rules are evaluated.
 * @return `true` if the current time is within quiet hours defined by `config`, `false` otherwise.
 */
/**
 * Get the current local time formatted as an HHMM wide string (e.g., "0930").
 * @return Current time in HHMM format.
 */
/**
 * Send a payload to the specified webhook URL.
 * @param url Destination webhook URL.
 * @param payload Payload to POST to the webhook.
 */
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
    std::condition_variable _cv;
    bool                _configChanged{ false };
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
