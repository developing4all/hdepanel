#include "waylandsupport.h"
#include "desktopapplications.h"
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QWindow>
#include <QProcess>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cstring>

WaylandSupport::WaylandSupport(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_display(nullptr)
    , m_registry(nullptr)
    , m_compositor(nullptr)
    , m_xdg_wm_base(nullptr)
    , m_updateTimer(nullptr)
{
}

WaylandSupport::~WaylandSupport()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
        delete m_updateTimer;
        m_updateTimer = nullptr;
    }
    if (m_display) {
        wl_display_disconnect(m_display);
    }
}

bool WaylandSupport::isAvailable()
{
    QString platform = QGuiApplication::platformName();
    QString sessionType = qgetenv("XDG_SESSION_TYPE");
    QString waylandDisplay = qgetenv("WAYLAND_DISPLAY");

    return platform.contains("wayland", Qt::CaseInsensitive)
        || sessionType == "wayland"
        || !waylandDisplay.isEmpty();
}

bool WaylandSupport::initialize()
{
    if (m_initialized)
        return true;

    if (!isAvailable()) {
        qDebug() << "WaylandSupport: Wayland not available";
        return false;
    }

    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        qDebug() << "WaylandSupport: Failed to connect to Wayland display";
        return false;
    }

    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &registryListener, this);
    wl_display_roundtrip(m_display);

    // Create timer in the GUI thread
    if (!m_updateTimer) {
        m_updateTimer = new QTimer(this);
        m_updateTimer->setInterval(500);
        connect(m_updateTimer, &QTimer::timeout, this, &WaylandSupport::updateWindows);
    }

    m_initialized = true;
    m_updateTimer->start();

    return true;
}

void WaylandSupport::updateWindows()
{
    if (!m_initialized || !m_display)
        return;

    wl_display_dispatch_pending(m_display);
    
    // Get all windows and emit signal
    QList<WaylandWindow> windows = getAllWindows();
    static int lastWindowCount = -1;
    if (windows.size() != lastWindowCount) {
        qDebug() << "WaylandSupport: Window count changed from" << lastWindowCount << "to" << windows.size();
        lastWindowCount = windows.size();
        for (const WaylandWindow& window : windows) {
            qDebug() << "  - Window:" << window.title << "(" << window.appId << ")";
        }
    }
    emit windowsUpdated(windows);
}

QList<WaylandWindow> WaylandSupport::getAllWindows()
{
    QList<WaylandWindow> windows;

    if (!m_initialized) {
        qDebug() << "WaylandSupport::getAllWindows() - not initialized";
        return windows;
    }

    // Check if we're running on GNOME (including Unity compatibility mode)
    QString desktop = qgetenv("XDG_CURRENT_DESKTOP").toLower();
    if (desktop.contains("unity") || desktop.contains("gnome")) {
        // Try GNOME Shell D-Bus first (most accurate)
        windows = getWindowsFromGnomeShell();
        if (!windows.isEmpty()) {
            //qDebug() << "WaylandSupport: Using GNOME Shell D-Bus, found" << windows.size() << "windows";
            return windows;
        }
    } else if (desktop.contains("plasma")) {
        qDebug() << "WaylandSupport: Using KDE Plasma";
        exit(0);
    } else if (desktop.contains("weston")) {
        qDebug() << "WaylandSupport: Using Weston";
        exit(0);
    } else if (desktop.contains("hypr")) {
        qDebug() << "WaylandSupport: Using Hyprland";
        exit(0);
    } else if (desktop.contains("river")) {
        qDebug() << "WaylandSupport: Using River";
        exit(0);
    } else if (desktop.contains("wayfire")) {
        qDebug() << "WaylandSupport: Using Wayfire";
        exit(0);
    } else if (desktop.contains("labwc")) {
        qDebug() << "WaylandSupport: Using Labwc";
        exit(0);
    } else {
        qDebug() << "WaylandSupport: Using Unknown";
        exit(0);
    }

    return windows;
}


QString WaylandSupport::getApplicationIcon(const QString& appId, const QString& wmClass)
{
    DesktopApplications* desktopApps = DesktopApplications::instance();
    if (desktopApps) {
        return desktopApps->getApplicationIcon(appId, wmClass);
    }
    
    // Fallback if DesktopApplications is not available
    QString fallbackIcon = !appId.isEmpty() ? appId.toLower() : wmClass.toLower();
    return fallbackIcon.isEmpty() ? "application-x-executable" : fallbackIcon;
}

// ---------------- Registry handling ----------------

const wl_registry_listener WaylandSupport::registryListener = {
    [](void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
        static_cast<WaylandSupport*>(data)->handleRegistryGlobal(registry, name, interface, version);
    },
    [](void*, wl_registry*, uint32_t) {}
};

void WaylandSupport::handleRegistryGlobal(wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
{
    Q_UNUSED(version);


    if (strcmp(interface, "wl_compositor") == 0) {
        m_compositor = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, 4));
    }
    //else if (strcmp(interface, "xdg_wm_base") == 0) {
    //    m_xdg_wm_base = static_cast<wl_interface*>(
    //        wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
    //    qDebug() << "WaylandSupport: Got xdg_wm_base";
    //}
    else if (strcmp(interface, "zwlr_foreign_toplevel_manager_v1") == 0) {
    }
}

WaylandWindow WaylandSupport::getWindowInfo(void* surface)
{
    Q_UNUSED(surface);
    WaylandWindow w;
    qDebug() << "WaylandSupport: Getting window info for surface:" << surface;
    return w;
}

bool WaylandSupport::activateWindow(const QString& appId)
{
    if (!m_initialized) {
        qDebug() << "WaylandSupport::activateWindow() - not initialized";
        return false;
    }
    
    // Check if we're running on GNOME (including Unity compatibility mode)
    QString desktop = qgetenv("XDG_CURRENT_DESKTOP").toLower();
    if (!desktop.contains("gnome") && !desktop.contains("unity")) {
        qDebug() << "WaylandSupport: Window activation only supported on GNOME/Unity";
        return false;
    }

    QDBusInterface interface("org.gnome.Shell",
        "/org/gnome/Shell",
        "org.gnome.Shell",
        QDBusConnection::sessionBus());

    // First, enable unsafe mode
    QDBusMessage unsafeMessage = interface.call("Eval", "global.context.unsafe_mode = true");
    if (unsafeMessage.type() != QDBusMessage::ReplyMessage || unsafeMessage.arguments().size() < 2) {
        qDebug() << "WaylandSupport: Failed to enable unsafe mode for window activation";
        return false;
    }
    bool unsafeSuccess = unsafeMessage.arguments()[0].toBool();
    if (!unsafeSuccess) {
        qDebug() << "WaylandSupport: Unsafe mode not enabled for window activation";
        return false;
    }

    // Find and activate the window with the given appId
    QString jsCode = QString(R"(
        (function() {
            const windows = global.get_window_actors()
                .filter(w => w.meta_window && w.meta_window.get_title)
                .map(w => {
                    const mw = w.meta_window;
                    const title = mw.get_title() || '';
                    const appId = mw.get_gtk_application_id() || mw.get_wm_class() || '';
                    const wmClass = mw.get_wm_class() || '';
                    const windowType = mw.get_window_type();
                    
                    // Filter out panels and system services
                    const appIdLower = appId.toLowerCase();
                    const wmClassLower = wmClass.toLowerCase();
                    const isPanel = appIdLower.includes('hdepanel') || 
                                    wmClassLower.includes('hdepanel') ||
                                    appIdLower.includes('hde/panel') || 
                                    wmClassLower.includes('hde/panel') ||
                                    appIdLower === 'hde/panel' ||
                                    wmClassLower === 'hde/panel';
                    
                    // Filter out system services and utilities
                    const isSystemService = appIdLower.includes('org.kde.xwaylandvideobridge') ||
                                           appIdLower.includes('org.kde.plasma') ||
                                           appIdLower.includes('org.gnome.shell') ||
                                           appIdLower.includes('com.canonical.unity') ||
                                           appIdLower.includes('com.ubuntu.') ||
                                           appIdLower.includes('org.freedesktop.') ||
                                           appIdLower.startsWith('gjs') ||
                                           appIdLower.startsWith('gnome-shell') ||
                                           wmClassLower.includes('xwaylandvideobridge') ||
                                           wmClassLower.includes('plasma') ||
                                           wmClassLower.includes('gnome-shell');
                    
                    return {
                        title: title,
                        app_id: appId,
                        wm_class: wmClass,
                        window_type: windowType,
                        visible: !mw.minimized,
                        focused: mw.has_focus(),
                        is_wayland: mw.get_client_type() === 1,
                        is_panel: isPanel,
                        is_system_service: isSystemService,
                        meta_window: mw,
                        window_actor: w
                    };
                })
                .filter(w => !w.is_panel && !w.is_system_service && w.visible && w.window_type === 0);
            
            // Find the window with matching appId
            const targetWindow = windows.find(w => 
                w.app_id === '%1' || w.wm_class === '%1'
            );
            
            if (targetWindow) {
                // Activate the window
                targetWindow.meta_window.activate(global.get_current_time());
                return true;
            } else {
                return false;
            }
        })()
    )").arg(appId);

    QDBusMessage message = interface.call("Eval", jsCode);
    
    if (message.type() == QDBusMessage::ReplyMessage && message.arguments().size() >= 2) {
        bool success = message.arguments()[0].toBool();
        QString result = message.arguments()[1].toString();
        
        if (success) {
            bool activated = result.toLower() == "true";
            if (activated) {
                qDebug() << "WaylandSupport: Successfully activated window for appId:" << appId;
                return true;
            } else {
                qDebug() << "WaylandSupport: No window found for appId:" << appId;
                return false;
            }
        } else {
            qDebug() << "WaylandSupport: JavaScript execution failed for window activation:" << result;
            return false;
        }
    } else {
        qDebug() << "WaylandSupport: D-Bus call failed for window activation:" << message.errorMessage();
        return false;
    }
}

QList<WaylandWindow> WaylandSupport::getWindowsFromGnomeShell()
{
    QList<WaylandWindow> windows;
    
    // Check if we're running on GNOME (including Unity compatibility mode)
    QString desktop = qgetenv("XDG_CURRENT_DESKTOP").toLower();
    if (!desktop.contains("gnome") && !desktop.contains("unity")) {
        qDebug() << "WaylandSupport: Not running on GNOME/Unity, desktop:" << desktop;
        return windows;
    }

    QDBusInterface interface("org.gnome.Shell",
        "/org/gnome/Shell",
        "org.gnome.Shell",
        QDBusConnection::sessionBus());

    // First, enable unsafe mode
    QDBusMessage unsafeMessage = interface.call("Eval", "global.context.unsafe_mode = true");
    if (unsafeMessage.type() != QDBusMessage::ReplyMessage || unsafeMessage.arguments().size() < 2) {
        qDebug() << "WaylandSupport: Failed to enable unsafe mode:" << unsafeMessage.errorMessage();
        return windows;
    }
    bool unsafeSuccess = unsafeMessage.arguments()[0].toBool();
    QString unsafeResult = unsafeMessage.arguments()[1].toString();
    if (!unsafeSuccess) {
        qDebug() << "WaylandSupport: Unsafe mode not enabled";
        return windows;
    }

    // Get window information using get_window_actors
    // This gets both Wayland native and XWayland (X11) windows
    QDBusMessage message = interface.call(
        "Eval",
        R"(
            global.get_window_actors()
                .filter(w => w.meta_window && w.meta_window.get_title)
                .map(w => {
                    const mw = w.meta_window;
                    const title = mw.get_title() || '';
                    const appId = mw.get_gtk_application_id() || mw.get_wm_class() || '';
                    const wmClass = mw.get_wm_class() || '';
                    const windowType = mw.get_window_type();
                    
                    // Filter out hdepanel and system services - only check app_id/wm_class, not title
                    // (title can contain "hdepanel" if editing hdepanel project files)
                    const appIdLower = appId.toLowerCase();
                    const wmClassLower = wmClass.toLowerCase();
                    const isPanel = appIdLower.includes('hdepanel') || 
                                    wmClassLower.includes('hdepanel') ||
                                    appIdLower.includes('hde/panel') || 
                                    wmClassLower.includes('hde/panel') ||
                                    appIdLower === 'hde/panel' ||
                                    wmClassLower === 'hde/panel';
                    
                    // Filter out system services and utilities
                    const isSystemService = appIdLower.includes('org.kde.xwaylandvideobridge') ||
                                           appIdLower.includes('org.kde.plasma') ||
                                           appIdLower.includes('org.gnome.shell') ||
                                           appIdLower.includes('com.canonical.unity') ||
                                           appIdLower.includes('com.ubuntu.') ||
                                           appIdLower.includes('org.freedesktop.') ||
                                           appIdLower.startsWith('gjs') ||
                                           appIdLower.startsWith('gnome-shell') ||
                                           wmClassLower.includes('xwaylandvideobridge') ||
                                           wmClassLower.includes('plasma') ||
                                           wmClassLower.includes('gnome-shell');
                    
                    return {
                        title: title,
                        id: mw.get_id(),
                        app_id: appId,
                        wm_class: wmClass,
                        window_type: windowType,
                        visible: !mw.minimized,
                        focused: mw.has_focus(),
                        is_wayland: mw.get_client_type() === 1, // 1=Wayland, 0=X11
                        is_panel: isPanel,
                        is_system_service: isSystemService,
                        surface: mw.get_id().toString()
                    };
                })
                .map(JSON.stringify)
                .join('\n')
        )"
    );

    if (message.type() == QDBusMessage::ReplyMessage && message.arguments().size() >= 2) {
        bool success = message.arguments()[0].toBool();
        QString jsResult = message.arguments()[1].toString();
            
            if (success && !jsResult.isEmpty()) {
                //qDebug() << "WaylandSupport: GNOME Shell returned:" << jsResult;
                
                // The result is a JSON string containing escaped JSON objects
                QString decodedJson = jsResult;
                
                // Remove outer quotes if present
                if (decodedJson.startsWith("\"") && decodedJson.endsWith("\"")) {
                    decodedJson = decodedJson.mid(1, decodedJson.length() - 2);
                }
                
                // Decode escaped sequences
                decodedJson.replace("\\n", "\n");
                decodedJson.replace("\\\"", "\"");
                decodedJson.replace("\\\\", "\\");
                
                // Split into individual JSON objects
                QStringList lines = decodedJson.split('\n', Qt::SkipEmptyParts);
                
                for (const QString& line : lines) {
                    QJsonParseError lineError;
                    QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &lineError);
                    if (lineError.error != QJsonParseError::NoError) {
                        continue;
                    }
                
                    QJsonObject obj = doc.object();
                    QString title = obj["title"].toString();
                    QString appId = obj["app_id"].toString();
                    QString wmClass = obj["wm_class"].toString();
                    int windowType = obj["window_type"].toInt();
                    bool visible = obj["visible"].toBool();
                    bool focused = obj["focused"].toBool();
                    bool isWayland = obj["is_wayland"].toBool();
                    bool isPanel = obj["is_panel"].toBool();
                    bool isSystemService = obj["is_system_service"].toBool();
                    
                    // Only add non-panel, non-system-service, visible, normal windows with titles
                    // window_type 0 = NORMAL, 1 = DIALOG, 2 = MODAL_DIALOG, etc.
                    if (!isPanel && !isSystemService && !title.isEmpty() && visible && windowType == 0) {
                        WaylandWindow window;
                        window.title = title;
                        window.appId = appId;
                        window.visible = visible;
                        window.focused = focused;
                        window.surface = reinterpret_cast<void*>(
                            static_cast<quintptr>(obj["id"].toVariant().toULongLong()));
                        
                        // Try to find icon using both app_id and wm_class
                        window.iconName = getApplicationIcon(appId, wmClass);
                        
                        windows.append(window);
                        /*
                        qDebug() << "WaylandSupport: Found window:" << title
                                 << "(" << (appId.isEmpty() ? wmClass : appId) << ")"
                                 << "[" << (isWayland ? "Wayland" : "X11") << "]";
                        */
                    }
                }
            } else {
                qDebug() << "WaylandSupport: JavaScript execution failed or empty result:" << jsResult;
            }
    } else {
        qDebug() << "WaylandSupport: D-Bus call failed:" << message.errorMessage();
    }
    
    return windows;
}
