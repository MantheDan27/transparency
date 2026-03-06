#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QGridLayout>
#include "devicemodel.h"
#include "alertmanager.h"

class OverviewPage : public QWidget {
    Q_OBJECT
public:
    explicit OverviewPage(DeviceModel *deviceModel, AlertManager *alertManager, QWidget *parent = nullptr);
    void refresh();

private:
    QWidget *createStatCard(const QString &title, const QString &value, const QColor &color);

    DeviceModel *m_deviceModel;
    AlertManager *m_alertManager;
    QLabel *m_totalDevicesLabel;
    QLabel *m_onlineLabel;
    QLabel *m_riskyLabel;
    QLabel *m_alertsLabel;
    QVBoxLayout *m_recentAlertsLayout;
};
