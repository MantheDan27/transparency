#include "Scanner.h"
#include "Models.h"
#include <iostream>
#include <cassert>
#include <string>

// Helper to check for a specific anomaly type and target IP
bool hasAnomaly(const std::vector<Anomaly>& anomalies, const std::wstring& type, const std::wstring& deviceIp = L"") {
    for (const auto& a : anomalies) {
        if (a.type == type && (deviceIp.empty() || a.deviceIp == deviceIp)) {
            return true;
        }
    }
    return false;
}

void testNewDevice() {
    ScanResult current;
    ScanResult previous;

    Device devNew;
    devNew.ip = L"192.168.1.100";
    devNew.mac = L"AA:BB:CC:DD:EE:FF";
    current.devices.push_back(devNew);

    Device prevDev;
    prevDev.ip = L"192.168.1.1";
    prevDev.mac = L"11:22:33:44:55:66";
    previous.devices.push_back(prevDev); // previous needs to be non-empty for new device

    auto anomalies = ScanEngine::AnalyzeAnomalies(current, previous);

    assert(hasAnomaly(anomalies, L"new_device", L"192.168.1.100"));
    assert(hasAnomaly(anomalies, L"device_offline", L"192.168.1.1"));
    std::cout << "testNewDevice passed" << std::endl;
}

void testRiskyPort() {
    ScanResult current;
    ScanResult previous;

    Device dev;
    dev.ip = L"192.168.1.100";
    dev.mac = L"AA:BB:CC:DD:EE:FF";
    dev.openPorts.push_back(23); // Telnet (critical)
    dev.openPorts.push_back(445); // SMB (high)
    current.devices.push_back(dev);

    auto anomalies = ScanEngine::AnalyzeAnomalies(current, previous);
    assert(hasAnomaly(anomalies, L"risky_port", L"192.168.1.100"));

    // Check specific ports are flagged
    bool found23 = false, found445 = false;
    for (const auto& a : anomalies) {
        if (a.type == L"risky_port" && a.deviceIp == L"192.168.1.100") {
            for (int p : a.affectedPorts) {
                if (p == 23) found23 = true;
                if (p == 445) found445 = true;
            }
        }
    }
    assert(found23 && found445);
    std::cout << "testRiskyPort passed" << std::endl;
}

void testPortChanged() {
    ScanResult current;
    ScanResult previous;

    Device dev;
    dev.ip = L"192.168.1.100";
    dev.mac = L"AA:BB:CC:DD:EE:FF";
    dev.prevPorts.push_back(80);
    dev.openPorts.push_back(80);
    dev.openPorts.push_back(8080); // new port
    current.devices.push_back(dev);

    auto anomalies = ScanEngine::AnalyzeAnomalies(current, previous);
    assert(hasAnomaly(anomalies, L"port_changed", L"192.168.1.100"));

    // Check that 8080 is listed as affected
    bool found8080 = false;
    for (const auto& a : anomalies) {
        if (a.type == L"port_changed" && a.deviceIp == L"192.168.1.100") {
            for (int p : a.affectedPorts) {
                if (p == 8080) found8080 = true;
            }
        }
    }
    assert(found8080);
    std::cout << "testPortChanged passed" << std::endl;
}

void testIpChanged() {
    ScanResult current;
    ScanResult previous;

    Device prevDev;
    prevDev.ip = L"192.168.1.100";
    prevDev.mac = L"AA:BB:CC:DD:EE:FF";
    previous.devices.push_back(prevDev);

    Device curDev;
    curDev.ip = L"192.168.1.101"; // IP changed
    curDev.mac = L"AA:BB:CC:DD:EE:FF"; // MAC same
    current.devices.push_back(curDev);

    auto anomalies = ScanEngine::AnalyzeAnomalies(current, previous);
    assert(hasAnomaly(anomalies, L"ip_changed", L"192.168.1.101"));
    std::cout << "testIpChanged passed" << std::endl;
}

void testDeviceOffline() {
    ScanResult current;
    ScanResult previous;

    Device prevDev;
    prevDev.ip = L"192.168.1.100";
    prevDev.mac = L"AA:BB:CC:DD:EE:FF";
    previous.devices.push_back(prevDev);

    // No devices in current

    auto anomalies = ScanEngine::AnalyzeAnomalies(current, previous);
    assert(hasAnomaly(anomalies, L"device_offline", L"192.168.1.100"));
    std::cout << "testDeviceOffline passed" << std::endl;
}

int main() {
    testNewDevice();
    testRiskyPort();
    testPortChanged();
    testIpChanged();
    testDeviceOffline();
    std::cout << "All ScanEngine anomalies tests passed!" << std::endl;
    return 0;
}
