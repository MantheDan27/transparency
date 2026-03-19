#include "Scanner.h"
#include <iostream>
#include <cassert>
#include <string>

using std::wstring;

void test_NoRiskPorts() {
    Device d;
    d.openPorts = { 443, 8080 }; // No specific risk ports
    d.deviceType = L"Computer";
    wstring risk = ScanEngine::ProfileIoTRisk(d);
    assert(risk.empty());
}

void test_TelnetRisk() {
    Device d;
    d.openPorts = { 23 };
    wstring risk = ScanEngine::ProfileIoTRisk(d);
    assert(risk.find(L"CRITICAL: Telnet") != wstring::npos);
}

void test_FtpRisk() {
    Device d;
    d.openPorts = { 21 };
    wstring risk = ScanEngine::ProfileIoTRisk(d);
    assert(risk.find(L"HIGH: FTP") != wstring::npos);
}

void test_HttpWithoutHttpsRisk() {
    Device d;
    d.openPorts = { 80 }; // 80 but not 443
    wstring risk = ScanEngine::ProfileIoTRisk(d);
    assert(risk.find(L"MEDIUM: HTTP admin panel") != wstring::npos);
}

void test_HttpWithHttpsNoRisk() {
    Device d;
    d.openPorts = { 80, 443 };
    wstring risk = ScanEngine::ProfileIoTRisk(d);
    assert(risk.find(L"MEDIUM: HTTP admin panel") == wstring::npos);
}

void test_UpnpRisk() {
    Device d;
    d.openPorts = { 1900 };
    wstring risk = ScanEngine::ProfileIoTRisk(d);
    assert(risk.find(L"MEDIUM: UPnP") != wstring::npos);
}

void test_CameraStreamRisk_Rtsp() {
    Device d;
    d.openPorts = { 554 };
    wstring risk = ScanEngine::ProfileIoTRisk(d);
    assert(risk.find(L"HIGH: Camera stream") != wstring::npos);
}

void test_CameraStreamRisk_Onvif() {
    Device d;
    d.openPorts = { 2020 };
    wstring risk = ScanEngine::ProfileIoTRisk(d);
    assert(risk.find(L"HIGH: Camera stream") != wstring::npos);
}

void test_SshOnIoT() {
    Device d;
    d.openPorts = { 22 };
    d.deviceType = L"IoT Device";
    wstring risk = ScanEngine::ProfileIoTRisk(d);
    assert(risk.find(L"INFO: SSH") != wstring::npos);
}

void test_SshNotOnIoT() {
    Device d;
    d.openPorts = { 22 };
    d.deviceType = L"Linux Server";
    wstring risk = ScanEngine::ProfileIoTRisk(d);
    assert(risk.find(L"INFO: SSH") == wstring::npos);
}

void test_SshOnIoTWithHttp() {
    Device d;
    d.openPorts = { 22, 80 }; // hasHttp is true, so no SSH warning
    d.deviceType = L"IoT Device";
    wstring risk = ScanEngine::ProfileIoTRisk(d);
    assert(risk.find(L"INFO: SSH") == wstring::npos);
    // But it will have HTTP without HTTPS warning
    assert(risk.find(L"MEDIUM: HTTP admin panel") != wstring::npos);
}

void test_MultipleRisks() {
    Device d;
    d.openPorts = { 21, 23 };
    wstring risk = ScanEngine::ProfileIoTRisk(d);
    assert(risk.find(L"CRITICAL: Telnet") != wstring::npos);
    assert(risk.find(L"HIGH: FTP") != wstring::npos);
}

#if defined(_MSC_VER) || defined(UNICODE)
int wmain(int argc, wchar_t** argv) {
#else
int main() {
#endif
    std::cout << "Running Scanner tests...\n";

    test_NoRiskPorts();
    test_TelnetRisk();
    test_FtpRisk();
    test_HttpWithoutHttpsRisk();
    test_HttpWithHttpsNoRisk();
    test_UpnpRisk();
    test_CameraStreamRisk_Rtsp();
    test_CameraStreamRisk_Onvif();
    test_SshOnIoT();
    test_SshNotOnIoT();
    test_SshOnIoTWithHttp();
    test_MultipleRisks();

    std::cout << "All Scanner tests passed successfully!\n";
    return 0;
}
