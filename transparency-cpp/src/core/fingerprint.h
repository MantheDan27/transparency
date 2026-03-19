#pragma once
#include "transparency/models.h"
#include <string>
#include <vector>
#include <map>
#include <set>

namespace core {

// Device type fingerprinting from observed evidence
std::wstring fingerprint_device_type(const Device& d);

// Build human-readable classification reason string
void build_classification_reason(Device& d);

// Calculate confidence score (0-100) based on available evidence
int calc_confidence(const Device& d);

// IoT behavioral risk profiling — returns plain-language risk summary or empty
std::wstring profile_iot_risk(const Device& d);

// Fill top-2 confidence alternative device types on a device
void fill_confidence_alternatives(Device& d);

// Compare current vs previous scan and generate anomalies
std::vector<Anomaly> analyze_anomalies(
    const ScanResult& current,
    const ScanResult& previous);

// OUI vendor lookup by MAC address prefix
std::wstring lookup_vendor(const std::wstring& mac);

// Port/service data tables
extern const std::map<int, std::wstring> PORT_NAMES;
extern const std::set<int> RISKY_PORTS;
extern const std::map<std::wstring, std::vector<int>> PORT_PROFILES;

} // namespace core
