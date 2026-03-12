#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "FirebaseClient.h"
#include <sstream>
#include <locale>
#include <codecvt>
#include <map>

// Global singleton instance
static FirebaseClient s_firebase;
FirebaseClient& GetFirebase() { return s_firebase; }

// Helper: wide string to UTF-8
static std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &result[0], size, nullptr, nullptr);
    return result;
}

// Helper: UTF-8 to wide string
static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &result[0], size);
    return result;
}

FirebaseClient::FirebaseClient() {
    _hSession = WinHttpOpen(L"Transparency/3.7",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
}

FirebaseClient::~FirebaseClient() {
    if (_hSession) WinHttpCloseHandle(_hSession);
}

void FirebaseClient::SetProjectId(const std::wstring& projectId) { _projectId = projectId; }
void FirebaseClient::SetApiKey(const std::wstring& apiKey) { _apiKey = apiKey; }
void FirebaseClient::SetIdToken(const std::wstring& idToken) { _idToken = idToken; }
void FirebaseClient::SetUserId(const std::wstring& userId) { _userId = userId; }

std::string FirebaseClient::EscapeJson(const std::wstring& str) {
    std::string utf8 = WideToUtf8(str);
    std::string out;
    out.reserve(utf8.size() + 16);
    for (char ch : utf8) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"':  out += "\\\""; break;
        case '\r': out += "\\r"; break;
        case '\n': out += "\\n"; break;
        case '\t': out += "\\t"; break;
        default:
            if ((unsigned char)ch < 0x20) out += ' ';
            else out += ch;
            break;
        }
    }
    return out;
}

std::wstring FirebaseClient::GetJsonValue(const std::string& json, const std::string& key) {
    // Simple JSON value extraction (no nested objects)
    std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return L"";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return L"";

    // Skip whitespace
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r'))
        pos++;

    if (pos >= json.size()) return L"";

    // Check for string value
    if (json[pos] == '"') {
        pos++;
        size_t end = pos;
        while (end < json.size() && json[end] != '"') {
            if (json[end] == '\\' && end + 1 < json.size()) end += 2;
            else end++;
        }
        return Utf8ToWide(json.substr(pos, end - pos));
    }

    // Non-string value (number, bool, null)
    size_t end = pos;
    while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ']')
        end++;
    return Utf8ToWide(json.substr(pos, end - pos));
}

std::string FirebaseClient::HttpRequest(const std::wstring& method, const std::wstring& url,
                                          const std::string& body) {
    if (!_hSession) {
        _lastError = L"HTTP session not initialized";
        return "";
    }

    // Parse URL
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp)) {
        _lastError = L"Failed to parse URL";
        return "";
    }

    HINTERNET hConnect = WinHttpConnect(_hSession, hostName, urlComp.nPort, 0);
    if (!hConnect) {
        _lastError = L"Failed to connect";
        return "";
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, method.c_str(), urlPath,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        _lastError = L"Failed to open request";
        return "";
    }

    // Add headers
    std::wstring headers = L"Content-Type: application/json\r\n";
    if (!_idToken.empty()) {
        headers += L"Authorization: Bearer " + _idToken + L"\r\n";
    }
    WinHttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

    // Send request
    BOOL bSent = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.c_str(),
        (DWORD)body.size(), (DWORD)body.size(), 0);

    if (!bSent) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        _lastError = L"Failed to send request";
        return "";
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        _lastError = L"Failed to receive response";
        return "";
    }

    // Read response body
    std::string response;
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
            response.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return response;
}

std::string FirebaseClient::BuildFirestoreUrl(const std::wstring& collection, const std::wstring& docId) {
    std::string url = "https://firestore.googleapis.com/v1/projects/";
    url += WideToUtf8(_projectId);
    url += "/databases/(default)/documents/users/";
    url += WideToUtf8(_userId);
    url += "/";
    url += WideToUtf8(collection);
    if (!docId.empty()) {
        url += "/";
        url += WideToUtf8(docId);
    }
    return url;
}

bool FirebaseClient::SignInWithEmail(const std::wstring& email, const std::wstring& password) {
    std::string url = "https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=";
    url += WideToUtf8(_apiKey);

    std::string body = "{";
    body += "\"email\":\"" + EscapeJson(email) + "\",";
    body += "\"password\":\"" + EscapeJson(password) + "\",";
    body += "\"returnSecureToken\":true}";

    std::string response = HttpRequest(L"POST", Utf8ToWide(url), body);
    if (response.empty()) return false;

    // Check for error
    if (response.find("\"error\"") != std::string::npos) {
        _lastError = GetJsonValue(response, "message");
        return false;
    }

    _idToken = GetJsonValue(response, "idToken");
    _userId = GetJsonValue(response, "localId");
    return !_idToken.empty() && !_userId.empty();
}

bool FirebaseClient::SignUp(const std::wstring& email, const std::wstring& password) {
    std::string url = "https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=";
    url += WideToUtf8(_apiKey);

    std::string body = "{";
    body += "\"email\":\"" + EscapeJson(email) + "\",";
    body += "\"password\":\"" + EscapeJson(password) + "\",";
    body += "\"returnSecureToken\":true}";

    std::string response = HttpRequest(L"POST", Utf8ToWide(url), body);
    if (response.empty()) return false;

    if (response.find("\"error\"") != std::string::npos) {
        _lastError = GetJsonValue(response, "message");
        return false;
    }

    _idToken = GetJsonValue(response, "idToken");
    _userId = GetJsonValue(response, "localId");
    return !_idToken.empty() && !_userId.empty();
}

std::string FirebaseClient::DeviceToJson(const Device& d) {
    std::string json = "{\"fields\":{";

    json += "\"ip\":{\"stringValue\":\"" + EscapeJson(d.ip) + "\"},";
    json += "\"mac\":{\"stringValue\":\"" + EscapeJson(d.mac) + "\"},";
    json += "\"hostname\":{\"stringValue\":\"" + EscapeJson(d.hostname) + "\"},";
    json += "\"vendor\":{\"stringValue\":\"" + EscapeJson(d.vendor) + "\"},";
    json += "\"deviceType\":{\"stringValue\":\"" + EscapeJson(d.deviceType) + "\"},";
    json += "\"customName\":{\"stringValue\":\"" + EscapeJson(d.customName) + "\"},";
    json += "\"notes\":{\"stringValue\":\"" + EscapeJson(d.notes) + "\"},";
    json += "\"trustState\":{\"stringValue\":\"" + EscapeJson(d.trustState) + "\"},";
    json += "\"location\":{\"stringValue\":\"" + EscapeJson(d.location) + "\"},";
    json += "\"firstSeen\":{\"stringValue\":\"" + EscapeJson(d.firstSeen) + "\"},";
    json += "\"lastSeen\":{\"stringValue\":\"" + EscapeJson(d.lastSeen) + "\"},";
    json += "\"confidence\":{\"integerValue\":\"" + std::to_string(d.confidence) + "\"},";
    json += "\"latencyMs\":{\"integerValue\":\"" + std::to_string(d.latencyMs) + "\"},";
    json += "\"sightingCount\":{\"integerValue\":\"" + std::to_string(d.sightingCount) + "\"},";
    json += "\"online\":{\"booleanValue\":" + std::string(d.online ? "true" : "false") + "},";

    // Open ports as array
    json += "\"openPorts\":{\"arrayValue\":{\"values\":[";
    for (size_t i = 0; i < d.openPorts.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"integerValue\":\"" + std::to_string(d.openPorts[i]) + "\"}";
    }
    json += "]}}";

    json += "}}";
    return json;
}

Device FirebaseClient::JsonToDevice(const std::string& json) {
    Device d;

    // Parse Firestore document format
    auto getField = [&](const std::string& fieldName) -> std::string {
        std::string search = "\"" + fieldName + "\":{";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";

        pos = json.find("Value\":", pos);
        if (pos == std::string::npos) return "";
        pos += 7; // Skip "Value":

        // Skip whitespace
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

        if (json[pos] == '"') {
            pos++;
            size_t end = json.find('"', pos);
            if (end != std::string::npos) return json.substr(pos, end - pos);
        } else {
            size_t end = pos;
            while (end < json.size() && json[end] != ',' && json[end] != '}') end++;
            return json.substr(pos, end - pos);
        }
        return "";
    };

    d.ip = Utf8ToWide(getField("ip"));
    d.mac = Utf8ToWide(getField("mac"));
    d.hostname = Utf8ToWide(getField("hostname"));
    d.vendor = Utf8ToWide(getField("vendor"));
    d.deviceType = Utf8ToWide(getField("deviceType"));
    d.customName = Utf8ToWide(getField("customName"));
    d.notes = Utf8ToWide(getField("notes"));
    d.trustState = Utf8ToWide(getField("trustState"));
    d.location = Utf8ToWide(getField("location"));
    d.firstSeen = Utf8ToWide(getField("firstSeen"));
    d.lastSeen = Utf8ToWide(getField("lastSeen"));

    std::string confStr = getField("confidence");
    if (!confStr.empty()) d.confidence = std::stoi(confStr);

    std::string latStr = getField("latencyMs");
    if (!latStr.empty()) d.latencyMs = std::stoi(latStr);

    std::string sightStr = getField("sightingCount");
    if (!sightStr.empty()) d.sightingCount = std::stoi(sightStr);

    std::string onlineStr = getField("online");
    d.online = (onlineStr == "true");

    return d;
}

bool FirebaseClient::SaveDevice(const Device& device) {
    if (_userId.empty() || _idToken.empty()) {
        _lastError = L"Not authenticated";
        return false;
    }

    // Use MAC as document ID (replace colons with underscores)
    std::wstring docId = device.mac;
    for (auto& c : docId) if (c == L':') c = L'_';
    if (docId.empty()) docId = device.ip;
    for (auto& c : docId) if (c == L'.') c = L'_';

    std::string url = BuildFirestoreUrl(L"devices", docId);
    std::string body = DeviceToJson(device);

    std::string response = HttpRequest(L"PATCH", Utf8ToWide(url), body);
    if (response.find("\"error\"") != std::string::npos) {
        _lastError = GetJsonValue(response, "message");
        return false;
    }
    return !response.empty();
}

bool FirebaseClient::SaveAllDevices(const std::vector<Device>& devices) {
    bool allOk = true;
    for (const auto& d : devices) {
        if (!SaveDevice(d)) allOk = false;
    }
    return allOk;
}

bool FirebaseClient::LoadDevices(std::vector<Device>& devices) {
    if (_userId.empty() || _idToken.empty()) {
        _lastError = L"Not authenticated";
        return false;
    }

    std::string url = BuildFirestoreUrl(L"devices");
    std::string response = HttpRequest(L"GET", Utf8ToWide(url), "");

    if (response.find("\"error\"") != std::string::npos) {
        _lastError = GetJsonValue(response, "message");
        return false;
    }

    devices.clear();

    // Parse documents array
    size_t pos = 0;
    while ((pos = response.find("\"name\":", pos)) != std::string::npos) {
        size_t docStart = response.rfind('{', pos);
        size_t docEnd = pos;
        int braceCount = 0;
        bool inDoc = false;
        for (size_t i = docStart; i < response.size(); i++) {
            if (response[i] == '{') { braceCount++; inDoc = true; }
            if (response[i] == '}') braceCount--;
            if (inDoc && braceCount == 0) { docEnd = i + 1; break; }
        }

        if (docEnd > docStart) {
            std::string docJson = response.substr(docStart, docEnd - docStart);
            Device d = JsonToDevice(docJson);
            if (!d.mac.empty() || !d.ip.empty()) {
                devices.push_back(d);
            }
        }
        pos = docEnd;
    }

    return true;
}

bool FirebaseClient::DeleteDevice(const std::wstring& deviceId) {
    if (_userId.empty() || _idToken.empty()) {
        _lastError = L"Not authenticated";
        return false;
    }

    std::string url = BuildFirestoreUrl(L"devices", deviceId);
    std::string response = HttpRequest(L"DELETE", Utf8ToWide(url), "");
    return response.find("\"error\"") == std::string::npos;
}

bool FirebaseClient::SaveScanResult(const ScanResult& result) {
    if (_userId.empty() || _idToken.empty()) {
        _lastError = L"Not authenticated";
        return false;
    }

    std::string json = "{\"fields\":{";
    json += "\"scannedAt\":{\"stringValue\":\"" + EscapeJson(result.scannedAt) + "\"},";
    json += "\"mode\":{\"stringValue\":\"" + EscapeJson(result.mode) + "\"},";
    json += "\"nicUsed\":{\"stringValue\":\"" + EscapeJson(result.nicUsed) + "\"},";
    json += "\"deviceCount\":{\"integerValue\":\"" + std::to_string(result.devices.size()) + "\"},";
    json += "\"anomalyCount\":{\"integerValue\":\"" + std::to_string(result.anomalies.size()) + "\"}";
    json += "}}";

    // Use timestamp as document ID
    std::wstring docId = result.scannedAt;
    for (auto& c : docId) {
        if (c == L':' || c == L' ' || c == L'/') c = L'_';
    }

    std::string url = BuildFirestoreUrl(L"scans", docId);
    std::string response = HttpRequest(L"PATCH", Utf8ToWide(url), json);
    return response.find("\"error\"") == std::string::npos;
}

bool FirebaseClient::LoadLatestScan(ScanResult& result) {
    // This would need a Firestore query - simplified for now
    return false;
}

bool FirebaseClient::SaveAlert(const Anomaly& alert) {
    if (_userId.empty() || _idToken.empty()) {
        _lastError = L"Not authenticated";
        return false;
    }

    std::string json = "{\"fields\":{";
    json += "\"type\":{\"stringValue\":\"" + EscapeJson(alert.type) + "\"},";
    json += "\"severity\":{\"stringValue\":\"" + EscapeJson(alert.severity) + "\"},";
    json += "\"deviceIp\":{\"stringValue\":\"" + EscapeJson(alert.deviceIp) + "\"},";
    json += "\"description\":{\"stringValue\":\"" + EscapeJson(alert.description) + "\"},";
    json += "\"explanation\":{\"stringValue\":\"" + EscapeJson(alert.explanation) + "\"},";
    json += "\"remediation\":{\"stringValue\":\"" + EscapeJson(alert.remediation) + "\"}";
    json += "}}";

    std::string url = BuildFirestoreUrl(L"alerts", L"");
    std::string response = HttpRequest(L"POST", Utf8ToWide(url), json);
    return response.find("\"error\"") == std::string::npos;
}

bool FirebaseClient::LoadAlerts(std::vector<Anomaly>& alerts) {
    if (_userId.empty() || _idToken.empty()) {
        _lastError = L"Not authenticated";
        return false;
    }

    std::string url = BuildFirestoreUrl(L"alerts");
    std::string response = HttpRequest(L"GET", Utf8ToWide(url), "");

    if (response.find("\"error\"") != std::string::npos) {
        _lastError = GetJsonValue(response, "message");
        return false;
    }

    alerts.clear();
    // Parse response - similar to LoadDevices
    // Simplified implementation
    return true;
}

bool FirebaseClient::SaveAlertRules(const std::vector<AlertRule>& rules) {
    if (_userId.empty() || _idToken.empty()) {
        _lastError = L"Not authenticated";
        return false;
    }

    // Save all rules as a single document
    std::string json = "{\"fields\":{\"rules\":{\"arrayValue\":{\"values\":[";
    for (size_t i = 0; i < rules.size(); i++) {
        if (i > 0) json += ",";
        json += "{\"mapValue\":{\"fields\":{";
        json += "\"id\":{\"stringValue\":\"" + EscapeJson(rules[i].id) + "\"},";
        json += "\"name\":{\"stringValue\":\"" + EscapeJson(rules[i].name) + "\"},";
        json += "\"eventType\":{\"stringValue\":\"" + EscapeJson(rules[i].eventType) + "\"},";
        json += "\"deviceFilter\":{\"stringValue\":\"" + EscapeJson(rules[i].deviceFilter) + "\"},";
        json += "\"webhookUrl\":{\"stringValue\":\"" + EscapeJson(rules[i].webhookUrl) + "\"},";
        json += "\"severity\":{\"stringValue\":\"" + EscapeJson(rules[i].severity) + "\"},";
        json += "\"debounceMinutes\":{\"integerValue\":\"" + std::to_string(rules[i].debounceMinutes) + "\"},";
        json += "\"enabled\":{\"booleanValue\":" + std::string(rules[i].enabled ? "true" : "false") + "}";
        json += "}}}";
    }
    json += "]}}}}";

    std::string url = BuildFirestoreUrl(L"settings", L"alertRules");
    std::string response = HttpRequest(L"PATCH", Utf8ToWide(url), json);
    return response.find("\"error\"") == std::string::npos;
}

bool FirebaseClient::LoadAlertRules(std::vector<AlertRule>& rules) {
    if (_userId.empty() || _idToken.empty()) {
        _lastError = L"Not authenticated";
        return false;
    }

    std::string url = BuildFirestoreUrl(L"settings", L"alertRules");
    std::string response = HttpRequest(L"GET", Utf8ToWide(url), "");

    if (response.find("\"error\"") != std::string::npos) {
        // No rules saved yet is OK
        rules.clear();
        return true;
    }

    rules.clear();
    // Parse rules from response - simplified
    // Full implementation would parse the arrayValue
    return true;
}

bool FirebaseClient::SaveUserSettings(const std::wstring& settingsJson) {
    if (_userId.empty() || _idToken.empty()) {
        _lastError = L"Not authenticated";
        return false;
    }

    std::string json = "{\"fields\":{\"data\":{\"stringValue\":\"" + EscapeJson(settingsJson) + "\"}}}";
    std::string url = BuildFirestoreUrl(L"settings", L"userPrefs");
    std::string response = HttpRequest(L"PATCH", Utf8ToWide(url), json);
    return response.find("\"error\"") == std::string::npos;
}

bool FirebaseClient::LoadUserSettings(std::wstring& settingsJson) {
    if (_userId.empty() || _idToken.empty()) {
        _lastError = L"Not authenticated";
        return false;
    }

    std::string url = BuildFirestoreUrl(L"settings", L"userPrefs");
    std::string response = HttpRequest(L"GET", Utf8ToWide(url), "");

    if (response.find("\"error\"") != std::string::npos) {
        settingsJson.clear();
        return true; // No settings yet is OK
    }

    settingsJson = GetJsonValue(response, "data");
    return true;
}

bool FirebaseClient::SyncUserDeviceData(std::vector<Device>& devices) {
    if (_userId.empty() || _idToken.empty()) {
        _lastError = L"Not authenticated";
        return false;
    }

    // Load cloud device data
    std::vector<Device> cloudDevices;
    if (!LoadDevices(cloudDevices)) {
        return false;
    }

    // Build lookup by MAC
    std::map<std::wstring, Device*> cloudByMac;
    for (auto& cd : cloudDevices) {
        if (!cd.mac.empty()) cloudByMac[cd.mac] = &cd;
    }

    // Merge cloud data into local devices
    for (auto& localDev : devices) {
        if (localDev.mac.empty()) continue;

        auto it = cloudByMac.find(localDev.mac);
        if (it != cloudByMac.end()) {
            Device* cloudDev = it->second;
            // Cloud has this device - merge user data from cloud
            if (localDev.customName.empty() && !cloudDev->customName.empty())
                localDev.customName = cloudDev->customName;
            if (localDev.notes.empty() && !cloudDev->notes.empty())
                localDev.notes = cloudDev->notes;
            if (localDev.trustState == L"unknown" && cloudDev->trustState != L"unknown")
                localDev.trustState = cloudDev->trustState;
            if (localDev.location.empty() && !cloudDev->location.empty())
                localDev.location = cloudDev->location;
        }

        // Save back to cloud if device has user data
        if (!localDev.customName.empty() || !localDev.notes.empty() ||
            localDev.trustState != L"unknown" || !localDev.location.empty()) {
            SaveDevice(localDev);
        }
    }

    return true;
}

void FirebaseClient::Logout() {
    _idToken.clear();
    _userId.clear();
}
