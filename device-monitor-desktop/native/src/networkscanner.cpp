#include "networkscanner.h"
#include <QEventLoop>
#include <QTimer>
#include <QRegularExpression>

const QList<int> NetworkScanner::SCAN_PORTS = {
    21, 22, 23, 25, 53, 80, 110, 135, 139, 143, 443, 445, 993, 995,
    3306, 3389, 5432, 5900, 8080, 8443, 8888,
    // IoT ports
    1883, 5683, 8883, 1900, 49152,
    // NAS ports
    548, 873, 2049, 5000, 9000,
    // Security ports
    554, 8000, 8081, 37777
};

const QList<int> NetworkScanner::RISKY_PORTS = {
    23, 135, 139, 445, 3389, 5900, 21, 25, 110
};

const QMap<QString, QString> NetworkScanner::OUI_DATABASE = {
    {"00:50:56", "VMware"}, {"00:0C:29", "VMware"}, {"00:1C:42", "Parallels"},
    {"08:00:27", "VirtualBox"}, {"00:15:5D", "Hyper-V"},
    {"AC:DE:48", "Apple"}, {"A8:5C:2C", "Apple"}, {"3C:22:FB", "Apple"},
    {"00:25:00", "Apple"}, {"F0:18:98", "Apple"},
    {"B8:27:EB", "Raspberry Pi"}, {"DC:A6:32", "Raspberry Pi"},
    {"30:AE:A4", "Espressif"}, {"24:6F:28", "Espressif"},
    {"00:1A:22", "Cisco"}, {"00:1B:54", "Cisco"},
    {"00:04:4B", "Nvidia"}, {"48:B0:2D", "Nvidia"},
    {"B0:A7:37", "Roku"}, {"AC:3A:7A", "Roku"},
    {"F0:72:EA", "Google"}, {"54:60:09", "Google"},
    {"FC:65:DE", "Amazon"}, {"A4:08:EA", "Amazon"},
    {"44:07:0B", "Google Nest"}, {"18:B4:30", "Nest Labs"},
    {"50:C7:BF", "TP-Link"}, {"60:32:B1", "TP-Link"},
    {"00:18:E7", "Cameo"}, {"9C:8E:CD", "Amcrest"},
    {"28:6D:97", "Samsung"}, {"5C:3A:45", "Samsung"},
    {"00:17:88", "Philips Hue"}, {"EC:B5:FA", "Philips Hue"},
    {"70:B3:D5", "Ring"}, {"34:2D:0D", "Ring"},
};

NetworkScanner::NetworkScanner(QObject *parent)
    : QObject(parent)
{
}

QString NetworkScanner::detectLocalSubnet()
{
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp))
            continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning))
            continue;

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                QString ip = entry.ip().toString();
                if (ip.startsWith("192.168.") || ip.startsWith("10.") || ip.startsWith("172.")) {
                    // Return subnet base (e.g., "192.168.1")
                    QStringList parts = ip.split('.');
                    if (parts.size() == 4) {
                        return parts[0] + "." + parts[1] + "." + parts[2];
                    }
                }
            }
        }
    }
    return "192.168.1";
}

void NetworkScanner::scanSubnet(const QString &subnet)
{
    if (m_scanning) return;

    m_scanning = true;
    m_stopRequested = false;
    m_scannedHosts = 0;
    m_totalHosts = 254;
    m_devicesFound = 0;
    emit scanStarted();

    for (int i = 1; i <= 254 && !m_stopRequested; ++i) {
        QString ip = subnet + "." + QString::number(i);
        scanHost(ip);
        m_scannedHosts++;
        emit scanProgress(m_scannedHosts, m_totalHosts);
    }

    m_scanning = false;
    emit scanFinished(m_devicesFound);
}

void NetworkScanner::scanHost(const QString &ip)
{
    // Quick ping check using TCP connect to common ports
    bool reachable = false;
    QList<int> openPorts;
    QList<int> riskyPorts;

    for (int port : SCAN_PORTS) {
        if (m_stopRequested) break;

        QTcpSocket socket;
        socket.connectToHost(ip, port);

        if (socket.waitForConnected(150)) {
            reachable = true;
            openPorts.append(port);
            if (RISKY_PORTS.contains(port)) {
                riskyPorts.append(port);
            }
            socket.disconnectFromHost();
        }
    }

    if (reachable) {
        Device dev;
        dev.ip = ip;
        dev.mac = getMacAddress(ip);
        dev.hostname = resolveHostname(ip);
        dev.vendor = lookupVendor(dev.mac);
        dev.openPorts = openPorts;
        dev.riskyPorts = riskyPorts;
        dev.isOnline = true;
        dev.firstSeen = QDateTime::currentDateTime();
        dev.lastSeen = QDateTime::currentDateTime();
        dev.deviceType = classifyDevice(dev);

        m_devicesFound++;
        emit deviceFound(dev);
    }
}

void NetworkScanner::stop()
{
    m_stopRequested = true;
}

bool NetworkScanner::isScanning() const
{
    return m_scanning;
}

QString NetworkScanner::getMacAddress(const QString &ip) const
{
    QProcess proc;
    proc.start("arp", QStringList() << "-n" << ip);
    proc.waitForFinished(2000);
    QString output = proc.readAllStandardOutput();

    QRegularExpression macRe("([0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2})");
    auto match = macRe.match(output);
    if (match.hasMatch()) {
        return match.captured(1).toUpper();
    }
    return "";
}

QString NetworkScanner::resolveHostname(const QString &ip) const
{
    QHostInfo info = QHostInfo::fromName(ip);
    if (info.error() == QHostInfo::NoError && !info.hostName().isEmpty()
        && info.hostName() != ip) {
        return info.hostName();
    }
    return "";
}

QString NetworkScanner::lookupVendor(const QString &mac) const
{
    if (mac.isEmpty()) return "";

    QString prefix = mac.left(8).toUpper();
    if (OUI_DATABASE.contains(prefix)) {
        return OUI_DATABASE[prefix];
    }
    return "";
}

QString NetworkScanner::classifyDevice(const Device &dev) const
{
    // Classify by vendor
    QString vendor = dev.vendor.toLower();
    if (vendor.contains("apple")) return "Computer/Phone";
    if (vendor.contains("samsung")) return "Phone/TV";
    if (vendor.contains("raspberry")) return "IoT/SBC";
    if (vendor.contains("espressif")) return "IoT Sensor";
    if (vendor.contains("vmware") || vendor.contains("virtualbox")
        || vendor.contains("hyper-v") || vendor.contains("parallels"))
        return "Virtual Machine";
    if (vendor.contains("roku")) return "Streaming Device";
    if (vendor.contains("google") || vendor.contains("nest")) return "Smart Home";
    if (vendor.contains("amazon")) return "Smart Speaker/IoT";
    if (vendor.contains("ring")) return "Security Camera";
    if (vendor.contains("philips") || vendor.contains("hue")) return "Smart Lighting";
    if (vendor.contains("tp-link")) return "Network Equipment";
    if (vendor.contains("cisco")) return "Network Equipment";
    if (vendor.contains("amcrest") || vendor.contains("cameo")) return "IP Camera";

    // Classify by ports
    if (dev.openPorts.contains(80) || dev.openPorts.contains(443)) {
        if (dev.openPorts.contains(22)) return "Server";
        if (dev.openPorts.contains(554)) return "IP Camera";
        if (dev.openPorts.contains(631)) return "Printer";
        return "Web Device";
    }
    if (dev.openPorts.contains(548) || dev.openPorts.contains(5000)) return "NAS";
    if (dev.openPorts.contains(3389)) return "Windows PC";
    if (dev.openPorts.contains(5900)) return "Computer (VNC)";
    if (dev.openPorts.contains(22)) return "Linux/Unix";
    if (dev.openPorts.contains(1883) || dev.openPorts.contains(8883)) return "IoT Device";

    return "Unknown";
}

QStringList NetworkScanner::commonPorts()
{
    return {
        "21 - FTP", "22 - SSH", "23 - Telnet", "25 - SMTP", "53 - DNS",
        "80 - HTTP", "110 - POP3", "135 - MS-RPC", "139 - NetBIOS",
        "143 - IMAP", "443 - HTTPS", "445 - SMB", "993 - IMAPS",
        "995 - POP3S", "3306 - MySQL", "3389 - RDP", "5432 - PostgreSQL",
        "5900 - VNC", "8080 - HTTP Alt", "8443 - HTTPS Alt"
    };
}
