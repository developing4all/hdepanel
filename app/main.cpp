#include "panelapplication.h"
#include <QDebug>
#include <QtCore/QProcessEnvironment>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>
#include <QFile>
#include <QCoreApplication>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>

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

    // ---- Single-instance guard ----
    // Prevent launching multiple hdepanel instances (which can fight over struts/tray selection
    // and cause both panels to move to unexpected positions).
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString lockDir = runtimeDir.isEmpty() ? QDir::tempPath() : runtimeDir;
    const QString lockPath = QDir(lockDir).filePath("hdepanel.lock");
    static QLockFile instanceLock(lockPath);
    instanceLock.setStaleLockTime(0); // rely on PID checking; don't auto-break locks
    if (!instanceLock.tryLock(0)) {
        qDebug() << "Another hdepanel instance is already running (lock:" << lockPath << "). Exiting.";
        return 0;
    }

    PanelApplication app(argc, argv);
    
    // Setup translations - must be heap-allocated to persist for application lifetime
    QTranslator* qtTranslator = new QTranslator(&app);
    QTranslator* appTranslator = new QTranslator(&app);
    
    // Get system locale (e.g., "en_US", "nl_NL", "ar_SA")
    QString locale = QLocale::system().name();
    QString language = locale.split('_').first(); // Extract just "nl" from "nl_NL"
    qDebug() << "System locale:" << locale << "Language:" << language;
    
    // Load Qt's built-in translations
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    qtTranslator->load("qt_" + locale, QLibraryInfo::path(QLibraryInfo::TranslationsPath));
#else
    qtTranslator->load("qt_" + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath));
#endif
    app.installTranslator(qtTranslator);
    
    // Try to find translation file - check both full locale and language code
    QString translationsPath;
    QString translationFile;
    
    QStringList searchPaths;
    searchPaths << "/usr/share/hdepanel/translations"
                << QCoreApplication::applicationDirPath() + "/translations"
                << QCoreApplication::applicationDirPath() + "/../translations";
    
    QStringList localeVariants;
    localeVariants << locale << language; // Try "nl_NL" then "nl"
    
    // Search for translation file
    for (const QString& path : searchPaths) {
        for (const QString& loc : localeVariants) {
            QString file = path + "/hdepanel_" + loc + ".qm";
            if (QFile::exists(file)) {
                translationsPath = path;
                translationFile = "hdepanel_" + loc;
                qDebug() << "Found translation file:" << file;
                break;
            }
        }
        if (!translationsPath.isEmpty()) break;
    }
    
    // Load the translation
    if (!translationsPath.isEmpty()) {
        if (appTranslator->load(translationFile, translationsPath)) {
            app.installTranslator(appTranslator);
            qDebug() << "✓ Loaded translation:" << translationFile << "from" << translationsPath;
        } else {
            qDebug() << "✗ Failed to load translation:" << translationFile;
        }
    } else {
        qDebug() << "No translation files found for" << locale << "or" << language << "- using default language";
    }
    
    app.init();

    // Clean shutdown on Ctrl-C
    signal(SIGINT, m_cleanup);

    return app.exec();
}