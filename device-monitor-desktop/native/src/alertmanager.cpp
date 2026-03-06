#include "alertmanager.h"
#include <QJsonDocument>
#include <QUuid>

AlertManager::AlertManager(QObject *parent)
    : QObject(parent)
{
    loadFromFile();
}

void AlertManager::addAlert(const Alert &alert)
{
    m_alerts.prepend(alert);
    emit alertAdded(alert);
    emit alertsChanged();
    saveToFile();
}

void AlertManager::acknowledgeAlert(const QString &id)
{
    for (auto &a : m_alerts) {
        if (a.id == id) {
            a.acknowledged = true;
            break;
        }
    }
    emit alertsChanged();
    saveToFile();
}

void AlertManager::acknowledgeAll()
{
    for (auto &a : m_alerts)
        a.acknowledged = true;
    emit alertsChanged();
    saveToFile();
}

void AlertManager::clearAll()
{
    m_alerts.clear();
    emit alertsChanged();
    saveToFile();
}

QList<Alert> AlertManager::allAlerts() const
{
    return m_alerts;
}

QList<Alert> AlertManager::unacknowledgedAlerts() const
{
    QList<Alert> result;
    for (const auto &a : m_alerts)
        if (!a.acknowledged) result.append(a);
    return result;
}

int AlertManager::unacknowledgedCount() const
{
    int count = 0;
    for (const auto &a : m_alerts)
        if (!a.acknowledged) ++count;
    return count;
}

void AlertManager::checkNewDevice(const QString &ip, const QString &mac, const QString &vendor)
{
    Alert alert;
    alert.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    alert.type = "new_device";
    alert.severity = "info";
    alert.title = "New Device Detected";
    alert.message = QString("New device found at %1 (%2) - %3")
                        .arg(ip, mac, vendor.isEmpty() ? "Unknown vendor" : vendor);
    alert.deviceIp = ip;
    alert.deviceMac = mac;
    alert.timestamp = QDateTime::currentDateTime();
    addAlert(alert);
}

void AlertManager::checkRiskyPorts(const QString &ip, const QList<int> &riskyPorts)
{
    if (riskyPorts.isEmpty()) return;

    QStringList portNames;
    QMap<int, QString> portLabels = {
        {23, "Telnet"}, {135, "MS-RPC"}, {139, "NetBIOS"}, {445, "SMB"},
        {3389, "RDP"}, {5900, "VNC"}, {21, "FTP"}, {25, "SMTP"}, {110, "POP3"}
    };

    for (int p : riskyPorts) {
        portNames << QString("%1 (%2)").arg(p).arg(portLabels.value(p, "Unknown"));
    }

    Alert alert;
    alert.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    alert.type = "risky_port";
    alert.severity = "critical";
    alert.title = "Risky Ports Detected";
    alert.message = QString("Device %1 has risky open ports: %2")
                        .arg(ip, portNames.join(", "));
    alert.deviceIp = ip;
    alert.timestamp = QDateTime::currentDateTime();
    addAlert(alert);
}

void AlertManager::checkDeviceOffline(const QString &ip, const QString &mac)
{
    Alert alert;
    alert.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    alert.type = "device_offline";
    alert.severity = "warning";
    alert.title = "Device Went Offline";
    alert.message = QString("Device %1 (%2) is no longer responding").arg(ip, mac);
    alert.deviceIp = ip;
    alert.deviceMac = mac;
    alert.timestamp = QDateTime::currentDateTime();
    addAlert(alert);
}

QString AlertManager::dataFilePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/alerts.json";
}

void AlertManager::saveToFile() const
{
    QJsonArray arr;
    for (const auto &a : m_alerts) {
        QJsonObject obj;
        obj["id"] = a.id;
        obj["type"] = a.type;
        obj["severity"] = a.severity;
        obj["title"] = a.title;
        obj["message"] = a.message;
        obj["deviceIp"] = a.deviceIp;
        obj["deviceMac"] = a.deviceMac;
        obj["timestamp"] = a.timestamp.toString(Qt::ISODate);
        obj["acknowledged"] = a.acknowledged;
        arr.append(obj);
    }
    QFile file(dataFilePath());
    if (file.open(QIODevice::WriteOnly))
        file.write(QJsonDocument(arr).toJson());
}

void AlertManager::loadFromFile()
{
    QFile file(dataFilePath());
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
    for (const auto &val : arr) {
        QJsonObject obj = val.toObject();
        Alert a;
        a.id = obj["id"].toString();
        a.type = obj["type"].toString();
        a.severity = obj["severity"].toString();
        a.title = obj["title"].toString();
        a.message = obj["message"].toString();
        a.deviceIp = obj["deviceIp"].toString();
        a.deviceMac = obj["deviceMac"].toString();
        a.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        a.acknowledged = obj["acknowledged"].toBool();
        m_alerts.append(a);
    }
}
