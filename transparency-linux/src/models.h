#pragma once
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <ctime>

struct Device {
    std::string ip;
    std::string mac;
    std::string hostname;
    std::string vendor;
    std::string deviceType;
    std::string osGuess;
    std::vector<int> openPorts;
    std::vector<int> prevPorts;
    std::vector<std::string> mdnsServices;
    std::string ssdpInfo;
    std::string netbiosName;
    int confidence = 0;
    int latencyMs = -1;
    bool isIPv6 = false;
    std::string firstSeen;
    std::string lastSeen;
    std::string trustState = "unknown";
    std::vector<std::string> tags;
    std::string notes;
    std::string customName;
    std::string location;
    bool online = true;
    std::string ipv6Address;
    std::string altType1;
    int altConf1 = 0;
    std::string altType2;
    int altConf2 = 0;
    bool iotRisk = false;
    std::string iotRiskDetail;
    std::vector<int> latencyHistory;
    bool isVM = false;
    bool isHypervisor = false;
};

struct Anomaly {
    std::string type;
    std::string severity;
    std::string deviceIp;
    std::string description;
    std::string explanation;
    std::string remediation;
    std::string category;
    std::vector<int> affectedPorts;
};

struct AlertRule {
    std::string id;
    std::string name;
    std::string eventType;
    std::string deviceFilter;
    std::string webhookUrl;
    std::string severity;
    int debounceMinutes = 5;
    bool enabled = true;
};

struct LedgerEntry {
    std::string timestamp;
    std::string action;
    std::string details;
};

struct ScanResult {
    std::vector<Device> devices;
    std::vector<Anomaly> anomalies;
    std::string scannedAt;
    std::string mode;
};

struct MonitorConfig {
    bool enabled = false;
    int intervalMinutes = 5;
    std::string quietHoursStart;
    std::string quietHoursEnd;
    bool alertOnOutage = true;
    bool alertOnGatewayMac = true;
    bool alertOnDnsChange = true;
    bool alertOnHighLatency = false;
    int highLatencyThresholdMs = 200;
};

struct PluginHook {
    std::string id;
    std::string name;
    std::string execPath;
    std::string eventType;
    bool enabled = true;
};

struct ScheduledScan {
    bool enabled = false;
    std::string mode = "quick";
    int intervalHours = 24;
    std::string timeOfDay = "03:00";
    std::string lastRun;
};

inline std::string nowTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    struct tm tm_buf;
    localtime_r(&t, &tm_buf);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
    return buf;
}
