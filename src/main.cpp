#include "core/mainwindow.h"
#include "core/theme.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QSettings>
#include <QStyleFactory>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

static void applyDarkTitleBar(QWidget* window, bool dark)
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(window->winId());
    BOOL useDark = dark ? TRUE : FALSE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 (Windows 11 build 22000+)
    // Falls back gracefully on older Windows
    DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark));
    // Also try attribute 19 (pre-release Windows 10)
    DwmSetWindowAttribute(hwnd, 19, &useDark, sizeof(useDark));
#else
    Q_UNUSED(window); Q_UNUSED(dark);
#endif
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Code Clarity");
    app.setOrganizationName("UniLogic");
    app.setApplicationVersion("0.1.0");

    // Use Fusion style so QPalette has full effect everywhere
    app.setStyle(QStyleFactory::create("Fusion"));

    // Read saved theme preference (default: dark)
    QSettings s("CodeClarity", "CodeClarity");
    bool isDark = s.value("theme/dark", true).toBool();

    // Apply global Qt stylesheet
    app.setStyleSheet(isDark ? Theme::appStyleSheet() : Theme::lightStyleSheet());

    // Set Fusion palette to match the theme so native decorations align
    QPalette pal = app.palette();
    if (isDark) {
        pal.setColor(QPalette::Window,          QColor(0x1e, 0x1e, 0x2e));
        pal.setColor(QPalette::WindowText,      QColor(0xcd, 0xd6, 0xf4));
        pal.setColor(QPalette::Base,            QColor(0x2a, 0x2a, 0x3c));
        pal.setColor(QPalette::AlternateBase,   QColor(0x2a, 0x2a, 0x3c));
        pal.setColor(QPalette::Text,            QColor(0xcd, 0xd6, 0xf4));
        pal.setColor(QPalette::Button,          QColor(0x2a, 0x2a, 0x3c));
        pal.setColor(QPalette::ButtonText,      QColor(0xcd, 0xd6, 0xf4));
        pal.setColor(QPalette::Highlight,       QColor(0x89, 0xb4, 0xfa));
        pal.setColor(QPalette::HighlightedText, QColor(0x1e, 0x1e, 0x2e));
        pal.setColor(QPalette::ToolTipBase,     QColor(0x2a, 0x2a, 0x3c));
        pal.setColor(QPalette::ToolTipText,     QColor(0xcd, 0xd6, 0xf4));
    } else {
        pal.setColor(QPalette::Window,          QColor(0xf5, 0xf5, 0xf7));
        pal.setColor(QPalette::WindowText,      QColor(0x1c, 0x1c, 0x1e));
        pal.setColor(QPalette::Base,            QColor(0xff, 0xff, 0xff));
        pal.setColor(QPalette::AlternateBase,   QColor(0xf0, 0xf0, 0xf5));
        pal.setColor(QPalette::Text,            QColor(0x1c, 0x1c, 0x1e));
        pal.setColor(QPalette::Button,          QColor(0xe8, 0xe8, 0xed));
        pal.setColor(QPalette::ButtonText,      QColor(0x1c, 0x1c, 0x1e));
        pal.setColor(QPalette::Highlight,       QColor(0x00, 0x71, 0xe3));
        pal.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    }
    app.setPalette(pal);

    // Set default font
    QFont defaultFont(Theme::SansFont, 10);
    app.setFont(defaultFont);

    MainWindow window;
    window.show();

    // Apply dark/light title bar via DWM on Windows
    applyDarkTitleBar(&window, isDark);

    return app.exec();
}
