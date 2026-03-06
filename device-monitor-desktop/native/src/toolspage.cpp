#include "toolspage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QTcpSocket>

ToolsPage::ToolsPage(QWidget *parent)
    : QWidget(parent), m_process(nullptr)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *title = new QLabel("NETWORK TOOLS");
    title->setStyleSheet("font-size: 18px; font-weight: bold; color: #c8ceda; letter-spacing: 2px;");
    layout->addWidget(title);

    // Input area
    auto *inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(8);

    auto *hostLabel = new QLabel("Host:");
    hostLabel->setStyleSheet("color: #555a68; font-weight: bold;");
    inputLayout->addWidget(hostLabel);

    m_hostInput = new QLineEdit();
    m_hostInput->setPlaceholderText("IP address or hostname");
    m_hostInput->setStyleSheet(
        "QLineEdit { background: #161b28; border: 1px solid #1e2538; border-radius: 4px;"
        "  padding: 6px 12px; color: #c8ceda; font-size: 12px; }"
        "QLineEdit:focus { border-color: #3d7fff; }"
    );
    inputLayout->addWidget(m_hostInput, 1);

    auto *portLabel = new QLabel("Port:");
    portLabel->setStyleSheet("color: #555a68; font-weight: bold;");
    inputLayout->addWidget(portLabel);

    m_portInput = new QLineEdit();
    m_portInput->setPlaceholderText("80");
    m_portInput->setFixedWidth(80);
    m_portInput->setStyleSheet(m_hostInput->styleSheet());
    inputLayout->addWidget(m_portInput);

    layout->addLayout(inputLayout);

    // Buttons
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(8);

    auto makeBtn = [](const QString &text, const QString &color) {
        auto *btn = new QPushButton(text);
        btn->setStyleSheet(QString(
            "QPushButton { background: %1; color: white; border: none; border-radius: 4px;"
            "  padding: 8px 16px; font-weight: bold; font-size: 11px; }"
            "QPushButton:hover { opacity: 0.8; }"
        ).arg(color));
        return btn;
    };

    auto *pingBtn = makeBtn("PING", "#3d7fff");
    auto *traceBtn = makeBtn("TRACEROUTE", "#7c3dff");
    auto *dnsBtn = makeBtn("DNS LOOKUP", "#00b8d4");
    auto *portBtn = makeBtn("PORT CHECK", "#ff6d00");

    connect(pingBtn, &QPushButton::clicked, this, &ToolsPage::runPing);
    connect(traceBtn, &QPushButton::clicked, this, &ToolsPage::runTraceroute);
    connect(dnsBtn, &QPushButton::clicked, this, &ToolsPage::runDnsLookup);
    connect(portBtn, &QPushButton::clicked, this, &ToolsPage::runPortCheck);

    btnLayout->addWidget(pingBtn);
    btnLayout->addWidget(traceBtn);
    btnLayout->addWidget(dnsBtn);
    btnLayout->addWidget(portBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    // Output console
    m_outputConsole = new QTextEdit();
    m_outputConsole->setReadOnly(true);
    m_outputConsole->setStyleSheet(
        "QTextEdit {"
        "  background: #0a0d12; border: 1px solid #1e2538; border-radius: 4px;"
        "  color: #c8ceda; font-family: 'IBM Plex Mono', monospace; font-size: 11px;"
        "  padding: 12px;"
        "}"
    );
    layout->addWidget(m_outputConsole, 1);
}

void ToolsPage::appendOutput(const QString &text, const QString &color)
{
    m_outputConsole->append(QString("<span style='color:%1'>%2</span>").arg(color, text.toHtmlEscaped()));
}

void ToolsPage::runPing()
{
    QString host = m_hostInput->text().trimmed();
    if (host.isEmpty()) {
        appendOutput("Error: Please enter a host", "#ff4060");
        return;
    }

    appendOutput("--- PING " + host + " ---", "#3d7fff");

    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
    }

    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, [this]() {
        appendOutput(m_process->readAllStandardOutput().trimmed());
    });
    connect(m_process, &QProcess::readyReadStandardError, [this]() {
        appendOutput(m_process->readAllStandardError().trimmed(), "#ff4060");
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this](int code, QProcess::ExitStatus) {
        appendOutput(QString("--- Ping finished (exit code: %1) ---").arg(code), "#555a68");
        m_process->deleteLater();
        m_process = nullptr;
    });

    m_process->start("ping", QStringList() << "-c" << "4" << host);
}

void ToolsPage::runTraceroute()
{
    QString host = m_hostInput->text().trimmed();
    if (host.isEmpty()) {
        appendOutput("Error: Please enter a host", "#ff4060");
        return;
    }

    appendOutput("--- TRACEROUTE " + host + " ---", "#7c3dff");

    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
    }

    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, [this]() {
        appendOutput(m_process->readAllStandardOutput().trimmed());
    });
    connect(m_process, &QProcess::readyReadStandardError, [this]() {
        appendOutput(m_process->readAllStandardError().trimmed(), "#ff4060");
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this](int code, QProcess::ExitStatus) {
        appendOutput(QString("--- Traceroute finished (exit code: %1) ---").arg(code), "#555a68");
        m_process->deleteLater();
        m_process = nullptr;
    });

    m_process->start("traceroute", QStringList() << "-m" << "20" << host);
}

void ToolsPage::runDnsLookup()
{
    QString host = m_hostInput->text().trimmed();
    if (host.isEmpty()) {
        appendOutput("Error: Please enter a hostname", "#ff4060");
        return;
    }

    appendOutput("--- DNS LOOKUP " + host + " ---", "#00b8d4");

    QHostInfo::lookupHost(host, this, [this, host](const QHostInfo &info) {
        if (info.error() != QHostInfo::NoError) {
            appendOutput("DNS lookup failed: " + info.errorString(), "#ff4060");
        } else {
            appendOutput("Hostname: " + info.hostName());
            for (const auto &addr : info.addresses()) {
                appendOutput("  Address: " + addr.toString(), "#00e576");
            }
        }
        appendOutput("--- DNS lookup finished ---", "#555a68");
    });
}

void ToolsPage::runPortCheck()
{
    QString host = m_hostInput->text().trimmed();
    QString portStr = m_portInput->text().trimmed();

    if (host.isEmpty()) {
        appendOutput("Error: Please enter a host", "#ff4060");
        return;
    }

    int port = portStr.isEmpty() ? 80 : portStr.toInt();
    if (port <= 0 || port > 65535) {
        appendOutput("Error: Invalid port number", "#ff4060");
        return;
    }

    appendOutput(QString("--- PORT CHECK %1:%2 ---").arg(host).arg(port), "#ff6d00");

    auto *socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::connected, [this, host, port, socket]() {
        appendOutput(QString("Port %1 on %2 is OPEN").arg(port).arg(host), "#00e576");
        socket->disconnectFromHost();
        socket->deleteLater();
        appendOutput("--- Port check finished ---", "#555a68");
    });
    connect(socket, &QAbstractSocket::errorOccurred, [this, host, port, socket](QAbstractSocket::SocketError) {
        appendOutput(QString("Port %1 on %2 is CLOSED/FILTERED — %3").arg(port).arg(host).arg(socket->errorString()), "#ff4060");
        socket->deleteLater();
        appendOutput("--- Port check finished ---", "#555a68");
    });

    socket->connectToHost(host, port);
}
