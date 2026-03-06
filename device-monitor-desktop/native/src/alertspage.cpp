#include "alertspage.h"
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QHBoxLayout>

AlertsPage::AlertsPage(AlertManager *alertManager, QWidget *parent)
    : QWidget(parent), m_alertManager(alertManager)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    // Header
    auto *headerLayout = new QHBoxLayout();
    auto *title = new QLabel("ALERTS");
    title->setStyleSheet("font-size: 18px; font-weight: bold; color: #c8ceda; letter-spacing: 2px;");
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    auto *ackAllBtn = new QPushButton("Acknowledge All");
    ackAllBtn->setStyleSheet(
        "QPushButton { background: #1e2538; color: #c8ceda; border: none; border-radius: 4px;"
        "  padding: 8px 16px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background: #2a3548; }"
    );
    connect(ackAllBtn, &QPushButton::clicked, [this]() {
        m_alertManager->acknowledgeAll();
        refresh();
    });
    headerLayout->addWidget(ackAllBtn);

    auto *clearBtn = new QPushButton("Clear All");
    clearBtn->setStyleSheet(
        "QPushButton { background: #2a1520; color: #ff4060; border: none; border-radius: 4px;"
        "  padding: 8px 16px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background: #3a1a28; }"
    );
    connect(clearBtn, &QPushButton::clicked, [this]() {
        m_alertManager->clearAll();
        refresh();
    });
    headerLayout->addWidget(clearBtn);

    mainLayout->addLayout(headerLayout);

    // Scrollable alerts list
    auto *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; }");

    auto *container = new QWidget();
    m_alertsLayout = new QVBoxLayout(container);
    m_alertsLayout->setContentsMargins(0, 0, 0, 0);
    m_alertsLayout->setSpacing(8);
    m_alertsLayout->addStretch();
    scrollArea->setWidget(container);

    mainLayout->addWidget(scrollArea, 1);

    refresh();
}

void AlertsPage::refresh()
{
    // Clear existing
    while (m_alertsLayout->count() > 1) {
        auto *item = m_alertsLayout->takeAt(0);
        if (item->widget()) delete item->widget();
        delete item;
    }

    auto alerts = m_alertManager->allAlerts();
    if (alerts.isEmpty()) {
        auto *empty = new QLabel("No alerts. Your network looks clean.");
        empty->setStyleSheet("color: #555a68; font-size: 13px; padding: 40px;");
        empty->setAlignment(Qt::AlignCenter);
        m_alertsLayout->insertWidget(0, empty);
        return;
    }

    for (int i = 0; i < alerts.size(); ++i) {
        const auto &alert = alerts[i];

        auto *card = new QFrame();
        QString borderColor = "#3d7fff";
        if (alert.severity == "critical") borderColor = "#ff4060";
        else if (alert.severity == "warning") borderColor = "#ffc832";

        QString bgColor = alert.acknowledged ? "#0f1219" : "#161b28";
        card->setStyleSheet(QString(
            "QFrame { background: %1; border-left: 3px solid %2; border-radius: 4px; padding: 8px; }"
        ).arg(bgColor, borderColor));

        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);

        auto *textLayout = new QVBoxLayout();
        textLayout->setSpacing(4);

        auto *titleLabel = new QLabel(alert.title);
        titleLabel->setStyleSheet(QString("font-weight: bold; color: %1; font-size: 12px;").arg(borderColor));

        auto *msgLabel = new QLabel(alert.message);
        msgLabel->setStyleSheet(QString("color: %1; font-size: 11px;").arg(alert.acknowledged ? "#3a3f4a" : "#8a8f9a"));
        msgLabel->setWordWrap(true);

        auto *timeLabel = new QLabel(alert.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
        timeLabel->setStyleSheet("color: #3a3f4a; font-size: 10px;");

        textLayout->addWidget(titleLabel);
        textLayout->addWidget(msgLabel);
        textLayout->addWidget(timeLabel);
        cardLayout->addLayout(textLayout, 1);

        if (!alert.acknowledged) {
            auto *ackBtn = new QPushButton("ACK");
            ackBtn->setFixedSize(48, 28);
            ackBtn->setStyleSheet(
                "QPushButton { background: #1e2538; color: #8a8f9a; border: none; border-radius: 4px; font-size: 10px; font-weight: bold; }"
                "QPushButton:hover { background: #2a3548; color: #c8ceda; }"
            );
            QString alertId = alert.id;
            connect(ackBtn, &QPushButton::clicked, [this, alertId]() {
                m_alertManager->acknowledgeAlert(alertId);
                refresh();
            });
            cardLayout->addWidget(ackBtn, 0, Qt::AlignTop);
        }

        m_alertsLayout->insertWidget(m_alertsLayout->count() - 1, card);
    }
}
