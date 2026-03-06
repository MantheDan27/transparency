#include "devicemodel.h"
#include <QJsonDocument>
#include <QColor>

DeviceModel::DeviceModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    loadFromFile();
}

int DeviceModel::rowCount(const QModelIndex &) const
{
    return m_devices.size();
}

int DeviceModel::columnCount(const QModelIndex &) const
{
    return ColCount;
}

QVariant DeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_devices.size())
        return {};

    const Device &dev = m_devices[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColStatus: return dev.isOnline ? "ONLINE" : "OFFLINE";
        case ColIP: return dev.ip;
        case ColMAC: return dev.mac;
        case ColHostname: return dev.hostname.isEmpty() ? "—" : dev.hostname;
        case ColVendor: return dev.vendor.isEmpty() ? "Unknown" : dev.vendor;
        case ColType: return dev.deviceType.isEmpty() ? "Unknown" : dev.deviceType;
        case ColPorts: {
            QStringList ports;
            for (int p : dev.openPorts) ports << QString::number(p);
            return ports.join(", ");
        }
        case ColRisk: return dev.riskyPorts.isEmpty() ? "Low" : "HIGH";
        case ColLastSeen: return dev.lastSeen.toString("yyyy-MM-dd hh:mm");
        }
    }

    if (role == Qt::ForegroundRole) {
        if (index.column() == ColStatus) {
            return dev.isOnline ? QColor(0x00, 0xe5, 0x76) : QColor(0x55, 0x5a, 0x68);
        }
        if (index.column() == ColRisk && !dev.riskyPorts.isEmpty()) {
            return QColor(0xff, 0x40, 0x60);
        }
    }

    if (role == Qt::BackgroundRole && !dev.riskyPorts.isEmpty()) {
        if (index.column() == ColRisk)
            return QColor(0xff, 0x40, 0x60, 0x20);
    }

    return {};
}

QVariant DeviceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case ColStatus: return "Status";
    case ColIP: return "IP Address";
    case ColMAC: return "MAC Address";
    case ColHostname: return "Hostname";
    case ColVendor: return "Vendor";
    case ColType: return "Type";
    case ColPorts: return "Open Ports";
    case ColRisk: return "Risk";
    case ColLastSeen: return "Last Seen";
    }
    return {};
}

void DeviceModel::setDevices(const QList<Device> &devices)
{
    beginResetModel();
    m_devices = devices;
    endResetModel();
    emit devicesChanged();
}

void DeviceModel::addOrUpdateDevice(const Device &device)
{
    int idx = findDeviceByMac(device.mac);
    if (idx >= 0) {
        m_devices[idx].ip = device.ip;
        m_devices[idx].isOnline = device.isOnline;
        m_devices[idx].openPorts = device.openPorts;
        m_devices[idx].riskyPorts = device.riskyPorts;
        m_devices[idx].lastSeen = device.lastSeen;
        if (!device.hostname.isEmpty())
            m_devices[idx].hostname = device.hostname;
        if (!device.vendor.isEmpty())
            m_devices[idx].vendor = device.vendor;
        if (!device.deviceType.isEmpty())
            m_devices[idx].deviceType = device.deviceType;
        emit dataChanged(index(idx, 0), index(idx, ColCount - 1));
        emit deviceUpdated(m_devices[idx]);
    } else {
        beginInsertRows(QModelIndex(), m_devices.size(), m_devices.size());
        m_devices.append(device);
        endInsertRows();
        emit deviceAdded(device);
    }
    emit devicesChanged();
    saveToFile();
}

void DeviceModel::clear()
{
    beginResetModel();
    m_devices.clear();
    endResetModel();
    emit devicesChanged();
}

const Device &DeviceModel::deviceAt(int row) const
{
    return m_devices[row];
}

QList<Device> DeviceModel::allDevices() const
{
    return m_devices;
}

int DeviceModel::onlineCount() const
{
    int count = 0;
    for (const auto &d : m_devices)
        if (d.isOnline) ++count;
    return count;
}

int DeviceModel::riskyCount() const
{
    int count = 0;
    for (const auto &d : m_devices)
        if (!d.riskyPorts.isEmpty()) ++count;
    return count;
}

int DeviceModel::totalCount() const
{
    return m_devices.size();
}

int DeviceModel::findDeviceByMac(const QString &mac) const
{
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].mac.compare(mac, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

QString DeviceModel::dataFilePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/devices.json";
}

void DeviceModel::saveToFile() const
{
    QJsonArray arr;
    for (const auto &dev : m_devices) {
        QJsonObject obj;
        obj["ip"] = dev.ip;
        obj["mac"] = dev.mac;
        obj["hostname"] = dev.hostname;
        obj["vendor"] = dev.vendor;
        obj["deviceType"] = dev.deviceType;
        obj["isOnline"] = dev.isOnline;
        obj["firstSeen"] = dev.firstSeen.toString(Qt::ISODate);
        obj["lastSeen"] = dev.lastSeen.toString(Qt::ISODate);
        obj["notes"] = dev.notes;
        obj["trustLevel"] = dev.trustLevel;

        QJsonArray ports, risky;
        for (int p : dev.openPorts) ports.append(p);
        for (int p : dev.riskyPorts) risky.append(p);
        obj["openPorts"] = ports;
        obj["riskyPorts"] = risky;

        arr.append(obj);
    }
    QFile file(dataFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson());
    }
}

void DeviceModel::loadFromFile()
{
    QFile file(dataFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
    for (const auto &val : arr) {
        QJsonObject obj = val.toObject();
        Device dev;
        dev.ip = obj["ip"].toString();
        dev.mac = obj["mac"].toString();
        dev.hostname = obj["hostname"].toString();
        dev.vendor = obj["vendor"].toString();
        dev.deviceType = obj["deviceType"].toString();
        dev.isOnline = obj["isOnline"].toBool();
        dev.firstSeen = QDateTime::fromString(obj["firstSeen"].toString(), Qt::ISODate);
        dev.lastSeen = QDateTime::fromString(obj["lastSeen"].toString(), Qt::ISODate);
        dev.notes = obj["notes"].toString();
        dev.trustLevel = obj["trustLevel"].toInt(50);

        for (const auto &p : obj["openPorts"].toArray())
            dev.openPorts.append(p.toInt());
        for (const auto &p : obj["riskyPorts"].toArray())
            dev.riskyPorts.append(p.toInt());

        m_devices.append(dev);
    }
}
