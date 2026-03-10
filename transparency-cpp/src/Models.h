#pragma once
#include <string>
#include <vector>

using std::wstring;
using std::vector;

// Evidence tag — tracks where a piece of device info came from
struct EvidenceTag {
    wstring field;    // "mac", "vendor", "hostname", "ports", "mdns", "ssdp", "netbios", "os"
    wstring source;   // "ARP table", "mDNS", "SSDP/UPnP", "NetBIOS", "Reverse DNS", "OUI lookup", "Port scan", "Banner grab"
    wstring value;    // the actual value discovered
};

// Per-network NIC pinning
struct NicPreference {
    wstring networkId;      // SSID or adapter name for wired
    wstring pinnedNicName;  // adapter FriendlyName user pinned
    wstring pinnedSubnet;   // e.g. "192.168.1.0/24"
};

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
    wstring location;        // e.g. "Home Office", "Living Room"
    bool online = true;
    wstring ipv6Address;

    // Confidence alternatives (top-2 competing types)
    wstring altType1;
    int     altConf1 = 0;
    wstring altType2;
    int     altConf2 = 0;

    // IoT behavioral risk profiling
    bool    iotRisk = false;
    wstring iotRiskDetail;   // plain-language risk summary

    // Latency history (last 7 readings, oldest first, -1 = no data)
    vector<int> latencyHistory;

    // Evidence & transparency
    vector<EvidenceTag> evidence;       // all protocol evidence for this device
    wstring classificationReason;       // human-readable: "vendor: Cisco, ports: 80,443"
    wstring subnet;                     // which subnet (e.g. "192.168.1.0/24")

    // Change tracking
    wstring prevHostname;               // hostname from prior scan
    vector<wstring> ipHistory;          // previously seen IPs
    int sightingCount = 1;             // how many scans this device has been seen in
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
    wstring nicUsed;        // adapter name used for this scan
    wstring nicReason;      // why it was chosen
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

// Plugin/script hook — executed when a matching event fires
struct PluginHook {
    wstring id;
    wstring name;
    wstring execPath;     // path to executable/script
    wstring eventType;    // matches Anomaly::type or "any"
    bool    enabled = true;
};

// Scheduled scan entry
struct ScheduledScan {
    bool    enabled = false;
    wstring mode    = L"quick";   // "quick" | "standard" | "deep"
    int     intervalHours = 24;   // run every N hours
    wstring timeOfDay = L"03:00"; // preferred start time HH:MM
    wstring lastRun;
};
