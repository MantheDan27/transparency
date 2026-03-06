#include <iostream>
#include <cassert>
#include "../src/Scanner.h"
#include "../src/Models.h"

void testCalcConfidence_EmptyDevice() {
    Device d;
    int score = ScanEngine::CalcConfidence(d);
    assert(score == 0);
    std::cout << "testCalcConfidence_EmptyDevice passed" << std::endl;
}

void testCalcConfidence_AllFieldsPopulated() {
    Device d;
    d.vendor = L"Apple";
    d.hostname = L"Jules-MacBook-Pro.local";
    d.openPorts = {80, 443};
    d.mdnsServices = {L"_http._tcp", L"_https._tcp"};
    d.ssdpInfo = L"Apple TV";
    d.netbiosName = L"JULES-MBP";
    d.latencyMs = 10;

    int score = ScanEngine::CalcConfidence(d);
    assert(score == 100);
    std::cout << "testCalcConfidence_AllFieldsPopulated passed" << std::endl;
}

void testCalcConfidence_PartialFields() {
    Device d;
    d.vendor = L"Apple"; // +20
    d.openPorts = {80, 443}; // +20
    d.latencyMs = 10; // +5

    int score = ScanEngine::CalcConfidence(d);
    assert(score == 45);
    std::cout << "testCalcConfidence_PartialFields passed" << std::endl;
}

int main() {
    testCalcConfidence_EmptyDevice();
    testCalcConfidence_AllFieldsPopulated();
    testCalcConfidence_PartialFields();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
