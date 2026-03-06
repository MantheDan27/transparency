#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QProcess>

class ToolsPage : public QWidget {
    Q_OBJECT
public:
    explicit ToolsPage(QWidget *parent = nullptr);

private:
    void runPing();
    void runTraceroute();
    void runDnsLookup();
    void runPortCheck();
    void appendOutput(const QString &text, const QString &color = "#c8ceda");

    QLineEdit *m_hostInput;
    QLineEdit *m_portInput;
    QTextEdit *m_outputConsole;
    QProcess *m_process;
};
