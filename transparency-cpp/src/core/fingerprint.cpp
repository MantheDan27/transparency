#include "core/fingerprint.h"
#include "core/oui_data.h"
#include "utils/string_utils.h"

#include <algorithm>
#include <sstream>

using std::wstring;
using std::string;
using std::vector;
using std::map;
using std::set;

namespace core {

// ─── Port/Service Tables ──────────────────────────────────────────────────────

const std::map<int, wstring> PORT_NAMES = {
    {20,   L"FTP-Data"},      {21,   L"FTP"},
    {22,   L"SSH"},           {23,   L"Telnet"},
    {25,   L"SMTP"},          {53,   L"DNS"},
    {67,   L"DHCP-Server"},   {68,   L"DHCP-Client"},
    {69,   L"TFTP"},          {80,   L"HTTP"},
    {110,  L"POP3"},          {119,  L"NNTP"},
    {123,  L"NTP"},           {135,  L"MSRPC"},
    {137,  L"NetBIOS-NS"},    {138,  L"NetBIOS-DG"},
    {139,  L"NetBIOS-SSN"},   {143,  L"IMAP"},
    {161,  L"SNMP"},          {162,  L"SNMP-Trap"},
    {389,  L"LDAP"},          {443,  L"HTTPS"},
    {445,  L"SMB"},           {465,  L"SMTPS"},
    {514,  L"Syslog"},        {515,  L"LPD/LPR"},
    {548,  L"AFP"},           {587,  L"SMTP-Submit"},
    {631,  L"IPP"},           {636,  L"LDAPS"},
    {873,  L"rsync"},         {993,  L"IMAPS"},
    {995,  L"POP3S"},         {1080, L"SOCKS"},
    {1194, L"OpenVPN"},       {1433, L"MSSQL"},
    {1521, L"Oracle-DB"},     {1723, L"PPTP"},
    {1883, L"MQTT"},          {2049, L"NFS"},
    {2082, L"cPanel"},        {2083, L"cPanel-SSL"},
    {2375, L"Docker"},        {2376, L"Docker-SSL"},
    {3000, L"Dev-Server"},    {3268, L"LDAP-GC"},
    {3306, L"MySQL"},         {3389, L"RDP"},
    {3690, L"SVN"},           {4000, L"ICQ"},
    {4443, L"HTTPS-Alt"},     {4500, L"IKE-NAT"},
    {5000, L"Synology-HTTP"}, {5001, L"Synology-HTTPS"},
    {5060, L"SIP"},           {5061, L"SIP-TLS"},
    {5353, L"mDNS"},          {5432, L"PostgreSQL"},
    {5900, L"VNC"},           {5985, L"WinRM-HTTP"},
    {5986, L"WinRM-HTTPS"},   {6379, L"Redis"},
    {7070, L"AJP"},           {7443, L"HTTPS-Alt"},
    {8008, L"Chromecast"},    {8009, L"AJP-Tomcat"},
    {8080, L"HTTP-Alt"},      {8083, L"MQTT-WS"},
    {8086, L"InfluxDB"},      {8088, L"HTTP-Alt2"},
    {8123, L"Home-Assistant"},{8443, L"HTTPS-Alt"},
    {8883, L"MQTT-SSL"},      {9000, L"Portainer"},
    {9090, L"Prometheus"},    {9091, L"Transmission"},
    {9100, L"Printer-PDL"},   {9123, L"Smart-Home"},
    {9200, L"Elasticsearch"}, {10000, L"Webmin"},
    {27017, L"MongoDB"},      {32400, L"Plex-Media"},
    {51413, L"BitTorrent"},
};

const std::set<int> RISKY_PORTS = {
    21, 23, 135, 137, 138, 139, 445,
    1433, 1521, 1723, 3306, 3389, 4443,
    5432, 5900, 2375, 27017, 6379
};

const std::map<wstring, std::vector<int>> PORT_PROFILES = {
    { L"common",   { 21,22,23,25,53,80,110,135,139,443,445,
                     3306,3389,5900,8080,8443 } },
    { L"iot",      { 80,443,1883,8008,8009,8080,8443,9123 } },
    { L"nas",      { 21,22,80,139,443,445,548,873,2049,
                     5000,5001,8080,8443 } },
    { L"security", { 22,80,443,8080,8443,10000 } },
    { L"full",     [] {
        std::vector<int> v;
        for (int i = 1; i <= 1024; i++) v.push_back(i);
        return v;
    }() },
};

// ─── LookupVendor ─────────────────────────────────────────────────────────────

wstring lookup_vendor(const wstring& mac) {
    if (mac.size() < 8) return L"";
    // Convert first 8 chars to uppercase narrow
    string prefix = utils::to_narrow(mac.substr(0, 8));
    for (char& c : prefix) c = (char)toupper((unsigned char)c);
    const char* v = LookupOuiVendor(prefix.c_str());
    if (v) return utils::to_wide(v);
    return L"";
}

// ─── FingerprintDeviceType ────────────────────────────────────────────────────

wstring fingerprint_device_type(const Device& d) {
    wstring vendor = d.vendor;
    std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::tolower);
    wstring hostname = d.hostname;
    std::transform(hostname.begin(), hostname.end(), hostname.begin(), ::tolower);
    wstring ssdp = d.ssdpInfo;
    std::transform(ssdp.begin(), ssdp.end(), ssdp.begin(), ::tolower);

    auto hasPort = [&](int p) {
        return std::find(d.openPorts.begin(), d.openPorts.end(), p) != d.openPorts.end();
    };
    auto hasMdns = [&](const wchar_t* svc) {
        for (auto& s : d.mdnsServices)
            if (s.find(svc) != wstring::npos) return true;
        return false;
    };
    auto vendorContains = [&](const wchar_t* v) {
        return vendor.find(v) != wstring::npos;
    };
    auto ssdpContains = [&](const wchar_t* v) {
        return ssdp.find(v) != wstring::npos;
    };
    auto hostContains = [&](const wchar_t* v) {
        return hostname.find(v) != wstring::npos;
    };

    // Network equipment
    if (vendorContains(L"cisco") || vendorContains(L"netgear") ||
        vendorContains(L"ubiquiti") || vendorContains(L"linksys") ||
        ssdpContains(L"router") || ssdpContains(L"gateway") ||
        hostContains(L"router") || hostContains(L"gateway"))
        return L"Router/Switch";

    // NAS
    if (vendorContains(L"synology") || vendorContains(L"qnap") ||
        (hasPort(5000) && hasPort(5001)) || hasPort(873) ||
        ssdpContains(L"nas") || hostContains(L"nas") || hostContains(L"diskstation"))
        return L"NAS / Storage";

    // Printer
    if (vendorContains(L"epson") || vendorContains(L"canon") || vendorContains(L"brother") ||
        vendorContains(L"hp") ||
        hasMdns(L"_ipp") || hasMdns(L"_printer") || hasPort(9100) || hasPort(631) ||
        ssdpContains(L"printer") || hostContains(L"printer"))
        return L"Printer";

    // Smart TV / Streaming
    if (vendorContains(L"roku") || hasMdns(L"_googlecast") || hasMdns(L"_airplay") ||
        hasMdns(L"_raop") || ssdpContains(L"tv") || ssdpContains(L"roku") ||
        ssdpContains(L"chromecast") || hostContains(L"appletv") || hostContains(L"roku"))
        return L"Smart TV / Streaming";

    // Smart Speaker
    if (vendorContains(L"sonos") || hasMdns(L"_sonos") || ssdpContains(L"sonos") ||
        vendorContains(L"bose") || vendorContains(L"google") && hasPort(8008) ||
        vendorContains(L"amazon") || ssdpContains(L"echo") || ssdpContains(L"alexa"))
        return L"Smart Speaker";

    // Smart Home Hub
    if (vendorContains(L"nest") || vendorContains(L"philips hue") ||
        hasMdns(L"_homekit") || hasMdns(L"_matter") ||
        ssdpContains(L"homekit") || ssdpContains(L"hue bridge") ||
        hostContains(L"homehub") || hostContains(L"home assistant") || hasPort(8123))
        return L"Smart Home Hub";

    // IoT device
    if (hasPort(1883) || hasPort(8883) || ssdpContains(L"iot") ||
        hostContains(L"iot") || hostContains(L"sensor") || hostContains(L"esp"))
        return L"IoT Device";

    // Raspberry Pi
    if (vendorContains(L"raspberry"))
        return L"Single-Board Computer";

    // Desktop PC / Windows
    if (hasPort(3389) || hasPort(445) || hasPort(139) || hasPort(135))
        return L"Windows PC";

    // Linux/Unix server
    if (hasPort(22) && (hasPort(80) || hasPort(443) || hasPort(8080)))
        return L"Linux Server";

    // macOS
    if (vendorContains(L"apple") && (hasMdns(L"_workstation") || hasPort(548)))
        return L"Mac";

    // Mobile/Phone
    if (vendorContains(L"apple") || vendorContains(L"samsung") ||
        vendorContains(L"xiaomi") || vendorContains(L"huawei"))
        return L"Mobile Device";

    // Generic computer
    if (vendorContains(L"dell") || vendorContains(L"hp") || vendorContains(L"lenovo") ||
        vendorContains(L"intel") || vendorContains(L"microsoft"))
        return L"Computer";

    return L"Unknown Device";
}

// ─── BuildClassificationReason ────────────────────────────────────────────────
// Populates dev.classificationReason with a human-readable evidence summary.

void build_classification_reason(Device& d) {
    wstring vendor = d.vendor;
    std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::tolower);
    wstring hostname = d.hostname;
    std::transform(hostname.begin(), hostname.end(), hostname.begin(), ::tolower);
    wstring ssdp = d.ssdpInfo;
    std::transform(ssdp.begin(), ssdp.end(), ssdp.begin(), ::tolower);

    wstring r;
    auto append = [&](const wstring& s) {
        if (!r.empty()) r += L", ";
        r += s;
    };

    // Vendor evidence
    if (!d.vendor.empty()) append(L"vendor: " + d.vendor);

    // Port evidence — list significant open ports
    if (!d.openPorts.empty()) {
        wstring portStr;
        int shown = 0;
        for (int p : d.openPorts) {
            if (shown >= 4) { portStr += L"..."; break; }
            auto it = PORT_NAMES.find(p);
            if (shown > 0) portStr += L",";
            portStr += std::to_wstring(p);
            if (it != PORT_NAMES.end()) portStr += L"/" + it->second;
            shown++;
        }
        append(L"ports: " + portStr);
    }

    // mDNS evidence
    if (!d.mdnsServices.empty()) {
        wstring svc;
        for (size_t i = 0; i < std::min((size_t)2, d.mdnsServices.size()); i++) {
            if (i > 0) svc += L",";
            svc += d.mdnsServices[i];
        }
        append(L"mDNS: " + svc);
    }

    // SSDP evidence
    if (!d.ssdpInfo.empty()) {
        wstring s = d.ssdpInfo;
        if (s.size() > 30) s = s.substr(0, 30) + L"...";
        append(L"SSDP: " + s);
    }

    // NetBIOS evidence
    if (!d.netbiosName.empty()) append(L"NetBIOS: " + d.netbiosName);

    // Hostname evidence
    if (!d.hostname.empty()) append(L"hostname: " + d.hostname);

    // OS guess from banner
    if (!d.osGuess.empty()) append(L"banner: " + d.osGuess);

    // Latency hint
    if (d.latencyMs >= 0 && d.latencyMs <= 2) append(L"latency: <2ms (local)");

    d.classificationReason = r;
}

// ─── CalcConfidence ────────────────────────────────────────────────────────────

int calc_confidence(const Device& d) {
    int score = 0;

    if (!d.vendor.empty()) score += 20;
    if (!d.hostname.empty()) score += 15;
    if (!d.openPorts.empty()) score += 20;
    if (!d.mdnsServices.empty()) score += 25;
    if (!d.ssdpInfo.empty()) score += 15;
    if (!d.netbiosName.empty()) score += 10;
    if (d.latencyMs >= 0) score += 5;

    return std::min(score, 100);
}

// ─── IoT Risk Profiling ────────────────────────────────────────────────────────
// Returns a plain-language risk summary for IoT devices, or empty string if no risk.

wstring profile_iot_risk(const Device& d) {
    auto hasPort = [&](int p) {
        return std::find(d.openPorts.begin(), d.openPorts.end(), p) != d.openPorts.end();
    };

    wstring risk;
    bool hasHttp  = hasPort(80);
    bool hasHttps = hasPort(443);
    bool hasTelnet = hasPort(23);
    bool hasFtp    = hasPort(21);
    bool hasUpnp   = hasPort(1900);
    bool hasSsh    = hasPort(22);
    bool hasRtsp   = hasPort(554);
    bool hasOnvif  = hasPort(2020) || hasPort(8899) || hasPort(37777);

    // Telnet on any device is critical
    if (hasTelnet) {
        risk += L"CRITICAL: Telnet (port 23) is open. "
                L"Credentials transmitted in cleartext — any device on the network can intercept them. "
                L"Disable Telnet immediately and enable SSH if remote access is needed.\r\n";
    }

    // FTP is high risk
    if (hasFtp) {
        risk += L"HIGH: FTP (port 21) is open. "
                L"Unencrypted file transfer — passwords and data are visible in plain text on the wire. "
                L"Disable FTP or replace with SFTP.\r\n";
    }

    // HTTP admin panel without HTTPS
    if (hasHttp && !hasHttps) {
        risk += L"MEDIUM: HTTP admin panel (port 80) without HTTPS. "
                L"Logging into this device over HTTP exposes your credentials on the local network. "
                L"Enable HTTPS in device settings, or access only from a trusted wired connection.\r\n";
    }

    // UPnP exposed
    if (hasUpnp) {
        risk += L"MEDIUM: UPnP (port 1900) is exposed. "
                L"UPnP has a history of critical vulnerabilities (CallStranger, NAT injection). "
                L"Disable UPnP in device and router settings if not required.\r\n";
    }

    // RTSP/camera stream exposed
    if (hasRtsp || hasOnvif) {
        risk += L"HIGH: Camera stream port (RTSP/ONVIF) is open. "
                L"Verify this camera requires authentication before connecting. "
                L"Many IP cameras have default or blank credentials. "
                L"Change the admin password and disable ONVIF if unused.\r\n";
    }

    // SSH without HTTPS (unusual for IoT — may indicate compromised device)
    if (hasSsh && !hasHttp && d.deviceType.find(L"IoT") != wstring::npos) {
        risk += L"INFO: SSH (port 22) open on what appears to be an IoT device. "
                L"Verify this is expected — some IoT devices include SSH backdoors. "
                L"Check that SSH uses key-based auth, not a default password.\r\n";
    }

    return risk;
}

// ─── FillConfidenceAlternatives ───────────────────────────────────────────────
// Populates altType1/altConf1/altType2/altConf2 on a device after fingerprinting.

void fill_confidence_alternatives(Device& d) {
    // Score each type category independently
    struct TypeScore { wstring type; int score; };
    std::vector<TypeScore> scores;

    auto hasPort = [&](int p) {
        return std::find(d.openPorts.begin(), d.openPorts.end(), p) != d.openPorts.end();
    };
    auto hasMdns = [&](const wchar_t* svc) {
        for (auto& s : d.mdnsServices)
            if (s.find(svc) != wstring::npos) return true;
        return false;
    };
    wstring vendor = d.vendor; std::transform(vendor.begin(), vendor.end(), vendor.begin(), ::tolower);
    wstring ssdp   = d.ssdpInfo; std::transform(ssdp.begin(), ssdp.end(), ssdp.begin(), ::tolower);
    wstring host   = d.hostname; std::transform(host.begin(), host.end(), host.begin(), ::tolower);

    // Helper to do a case-insensitive match against a lowercased input string,
    // where 'v' must already be lowercased before calling this lambda.
    auto vendorHas = [&](const wchar_t* v) { return vendor.find(v) != wstring::npos; };
    auto ssdpHas   = [&](const wchar_t* v) { return ssdp.find(v) != wstring::npos; };
    auto hostHas   = [&](const wchar_t* v) { return host.find(v) != wstring::npos; };

    auto add = [&](const wchar_t* type, int sc) { scores.push_back({ type, sc }); };

    add(L"Router/Switch",      (vendorHas(L"cisco")||vendorHas(L"netgear")||vendorHas(L"ubiquiti")||ssdpHas(L"router")||hostHas(L"router")) ? 80 : 5);
    add(L"NAS / Storage",      (vendorHas(L"synology")||vendorHas(L"qnap")||(hasPort(5000)&&hasPort(5001))||hostHas(L"nas")) ? 80 : 5);
    add(L"Printer",            (hasMdns(L"_ipp")||hasMdns(L"_printer")||hasPort(9100)||hasPort(631)||vendorHas(L"epson")||vendorHas(L"canon")||vendorHas(L"brother")) ? 80 : 5);
    add(L"Smart TV / Streaming",(hasMdns(L"_googlecast")||hasMdns(L"_airplay")||vendorHas(L"roku")||ssdpHas(L"tv")) ? 80 : 5);
    add(L"Smart Speaker",      (hasMdns(L"_sonos")||vendorHas(L"sonos")||vendorHas(L"amazon")||ssdpHas(L"echo")) ? 80 : 5);
    add(L"Smart Home Hub",     (hasMdns(L"_homekit")||hasMdns(L"_matter")||vendorHas(L"philips hue")||hasPort(8123)) ? 80 : 5);
    add(L"IoT Device",         (hasPort(1883)||hasPort(8883)||hostHas(L"esp")||hostHas(L"sensor")) ? 75 : 5);
    add(L"Windows PC",         (hasPort(3389)||hasPort(445)||(hasPort(139)&&hasPort(135))) ? 80 : 5);
    add(L"Linux Server",       (hasPort(22)&&(hasPort(80)||hasPort(443)||hasPort(8080))) ? 75 : 5);
    add(L"Mac",                (vendorHas(L"apple")&&(hasMdns(L"_workstation")||hasPort(548))) ? 80 : 5);
    add(L"Mobile Device",      (vendorHas(L"apple")||vendorHas(L"samsung")||vendorHas(L"xiaomi")) ? 50 : 5);
    add(L"Computer",           (vendorHas(L"dell")||vendorHas(L"hp")||vendorHas(L"lenovo")||vendorHas(L"intel")) ? 60 : 5);
    add(L"Unknown Device",     15);

    // Sort descending
    std::sort(scores.begin(), scores.end(), [](const TypeScore& a, const TypeScore& b) {
        return a.score > b.score;
    });

    // Remove primary type from alternatives
    int altIdx = 0;
    for (auto& ts : scores) {
        if (ts.type == d.deviceType) continue;
        if (altIdx == 0) { d.altType1 = ts.type; d.altConf1 = ts.score; altIdx++; }
        else if (altIdx == 1) { d.altType2 = ts.type; d.altConf2 = ts.score; break; }
    }
}

// ─── AnalyzeAnomalies ────────────────────────────────────────────────────────

std::vector<Anomaly> analyze_anomalies(
    const ScanResult& current,
    const ScanResult& previous)
{
    std::vector<Anomaly> anomalies;

    // Build previous device lookup by MAC
    std::map<wstring, const Device*> prevByMac;
    std::map<wstring, const Device*> prevByIp;
    for (auto& d : previous.devices) {
        if (!d.mac.empty()) prevByMac[d.mac] = &d;
        prevByIp[d.ip] = &d;
    }

    std::set<wstring> currentMacs;
    std::set<wstring> currentIps;
    for (auto& d : current.devices) {
        if (!d.mac.empty()) currentMacs.insert(d.mac);
        currentIps.insert(d.ip);
    }

    // Per-device anomalies
    for (auto& dev : current.devices) {
        // New device
        bool isNew = (prevByMac.find(dev.mac) == prevByMac.end() &&
                      prevByIp.find(dev.ip) == prevByIp.end());
        if (isNew && !previous.devices.empty()) {
            Anomaly a;
            a.type = L"new_device";
            a.severity = L"medium";
            a.deviceIp = dev.ip;
            a.description = L"New device detected: " + dev.ip +
                            (dev.hostname.empty() ? L"" : L" (" + dev.hostname + L")");
            a.explanation = L"A device was found on your network that was not present in the previous scan. "
                            L"This could be a new device you added, a guest, or an unauthorized device. "
                            L"Vendor: " + (dev.vendor.empty() ? L"Unknown" : dev.vendor) + L".";
            a.remediation = L"1. Identify the device by its MAC address and vendor. "
                            L"2. If authorized, mark it as 'Known' in the device list. "
                            L"3. If unrecognized, consider blocking it on your router's MAC filter. "
                            L"4. Enable router logs to track when it connected.";
            a.traceSource = L"ArpTable+PingSweep";
            anomalies.push_back(a);
        }

        // Risky ports
        for (int port : dev.openPorts) {
            if (RISKY_PORTS.count(port) == 0) continue;

            Anomaly a;
            a.type = L"risky_port";
            a.deviceIp = dev.ip;
            a.affectedPorts.push_back(port);

            switch (port) {
            case 21:
                a.severity = L"high";
                a.description = L"FTP (port 21) is open on " + dev.ip;
                a.explanation = L"FTP transmits data and credentials in plaintext, making it vulnerable to "
                                L"man-in-the-middle attacks and packet sniffing. Modern replacements (SFTP/FTPS) "
                                L"should be used instead.";
                a.remediation = L"1. Disable FTP if not actively used. "
                                L"2. Replace with SFTP (port 22) or FTPS (port 990). "
                                L"3. If FTP is required, restrict access with a firewall to known IPs only. "
                                L"4. Enable FTP over TLS (FTPS) at minimum.";
                break;
            case 23:
                a.severity = L"critical";
                a.description = L"Telnet (port 23) is open on " + dev.ip;
                a.explanation = L"Telnet transmits everything - including usernames and passwords - in plaintext. "
                                L"It has been deprecated for decades and should never be used on a network. "
                                L"This is a serious security risk as any attacker who can see network traffic "
                                L"can capture credentials.";
                a.remediation = L"1. Immediately disable Telnet on this device. "
                                L"2. Enable SSH instead (port 22). "
                                L"3. If the device is a router or switch, access it via the web interface or SSH. "
                                L"4. If this is an IoT device, check for a firmware update that adds SSH.";
                break;
            case 135:
                a.severity = L"high";
                a.description = L"MSRPC (port 135) exposed on " + dev.ip;
                a.explanation = L"Windows Remote Procedure Call (MSRPC) on port 135 is used for DCOM and WMI. "
                                L"This port has been exploited by numerous worms (MS03-026, Blaster) and should "
                                L"not be accessible across network segments. Exposure increases lateral movement risk.";
                a.remediation = L"1. Block port 135 at the firewall/router. "
                                L"2. Ensure Windows Firewall is enabled on this device. "
                                L"3. Apply all Windows security updates. "
                                L"4. Consider network segmentation to isolate Windows hosts.";
                break;
            case 139:
                a.severity = L"high";
                a.description = L"NetBIOS Session Service (port 139) on " + dev.ip;
                a.explanation = L"NetBIOS over TCP/IP (port 139) is an older Windows file sharing protocol. "
                                L"Combined with SMB, it can expose file shares and be exploited for credential "
                                L"attacks (Pass-the-Hash). EternalBlue/WannaCry also targeted this.";
                a.remediation = L"1. Disable NetBIOS over TCP/IP if not needed. "
                                L"2. Block ports 137-139 at the firewall. "
                                L"3. Use SMB2/SMB3 (port 445) instead if file sharing is needed. "
                                L"4. Ensure SMBv1 is disabled on all Windows hosts.";
                break;
            case 445:
                a.severity = L"high";
                a.description = L"SMB (port 445) exposed on " + dev.ip;
                a.explanation = L"Server Message Block (SMB) is used for Windows file sharing. "
                                L"This port was targeted by EternalBlue (NSA exploit) used in the WannaCry "
                                L"and NotPetya ransomware attacks. Exposure to the internet or untrusted "
                                L"networks is extremely dangerous. Even on local networks, SMBv1 has severe vulnerabilities.";
                a.remediation = L"1. Block SMB (445) at the network perimeter - NEVER expose to internet. "
                                L"2. Disable SMBv1 on all Windows machines. "
                                L"3. Apply MS17-010 patch if not already done. "
                                L"4. Consider requiring authentication before accessing shares. "
                                L"5. Use Windows Defender Firewall to restrict SMB to known hosts.";
                break;
            case 1433:
                a.severity = L"high";
                a.description = L"SQL Server (port 1433) exposed on " + dev.ip;
                a.explanation = L"Microsoft SQL Server listening on port 1433 is a target for SQL injection, "
                                L"brute force, and exploitation. Direct database access from the network "
                                L"bypasses application-layer security controls and can lead to data theft.";
                a.remediation = L"1. Restrict SQL Server access to the application server only via firewall. "
                                L"2. Disable the SA account or ensure it has a strong password. "
                                L"3. Enable SQL Server auditing. "
                                L"4. Use Windows Authentication instead of SQL Authentication where possible. "
                                L"5. Apply all SQL Server security updates.";
                break;
            case 1723:
                a.severity = L"medium";
                a.description = L"PPTP VPN (port 1723) on " + dev.ip;
                a.explanation = L"PPTP (Point-to-Point Tunneling Protocol) is a deprecated VPN protocol "
                                L"with known cryptographic weaknesses. MS-CHAPv2, used by PPTP, can be "
                                L"cracked in under a day with modern hardware. PPTP should be considered broken.";
                a.remediation = L"1. Replace PPTP with OpenVPN, WireGuard, or IKEv2/IPSec. "
                                L"2. If PPTP must be used temporarily, ensure strong passwords. "
                                L"3. Consider limiting PPTP access to specific client IPs at the firewall.";
                break;
            case 3306:
                a.severity = L"high";
                a.description = L"MySQL (port 3306) exposed on " + dev.ip;
                a.explanation = L"MySQL/MariaDB database server directly accessible on the network. "
                                L"Databases should never be directly accessible across the network unless "
                                L"absolutely necessary. Attackers can attempt brute force, exploit known "
                                L"vulnerabilities, or steal data if they gain access.";
                a.remediation = L"1. Bind MySQL to 127.0.0.1 if only local access is needed. "
                                L"2. Use a firewall to restrict port 3306 to the application server IP only. "
                                L"3. Disable the root user's remote login capability. "
                                L"4. Use strong passwords for all database accounts. "
                                L"5. Apply all MySQL/MariaDB security patches.";
                break;
            case 3389:
                a.severity = L"high";
                a.description = L"RDP (port 3389) exposed on " + dev.ip;
                a.explanation = L"Remote Desktop Protocol (RDP) is a frequent target for ransomware groups. "
                                L"BlueKeep (CVE-2019-0708) and DejaBlue are critical RDP vulnerabilities. "
                                L"Brute force attacks against RDP are extremely common. Over 4 million "
                                L"RDP servers are scanned daily by attackers on the internet.";
                a.remediation = L"1. Do NOT expose RDP directly to the internet. "
                                L"2. Use a VPN to access RDP internally. "
                                L"3. Enable Network Level Authentication (NLA). "
                                L"4. Change RDP port from 3389 to a non-standard port. "
                                L"5. Enable account lockout policies. "
                                L"6. Use RD Gateway if external access is required. "
                                L"7. Apply all BlueKeep/DejaBlue patches.";
                break;
            case 5900:
                a.severity = L"high";
                a.description = L"VNC (port 5900) exposed on " + dev.ip;
                a.explanation = L"VNC (Virtual Network Computing) provides remote desktop access. "
                                L"VNC often uses weak authentication (just a password, no username) "
                                L"and many implementations have been found to have critical vulnerabilities. "
                                L"If exposed to the internet, VNC servers are quickly found and attacked.";
                a.remediation = L"1. Never expose VNC directly to the internet. "
                                L"2. Tunnel VNC over SSH (ssh -L 5900:localhost:5900). "
                                L"3. Use a firewall to restrict port 5900 to trusted IPs only. "
                                L"4. Use a strong VNC password (8+ chars). "
                                L"5. Consider replacing VNC with RDP or a commercial solution with MFA.";
                break;
            default:
                a.severity = L"medium";
                a.description = L"Potentially risky port " + std::to_wstring(port) + L" open on " + dev.ip;
                a.explanation = L"Port " + std::to_wstring(port) + L" is flagged as potentially risky.";
                a.remediation = L"Review whether this service needs to be accessible from the network. "
                                L"If not, disable the service or firewall the port.";
            }

            a.traceSource = L"PortScan";
            anomalies.push_back(a);
        }

        // Port changes
        if (!dev.prevPorts.empty() && dev.prevPorts != dev.openPorts) {
            std::vector<int> newPorts, closedPorts;
            for (int p : dev.openPorts) {
                if (std::find(dev.prevPorts.begin(), dev.prevPorts.end(), p) == dev.prevPorts.end())
                    newPorts.push_back(p);
            }
            for (int p : dev.prevPorts) {
                if (std::find(dev.openPorts.begin(), dev.openPorts.end(), p) == dev.openPorts.end())
                    closedPorts.push_back(p);
            }

            if (!newPorts.empty()) {
                Anomaly a;
                a.type = L"port_changed";
                a.severity = L"medium";
                a.deviceIp = dev.ip;
                a.affectedPorts = newPorts;

                wstring portList;
                for (int p : newPorts) {
                    auto it = PORT_NAMES.find(p);
                    portList += std::to_wstring(p);
                    if (it != PORT_NAMES.end()) portList += L" (" + it->second + L")";
                    portList += L", ";
                }
                if (!portList.empty()) portList.pop_back(), portList.pop_back();

                a.description = L"New ports opened on " + dev.ip + L": " + portList;
                a.explanation = L"Services that were not running in the previous scan are now active. "
                                L"This could indicate new software was installed, a service was started, "
                                L"or a potential compromise installing a backdoor.";
                a.remediation = L"1. Verify the new service is intentional. "
                                L"2. Check which process is listening on these ports (netstat -ano). "
                                L"3. If unexpected, investigate for signs of compromise. "
                                L"4. Review installed software and startup items.";
                a.traceSource = L"PortScan+Comparison";
                anomalies.push_back(a);
            }
        }

        // IP changed for same MAC
        if (!dev.mac.empty()) {
            auto it = prevByMac.find(dev.mac);
            if (it != prevByMac.end()) {
                if (it->second->ip != dev.ip) {
                    Anomaly a;
                    a.type = L"ip_changed";
                    a.severity = L"low";
                    a.deviceIp = dev.ip;
                    a.description = L"IP address changed for " + dev.mac +
                                    L": was " + it->second->ip + L", now " + dev.ip;
                    a.explanation = L"A device's IP address changed between scans. This is common when DHCP "
                                    L"leases expire and the device gets a new address. However, it can also "
                                    L"indicate IP spoofing or ARP poisoning in rare cases.";
                    a.remediation = L"1. If this is unexpected, verify the device's MAC address physically. "
                                    L"2. Consider assigning a static IP or DHCP reservation to this device. "
                                    L"3. Monitor for further IP changes.";
                    a.traceSource = L"ArpTable+Comparison";
                    anomalies.push_back(a);
                }

                // Hostname changed
                if (!dev.hostname.empty() && !it->second->hostname.empty() &&
                    dev.hostname != it->second->hostname) {
                    Anomaly a;
                    a.type = L"hostname_changed";
                    a.severity = L"low";
                    a.deviceIp = dev.ip;
                    a.description = L"Hostname changed on " + dev.ip + L": was \"" +
                                    it->second->hostname + L"\", now \"" + dev.hostname + L"\"";
                    a.explanation = L"A device's hostname changed between scans. This could indicate a device "
                                    L"reconfiguration, OS reinstall, or a different device using the same MAC. "
                                    L"On Wi-Fi with MAC randomization, this may indicate a reconnecting device.";
                    a.remediation = L"1. Verify the device identity if the hostname change is unexpected. "
                                    L"2. If this is a known device, update its custom name for clarity.";
                    a.traceSource = L"DNS+Comparison";
                    anomalies.push_back(a);
                }
            }
        }
    }

    // Device offline
    for (auto& prev : previous.devices) {
        if (currentIps.find(prev.ip) == currentIps.end() &&
            (prev.mac.empty() || currentMacs.find(prev.mac) == currentMacs.end())) {
            Anomaly a;
            a.type = L"device_offline";
            a.severity = L"low";
            a.deviceIp = prev.ip;
            a.description = L"Device " + prev.ip +
                            (prev.hostname.empty() ? L"" : L" (" + prev.hostname + L")") +
                            L" is no longer reachable";
            a.explanation = L"A device that was present in the previous scan is no longer responding. "
                            L"This could mean the device was turned off, unplugged, or left the network. "
                            L"It could also indicate a connectivity issue.";
            a.remediation = L"1. Check if the device was intentionally disconnected. "
                            L"2. Verify the device is powered on. "
                            L"3. Check for network issues if multiple devices are offline.";
            a.traceSource = L"PingSweep+Comparison";
            anomalies.push_back(a);
        }
    }

    return anomalies;
}

} // namespace core
