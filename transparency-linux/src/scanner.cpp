#include "scanner.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <regex>
#include <array>
#include <set>

const std::map<int, std::string> ScanEngine::PORT_NAMES = {
    {20,"FTP-Data"},{21,"FTP"},{22,"SSH"},{23,"Telnet"},{25,"SMTP"},{53,"DNS"},
    {80,"HTTP"},{110,"POP3"},{111,"RPCBind"},{135,"MS-RPC"},{139,"NetBIOS"},
    {143,"IMAP"},{443,"HTTPS"},{445,"SMB"},{515,"LPD"},{548,"AFP"},
    {554,"RTSP"},{631,"IPP"},{873,"rsync"},{902,"VMware"},{903,"VMware-HTTP"},
    {993,"IMAPS"},{995,"POP3S"},{1400,"Sonos"},{1443,"Sonos-HTTPS"},
    {1883,"MQTT"},{2049,"NFS"},{2376,"Docker"},{2377,"Docker-Swarm"},
    {3260,"iSCSI"},{3306,"MySQL"},{3389,"RDP"},{5000,"Synology"},
    {5001,"Synology-SSL"},{5432,"PostgreSQL"},{5683,"CoAP"},{5900,"VNC"},
    {5985,"WinRM"},{5986,"WinRM-SSL"},{6443,"Kubernetes"},{7000,"AirPlay"},
    {8006,"Proxmox"},{8008,"Chromecast"},{8009,"Chromecast-Int"},
    {8080,"HTTP-Alt"},{8443,"HTTPS-Alt"},{8883,"MQTT-SSL"},
    {9090,"Cockpit"},{9100,"Printer"},{9123,"Shelly"},
    {16509,"libvirt"},{16514,"libvirt-TLS"},{34567,"DVR"},{37777,"Dahua"},
};

static const std::vector<int> COMMON_PORTS = {21,22,23,53,80,135,139,443,445,554,902,903,1883,2376,3306,3389,5000,5001,5900,5985,8006,8080,8443,9090,9100,16509};
static const std::vector<int> QUICK_PORTS = {22,80,443,3389,5900,8080};
static const std::vector<int> RISKY_PORTS = {23,135,139,445,3389,5900};
static const std::vector<int> VM_PORTS = {902,903,8006,2376,2377,6443,16509,16514,5985,5986};

struct OuiEntry { const char* prefix; const char* vendor; };
static const OuiEntry OUI_TABLE[] = {
    {"00:0C:29","VMware"},{"00:50:56","VMware"},{"00:05:69","VMware"},
    {"00:1C:14","VMware"},{"08:00:27","VirtualBox"},{"52:54:00","QEMU/KVM"},
    {"00:16:3E","Xen"},{"00:1C:42","Parallels"},{"00:15:5D","Hyper-V"},
    {"00:0F:4B","Oracle VM"},{"02:42:AC","Docker"},{"02:42:00","Docker"},
    {"0A:58:0A","Kubernetes"},
    {"AC:DE:48","Apple"},{"A4:83:E7","Apple"},{"F0:18:98","Apple"},
    {"3C:22:FB","Apple"},{"DC:A6:32","Raspberry Pi"},{"B8:27:EB","Raspberry Pi"},
    {"E4:5F:01","Raspberry Pi"},{"28:CD:C1","Raspberry Pi"},
    {"00:1A:2B","Cisco"},{"00:25:B5","Cisco"},{"00:50:F1","Cisco"},
    {"F8:B7:E2","Cisco"},{"00:1E:58","D-Link"},{"28:10:7B","D-Link"},
    {"C0:A0:BB","D-Link"},{"00:14:6C","Netgear"},{"20:4E:7F","Netgear"},
    {"A4:2B:8C","Netgear"},{"EC:08:6B","TP-Link"},{"50:C7:BF","TP-Link"},
    {"98:DA:C4","TP-Link"},{"60:A4:4C","TP-Link"},{"14:EB:B6","TP-Link"},
    {"B0:4E:26","TP-Link"},{"00:11:32","Synology"},{"00:1B:21","Intel"},
    {"3C:97:0E","Intel"},{"A4:BF:01","Intel"},{"00:1A:11","Google"},
    {"54:60:09","Google"},{"F4:F5:D8","Google"},{"48:D6:D5","Google"},
    {"30:FD:38","Google"},{"58:CB:52","Google"},
    {"FC:A1:83","Samsung"},{"8C:F5:A3","Samsung"},{"B4:79:A7","Samsung"},
    {"00:17:C4","HP"},{"3C:D9:2B","HP"},{"00:1E:68","Ubiquiti"},
    {"24:A4:3C","Ubiquiti"},{"B4:FB:E4","Ubiquiti"},{"F0:9F:C2","Ubiquiti"},
    {"68:D7:9A","Ubiquiti"},{"FC:EC:DA","Ubiquiti"},
    {"44:D9:E7","Ubiquiti"},{"74:83:C2","Ubiquiti"},
    {"70:A7:41","Sonos"},{"54:2A:1B","Sonos"},{"B8:E9:37","Sonos"},
    {"00:04:4B","Roku"},{"D8:31:34","Roku"},{"B0:A7:37","Roku"},
    {"AC:3A:7A","Roku"},
    {"00:25:90","Dell"},{"F8:DB:88","Dell"},
    {"00:0D:3A","Microsoft"},{"28:18:78","Microsoft"},
    {"A8:13:74","Eero"},{"50:F5:DA","Amazon"},{"14:91:82","Amazon"},
    {"44:07:0B","Amazon"},{"FC:65:DE","Amazon"},
    {"00:17:88","Philips Hue"},{"EC:B5:FA","Philips Hue"},
    {"60:01:94","Espressif"},{"24:6F:28","Espressif"},
    {"CC:50:E3","Espressif"},{"A4:CF:12","Espressif"},
    {"B4:E6:2D","Espressif"},{"30:AE:A4","Espressif"},
    {"AC:67:B2","Espressif"},{"D8:F1:5B","Espressif"},
    {"94:B9:7E","Espressif"},{"84:CC:A8","Espressif"},
    {"40:F5:20","Espressif"},{"EC:FA:BC","Espressif"},
    {"7C:DF:A1","Espressif"},
    {"68:FF:7B","Shelly"},{"E8:DB:84","Shelly"},
    {"00:11:32","Synology"},{"00:09:B0","QNAP"},
    {"B0:BE:76","TP-Link (Kasa)"},{"54:AF:97","TP-Link (Kasa)"},
    {"B0:95:75","TP-Link (Kasa)"},
    {"90:DD:5D","Apple"},{"F4:5C:89","Apple"},{"BC:D0:74","Apple"},
    {"78:7B:8A","Apple"},{"A8:51:AB","Apple"},
    {"2C:F0:A2","Apple"},{"A0:78:17","Apple"},
    {nullptr, nullptr}
};

static bool isValidIp(const std::string& ip) {
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;
    if (inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) == 1) return true;
    if (inet_pton(AF_INET6, ip.c_str(), &(sa6.sin6_addr)) == 1) return true;
    return false;
}

static std::string execCommand(const std::string& cmd) {
    std::array<char, 4096> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe))
        result += buffer.data();
    pclose(pipe);
    return result;
}

void ScanEngine::Cancel() { _cancelled = true; }

std::vector<ScanEngine::NetworkInfo> ScanEngine::getLocalNetworks() {
    std::vector<NetworkInfo> nets;
    struct ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) != 0) return nets;

    for (auto* ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        std::string name = ifa->ifa_name;
        if (name == "lo") continue;

        auto* sa = (struct sockaddr_in*)ifa->ifa_addr;
        auto* nm = (struct sockaddr_in*)ifa->ifa_netmask;
        char ipBuf[INET_ADDRSTRLEN], nmBuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sa->sin_addr, ipBuf, sizeof(ipBuf));
        inet_ntop(AF_INET, &nm->sin_addr, nmBuf, sizeof(nmBuf));

        uint32_t mask = ntohl(nm->sin_addr.s_addr);
        int prefix = 0;
        for (uint32_t m = mask; m & 0x80000000; m <<= 1) prefix++;

        NetworkInfo ni;
        ni.ip = ipBuf;
        ni.netmask = nmBuf;
        ni.iface = name;
        ni.prefix = prefix;
        ni.gateway = getDefaultGateway();
        nets.push_back(ni);
    }
    freeifaddrs(ifap);
    return nets;
}

std::string ScanEngine::getDefaultGateway() {
    std::ifstream f("/proc/net/route");
    std::string line;
    std::getline(f, line); // header
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string iface, dest, gw;
        iss >> iface >> dest >> gw;
        if (dest == "00000000") {
            uint32_t gwAddr = std::stoul(gw, nullptr, 16);
            struct in_addr addr;
            addr.s_addr = gwAddr;
            return inet_ntoa(addr);
        }
    }
    return "";
}

std::map<std::string, std::string> ScanEngine::readArpTable() {
    std::map<std::string, std::string> table;
    std::ifstream f("/proc/net/arp");
    std::string line;
    std::getline(f, line); // header
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string ip, type, flags, mac;
        iss >> ip >> type >> flags >> mac;
        if (mac != "00:00:00:00:00:00" && !mac.empty()) {
            // Uppercase MAC
            std::transform(mac.begin(), mac.end(), mac.begin(), ::toupper);
            table[ip] = mac;
        }
    }
    return table;
}

std::vector<std::string> ScanEngine::generateSubnetIPs(const std::string& ip, int prefix) {
    std::vector<std::string> ips;
    struct in_addr addr;
    inet_aton(ip.c_str(), &addr);
    uint32_t base = ntohl(addr.s_addr);
    uint32_t mask = prefix < 32 ? (~0U << (32 - prefix)) : ~0U;
    uint32_t network = base & mask;
    uint32_t count = ~mask;
    if (count > 1024) count = 1024; // limit

    for (uint32_t i = 1; i < count; i++) {
        uint32_t hostAddr = htonl(network + i);
        struct in_addr a;
        a.s_addr = hostAddr;
        ips.push_back(inet_ntoa(a));
    }
    return ips;
}

bool ScanEngine::pingHost(const std::string& ip, int timeoutMs) {
    if (!isValidIp(ip)) return false;
    // Validate IP address to prevent command injection
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;
    if (inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 1 &&
        inet_pton(AF_INET6, ip.c_str(), &(sa6.sin6_addr)) != 1) {
        return false; // Invalid IP address
    }

    std::string cmd = "ping -c 1 -W " + std::to_string(timeoutMs / 1000 + 1) +
                      " " + ip + " > /dev/null 2>&1";
    return system(cmd.c_str()) == 0;
}

int ScanEngine::measureLatency(const std::string& ip) {
    if (!isValidIp(ip)) return -1;
    // Validate IP address to prevent command injection
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;
    if (inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 1 &&
        inet_pton(AF_INET6, ip.c_str(), &(sa6.sin6_addr)) != 1) {
        return -1; // Invalid IP address
    }

    std::string out = execCommand("ping -c 1 -W 2 " + ip + " 2>/dev/null");
    auto pos = out.find("time=");
    if (pos != std::string::npos) {
        try { return (int)std::stof(out.substr(pos + 5)); }
        catch (...) {}
    }
    return -1;
}

bool ScanEngine::probePort(const std::string& ip, int port, int timeoutMs) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    int ret = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == 0) { close(sock); return true; }
    if (errno != EINPROGRESS) { close(sock); return false; }

    struct pollfd pfd = {sock, POLLOUT, 0};
    ret = poll(&pfd, 1, timeoutMs);
    if (ret > 0 && (pfd.revents & POLLOUT)) {
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);
        close(sock);
        return err == 0;
    }
    close(sock);
    return false;
}

std::vector<int> ScanEngine::scanPorts(const std::string& ip, const std::vector<int>& ports, int timeoutMs) {
    std::vector<int> open;
    std::mutex mtx;

    int numThreads = std::min(32, (int)ports.size());
    std::vector<std::thread> threads;
    std::atomic<size_t> idx{0};

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&]() {
            while (!_cancelled) {
                size_t current_idx = idx.fetch_add(1);
                if (current_idx >= ports.size()) break;

                int port = ports[current_idx];
                if (probePort(ip, port, timeoutMs)) {
                    std::lock_guard<std::mutex> lk(mtx);
                    open.push_back(port);
                }
            }
        });
    }
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    std::sort(open.begin(), open.end());
    return open;
}

std::string ScanEngine::resolveHostname(const std::string& ip) {
    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &sa.sin_addr);
    char host[NI_MAXHOST];
    if (getnameinfo((struct sockaddr*)&sa, sizeof(sa), host, sizeof(host),
                    nullptr, 0, NI_NAMEREQD) == 0) {
        return host;
    }
    return "";
}

std::string ScanEngine::grabBanner(const std::string& ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";

    struct timeval tv = {2, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(sock);
        return "";
    }

    // For HTTP ports, send a request
    if (port == 80 || port == 8080 || port == 8443 || port == 443) {
        std::string req = "GET / HTTP/1.0\r\nHost: " + ip + "\r\n\r\n";
        send(sock, req.c_str(), req.size(), 0);
    }

    char buf[1024] = {};
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    close(sock);
    if (n > 0) return std::string(buf, n);
    return "";
}

std::string ScanEngine::lookupVendor(const std::string& mac) {
    if (mac.size() < 8) return "";
    std::string prefix = mac.substr(0, 8);
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::toupper);
    for (const auto* e = OUI_TABLE; e->prefix; e++) {
        if (prefix == e->prefix) return e->vendor;
    }
    return "";
}

void ScanEngine::detectVM(Device& dev) {
    static const std::vector<std::pair<std::string, std::string>> vmOuis = {
        {"00:0C:29","VMware"},{"00:50:56","VMware"},{"00:05:69","VMware"},
        {"00:1C:14","VMware"},{"08:00:27","VirtualBox"},{"52:54:00","QEMU/KVM"},
        {"00:16:3E","Xen"},{"00:1C:42","Parallels"},{"00:15:5D","Hyper-V"},
        {"00:0F:4B","Oracle VM"},{"02:42:AC","Docker"},{"02:42:00","Docker"},
        {"0A:58:0A","Kubernetes"},
    };

    std::string macUp = dev.mac;
    std::transform(macUp.begin(), macUp.end(), macUp.begin(), ::toupper);
    std::string prefix = macUp.size() >= 8 ? macUp.substr(0, 8) : "";

    for (auto& [oui, name] : vmOuis) {
        if (prefix == oui) {
            dev.isVM = true;
            dev.deviceType = "Virtual Machine (" + name + ")";
            dev.confidence = std::max(dev.confidence, 70);
            break;
        }
    }

    // Check hypervisor management ports
    static const std::set<int> hypervisorPorts = {902,903,8006,2376,2377,6443,16509,16514,5985,5986};
    int vmPortCount = 0;
    for (int p : dev.openPorts) {
        if (hypervisorPorts.count(p)) vmPortCount++;
    }
    if (vmPortCount >= 2) {
        dev.isHypervisor = true;
        dev.deviceType = "Hypervisor Host";
        dev.confidence = std::max(dev.confidence, 80);
    }

    // Hostname patterns
    std::string hostLower = dev.hostname;
    std::transform(hostLower.begin(), hostLower.end(), hostLower.begin(), ::tolower);
    static const std::vector<std::string> vmPatterns = {
        "vm-","vm_","docker","container","kube","k8s","proxmox","esxi",
        "hyperv","vbox","qemu","libvirt","lxc","xen"
    };
    for (auto& pat : vmPatterns) {
        if (hostLower.find(pat) != std::string::npos) {
            dev.isVM = true;
            if (pat == "proxmox" || pat == "esxi" || pat == "hyperv")
                dev.isHypervisor = true;
            dev.confidence = std::max(dev.confidence, 60);
            break;
        }
    }
}

void ScanEngine::fingerprintDevice(Device& dev) {
    int score = 0;
    std::string type = "Unknown Device";
    std::string alt1, alt2;
    int alt1c = 0, alt2c = 0;

    struct TypeScore { std::string type; int score; };
    std::vector<TypeScore> candidates;

    // Vendor-based hints
    std::string vendorLower = dev.vendor;
    std::transform(vendorLower.begin(), vendorLower.end(), vendorLower.begin(), ::tolower);

    auto hasPort = [&](int p) {
        return std::find(dev.openPorts.begin(), dev.openPorts.end(), p) != dev.openPorts.end();
    };

    // Router/Gateway detection
    if (hasPort(53) && (hasPort(80) || hasPort(443))) {
        candidates.push_back({"Router/Gateway", 60});
    }
    // NAS detection
    if (hasPort(445) && (hasPort(548) || hasPort(5000) || hasPort(5001) || hasPort(2049))) {
        candidates.push_back({"NAS", 65});
    }
    if (vendorLower.find("synology") != std::string::npos || vendorLower.find("qnap") != std::string::npos) {
        candidates.push_back({"NAS", 80});
    }
    // Printer
    if (hasPort(9100) || hasPort(631) || hasPort(515)) {
        candidates.push_back({"Printer", 70});
    }
    // Smart Speaker
    if (hasPort(1400) || (hasPort(8008) && hasPort(8009))) {
        candidates.push_back({"Smart Speaker/Display", 65});
    }
    if (vendorLower.find("sonos") != std::string::npos) {
        candidates.push_back({"Smart Speaker", 85});
    }
    if (vendorLower.find("google") != std::string::npos && hasPort(8008)) {
        candidates.push_back({"Smart Display (Google)", 75});
    }
    // Camera/DVR
    if (hasPort(554) || hasPort(34567) || hasPort(37777)) {
        candidates.push_back({"Camera/DVR", 65});
    }
    // Windows PC
    if (hasPort(135) && hasPort(445)) {
        candidates.push_back({"Windows PC", 55});
    }
    if (hasPort(3389)) {
        candidates.push_back({"Windows PC", 50});
    }
    // Linux Server
    if (hasPort(22) && !hasPort(135)) {
        candidates.push_back({"Linux Server", 40});
    }
    // Apple device
    if (vendorLower.find("apple") != std::string::npos) {
        candidates.push_back({"Apple Device", 60});
    }
    // Raspberry Pi
    if (vendorLower.find("raspberry") != std::string::npos) {
        candidates.push_back({"Raspberry Pi", 85});
    }
    // IoT / Espressif
    if (vendorLower.find("espressif") != std::string::npos || vendorLower.find("shelly") != std::string::npos) {
        candidates.push_back({"IoT Device", 75});
    }
    // TP-Link Kasa
    if (vendorLower.find("kasa") != std::string::npos) {
        candidates.push_back({"Smart Plug/Switch", 80});
    }
    // Roku
    if (vendorLower.find("roku") != std::string::npos) {
        candidates.push_back({"Streaming Device (Roku)", 85});
    }
    // Amazon
    if (vendorLower.find("amazon") != std::string::npos) {
        candidates.push_back({"Amazon Echo/Fire", 70});
    }
    // Philips Hue
    if (vendorLower.find("philips") != std::string::npos) {
        candidates.push_back({"Smart Light Bridge", 80});
    }
    // Ubiquiti
    if (vendorLower.find("ubiquiti") != std::string::npos) {
        candidates.push_back({"Network Equipment (Ubiquiti)", 80});
    }
    // Generic with ports
    if (hasPort(80) || hasPort(443)) {
        candidates.push_back({"Network Device", 20});
    }

    // Hostname hints
    std::string hostLower = dev.hostname;
    std::transform(hostLower.begin(), hostLower.end(), hostLower.begin(), ::tolower);
    if (hostLower.find("router") != std::string::npos || hostLower.find("gateway") != std::string::npos)
        candidates.push_back({"Router/Gateway", 55});
    if (hostLower.find("printer") != std::string::npos)
        candidates.push_back({"Printer", 60});
    if (hostLower.find("nas") != std::string::npos)
        candidates.push_back({"NAS", 55});
    if (hostLower.find("camera") != std::string::npos || hostLower.find("cam") != std::string::npos)
        candidates.push_back({"Camera/DVR", 55});

    // Sort by score descending
    std::sort(candidates.begin(), candidates.end(),
              [](auto& a, auto& b) { return a.score > b.score; });

    // Merge duplicates (take highest)
    std::map<std::string, int> merged;
    for (auto& c : candidates) {
        merged[c.type] = std::max(merged[c.type], c.score);
    }
    candidates.clear();
    for (auto& [t, s] : merged) candidates.push_back({t, s});
    std::sort(candidates.begin(), candidates.end(),
              [](auto& a, auto& b) { return a.score > b.score; });

    if (!candidates.empty()) {
        type = candidates[0].type;
        score = candidates[0].score;
    }
    if (candidates.size() > 1) {
        alt1 = candidates[1].type;
        alt1c = candidates[1].score;
    }
    if (candidates.size() > 2) {
        alt2 = candidates[2].type;
        alt2c = candidates[2].score;
    }

    // Don't overwrite VM detection
    if (!dev.isVM && !dev.isHypervisor) {
        dev.deviceType = type;
        dev.confidence = std::min(score, 100);
    }
    dev.altType1 = alt1;
    dev.altConf1 = alt1c;
    dev.altType2 = alt2;
    dev.altConf2 = alt2c;
}

void ScanEngine::assessIoTRisk(Device& dev) {
    std::vector<std::string> risks;
    for (int p : dev.openPorts) {
        if (p == 23) risks.push_back("Telnet (unencrypted remote access)");
        if (p == 1883) risks.push_back("MQTT (IoT without TLS)");
        if (p == 5900) risks.push_back("VNC (unencrypted screen sharing)");
        if (p == 135 || p == 139) risks.push_back("NetBIOS/MS-RPC (lateral movement)");
    }
    // Risky combos
    auto has = [&](int p) { return std::find(dev.openPorts.begin(), dev.openPorts.end(), p) != dev.openPorts.end(); };
    if (has(23) && has(80)) risks.push_back("Telnet+HTTP combo (likely default credentials)");
    if (has(445) && has(3389)) risks.push_back("SMB+RDP combo (high attack surface)");

    if (!risks.empty()) {
        dev.iotRisk = true;
        std::string detail;
        for (auto& r : risks) {
            if (!detail.empty()) detail += "; ";
            detail += r;
        }
        dev.iotRiskDetail = detail;
    }
}

void ScanEngine::generateAnomalies(ScanResult& result, const std::vector<Device>& prev) {
    std::map<std::string, const Device*> prevMap;
    for (auto& d : prev) {
        std::string key = d.mac.empty() ? d.ip : d.mac;
        prevMap[key] = &d;
    }

    for (auto& dev : result.devices) {
        std::string key = dev.mac.empty() ? dev.ip : dev.mac;

        if (prevMap.find(key) == prevMap.end()) {
            Anomaly a;
            a.type = "new_device";
            a.severity = "medium";
            a.deviceIp = dev.ip;
            a.description = "New device discovered: " + dev.ip + " (" + dev.vendor + ")";
            a.explanation = "An unknown device has joined your network. This could be a legitimate new device or unauthorized access.";
            a.remediation = "Identify the device owner and set trust state. If unknown, consider blocking.";
            a.category = "inventory";
            result.anomalies.push_back(a);
        }

        // Risky ports
        for (int p : dev.openPorts) {
            if (std::find(RISKY_PORTS.begin(), RISKY_PORTS.end(), p) != RISKY_PORTS.end()) {
                Anomaly a;
                a.type = "risky_port";
                a.severity = "high";
                a.deviceIp = dev.ip;
                auto it = PORT_NAMES.find(p);
                std::string pname = it != PORT_NAMES.end() ? it->second : std::to_string(p);
                a.description = "Risky port " + std::to_string(p) + " (" + pname + ") open on " + dev.ip;
                a.explanation = "This port exposes a service that may accept unencrypted credentials or allow lateral movement.";
                a.remediation = "Disable the service if not needed, or restrict access with a firewall.";
                a.category = "exposure";
                a.affectedPorts = {p};
                result.anomalies.push_back(a);
            }
        }

        // VM/Hypervisor detection anomaly
        if (dev.isVM) {
            Anomaly a;
            a.type = "vm_detected";
            a.severity = "low";
            a.deviceIp = dev.ip;
            a.description = "Virtual machine detected: " + dev.ip + " (" + dev.deviceType + ")";
            a.explanation = "This device appears to be a virtual machine based on MAC address and port analysis.";
            a.remediation = "Verify this VM is authorized. Ensure hypervisor is patched and properly secured.";
            a.category = "virtualization";
            result.anomalies.push_back(a);
        }
        if (dev.isHypervisor) {
            Anomaly a;
            a.type = "hypervisor_detected";
            a.severity = "medium";
            a.deviceIp = dev.ip;
            a.description = "Hypervisor host detected: " + dev.ip + " (" + dev.deviceType + ")";
            a.explanation = "A hypervisor management interface is accessible on this device. Compromise could affect all hosted VMs.";
            a.remediation = "Restrict management port access to admin VLANs. Enable strong authentication.";
            a.category = "virtualization";
            result.anomalies.push_back(a);
        }
    }
}

ScanResult ScanEngine::doScan(const std::string& mode, const std::vector<int>& ports, ProgressCallback cb) {
    _cancelled = false;
    ScanResult result;
    result.mode = mode;

    if (cb) cb(0, "Discovering local networks...");
    auto nets = getLocalNetworks();
    if (nets.empty()) {
        result.scannedAt = nowTimestamp();
        if (cb) cb(100, "No networks found");
        return result;
    }

    if (cb) cb(5, "Reading ARP table...");
    // Trigger ARP population by pinging broadcast
    for (auto& net : nets) {
        execCommand("ping -c 1 -b -W 1 " + net.ip.substr(0, net.ip.rfind('.')) + ".255 2>/dev/null");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    if (cb) cb(10, "Scanning subnet...");

    std::map<std::string, Device> deviceMap;

    for (auto& net : nets) {
        auto ips = generateSubnetIPs(net.ip, net.prefix);
        auto arpTable = readArpTable();

        // Add ARP-known hosts
        for (auto& [ip, mac] : arpTable) {
            if (deviceMap.find(ip) == deviceMap.end()) {
                Device dev;
                dev.ip = ip;
                dev.mac = mac;
                dev.online = true;
                dev.firstSeen = nowTimestamp();
                dev.lastSeen = nowTimestamp();
                deviceMap[ip] = dev;
            }
        }

        int total = std::min((int)ips.size(), 254);
        int pingCount = 0;

        // Ping sweep (parallel)
        if (cb) cb(15, "Pinging " + std::to_string(total) + " hosts...");
        std::mutex mtx;
        std::vector<std::thread> threads;
        int active = 0;
        std::mutex activeMtx;
        std::condition_variable cv;
        const int maxPingThreads = 64;

        for (int i = 0; i < total && !_cancelled; i++) {
            {
                std::unique_lock<std::mutex> lk(activeMtx);
                cv.wait(lk, [&] { return active < maxPingThreads; });
                active++;
            }
            threads.emplace_back([&, i]() {
                bool alive = pingHost(ips[i], 1500);
                if (alive) {
                    std::lock_guard<std::mutex> lk(mtx);
                    if (deviceMap.find(ips[i]) == deviceMap.end()) {
                        Device dev;
                        dev.ip = ips[i];
                        dev.online = true;
                        dev.firstSeen = nowTimestamp();
                        dev.lastSeen = nowTimestamp();
                        deviceMap[ips[i]] = dev;
                    }
                }
                {
                    std::lock_guard<std::mutex> lk(activeMtx);
                    active--;
                    pingCount++;
                }
                cv.notify_one();
            });
        }
        for (auto& t : threads) t.join();

        // Re-read ARP table after pings
        auto arpAfter = readArpTable();
        for (auto& [ip, mac] : arpAfter) {
            if (deviceMap.count(ip)) {
                deviceMap[ip].mac = mac;
            }
        }
    }

    if (_cancelled) { result.scannedAt = nowTimestamp(); return result; }

    // Port scanning
    int devIdx = 0;
    int devTotal = (int)deviceMap.size();
    for (auto& [ip, dev] : deviceMap) {
        if (_cancelled) break;
        devIdx++;
        int pct = 30 + (devIdx * 50 / std::max(devTotal, 1));
        if (cb) cb(pct, "Scanning " + ip + " (" + std::to_string(devIdx) + "/" + std::to_string(devTotal) + ")");

        dev.openPorts = scanPorts(ip, ports, 800);
        dev.latencyMs = measureLatency(ip);
        dev.hostname = resolveHostname(ip);
        dev.vendor = lookupVendor(dev.mac);

        detectVM(dev);
        fingerprintDevice(dev);
        assessIoTRisk(dev);
    }

    // Build result
    if (cb) cb(90, "Analyzing results...");
    for (auto& [ip, dev] : deviceMap) {
        result.devices.push_back(dev);
    }

    // Sort: gateway first, then by IP
    auto gw = getDefaultGateway();
    std::sort(result.devices.begin(), result.devices.end(), [&](auto& a, auto& b) {
        if (a.ip == gw) return true;
        if (b.ip == gw) return false;
        return a.ip < b.ip;
    });

    generateAnomalies(result, {});
    result.scannedAt = nowTimestamp();

    if (cb) cb(100, "Scan complete: " + std::to_string(result.devices.size()) + " devices found");
    return result;
}

std::future<ScanResult> ScanEngine::QuickScan(ProgressCallback cb) {
    return std::async(std::launch::async, [this, cb]() {
        return doScan("quick", QUICK_PORTS, cb);
    });
}

std::future<ScanResult> ScanEngine::StandardScan(ProgressCallback cb) {
    return std::async(std::launch::async, [this, cb]() {
        return doScan("standard", COMMON_PORTS, cb);
    });
}

std::future<ScanResult> ScanEngine::DeepScan(ProgressCallback cb) {
    std::vector<int> deepPorts;
    for (int i = 1; i <= 1024; i++) deepPorts.push_back(i);
    return std::async(std::launch::async, [this, cb, deepPorts]() {
        return doScan("deep", deepPorts, cb);
    });
}
