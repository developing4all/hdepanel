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

#include "hyprlandwindowmanager.h"
#include "../waylandsupport.h"
#include <QDebug>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegExp>

HyprlandWindowManager::HyprlandWindowManager(QObject* parent)
    : WindowManager(parent)
{
    setupUpdateTimer(1000); // Hyprland can be polled less frequently
}

HyprlandWindowManager::~HyprlandWindowManager()
{
}

bool HyprlandWindowManager::initialize()
{
    if (m_initialized) {
        return true;
    }

    if (!isAvailable()) {
        qDebug() << "HyprlandWindowManager: Hyprland not available";
        return false;
    }

    // Test hyprctl connectivity
    QString version = getVersion();
    if (version == "Unknown") {
        qDebug() << "HyprlandWindowManager: Failed to get Hyprland version";
        return false;
    }

    m_initialized = true;
    qDebug() << "HyprlandWindowManager: Initialized successfully, version:" << version;
    return true;
}

bool HyprlandWindowManager::isAvailable()
{
    // Check if hyprctl is available and Hyprland is running
    QProcess process;
    process.start("hyprctl", QStringList() << "version");
    process.waitForFinished(1000);
    
    return process.exitCode() == 0;
}

QList<WaylandWindow> HyprlandWindowManager::getAllWindows()
{
    QList<WaylandWindow> windows;
    
    if (!m_initialized) {
        return windows;
    }

    QJsonArray windowsArray = getHyprctlJson("clients -j");
    return parseWindowsFromJson(windowsArray);
}

bool HyprlandWindowManager::activateWindow(const QString& appId)
{
    if (!m_initialized) {
        return false;
    }

    QString windowAddress = findWindowAddress(appId);
    if (windowAddress.isEmpty()) {
        qDebug() << "HyprlandWindowManager: No window found for appId:" << appId;
        return false;
    }

    return executeWindowAction(windowAddress, "focus");
}

bool HyprlandWindowManager::closeWindow(const QString& appId)
{
    if (!m_initialized) {
        return false;
    }

    QString windowAddress = findWindowAddress(appId);
    if (windowAddress.isEmpty()) {
        qDebug() << "HyprlandWindowManager: No window found for appId:" << appId;
        return false;
    }

    return executeWindowAction(windowAddress, "close");
}

bool HyprlandWindowManager::minimizeWindow(const QString& appId)
{
    if (!m_initialized) {
        return false;
    }

    QString windowAddress = findWindowAddress(appId);
    if (windowAddress.isEmpty()) {
        return false;
    }

    return executeWindowAction(windowAddress, "minimize");
}

bool HyprlandWindowManager::maximizeWindow(const QString& appId)
{
    if (!m_initialized) {
        return false;
    }

    QString windowAddress = findWindowAddress(appId);
    if (windowAddress.isEmpty()) {
        return false;
    }

    return executeWindowAction(windowAddress, "maximize");
}

bool HyprlandWindowManager::unmaximizeWindow(const QString& appId)
{
    if (!m_initialized) {
        return false;
    }

    QString windowAddress = findWindowAddress(appId);
    if (windowAddress.isEmpty()) {
        return false;
    }

    return executeWindowAction(windowAddress, "unmaximize");
}

bool HyprlandWindowManager::moveWindowToWorkspace(const QString& appId, int workspace)
{
    if (!m_initialized) {
        return false;
    }

    QString windowAddress = findWindowAddress(appId);
    if (windowAddress.isEmpty()) {
        return false;
    }

    return moveWindowToWorkspaceHyprland(windowAddress, workspace);
}

bool HyprlandWindowManager::moveWindowToMonitor(const QString& appId, int monitor)
{
    if (!m_initialized) {
        return false;
    }

    QString windowAddress = findWindowAddress(appId);
    if (windowAddress.isEmpty()) {
        return false;
    }

    return moveWindowToMonitorHyprland(windowAddress, monitor);
}

QString HyprlandWindowManager::getVersion() const
{
    QString output = executeHyprctl("version");
    if (output.isEmpty()) {
        return QString();
    }
    
    // Parse version from plain text output
    // Format: "Hyprland, built from branch  at commit 918d8340afd652b011b937d29d5eea0be08467f5"
    QRegExp commitRegex("commit ([a-f0-9]+)");
    if (commitRegex.indexIn(output) != -1) {
        return commitRegex.cap(1);
    }
    
    return QString();
}

int HyprlandWindowManager::getCurrentWorkspace() const
{
    QString output = executeHyprctl("activeworkspace");
    if (output.isEmpty()) {
        return 1; // Default to workspace 1
    }
    
    // Parse workspace ID from plain text output
    // Format: "workspace ID 1 (1) on monitor eDP-1:"
    QRegExp workspaceRegex("workspace ID (\\d+)");
    if (workspaceRegex.indexIn(output) != -1) {
        return workspaceRegex.cap(1).toInt();
    }
    
    return 1; // Default to workspace 1
}

int HyprlandWindowManager::getWorkspaceCount() const
{
    QJsonArray workspaces = getWorkspaces();
    return workspaces.size();
}

QStringList HyprlandWindowManager::getWorkspaceNames() const
{
    QStringList names;
    QJsonArray workspaces = getWorkspaces();
    
    for (const QJsonValue& value : workspaces) {
        QJsonObject workspace = value.toObject();
        QString name = workspace["name"].toString();
        if (name.isEmpty()) {
            name = QString("Workspace %1").arg(workspace["id"].toInt());
        }
        names << name;
    }
    
    return names;
}

int HyprlandWindowManager::getMonitorCount() const
{
    QJsonArray monitors = getMonitors();
    return monitors.size();
}

QStringList HyprlandWindowManager::getMonitorNames() const
{
    QStringList names;
    QJsonArray monitors = getMonitors();
    
    for (const QJsonValue& value : monitors) {
        QJsonObject monitor = value.toObject();
        QString name = monitor["name"].toString();
        if (name.isEmpty()) {
            name = QString("Monitor %1").arg(monitor["id"].toInt());
        }
        names << name;
    }
    
    return names;
}

void HyprlandWindowManager::updateWindows()
{
    if (m_monitoring) {
        QList<WaylandWindow> windows = getAllWindows();
        emitWindowsUpdated(windows);
    }
}

// Private helper methods
QString HyprlandWindowManager::executeHyprctl(const QString& command) const
{
    QProcess process;
    QStringList args = command.split(' ', Qt::SkipEmptyParts);
    qDebug() << "HyprlandWindowManager: Executing hyprctl with args:" << args;
    
    process.start("hyprctl", args);
    process.waitForFinished(2000);
    
    QString output = process.readAllStandardOutput();
    QString error = process.readAllStandardError();
    
    qDebug() << "HyprlandWindowManager: hyprctl output length:" << output.length() << "error:" << error;
    
    if (process.exitCode() != 0) {
        qDebug() << "HyprlandWindowManager: hyprctl command failed:" << command << "exit code:" << process.exitCode() << "error:" << error;
        return QString();
    }
    
    if (output.isEmpty()) {
        qDebug() << "HyprlandWindowManager: hyprctl command returned empty output:" << command;
    }
    
    return output;
}

QJsonArray HyprlandWindowManager::getHyprctlJson(const QString& command) const
{
    QString output = executeHyprctl(command);
    if (output.isEmpty()) {
        return QJsonArray();
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qDebug() << "HyprlandWindowManager: JSON parse error:" << error.errorString();
        return QJsonArray();
    }
    
    if (doc.isArray()) {
        return doc.array();
    } else if (doc.isObject()) {
        QJsonArray array;
        array.append(doc.object());
        return array;
    }
    
    return QJsonArray();
}

QJsonObject HyprlandWindowManager::getHyprctlObject(const QString& command) const
{
    QString output = executeHyprctl(command);
    if (output.isEmpty()) {
        return QJsonObject();
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        qDebug() << "HyprlandWindowManager: JSON parse error:" << error.errorString();
        return QJsonObject();
    }
    
    return doc.object();
}

QList<WaylandWindow> HyprlandWindowManager::parseWindowsFromJson(const QJsonArray& windows) const
{
    QList<WaylandWindow> result;
    
    for (const QJsonValue& value : windows) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject obj = value.toObject();
        QString title = obj["title"].toString();
        QString appId = obj["class"].toString();
        QString wmClass = obj["class"].toString();
        
        if (title.isEmpty() || appId.isEmpty()) {
            continue;
        }
        
        WaylandWindow window;
        
        // Basic identification
        window.title = title;
        window.appId = appId;
        window.wmClass = wmClass;
        window.wmInstanceClass = obj["initialClass"].toString();
        
        // Workspace information
        window.workspaceIndex = obj["workspace"].toObject()["id"].toInt();
        window.workspaceName = QString("Workspace %1").arg(window.workspaceIndex);
        window.onAllWorkspaces = false;
        window.isOnCurrentWorkspace = (window.workspaceIndex == getCurrentWorkspace());
        
        // Monitor information
        window.monitorIndex = obj["monitor"].toInt();
        
        // Geometry
        QJsonObject at = obj["at"].toObject();
        QJsonObject size = obj["size"].toObject();
        window.x = at["x"].toInt();
        window.y = at["y"].toInt();
        window.width = size["width"].toInt();
        window.height = size["height"].toInt();
        
        // Window state
        window.visible = obj["visible"].toBool();
        window.focused = obj["focused"].toBool();
        window.minimized = obj["minimized"].toBool();
        window.maximized = obj["maximized"].toBool();
        window.maximizedHorizontally = obj["maximizedHorizontally"].toBool();
        window.maximizedVertically = obj["maximizedVertically"].toBool();
        
        // Window flags
        window.demandsAttention = obj["urgent"].toBool();
        window.urgent = obj["urgent"].toBool();
        window.skipTaskbar = obj["skipTaskbar"].toBool();
        window.skipPager = obj["skipPager"].toBool();
        window.decorated = obj["decorated"].toBool();
        window.resizable = obj["resizable"].toBool();
        window.moveable = obj["moveable"].toBool();
        
        // Client type
        window.isWayland = obj["wayland"].toBool();
        window.isX11 = !window.isWayland;
        window.clientType = window.isWayland ? "wayland" : "x11";
        
        // Process information
        window.pid = obj["pid"].toInt();
        
        // Visual properties
        window.opacity = obj["opacity"].toDouble();
        
        // Group information
        window.hasGroup = obj["grouped"].toBool();
        window.groupLeaderId = 0; // Hyprland doesn't expose this directly
        
        // Timestamps
        window.createdTime = 0; // Hyprland doesn't expose this
        window.focusTime = 0; // Hyprland doesn't expose this
        
        // Legacy fields for compatibility
        window.surface = reinterpret_cast<void*>(
            static_cast<quintptr>(obj["address"].toString().toULongLong(nullptr, 16)));
        window.toplevel = nullptr;
        
        result.append(window);
    }
    
    return result;
}

QString HyprlandWindowManager::findWindowAddress(const QString& appId) const
{
    QJsonArray windows = getHyprctlJson("clients -j");
    
    for (const QJsonValue& value : windows) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject obj = value.toObject();
        QString windowClass = obj["class"].toString();
        QString title = obj["title"].toString();
        
        if (windowClass == appId || title == appId) {
            return obj["address"].toString();
        }
    }
    
    return QString();
}

bool HyprlandWindowManager::executeWindowAction(const QString& windowAddress, const QString& action) const
{
    QString command;
    
    if (action == "focus") {
        command = QString("dispatch focuswindow address:%1").arg(windowAddress);
    } else if (action == "close") {
        command = QString("dispatch closewindow address:%1").arg(windowAddress);
    } else if (action == "minimize") {
        command = QString("dispatch minimize address:%1").arg(windowAddress);
    } else if (action == "maximize") {
        command = QString("dispatch maximize address:%1").arg(windowAddress);
    } else if (action == "unmaximize") {
        command = QString("dispatch unmaximize address:%1").arg(windowAddress);
    } else {
        qDebug() << "HyprlandWindowManager: Unknown action:" << action;
        return false;
    }
    
    QString result = executeHyprctl(command);
    return !result.isEmpty();
}

bool HyprlandWindowManager::moveWindowToWorkspaceHyprland(const QString& windowAddress, int workspace) const
{
    QString command = QString("dispatch movetoworkspace %1,address:%2").arg(workspace).arg(windowAddress);
    QString result = executeHyprctl(command);
    return !result.isEmpty();
}

bool HyprlandWindowManager::moveWindowToMonitorHyprland(const QString& windowAddress, int monitor) const
{
    QString command = QString("dispatch movewindow mon:%1,address:%2").arg(monitor).arg(windowAddress);
    QString result = executeHyprctl(command);
    return !result.isEmpty();
}

QJsonArray HyprlandWindowManager::getMonitors() const
{
    return getHyprctlJson("monitors -j");
}

QJsonArray HyprlandWindowManager::getWorkspaces() const
{
    return getHyprctlJson("workspaces -j");
}
