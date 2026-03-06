#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Device Monitor Desktop");
    app.setApplicationVersion("3.4.0");
    app.setOrganizationName("DeviceMonitor");

    // Dark theme palette
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(0x0b, 0x0e, 0x14));
    darkPalette.setColor(QPalette::WindowText, QColor(0xc8, 0xce, 0xda));
    darkPalette.setColor(QPalette::Base, QColor(0x11, 0x15, 0x20));
    darkPalette.setColor(QPalette::AlternateBase, QColor(0x16, 0x1b, 0x28));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(0x1a, 0x1f, 0x2e));
    darkPalette.setColor(QPalette::ToolTipText, QColor(0xc8, 0xce, 0xda));
    darkPalette.setColor(QPalette::Text, QColor(0xc8, 0xce, 0xda));
    darkPalette.setColor(QPalette::Button, QColor(0x16, 0x1b, 0x28));
    darkPalette.setColor(QPalette::ButtonText, QColor(0xc8, 0xce, 0xda));
    darkPalette.setColor(QPalette::BrightText, QColor(0xff, 0x40, 0x60));
    darkPalette.setColor(QPalette::Link, QColor(0x3d, 0x7f, 0xff));
    darkPalette.setColor(QPalette::Highlight, QColor(0x3d, 0x7f, 0xff));
    darkPalette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    app.setPalette(darkPalette);

    QFont defaultFont("IBM Plex Mono", 10);
    app.setFont(defaultFont);

    MainWindow window;
    window.show();

    return app.exec();
}
