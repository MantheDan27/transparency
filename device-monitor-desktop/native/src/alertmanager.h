#pragma once

#include <QObject>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QStandardPaths>
#include <QDir>

struct Alert {
    QString id;
    QString type;      // "new_device", "risky_port", "device_offline", "port_change"
    QString severity;   // "info", "warning", "critical"
    QString title;
    QString message;
    QString deviceIp;
    QString deviceMac;
    QDateTime timestamp;
    bool acknowledged = false;
};

class AlertManager : public QObject {
    Q_OBJECT
public:
    explicit AlertManager(QObject *parent = nullptr);

    void addAlert(const Alert &alert);
    void acknowledgeAlert(const QString &id);
    void acknowledgeAll();
    void clearAll();

    QList<Alert> allAlerts() const;
    QList<Alert> unacknowledgedAlerts() const;
    int unacknowledgedCount() const;

    // Alert generators
    void checkNewDevice(const QString &ip, const QString &mac, const QString &vendor);
    void checkRiskyPorts(const QString &ip, const QList<int> &riskyPorts);
    void checkDeviceOffline(const QString &ip, const QString &mac);

    void saveToFile() const;
    void loadFromFile();

signals:
    void alertAdded(const Alert &alert);
    void alertsChanged();

private:
    QList<Alert> m_alerts;
    int m_nextId = 1;
    QString dataFilePath() const;
};
