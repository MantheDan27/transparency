#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include "alertmanager.h"

class AlertsPage : public QWidget {
    Q_OBJECT
public:
    explicit AlertsPage(AlertManager *alertManager, QWidget *parent = nullptr);
    void refresh();

private:
    AlertManager *m_alertManager;
    QVBoxLayout *m_alertsLayout;
};
