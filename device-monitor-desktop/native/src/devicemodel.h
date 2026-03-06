#pragma once

#include <QAbstractTableModel>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <vector>

struct Device {
    QString ip;
    QString mac;
    QString hostname;
    QString vendor;
    QString deviceType;
    QList<int> openPorts;
    QList<int> riskyPorts;
    bool isOnline = false;
    QDateTime firstSeen;
    QDateTime lastSeen;
    QString notes;
    int trustLevel = 50; // 0-100
};

class DeviceModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColStatus = 0,
        ColIP,
        ColMAC,
        ColHostname,
        ColVendor,
        ColType,
        ColPorts,
        ColRisk,
        ColLastSeen,
        ColCount
    };

    explicit DeviceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setDevices(const QList<Device> &devices);
    void addOrUpdateDevice(const Device &device);
    void clear();
    const Device &deviceAt(int row) const;
    QList<Device> allDevices() const;

    int onlineCount() const;
    int riskyCount() const;
    int totalCount() const;

    void saveToFile() const;
    void loadFromFile();

signals:
    void deviceAdded(const Device &device);
    void deviceUpdated(const Device &device);
    void devicesChanged();

private:
    QList<Device> m_devices;
    QString dataFilePath() const;
    int findDeviceByMac(const QString &mac) const;
};
