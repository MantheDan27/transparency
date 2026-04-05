#include "Scanner.h"
#include "Models.h"
#include <iostream>
#include <cassert>

void test_fingerprint() {
    // 1. Router/Switch
    {
        Device d;
        d.vendor = L"Cisco Systems";
        assert(ScanEngine::FingerprintDeviceType(d) == L"Router/Switch");
    }
    {
        Device d;
        d.ssdpInfo = L"Home Gateway";
        assert(ScanEngine::FingerprintDeviceType(d) == L"Router/Switch");
    }

    // 2. NAS / Storage
    {
        Device d;
        d.vendor = L"Synology Inc.";
        assert(ScanEngine::FingerprintDeviceType(d) == L"NAS / Storage");
    }
    {
        Device d;
        d.openPorts = { 5000, 5001 };
        assert(ScanEngine::FingerprintDeviceType(d) == L"NAS / Storage");
    }

    // 3. Printer
    {
        Device d;
        d.vendor = L"HP Inc.";
        assert(ScanEngine::FingerprintDeviceType(d) == L"Printer");
    }
    {
        Device d;
        d.mdnsServices = { L"_ipp._tcp" };
        assert(ScanEngine::FingerprintDeviceType(d) == L"Printer");
    }

    // 4. Smart TV / Streaming
    {
        Device d;
        d.vendor = L"Roku, Inc.";
        assert(ScanEngine::FingerprintDeviceType(d) == L"Smart TV / Streaming");
    }
    {
        Device d;
        d.mdnsServices = { L"_googlecast._tcp" };
        assert(ScanEngine::FingerprintDeviceType(d) == L"Smart TV / Streaming");
    }

    // 5. Smart Speaker
    {
        Device d;
        d.vendor = L"Sonos, Inc.";
        assert(ScanEngine::FingerprintDeviceType(d) == L"Smart Speaker");
    }
    {
        Device d;
        d.ssdpInfo = L"Echo Dot";
        assert(ScanEngine::FingerprintDeviceType(d) == L"Smart Speaker");
    }

    // 6. Smart Home Hub
    {
        Device d;
        d.vendor = L"Philips Hue";
        assert(ScanEngine::FingerprintDeviceType(d) == L"Smart Home Hub");
    }
    {
        Device d;
        d.hostname = L"home assistant";
        assert(ScanEngine::FingerprintDeviceType(d) == L"Smart Home Hub");
    }

    // 7. IoT Device
    {
        Device d;
        d.openPorts = { 1883 }; // MQTT
        assert(ScanEngine::FingerprintDeviceType(d) == L"IoT Device");
    }
    {
        Device d;
        d.hostname = L"esp8266-sensor";
        assert(ScanEngine::FingerprintDeviceType(d) == L"IoT Device");
    }

    // 8. Single-Board Computer
    {
        Device d;
        d.vendor = L"Raspberry Pi Trading Ltd";
        assert(ScanEngine::FingerprintDeviceType(d) == L"Single-Board Computer");
    }

    // 9. Windows PC
    {
        Device d;
        d.openPorts = { 3389 }; // RDP
        assert(ScanEngine::FingerprintDeviceType(d) == L"Windows PC");
    }
    {
        Device d;
        d.openPorts = { 445 }; // SMB
        assert(ScanEngine::FingerprintDeviceType(d) == L"Windows PC");
    }

    // 10. Linux Server
    {
        Device d;
        d.openPorts = { 22, 80 }; // SSH + HTTP
        assert(ScanEngine::FingerprintDeviceType(d) == L"Linux Server");
    }

    // 11. Mac
    {
        Device d;
        d.vendor = L"Apple, Inc.";
        d.openPorts = { 548 }; // AFP
        assert(ScanEngine::FingerprintDeviceType(d) == L"Mac");
    }
    {
        Device d;
        d.vendor = L"Apple, Inc.";
        d.mdnsServices = { L"_workstation._tcp" };
        assert(ScanEngine::FingerprintDeviceType(d) == L"Mac");
    }

    // 12. Mobile Device
    {
        Device d;
        d.vendor = L"Apple, Inc.";
        // Without workstation mdns or AFP port, Apple falls through to Mobile Device
        assert(ScanEngine::FingerprintDeviceType(d) == L"Mobile Device");
    }
    {
        Device d;
        d.vendor = L"Samsung Electronics Co.,Ltd";
        assert(ScanEngine::FingerprintDeviceType(d) == L"Mobile Device");
    }

    // 13. Computer
    {
        Device d;
        d.vendor = L"Dell Inc.";
        assert(ScanEngine::FingerprintDeviceType(d) == L"Computer");
    }

    // 14. Unknown Device
    {
        Device d;
        d.vendor = L"Some Unknown Vendor";
        d.openPorts = { 12345 };
        assert(ScanEngine::FingerprintDeviceType(d) == L"Unknown Device");
    }

    std::cout << "All tests passed successfully!" << std::endl;
}

int main() {
    test_fingerprint();
    return 0;
}
