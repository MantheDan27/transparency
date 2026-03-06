#include "mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSplitter>
#include <QFrame>
#include <QMessageBox>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_deviceModel = new DeviceModel(this);
    m_alertManager = new AlertManager(this);
    m_scanner = new NetworkScanner();
    m_scanThread = new QThread(this);
    m_scanner->moveToThread(m_scanThread);
    m_scanThread->start();

    setupUI();
    setupStatusBar();

    // Scanner connections (cross-thread)
    connect(m_scanner, &NetworkScanner::deviceFound, this, &MainWindow::onDeviceFound, Qt::QueuedConnection);
    connect(m_scanner, &NetworkScanner::scanFinished, this, &MainWindow::onScanFinished, Qt::QueuedConnection);
    connect(m_scanner, &NetworkScanner::scanProgress, this, &MainWindow::onScanProgress, Qt::QueuedConnection);
    connect(m_scanner, &NetworkScanner::scanStarted, this, [this]() {
        m_statusLabel->setText("Scanning network...");
        m_progressBar->setVisible(true);
        m_scanButton->setText("STOP SCAN");
    }, Qt::QueuedConnection);

    // Refresh on data changes
    connect(m_deviceModel, &DeviceModel::devicesChanged, this, &MainWindow::refreshAll);
    connect(m_alertManager, &AlertManager::alertsChanged, this, &MainWindow::refreshAll);

    setWindowTitle("Device Monitor Desktop v3.4.0");
    resize(1200, 800);
    setMinimumSize(900, 600);
}

MainWindow::~MainWindow()
{
    m_scanner->stop();
    m_scanThread->quit();
    m_scanThread->wait();
    delete m_scanner;
}

void MainWindow::setupUI()
{
    auto *centralWidget = new QWidget();
    auto *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Sidebar
    auto *sidebar = new QFrame();
    sidebar->setFixedWidth(220);
    sidebar->setStyleSheet(
        "QFrame { background: #0f1219; border-right: 1px solid #1e2538; }"
    );

    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    // App title in sidebar
    auto *appTitle = new QLabel("DEVICE MONITOR");
    appTitle->setStyleSheet(
        "font-size: 13px; font-weight: bold; color: #3d7fff; letter-spacing: 2px;"
        "padding: 20px 16px 8px 16px;"
    );
    sidebarLayout->addWidget(appTitle);

    auto *appVersion = new QLabel("v3.4.0 — Native C++");
    appVersion->setStyleSheet("font-size: 9px; color: #3a3f4a; padding: 0 16px 16px 16px;");
    sidebarLayout->addWidget(appVersion);

    // Separator
    auto *sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("background: #1e2538; max-height: 1px;");
    sidebarLayout->addWidget(sep);

    // Navigation
    m_navList = new QListWidget();
    m_navList->addItem("  Overview");
    m_navList->addItem("  Devices");
    m_navList->addItem("  Alerts");
    m_navList->addItem("  Tools");
    m_navList->setCurrentRow(0);
    m_navList->setStyleSheet(
        "QListWidget {"
        "  background: transparent; border: none; outline: none; padding: 8px 0;"
        "}"
        "QListWidget::item {"
        "  color: #555a68; font-size: 12px; font-weight: bold;"
        "  padding: 10px 16px; border: none; letter-spacing: 1px;"
        "}"
        "QListWidget::item:selected {"
        "  color: #c8ceda; background: #161b28; border-left: 3px solid #3d7fff;"
        "}"
        "QListWidget::item:hover:!selected {"
        "  color: #8a8f9a; background: #111520;"
        "}"
    );
    connect(m_navList, &QListWidget::currentRowChanged, this, &MainWindow::onPageChanged);
    sidebarLayout->addWidget(m_navList);

    sidebarLayout->addStretch();

    // Scan button at bottom of sidebar
    m_scanButton = new QPushButton("SCAN NETWORK");
    m_scanButton->setStyleSheet(
        "QPushButton {"
        "  background: #3d7fff; color: white; border: none; border-radius: 6px;"
        "  padding: 12px; font-weight: bold; font-size: 12px; letter-spacing: 1px;"
        "  margin: 16px;"
        "}"
        "QPushButton:hover { background: #5a94ff; }"
        "QPushButton:pressed { background: #2a6aee; }"
    );
    connect(m_scanButton, &QPushButton::clicked, [this]() {
        if (m_scanner->isScanning()) {
            onStopScan();
        } else {
            onStartScan();
        }
    });
    sidebarLayout->addWidget(m_scanButton);

    mainLayout->addWidget(sidebar);

    // Page stack
    m_pageStack = new QStackedWidget();
    m_overviewPage = new OverviewPage(m_deviceModel, m_alertManager);
    m_devicesPage = new DevicesPage(m_deviceModel);
    m_alertsPage = new AlertsPage(m_alertManager);
    m_toolsPage = new ToolsPage();

    m_pageStack->addWidget(m_overviewPage);
    m_pageStack->addWidget(m_devicesPage);
    m_pageStack->addWidget(m_alertsPage);
    m_pageStack->addWidget(m_toolsPage);

    mainLayout->addWidget(m_pageStack, 1);

    setCentralWidget(centralWidget);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("Ready");
    m_statusLabel->setStyleSheet("color: #555a68; font-size: 11px; padding: 0 8px;");

    m_progressBar = new QProgressBar();
    m_progressBar->setFixedWidth(200);
    m_progressBar->setFixedHeight(12);
    m_progressBar->setVisible(false);
    m_progressBar->setStyleSheet(
        "QProgressBar {"
        "  background: #161b28; border: none; border-radius: 6px;"
        "}"
        "QProgressBar::chunk {"
        "  background: #3d7fff; border-radius: 6px;"
        "}"
    );

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_progressBar);
    statusBar()->setStyleSheet("QStatusBar { background: #0b0e14; border-top: 1px solid #1e2538; }");
}

void MainWindow::onStartScan()
{
    QString subnet = NetworkScanner::detectLocalSubnet();
    m_statusLabel->setText("Starting scan on " + subnet + ".0/24...");

    QMetaObject::invokeMethod(m_scanner, [this, subnet]() {
        m_scanner->scanSubnet(subnet);
    }, Qt::QueuedConnection);
}

void MainWindow::onStopScan()
{
    m_scanner->stop();
    m_statusLabel->setText("Scan stopped");
    m_progressBar->setVisible(false);
    m_scanButton->setText("SCAN NETWORK");
}

void MainWindow::onDeviceFound(const Device &device)
{
    // Check if this is a new device
    bool isNew = true;
    for (const auto &d : m_deviceModel->allDevices()) {
        if (d.mac == device.mac || (device.mac.isEmpty() && d.ip == device.ip)) {
            isNew = false;
            break;
        }
    }

    m_deviceModel->addOrUpdateDevice(device);

    if (isNew) {
        m_alertManager->checkNewDevice(device.ip, device.mac, device.vendor);
    }

    if (!device.riskyPorts.isEmpty()) {
        m_alertManager->checkRiskyPorts(device.ip, device.riskyPorts);
    }
}

void MainWindow::onScanFinished(int devicesFound)
{
    m_statusLabel->setText(QString("Scan complete — %1 devices found").arg(devicesFound));
    m_progressBar->setVisible(false);
    m_scanButton->setText("SCAN NETWORK");
    refreshAll();
}

void MainWindow::onScanProgress(int current, int total)
{
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
    m_statusLabel->setText(QString("Scanning... %1/%2 hosts").arg(current).arg(total));
}

void MainWindow::onPageChanged(int index)
{
    m_pageStack->setCurrentIndex(index);
    refreshAll();
}

void MainWindow::refreshAll()
{
    m_overviewPage->refresh();
    m_alertsPage->refresh();
}
