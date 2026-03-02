#pragma once
#include <string>
#include <vector>

using std::wstring;
using std::vector;

struct Device {
    wstring ip;
    wstring mac;
    wstring hostname;
    wstring vendor;
    wstring deviceType;
    wstring osGuess;
    vector<int> openPorts;
    vector<int> prevPorts;
    vector<wstring> mdnsServices;
    wstring ssdpInfo;
    wstring netbiosName;
    int confidence = 0;
    int latencyMs = -1;
    bool isIPv6 = false;
    wstring firstSeen;
    wstring lastSeen;
    wstring trustState = L"unknown";
    vector<wstring> tags;
    wstring notes;
    wstring customName;
    bool online = true;
    wstring ipv6Address;
};

struct Anomaly {
    wstring type;
    wstring severity;
    wstring deviceIp;
    wstring description;
    wstring explanation;
    wstring remediation;
    wstring traceSource;
    vector<int> affectedPorts;
};

struct AlertRule {
    wstring id;
    wstring name;
    wstring eventType;
    wstring deviceFilter;
    wstring webhookUrl;
    wstring severity;
    int debounceMinutes = 5;
    bool enabled = true;
    wstring quietHoursStart;
    wstring quietHoursEnd;
};

struct LedgerEntry {
    wstring timestamp;
    wstring action;
    wstring details;
};

struct ScanResult {
    vector<Device> devices;
    vector<Anomaly> anomalies;
    wstring scannedAt;
    wstring mode;
};

struct MonitorConfig {
    bool enabled = false;
    int intervalMinutes = 5;
    wstring quietHoursStart;
    wstring quietHoursEnd;
    bool alertOnOutage = true;
    bool alertOnGatewayMac = true;
    bool alertOnDnsChange = true;
    bool alertOnHighLatency = false;
    int highLatencyThresholdMs = 200;
};

struct InternetStatus {
    bool online = false;
    int latencyMs = -1;
    wstring lastCheck;
};
