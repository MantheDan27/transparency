#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <functional>
#include "Models.h"

#pragma comment(lib, "winhttp.lib")

// Firebase REST API client for Firestore
class FirebaseClient {
public:
    FirebaseClient();
    ~FirebaseClient();

    // Configuration
    void SetProjectId(const std::wstring& projectId);
    void SetApiKey(const std::wstring& apiKey);
    void SetIdToken(const std::wstring& idToken);
    void SetUserId(const std::wstring& userId);

    // Authentication
    bool SignInWithEmail(const std::wstring& email, const std::wstring& password);
    bool SignUp(const std::wstring& email, const std::wstring& password);
    bool IsAuthenticated() const { return !_idToken.empty(); }
    std::wstring GetUserId() const { return _userId; }
    std::wstring GetLastError() const { return _lastError; }

    // Device operations
    bool SaveDevice(const Device& device);
    bool SaveAllDevices(const std::vector<Device>& devices);
    bool LoadDevices(std::vector<Device>& devices);
    bool DeleteDevice(const std::wstring& deviceId);

    // Scan results
    bool SaveScanResult(const ScanResult& result);
    bool LoadLatestScan(ScanResult& result);

    // Alerts
    bool SaveAlert(const Anomaly& alert);
    bool LoadAlerts(std::vector<Anomaly>& alerts);

private:
    std::wstring _projectId;
    std::wstring _apiKey;
    std::wstring _idToken;
    std::wstring _userId;
    std::wstring _lastError;
    HINTERNET _hSession = nullptr;

    // HTTP helpers
    std::string HttpRequest(const std::wstring& method, const std::wstring& url,
                            const std::string& body = "");
    std::string BuildFirestoreUrl(const std::wstring& collection, const std::wstring& docId = L"");

    // JSON helpers
    std::string DeviceToJson(const Device& d);
    Device JsonToDevice(const std::string& json);
    std::string EscapeJson(const std::wstring& str);
    std::wstring GetJsonValue(const std::string& json, const std::string& key);
};

// Global instance
FirebaseClient& GetFirebase();
