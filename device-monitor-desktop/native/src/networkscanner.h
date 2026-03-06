#pragma once

#include <QObject>
#include <QThread>
#include <QTcpSocket>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QProcess>
#include <QMap>
#include "devicemodel.h"

class NetworkScanner : public QObject {
    Q_OBJECT
public:
    explicit NetworkScanner(QObject *parent = nullptr);

    void scanSubnet(const QString &subnet);
    void scanHost(const QString &ip);
    void stop();
    bool isScanning() const;

    static QString detectLocalSubnet();
    static QStringList commonPorts();

signals:
    void scanStarted();
    void scanProgress(int current, int total);
    void deviceFound(const Device &device);
    void scanFinished(int devicesFound);
    void scanError(const QString &error);

private:
    void scanPortsForHost(const QString &ip);
    QString lookupVendor(const QString &mac) const;
    QString classifyDevice(const Device &dev) const;
    QString resolveHostname(const QString &ip) const;
    QString getMacAddress(const QString &ip) const;

    bool m_scanning = false;
    bool m_stopRequested = false;
    int m_scannedHosts = 0;
    int m_totalHosts = 0;
    int m_devicesFound = 0;

    static const QList<int> SCAN_PORTS;
    static const QList<int> RISKY_PORTS;
    static const QMap<QString, QString> OUI_DATABASE;
};
