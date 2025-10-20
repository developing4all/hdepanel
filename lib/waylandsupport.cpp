#include "waylandsupport.h"
#include "desktopapplications.h"
#include "desktopdatastore.h"
#include "windowmanager.h"
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
    , m_windowManager(nullptr)
    , m_useWindowManager(true)
    , m_initialized(false)
    , m_display(nullptr)
    , m_registry(nullptr)
    , m_compositor(nullptr)
    , m_xdg_wm_base(nullptr)
    , m_updateTimer(nullptr)
{
    // Try to create a window manager first
    m_windowManager = WindowManagerFactory::createWindowManager(this);
    if (m_windowManager) {
        m_useWindowManager = m_windowManager->initialize();
        if (m_useWindowManager) {
            // Connect window manager signals
            connect(m_windowManager, &WindowManager::windowsUpdated,
                    this, &WaylandSupport::onWindowManagerWindowsUpdated);
            connect(m_windowManager, &WindowManager::windowCreated,
                    this, &WaylandSupport::onWindowManagerWindowCreated);
            connect(m_windowManager, &WindowManager::windowClosed,
                    this, &WaylandSupport::onWindowManagerWindowClosed);
            connect(m_windowManager, &WindowManager::windowActivated,
                    this, &WaylandSupport::onWindowManagerWindowActivated);
            connect(m_windowManager, &WindowManager::workspaceChanged,
                    this, &WaylandSupport::onWindowManagerWorkspaceChanged);
            connect(m_windowManager, &WindowManager::monitorChanged,
                    this, &WaylandSupport::onWindowManagerMonitorChanged);
            
            qDebug() << "WaylandSupport: Using" << m_windowManager->getWindowManagerName() << "window manager";
        } else {
            qDebug() << "WaylandSupport: Window manager initialization failed, falling back to legacy mode";
            delete m_windowManager;
            m_windowManager = nullptr;
            m_useWindowManager = false;
        }
    } else {
        qDebug() << "WaylandSupport: No window manager available, using legacy mode";
        m_useWindowManager = false;
    }
}

WaylandSupport::~WaylandSupport()
{
    // Stop and delete timer first
    if (m_updateTimer) {
        m_updateTimer->stop();
        delete m_updateTimer;
        m_updateTimer = nullptr;
    }
    
    // Clean up window manager
    if (m_windowManager) {
        delete m_windowManager;
        m_windowManager = nullptr;
    }
    
    // Clean up Wayland resources
    if (m_xdg_wm_base) {
        // xdg_wm_base_destroy is not available in the current Wayland headers
        // The resource will be cleaned up when the display is disconnected
        m_xdg_wm_base = nullptr;
    }
    if (m_compositor) {
        wl_compositor_destroy(m_compositor);
        m_compositor = nullptr;
    }
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
    if (m_display) {
        wl_display_disconnect(m_display);
        m_display = nullptr;
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
            qDebug() << "  - Window:" << window.title 
                     << "(" << window.appId << ")"
                     << "[WS:" << window.workspaceIndex 
                     << ", Monitor:" << window.monitorIndex
                     << ", " << (window.isWayland ? "Wayland" : "X11")
                     << ", " << (window.focused ? "Focused" : "Unfocused")
                     << "]";
        }
    }
    emit windowsUpdated(windows);
}

QList<WaylandWindow> WaylandSupport::getAllWindows()
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->getAllWindows();
    }
    
    // Legacy implementation for backward compatibility
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
        // KDE Plasma detected but window management not implemented - return empty list silently
        return windows;
    } else if (desktop.contains("weston")) {
        // Weston detected but window management not implemented - return empty list silently
        return windows;
    } else if (desktop.contains("hypr")) {
        // Hyprland detected but window management not implemented - return empty list silently
        return windows;
    } else if (desktop.contains("river")) {
        // River detected but window management not implemented - return empty list silently
        return windows;
    } else if (desktop.contains("wayfire")) {
        // Wayfire detected but window management not implemented - return empty list silently
        return windows;
    } else if (desktop.contains("labwc")) {
        // Labwc detected but window management not implemented - return empty list silently
        return windows;
    } else {
        // Unknown compositor detected but window management not implemented - return empty list silently
        return windows;
    }

    return windows;
}


QString WaylandSupport::getApplicationIcon(const QString& appId, const QString& wmClass)
{
    DesktopDataStore* dataStore = DesktopDataStore::instance();
    if (dataStore) {
        // Search for applications by executable name or WM class
        QList<DesktopEntryData> matches = dataStore->searchByExecutable(appId);
        if (matches.isEmpty() && !wmClass.isEmpty()) {
            // Try searching by WM class in the startupWMClass field
            QList<DesktopEntryData> allEntries = dataStore->getAllDesktopEntries();
            foreach (const DesktopEntryData& entry, allEntries) {
                if (entry.startupWMClass == wmClass) {
                    matches.append(entry);
                    break;
                }
            }
        }
        if (!matches.isEmpty()) {
            return matches.first().icon;
        }
    }
    
    // Fallback if DesktopDataStore is not available
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
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->activateWindow(appId);
    }
    
    // Legacy implementation for backward compatibility
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

    // First, try to use the extension's safe ActivateWindow method
    QDBusInterface extensionInterface("org.hdepanel.WindowList",
        "/org/hdepanel/WindowList",
        "org.hdepanel.WindowList",
        QDBusConnection::sessionBus());
    
    if (extensionInterface.isValid()) {
        QDBusReply<bool> reply = extensionInterface.call("ActivateWindow", appId);
        
        if (reply.isValid()) {
            if (reply.value()) {
                qDebug() << "WaylandSupport: Successfully activated window for appId:" << appId;
                return true;
            } else {
                qDebug() << "WaylandSupport: No window found for appId:" << appId;
                return false;
            }
        } else {
            qDebug() << "WaylandSupport: Extension ActivateWindow call failed:" << reply.error().message();
            // Fall through to unsafe method
        }
    } else {
        qDebug() << "WaylandSupport: Extension not available for window activation, falling back to unsafe mode";
        // Fall through to unsafe method
    }

    // Fallback: use unsafe mode (requires extra permissions)
    QDBusInterface shellInterface("org.gnome.Shell",
        "/org/gnome/Shell",
        "org.gnome.Shell",
        QDBusConnection::sessionBus());

    // First, enable unsafe mode
    QDBusMessage unsafeMessage = shellInterface.call("Eval", "global.context.unsafe_mode = true");
    if (unsafeMessage.type() != QDBusMessage::ReplyMessage || unsafeMessage.arguments().size() < 2) {
        qDebug() << "WaylandSupport: Failed to enable unsafe mode for window activation";
        return false;
    }
    bool unsafeSuccess = unsafeMessage.arguments()[0].toBool();
    if (!unsafeSuccess) {
        qDebug() << "WaylandSupport: Unsafe mode not enabled for window activation";
        qDebug() << "WaylandSupport: Please install the HDEPanel extension from gnome-extension/hdepanel-window-list@hdepanel";
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

    QDBusMessage message = shellInterface.call("Eval", jsCode);
    
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

bool WaylandSupport::closeWindow(const QString& appId)
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->closeWindow(appId);
    }
    
    // Legacy implementation for backward compatibility
    if (!m_initialized) {
        qDebug() << "WaylandSupport::closeWindow() - not initialized";
        return false;
    }
    
    // Check if we're running on GNOME (including Unity compatibility mode)
    QString desktop = qgetenv("XDG_CURRENT_DESKTOP").toLower();
    if (!desktop.contains("gnome") && !desktop.contains("unity")) {
        qDebug() << "WaylandSupport: Window closing only supported on GNOME/Unity";
        return false;
    }

    // Try to use the extension's safe CloseWindow method
    QDBusInterface extensionInterface("org.hdepanel.WindowList",
        "/org/hdepanel/WindowList",
        "org.hdepanel.WindowList",
        QDBusConnection::sessionBus());
    
    if (extensionInterface.isValid()) {
        QDBusReply<bool> reply = extensionInterface.call("CloseWindow", appId);
        
        if (reply.isValid()) {
            if (reply.value()) {
                qDebug() << "WaylandSupport: Successfully closed window for appId:" << appId;
                return true;
            } else {
                qDebug() << "WaylandSupport: No window found to close for appId:" << appId;
                return false;
            }
        } else {
            qDebug() << "WaylandSupport: Extension CloseWindow call failed:" << reply.error().message();
            return false;
        }
    } else {
        qDebug() << "WaylandSupport: Extension not available for window closing";
        return false;
    }
}

bool WaylandSupport::minimizeWindow(const QString& appId)
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->minimizeWindow(appId);
    }
    return false; // Not implemented in legacy mode
}

bool WaylandSupport::maximizeWindow(const QString& appId)
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->maximizeWindow(appId);
    }
    return false; // Not implemented in legacy mode
}

bool WaylandSupport::unmaximizeWindow(const QString& appId)
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->unmaximizeWindow(appId);
    }
    return false; // Not implemented in legacy mode
}

bool WaylandSupport::moveWindowToWorkspace(const QString& appId, int workspace)
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->moveWindowToWorkspace(appId, workspace);
    }
    return false; // Not implemented in legacy mode
}

bool WaylandSupport::moveWindowToMonitor(const QString& appId, int monitor)
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->moveWindowToMonitor(appId, monitor);
    }
    return false; // Not implemented in legacy mode
}

// Window manager information methods
QString WaylandSupport::getWindowManagerName() const
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->getWindowManagerName();
    }
    return "Legacy Wayland Support";
}

WindowManager::Type WaylandSupport::getWindowManagerType() const
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->getType();
    }
    return WindowManager::Type::Unknown;
}

QString WaylandSupport::getWindowManagerVersion() const
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->getVersion();
    }
    return "Unknown";
}

bool WaylandSupport::supportsWorkspaces() const
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->supportsWorkspaces();
    }
    return false;
}

bool WaylandSupport::supportsMultipleMonitors() const
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->supportsMultipleMonitors();
    }
    return false;
}

int WaylandSupport::getCurrentWorkspace() const
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->getCurrentWorkspace();
    }
    return 0;
}

int WaylandSupport::getWorkspaceCount() const
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->getWorkspaceCount();
    }
    return 1;
}

QStringList WaylandSupport::getWorkspaceNames() const
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->getWorkspaceNames();
    }
    return QStringList() << "Workspace 1";
}

int WaylandSupport::getMonitorCount() const
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->getMonitorCount();
    }
    return 1;
}

QStringList WaylandSupport::getMonitorNames() const
{
    if (m_useWindowManager && m_windowManager) {
        return m_windowManager->getMonitorNames();
    }
    return QStringList() << "Monitor 1";
}

// Signal handlers for window manager events
void WaylandSupport::onWindowManagerWindowsUpdated(const QList<WaylandWindow>& windows)
{
    emit windowsUpdated(windows);
}

void WaylandSupport::onWindowManagerWindowCreated(const WaylandWindow& window)
{
    emit windowCreated(window);
}

void WaylandSupport::onWindowManagerWindowClosed(const WaylandWindow& window)
{
    emit windowClosed(window);
}

void WaylandSupport::onWindowManagerWindowActivated(const WaylandWindow& window)
{
    emit windowActivated(window);
}

void WaylandSupport::onWindowManagerWorkspaceChanged(int workspace)
{
    emit workspaceChanged(workspace);
}

void WaylandSupport::onWindowManagerMonitorChanged(int monitor)
{
    emit monitorChanged(monitor);
}

QList<WaylandWindow> WaylandSupport::getWindowsFromExtension()
{
    QList<WaylandWindow> windows;
    
    // Try to call our GNOME Shell extension
    QDBusInterface iface("org.gnome.Shell",
                         "/org/hdepanel/WindowList",
                         "org.hdepanel.WindowList",
                         QDBusConnection::sessionBus());
    
    if (!iface.isValid()) {
        // Extension not installed or not enabled
        return windows;
    }
    
    QDBusReply<QString> reply = iface.call("GetWindows");
    
    if (!reply.isValid()) {
        qDebug() << "WaylandSupport: HDEPanel extension call failed:" << reply.error().message();
        return windows;
    }
    
    QString jsonData = reply.value();
    if (jsonData.isEmpty()) {
        return windows;
    }
    
    // Parse JSON array
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qDebug() << "WaylandSupport: JSON parse error:" << error.errorString();
        return windows;
    }
    
    if (!doc.isArray()) {
        qDebug() << "WaylandSupport: Expected JSON array";
        return windows;
    }
    
    QJsonArray windowsArray = doc.array();
    
    for (const QJsonValue& value : windowsArray) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject obj = value.toObject();
        QString title = obj["title"].toString();
        QString appId = obj["app_id"].toString();
        QString wmClass = obj["wm_class"].toString();
        
        if (title.isEmpty()) {
            continue;
        }
        
        WaylandWindow window;
        
        // Basic identification
        window.title = title;
        window.appId = appId;
        window.wmClass = wmClass;
        window.wmInstanceClass = obj["wm_instance_class"].toString();
        window.role = obj["role"].toString();
        
        // Workspace/Desktop information
        window.workspaceIndex = obj["workspace_index"].toInt();
        window.workspaceName = obj["workspace_name"].toString();
        window.onAllWorkspaces = obj["on_all_workspaces"].toBool();
        window.isOnCurrentWorkspace = obj["is_on_current_workspace"].toBool();
        
        // Screen/Monitor information
        window.monitorIndex = obj["monitor_index"].toInt();
        
        // Geometry
        window.x = obj["x"].toInt();
        window.y = obj["y"].toInt();
        window.width = obj["width"].toInt();
        window.height = obj["height"].toInt();
        
        // Window state
        window.visible = obj["visible"].toBool();
        window.focused = obj["focused"].toBool();
        window.minimized = obj["minimized"].toBool();
        window.maximized = obj["maximized"].toBool();
        window.maximizedHorizontally = obj["maximized_horizontally"].toBool();
        window.maximizedVertically = obj["maximized_vertically"].toBool();
        
        // Window flags
        window.demandsAttention = obj["demands_attention"].toBool();
        window.urgent = obj["urgent"].toBool();
        window.skipTaskbar = obj["skip_taskbar"].toBool();
        window.skipPager = obj["skip_pager"].toBool();
        window.decorated = obj["decorated"].toBool();
        window.resizable = obj["resizable"].toBool();
        window.moveable = obj["moveable"].toBool();
        
        // Client type
        window.isWayland = obj["is_wayland"].toBool();
        window.isX11 = obj["is_x11"].toBool();
        window.clientType = obj["client_type"].toString();
        
        // Process information
        window.pid = obj["pid"].toInt();
        window.sandboxedAppId = obj["sandboxed_app_id"].toString();
        
        // Visual properties
        window.opacity = obj["opacity"].toDouble();
        
        // Group information
        window.hasGroup = obj["has_group"].toBool();
        window.groupLeaderId = obj["group_leader_id"].toVariant().toULongLong();
        
        // Timestamps
        window.createdTime = obj["created_time"].toVariant().toULongLong();
        window.focusTime = obj["focus_time"].toVariant().toULongLong();
        
        // Legacy fields for compatibility
        window.surface = reinterpret_cast<void*>(
            static_cast<quintptr>(obj["id"].toVariant().toULongLong()));
        window.toplevel = nullptr;
        
        // Try to find icon using both app_id and wm_class
        window.iconName = getApplicationIcon(appId, wmClass);
        
        windows.append(window);
    }
    
    return windows;
}

QList<WaylandWindow> WaylandSupport::getWindowsFromGnomeShell()
{
    QList<WaylandWindow> windows;
    
    // First, try to use our extension (safe method)
    windows = getWindowsFromExtension();
    
    // If extension returned results, use them
    if (!windows.isEmpty()) {
        return windows;
    }
    
    // Otherwise, fall back to unsafe eval method
    static bool warnedAboutUnsafeMode = false;
    if (!warnedAboutUnsafeMode) {
        qDebug() << "WaylandSupport: HDEPanel extension not found, falling back to unsafe mode";
        qDebug() << "WaylandSupport: Install the extension from gnome-extension/hdepanel-window-list@hdepanel";
        warnedAboutUnsafeMode = true;
    }
    
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
