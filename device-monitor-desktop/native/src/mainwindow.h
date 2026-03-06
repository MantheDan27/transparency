#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QLabel>
#include <QProgressBar>
#include <QStatusBar>
#include <QThread>
#include "devicemodel.h"
#include "networkscanner.h"
#include "alertmanager.h"
#include "overviewpage.h"
#include "devicespage.h"
#include "alertspage.h"
#include "toolspage.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartScan();
    void onStopScan();
    void onDeviceFound(const Device &device);
    void onScanFinished(int devicesFound);
    void onScanProgress(int current, int total);
    void onPageChanged(int index);

private:
    void setupUI();
    void setupSidebar();
    void setupStatusBar();
    void refreshAll();

    // Core components
    DeviceModel *m_deviceModel;
    NetworkScanner *m_scanner;
    AlertManager *m_alertManager;
    QThread *m_scanThread;

    // UI
    QStackedWidget *m_pageStack;
    QListWidget *m_navList;
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;
    QPushButton *m_scanButton;

    // Pages
    OverviewPage *m_overviewPage;
    DevicesPage *m_devicesPage;
    AlertsPage *m_alertsPage;
    ToolsPage *m_toolsPage;
};
