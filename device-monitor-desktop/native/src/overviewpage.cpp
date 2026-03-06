#include "overviewpage.h"
#include <QFrame>
#include <QScrollArea>

OverviewPage::OverviewPage(DeviceModel *deviceModel, AlertManager *alertManager, QWidget *parent)
    : QWidget(parent), m_deviceModel(deviceModel), m_alertManager(alertManager)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(20);

    // Title
    auto *title = new QLabel("NETWORK OVERVIEW");
    title->setStyleSheet("font-size: 18px; font-weight: bold; color: #c8ceda; letter-spacing: 2px;");
    mainLayout->addWidget(title);

    // Stats grid
    auto *statsGrid = new QGridLayout();
    statsGrid->setSpacing(16);

    m_totalDevicesLabel = new QLabel("0");
    m_onlineLabel = new QLabel("0");
    m_riskyLabel = new QLabel("0");
    m_alertsLabel = new QLabel("0");

    statsGrid->addWidget(createStatCard("TOTAL DEVICES", "0", QColor(0x3d, 0x7f, 0xff)), 0, 0);
    statsGrid->addWidget(createStatCard("ONLINE", "0", QColor(0x00, 0xe5, 0x76)), 0, 1);
    statsGrid->addWidget(createStatCard("AT RISK", "0", QColor(0xff, 0x40, 0x60)), 0, 2);
    statsGrid->addWidget(createStatCard("ALERTS", "0", QColor(0xff, 0xc8, 0x32)), 0, 3);

    mainLayout->addLayout(statsGrid);

    // Recent alerts section
    auto *alertsTitle = new QLabel("RECENT ALERTS");
    alertsTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #8a8f9a; letter-spacing: 1px; margin-top: 12px;");
    mainLayout->addWidget(alertsTitle);

    auto *alertsScroll = new QScrollArea();
    alertsScroll->setWidgetResizable(true);
    alertsScroll->setFrameShape(QFrame::NoFrame);
    alertsScroll->setStyleSheet("QScrollArea { background: transparent; }");

    auto *alertsContainer = new QWidget();
    m_recentAlertsLayout = new QVBoxLayout(alertsContainer);
    m_recentAlertsLayout->setContentsMargins(0, 0, 0, 0);
    m_recentAlertsLayout->setSpacing(8);
    m_recentAlertsLayout->addStretch();
    alertsScroll->setWidget(alertsContainer);

    mainLayout->addWidget(alertsScroll, 1);

    refresh();
}

QWidget *OverviewPage::createStatCard(const QString &title, const QString &value, const QColor &color)
{
    auto *card = new QFrame();
    card->setFrameShape(QFrame::StyledPanel);
    card->setStyleSheet(QString(
        "QFrame { background: #161b28; border: 1px solid #1e2538; border-radius: 8px; padding: 16px; }"
    ));

    auto *layout = new QVBoxLayout(card);
    layout->setSpacing(8);

    auto *titleLabel = new QLabel(title);
    titleLabel->setStyleSheet("font-size: 11px; color: #555a68; letter-spacing: 1px; font-weight: bold;");

    auto *valueLabel = new QLabel(value);
    valueLabel->setStyleSheet(QString("font-size: 32px; font-weight: bold; color: %1;").arg(color.name()));
    valueLabel->setObjectName(title); // Use title as object name for lookup

    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);

    // Store reference based on title
    if (title == "TOTAL DEVICES") m_totalDevicesLabel = valueLabel;
    else if (title == "ONLINE") m_onlineLabel = valueLabel;
    else if (title == "AT RISK") m_riskyLabel = valueLabel;
    else if (title == "ALERTS") m_alertsLabel = valueLabel;

    return card;
}

void OverviewPage::refresh()
{
    m_totalDevicesLabel->setText(QString::number(m_deviceModel->totalCount()));
    m_onlineLabel->setText(QString::number(m_deviceModel->onlineCount()));
    m_riskyLabel->setText(QString::number(m_deviceModel->riskyCount()));
    m_alertsLabel->setText(QString::number(m_alertManager->unacknowledgedCount()));

    // Clear existing alert widgets
    while (m_recentAlertsLayout->count() > 1) {
        auto *item = m_recentAlertsLayout->takeAt(0);
        if (item->widget()) delete item->widget();
        delete item;
    }

    // Add recent alerts (last 10)
    auto alerts = m_alertManager->allAlerts();
    int count = qMin(alerts.size(), 10);
    for (int i = 0; i < count; ++i) {
        const auto &alert = alerts[i];

        auto *alertWidget = new QFrame();
        QString borderColor = "#3d7fff";
        if (alert.severity == "critical") borderColor = "#ff4060";
        else if (alert.severity == "warning") borderColor = "#ffc832";

        alertWidget->setStyleSheet(QString(
            "QFrame { background: #161b28; border-left: 3px solid %1; border-radius: 4px; padding: 10px; }"
        ).arg(borderColor));

        auto *alertLayout = new QVBoxLayout(alertWidget);
        alertLayout->setSpacing(4);
        alertLayout->setContentsMargins(12, 8, 12, 8);

        auto *alertTitle = new QLabel(alert.title);
        alertTitle->setStyleSheet(QString("font-weight: bold; color: %1; font-size: 12px;").arg(borderColor));

        auto *alertMsg = new QLabel(alert.message);
        alertMsg->setStyleSheet("color: #8a8f9a; font-size: 11px;");
        alertMsg->setWordWrap(true);

        auto *alertTime = new QLabel(alert.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
        alertTime->setStyleSheet("color: #3a3f4a; font-size: 10px;");

        alertLayout->addWidget(alertTitle);
        alertLayout->addWidget(alertMsg);
        alertLayout->addWidget(alertTime);

        m_recentAlertsLayout->insertWidget(m_recentAlertsLayout->count() - 1, alertWidget);
    }

    if (count == 0) {
        auto *noAlerts = new QLabel("No alerts yet. Run a network scan to get started.");
        noAlerts->setStyleSheet("color: #555a68; font-size: 12px; padding: 20px;");
        m_recentAlertsLayout->insertWidget(0, noAlerts);
    }
}
