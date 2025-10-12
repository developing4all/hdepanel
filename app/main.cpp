#include "panelapplication.h"
#include <QDebug>
#include <QtCore/QProcessEnvironment>

#include <signal.h>


static void m_cleanup(int sig)
{
    qDebug() << "Bye :)";
    // your destructor stuff
    if (sig == SIGINT) qApp->quit();
}


int main(int argc, char** argv)
{
    // Auto-detect platform based on environment
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        const QByteArray sessionType = qgetenv("XDG_SESSION_TYPE");
        const QByteArray desktopEnv = qgetenv("XDG_CURRENT_DESKTOP").toLower();
        const QByteArray gnomeMode  = qgetenv("GNOME_SHELL_SESSION_MODE").toLower();

        const bool hasX11 = qEnvironmentVariableIsSet("DISPLAY") || sessionType == "x11";
        const bool hasWayland = qEnvironmentVariableIsSet("WAYLAND_DISPLAY") || sessionType == "wayland";

        qDebug() << "Platform detection - XDG_SESSION_TYPE:" << sessionType;
        qDebug() << "Platform detection - DISPLAY:" << qgetenv("DISPLAY");
        qDebug() << "Platform detection - WAYLAND_DISPLAY:" << qgetenv("WAYLAND_DISPLAY");
        qDebug() << "Platform detection - hasX11:" << hasX11 << "hasWayland:" << hasWayland;
        qDebug() << "Platform detection - XDG_CURRENT_DESKTOP:" << desktopEnv;
        qDebug() << "Platform detection - GNOME_SHELL_SESSION_MODE:" << gnomeMode;

        const bool isGnomeLike =
            desktopEnv.contains("gnome") ||
            desktopEnv.contains("ubuntu") ||   // Ubuntu's "ubuntu:GNOME"
            desktopEnv.contains("unity") ||
            gnomeMode.contains("ubuntu");

        // 🔹 If GNOME/Unity Wayland, use Xwayland instead (Mutter lacks layer-shell)
        if (hasWayland && isGnomeLike) {
            qputenv("QT_QPA_PLATFORM", QByteArray("xcb"));
            qDebug() << "Detected GNOME/Unity Wayland → using Xwayland fallback for proper panel placement.";
        }
        // 🔹 Native Wayland (wlroots, KDE, etc.)
        else if (hasWayland) {
            qputenv("QT_QPA_PLATFORM", QByteArray("wayland"));
            qputenv("QT_WAYLAND_DISABLE_WINDOWDECORATION", QByteArray("1"));
            qDebug() << "Using native Wayland platform (layer-shell capable).";
        }
        // 🔹 X11 sessions
        else if (hasX11) {
            qputenv("QT_QPA_PLATFORM", QByteArray("xcb"));
            qDebug() << "Using X11 (xcb) platform.";
        }
        // 🔹 Fallback
        else {
            qputenv("QT_QPA_PLATFORM", QByteArray("xcb"));
            qDebug() << "Defaulting to X11 (xcb) platform.";
        }
    }

    PanelApplication app(argc, argv);
    app.init();

    // Clean shutdown on Ctrl-C
    signal(SIGINT, m_cleanup);

    return app.exec();
}