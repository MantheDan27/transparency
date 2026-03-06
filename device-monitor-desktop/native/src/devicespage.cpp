#include "devicespage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QTextEdit>

DevicesPage::DevicesPage(DeviceModel *model, QWidget *parent)
    : QWidget(parent), m_model(model)
{
    setupUI();
}

void DevicesPage::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    // Header
    auto *headerLayout = new QHBoxLayout();
    auto *title = new QLabel("DEVICES");
    title->setStyleSheet("font-size: 18px; font-weight: bold; color: #c8ceda; letter-spacing: 2px;");
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    // Search
    m_searchBox = new QLineEdit();
    m_searchBox->setPlaceholderText("Search devices...");
    m_searchBox->setFixedWidth(280);
    m_searchBox->setStyleSheet(
        "QLineEdit {"
        "  background: #161b28; border: 1px solid #1e2538; border-radius: 4px;"
        "  padding: 6px 12px; color: #c8ceda; font-size: 12px;"
        "}"
        "QLineEdit:focus { border-color: #3d7fff; }"
    );
    headerLayout->addWidget(m_searchBox);
    layout->addLayout(headerLayout);

    // Table
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1); // Search all columns

    m_tableView = new QTableView();
    m_tableView->setModel(m_proxyModel);
    m_tableView->setSortingEnabled(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableView->setShowGrid(false);

    m_tableView->setStyleSheet(
        "QTableView {"
        "  background: #0b0e14; alternate-background-color: #0f1219;"
        "  border: 1px solid #1e2538; border-radius: 4px;"
        "  gridline-color: transparent; color: #c8ceda; font-size: 11px;"
        "}"
        "QTableView::item { padding: 6px 8px; border-bottom: 1px solid #161b28; }"
        "QTableView::item:selected { background: #1a2744; }"
        "QHeaderView::section {"
        "  background: #161b28; color: #555a68; font-size: 10px;"
        "  font-weight: bold; letter-spacing: 1px; padding: 8px;"
        "  border: none; border-bottom: 1px solid #1e2538;"
        "}"
    );

    layout->addWidget(m_tableView, 1);

    // Connect search
    connect(m_searchBox, &QLineEdit::textChanged, m_proxyModel, &QSortFilterProxyModel::setFilterFixedString);

    // Double-click for details
    connect(m_tableView, &QTableView::doubleClicked, this, &DevicesPage::onDeviceDoubleClicked);
}

void DevicesPage::onDeviceDoubleClicked(const QModelIndex &index)
{
    auto sourceIndex = m_proxyModel->mapToSource(index);
    if (!sourceIndex.isValid()) return;

    const Device &dev = m_model->deviceAt(sourceIndex.row());

    auto *dialog = new QDialog(this);
    dialog->setWindowTitle("Device Details — " + dev.ip);
    dialog->setMinimumSize(480, 400);
    dialog->setStyleSheet("QDialog { background: #0b0e14; }");

    auto *layout = new QVBoxLayout(dialog);
    layout->setSpacing(12);
    layout->setContentsMargins(24, 24, 24, 24);

    auto *titleLabel = new QLabel(dev.ip + (dev.hostname.isEmpty() ? "" : " (" + dev.hostname + ")"));
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #c8ceda;");
    layout->addWidget(titleLabel);

    auto *form = new QFormLayout();
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight);

    auto addField = [&](const QString &label, const QString &value, const QString &color = "#c8ceda") {
        auto *lbl = new QLabel(label);
        lbl->setStyleSheet("color: #555a68; font-weight: bold; font-size: 11px;");
        auto *val = new QLabel(value);
        val->setStyleSheet(QString("color: %1; font-size: 12px;").arg(color));
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        form->addRow(lbl, val);
    };

    addField("STATUS", dev.isOnline ? "ONLINE" : "OFFLINE",
             dev.isOnline ? "#00e576" : "#555a68");
    addField("MAC ADDRESS", dev.mac.isEmpty() ? "Unknown" : dev.mac);
    addField("VENDOR", dev.vendor.isEmpty() ? "Unknown" : dev.vendor);
    addField("TYPE", dev.deviceType.isEmpty() ? "Unknown" : dev.deviceType);

    QStringList portStrs;
    for (int p : dev.openPorts) portStrs << QString::number(p);
    addField("OPEN PORTS", portStrs.isEmpty() ? "None" : portStrs.join(", "));

    QStringList riskyStrs;
    for (int p : dev.riskyPorts) riskyStrs << QString::number(p);
    addField("RISKY PORTS", riskyStrs.isEmpty() ? "None" : riskyStrs.join(", "),
             riskyStrs.isEmpty() ? "#00e576" : "#ff4060");

    addField("FIRST SEEN", dev.firstSeen.toString("yyyy-MM-dd hh:mm:ss"));
    addField("LAST SEEN", dev.lastSeen.toString("yyyy-MM-dd hh:mm:ss"));
    addField("TRUST LEVEL", QString::number(dev.trustLevel) + "%");

    layout->addLayout(form);

    auto *closeBtn = new QPushButton("Close");
    closeBtn->setStyleSheet(
        "QPushButton { background: #1e2538; color: #c8ceda; border: none; border-radius: 4px;"
        "  padding: 8px 24px; font-weight: bold; }"
        "QPushButton:hover { background: #2a3548; }"
    );
    connect(closeBtn, &QPushButton::clicked, dialog, &QDialog::accept);
    layout->addStretch();
    layout->addWidget(closeBtn, 0, Qt::AlignRight);

    dialog->exec();
    dialog->deleteLater();
}
