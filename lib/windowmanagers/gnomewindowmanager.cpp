/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * This Files has been imported to hde from qtpanel
 *
 * Copyright: 2015-2025 Haydar Alkaduhimi
 * Authors:
 *   Haydar Alkaduhimi <haydar@developing4all.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "gnomewindowmanager.h"
#include "../waylandsupport.h"
#include "../desktopapplications.h"
#include "../desktopdatastore.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>

GNOMEWindowManager::GNOMEWindowManager(QObject* parent)
    : WindowManager(parent)
    , m_extensionInterface(nullptr)
    , m_shellInterface(nullptr)
{
    setupUpdateTimer(500);
}

GNOMEWindowManager::~GNOMEWindowManager()
{
    if (m_extensionInterface) {
        delete m_extensionInterface;
    }
    if (m_shellInterface) {
        delete m_shellInterface;
    }
}

bool GNOMEWindowManager::initialize()
{
    if (m_initialized) {
        return true;
    }

    if (!isAvailable()) {
        qDebug() << "GNOMEWindowManager: GNOME Shell not available";
        return false;
    }

    // Initialize D-Bus interfaces
    m_extensionInterface = new QDBusInterface("org.hdepanel.WindowList",
        "/org/hdepanel/WindowList",
        "org.hdepanel.WindowList",
        QDBusConnection::sessionBus(), this);

    m_shellInterface = new QDBusInterface("org.gnome.Shell",
        "/org/gnome/Shell",
        "org.gnome.Shell",
        QDBusConnection::sessionBus(), this);

    if (!m_extensionInterface->isValid() && !m_shellInterface->isValid()) {
        qDebug() << "GNOMEWindowManager: No valid D-Bus interfaces available";
        return false;
    }

    m_initialized = true;
    qDebug() << "GNOMEWindowManager: Initialized successfully";
    return true;
}

bool GNOMEWindowManager::isAvailable()
{
    QString desktop = qgetenv("XDG_CURRENT_DESKTOP").toLower();
    QString sessionType = qgetenv("XDG_SESSION_TYPE").toLower();
    
    return (desktop.contains("gnome") || desktop.contains("unity")) &&
           (sessionType == "wayland" || sessionType == "x11");
}

QList<WaylandWindow> GNOMEWindowManager::getAllWindows()
{
    QList<WaylandWindow> windows;

    if (!m_initialized) {
        return windows;
    }

    // Try extension first (safer method)
    windows = getWindowsFromExtension();
    if (!windows.isEmpty()) {
        return windows;
    }

    // Fallback to direct GNOME Shell access
    windows = getWindowsFromGnomeShell();
    return windows;
}

QList<WaylandWindow> GNOMEWindowManager::getWindowsFromExtension()
{
    QList<WaylandWindow> windows;
    
    if (!m_extensionInterface || !m_extensionInterface->isValid()) {
        return windows;
    }
    
    QDBusReply<QString> reply = m_extensionInterface->call("GetWindows");
    
    if (!reply.isValid()) {
        qDebug() << "GNOMEWindowManager: Extension call failed:" << reply.error().message();
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
        qDebug() << "GNOMEWindowManager: JSON parse error:" << error.errorString();
        return windows;
    }
    
    if (!doc.isArray()) {
        qDebug() << "GNOMEWindowManager: Expected JSON array";
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
        
        // Set icon name - try to get from JSON first, then fallback to app detection
        window.iconName = obj["icon_name"].toString();
        if (window.iconName.isEmpty()) {
            // Fallback to app detection using DesktopDataStore
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
                    window.iconName = matches.first().icon;
                } else {
                    // Final fallback
                    QString fallbackIcon = !appId.isEmpty() ? appId.toLower() : wmClass.toLower();
                    window.iconName = fallbackIcon.isEmpty() ? "application-x-executable" : fallbackIcon;
                }
            } else {
                // Final fallback
                QString fallbackIcon = !appId.isEmpty() ? appId.toLower() : wmClass.toLower();
                window.iconName = fallbackIcon.isEmpty() ? "application-x-executable" : fallbackIcon;
            }
        }
        
        windows.append(window);
    }
    
    return windows;
}

QList<WaylandWindow> GNOMEWindowManager::getWindowsFromGnomeShell()
{
    // Fallback implementation using direct GNOME Shell D-Bus calls
    // This is the same as the current WaylandSupport implementation
    QList<WaylandWindow> windows;
    
    if (!m_shellInterface || !m_shellInterface->isValid()) {
        return windows;
    }
    
    // Enable unsafe mode first
    QDBusMessage unsafeMessage = m_shellInterface->call("Eval", "global.context.unsafe_mode = true");
    if (unsafeMessage.type() != QDBusMessage::ReplyMessage) {
        qDebug() << "GNOMEWindowManager: Failed to enable unsafe mode";
        return windows;
    }
    
    // Get window information using JavaScript
    QString jsCode = R"(
        global.get_window_actors()
            .filter(w => w.meta_window && w.meta_window.get_title)
            .map(w => {
                const mw = w.meta_window;
                // Get maximized state with GNOME 49+ compatibility
                let maximized = 0;
                if (typeof mw.get_maximized === 'function') {
                    maximized = mw.get_maximized();
                } else if (typeof mw.get_maximized_horizontally === 'function' && typeof mw.get_maximized_vertically === 'function') {
                    // GNOME 49+ uses separate methods
                    // Meta.MaximizeFlags: HORIZONTAL = 1, VERTICAL = 2
                    maximized = (mw.get_maximized_horizontally() ? 1 : 0) |
                               (mw.get_maximized_vertically() ? 2 : 0);
                }
                return {
                    id: mw.get_id(),
                    title: mw.get_title() || '',
                    app_id: mw.get_gtk_application_id() || mw.get_wm_class() || '',
                    wm_class: mw.get_wm_class() || '',
                    visible: !mw.minimized && mw.showing_on_its_workspace(),
                    focused: mw.has_focus(),
                    minimized: mw.minimized,
                    maximized: maximized !== 0,
                    is_wayland: mw.get_client_type() === 1
                };
            })
            .filter(w => w.title && w.wm_class && !w.wm_class.includes('hdepanel'))
            .map(JSON.stringify)
            .join('\n')
    )";
    
    QDBusMessage message = m_shellInterface->call("Eval", jsCode);
    
    if (message.type() == QDBusMessage::ReplyMessage && message.arguments().size() >= 2) {
        bool success = message.arguments()[0].toBool();
        QString jsResult = message.arguments()[1].toString();
        
        if (success && !jsResult.isEmpty()) {
            // Parse the result (simplified for this example)
            QStringList lines = jsResult.split('\n', Qt::SkipEmptyParts);
            for (const QString& line : lines) {
                QJsonParseError lineError;
                QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &lineError);
                if (lineError.error == QJsonParseError::NoError) {
                    QJsonObject obj = doc.object();
                    WaylandWindow window;
                    window.title = obj["title"].toString();
                    window.appId = obj["app_id"].toString();
                    window.wmClass = obj["wm_class"].toString();
                    window.visible = obj["visible"].toBool();
                    window.focused = obj["focused"].toBool();
                    window.minimized = obj["minimized"].toBool();
                    window.maximized = obj["maximized"].toBool();
                    window.isWayland = obj["is_wayland"].toBool();
                    window.surface = reinterpret_cast<void*>(
                        static_cast<quintptr>(obj["id"].toVariant().toULongLong()));
                    windows.append(window);
                }
            }
        }
    }
    
    return windows;
}

bool GNOMEWindowManager::activateWindow(const QString& appId)
{
    if (!m_initialized) {
        return false;
    }

    // Try extension first
    if (m_extensionInterface && m_extensionInterface->isValid()) {
        QDBusReply<bool> reply = m_extensionInterface->call("ActivateWindow", appId);
        if (reply.isValid()) {
            return reply.value();
        }
    }

    // Fallback to shell method
    return executeWindowAction(appId, "activate");
}

bool GNOMEWindowManager::closeWindow(const QString& appId)
{
    if (!m_initialized) {
        return false;
    }

    // Try extension first
    if (m_extensionInterface && m_extensionInterface->isValid()) {
        QDBusReply<bool> reply = m_extensionInterface->call("CloseWindow", appId);
        if (reply.isValid()) {
            return reply.value();
        }
    }

    // Fallback to shell method
    return executeWindowAction(appId, "close");
}

bool GNOMEWindowManager::minimizeWindow(const QString& appId)
{
    return executeWindowAction(appId, "minimize");
}

bool GNOMEWindowManager::maximizeWindow(const QString& appId)
{
    return executeWindowAction(appId, "maximize");
}

bool GNOMEWindowManager::unmaximizeWindow(const QString& appId)
{
    return executeWindowAction(appId, "unmaximize");
}

bool GNOMEWindowManager::moveWindowToWorkspace(const QString& appId, int workspace)
{
    return moveWindowToWorkspaceScript(appId, workspace);
}

bool GNOMEWindowManager::moveWindowToMonitor(const QString& appId, int monitor)
{
    return moveWindowToMonitorScript(appId, monitor);
}

QString GNOMEWindowManager::getVersion() const
{
    if (m_shellInterface && m_shellInterface->isValid()) {
        QDBusReply<QString> reply = m_shellInterface->call("Version");
        if (reply.isValid()) {
            return reply.value();
        }
    }
    return "Unknown";
}

int GNOMEWindowManager::getCurrentWorkspace() const
{
    if (m_shellInterface && m_shellInterface->isValid()) {
        QString jsCode = "global.workspace_manager.get_active_workspace().index()";
        QDBusMessage message = m_shellInterface->call("Eval", jsCode);
        if (message.type() == QDBusMessage::ReplyMessage && message.arguments().size() >= 2) {
            bool success = message.arguments()[0].toBool();
            if (success) {
                return message.arguments()[1].toString().toInt();
            }
        }
    }
    return 0;
}

int GNOMEWindowManager::getWorkspaceCount() const
{
    if (m_shellInterface && m_shellInterface->isValid()) {
        QString jsCode = "global.workspace_manager.get_n_workspaces()";
        QDBusMessage message = m_shellInterface->call("Eval", jsCode);
        if (message.type() == QDBusMessage::ReplyMessage && message.arguments().size() >= 2) {
            bool success = message.arguments()[0].toBool();
            if (success) {
                return message.arguments()[1].toString().toInt();
            }
        }
    }
    return 1;
}

QStringList GNOMEWindowManager::getWorkspaceNames() const
{
    QStringList names;
    int count = getWorkspaceCount();
    for (int i = 0; i < count; ++i) {
        names << QString("Workspace %1").arg(i + 1);
    }
    return names;
}

int GNOMEWindowManager::getMonitorCount() const
{
    if (m_shellInterface && m_shellInterface->isValid()) {
        QString jsCode = "global.display.get_n_monitors()";
        QDBusMessage message = m_shellInterface->call("Eval", jsCode);
        if (message.type() == QDBusMessage::ReplyMessage && message.arguments().size() >= 2) {
            bool success = message.arguments()[0].toBool();
            if (success) {
                return message.arguments()[1].toString().toInt();
            }
        }
    }
    return 1;
}

QStringList GNOMEWindowManager::getMonitorNames() const
{
    QStringList names;
    int count = getMonitorCount();
    for (int i = 0; i < count; ++i) {
        names << QString("Monitor %1").arg(i + 1);
    }
    return names;
}

bool GNOMEWindowManager::executeWindowAction(const QString& appId, const QString& action)
{
    if (!m_shellInterface || !m_shellInterface->isValid()) {
        return false;
    }

    QString script = findWindowScript(appId, action);
    return callShellMethod("Eval", script);
}

QString GNOMEWindowManager::findWindowScript(const QString& appId, const QString& action)
{
    return QString(R"(
        (function() {
            const windows = global.get_window_actors()
                .filter(w => w.meta_window && w.meta_window.get_title)
                .map(w => {
                    const mw = w.meta_window;
                    const appId = mw.get_gtk_application_id() || mw.get_wm_class() || '';
                    const wmClass = mw.get_wm_class() || '';
                    return {
                        app_id: appId,
                        wm_class: wmClass,
                        meta_window: mw
                    };
                })
                .filter(w => w.app_id === '%1' || w.wm_class === '%1');
            
            if (windows.length > 0) {
                const window = windows[0];
                switch('%2') {
                    case 'activate':
                        window.meta_window.activate(global.get_current_time());
                        break;
                    case 'close':
                        window.meta_window.delete(global.get_current_time());
                        break;
                    case 'minimize':
                        window.meta_window.minimize();
                        break;
                    case 'maximize':
                        window.meta_window.maximize(1);
                        break;
                    case 'unmaximize':
                        window.meta_window.unmaximize(1);
                        break;
                }
                return true;
            }
            return false;
        })()
    )").arg(appId, action);
}

bool GNOMEWindowManager::moveWindowToWorkspaceScript(const QString& appId, int workspace)
{
    if (!m_shellInterface || !m_shellInterface->isValid()) {
        return false;
    }

    QString script = QString(R"(
        (function() {
            const targetWorkspace = global.workspace_manager.get_workspace_by_index(%2);
            if (!targetWorkspace) return false;
            
            const windows = global.get_window_actors()
                .filter(w => w.meta_window && w.meta_window.get_title)
                .map(w => {
                    const mw = w.meta_window;
                    const appId = mw.get_gtk_application_id() || mw.get_wm_class() || '';
                    const wmClass = mw.get_wm_class() || '';
                    return {
                        app_id: appId,
                        wm_class: wmClass,
                        meta_window: mw
                    };
                })
                .filter(w => w.app_id === '%1' || w.wm_class === '%1');
            
            if (windows.length > 0) {
                windows[0].meta_window.change_workspace(targetWorkspace);
                return true;
            }
            return false;
        })()
    )").arg(appId).arg(workspace);

    return callShellMethod("Eval", script);
}

bool GNOMEWindowManager::moveWindowToMonitorScript(const QString& appId, int monitor)
{
    if (!m_shellInterface || !m_shellInterface->isValid()) {
        return false;
    }

    QString script = QString(R"(
        (function() {
            const windows = global.get_window_actors()
                .filter(w => w.meta_window && w.meta_window.get_title)
                .map(w => {
                    const mw = w.meta_window;
                    const appId = mw.get_gtk_application_id() || mw.get_wm_class() || '';
                    const wmClass = mw.get_wm_class() || '';
                    return {
                        app_id: appId,
                        wm_class: wmClass,
                        meta_window: mw
                    };
                })
                .filter(w => w.app_id === '%1' || w.wm_class === '%1');
            
            if (windows.length > 0) {
                // Move to monitor (simplified implementation)
                const rect = windows[0].meta_window.get_frame_rect();
                windows[0].meta_window.move_resize_frame(1, %2 * 1920, rect.y, rect.width, rect.height);
                return true;
            }
            return false;
        })()
    )").arg(appId).arg(monitor);

    return callShellMethod("Eval", script);
}

bool GNOMEWindowManager::callShellMethod(const QString& method, const QString& script)
{
    if (!m_shellInterface || !m_shellInterface->isValid()) {
        return false;
    }

    QDBusMessage message = m_shellInterface->call(method, script);
    
    if (message.type() == QDBusMessage::ReplyMessage && message.arguments().size() >= 2) {
        bool success = message.arguments()[0].toBool();
        return success;
    }
    
    return false;
}

void GNOMEWindowManager::updateWindows()
{
    if (m_monitoring) {
        QList<WaylandWindow> windows = getAllWindows();
        emitWindowsUpdated(windows);
    }
}
