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

#include "wayfirewindowmanager.h"
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonParseError>

WayfireWindowManager::WayfireWindowManager(QObject* parent)
    : WindowManager(parent)
    , m_updateTimer(nullptr)
    , m_initialized(false)
{
    // Try to find Wayfire socket
    QStringList possiblePaths = {
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/wayfire-0",
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/wayfire-1",
        "/tmp/wayfire-0",
        "/tmp/wayfire-1",
        qgetenv("WAYFIRE_SOCKET")
    };
    
    for (const QString& path : possiblePaths) {
        if (QFile::exists(path)) {
            m_socketPath = path;
            break;
        }
    }
    
    if (m_socketPath.isEmpty()) {
        qDebug() << "WayfireWindowManager: No Wayfire socket found";
    } else {
        qDebug() << "WayfireWindowManager: Found Wayfire socket at" << m_socketPath;
    }
}

WayfireWindowManager::~WayfireWindowManager()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
        delete m_updateTimer;
        m_updateTimer = nullptr;
    }
}

bool WayfireWindowManager::initialize()
{
    if (m_initialized) {
        return true;
    }
    
    if (!isAvailable()) {
        qDebug() << "WayfireWindowManager: Wayfire not available";
        return false;
    }
    
    // Create update timer
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(1000); // Update every second
    connect(m_updateTimer, &QTimer::timeout, this, &WayfireWindowManager::updateWindows);
    
    m_initialized = true;
    m_updateTimer->start();
    
    qDebug() << "WayfireWindowManager: Initialized successfully";
    return true;
}

bool WayfireWindowManager::isAvailable()
{
    if (m_socketPath.isEmpty()) {
        return false;
    }
    
    // Test if we can communicate with Wayfire
    QStringList result = executeWayfireCommand(QStringList() << "version");
    return !result.isEmpty() && result.first().contains("wayfire");
}

QList<WaylandWindow> WayfireWindowManager::getAllWindows()
{
    QList<WaylandWindow> windows;
    
    if (!m_initialized) {
        return windows;
    }
    
    // Get list of views (windows) from Wayfire
    QStringList result = executeWayfireCommand(QStringList() << "list-views");
    
    for (const QString& line : result) {
        if (line.isEmpty()) continue;
        
        // Parse view information
        // Format: <view_id> <title> <app_id> <workspace> <output>
        QStringList parts = line.split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 5) continue;
        
        WaylandWindow window;
        window.title = parts[1];
        window.appId = parts[2];
        window.workspaceIndex = parts[3].toInt();
        window.monitorIndex = 0; // Wayfire doesn't provide monitor info in basic list
        window.visible = true;
        window.focused = false; // Would need additional command to determine
        window.isWayland = true;
        window.isX11 = false;
        window.surface = reinterpret_cast<void*>(parts[0].toULongLong());
        
        // Try to get more detailed information
        QStringList details = executeWayfireCommand(QStringList() << "view-info" << parts[0]);
        for (const QString& detail : details) {
            if (detail.startsWith("title:")) {
                window.title = detail.mid(6).trimmed();
            } else if (detail.startsWith("app-id:")) {
                window.appId = detail.mid(7).trimmed();
            } else if (detail.startsWith("focused:")) {
                window.focused = detail.mid(8).trimmed() == "true";
            }
        }
        
        windows.append(window);
    }
    
    return windows;
}

bool WayfireWindowManager::activateWindow(const QString& appId)
{
    if (!m_initialized) return false;
    
    // Find window by app ID
    QList<WaylandWindow> windows = getAllWindows();
    for (const WaylandWindow& window : windows) {
        if (window.appId == appId) {
            QStringList result = executeWayfireCommand(QStringList() << "focus-view" << QString::number(reinterpret_cast<quintptr>(window.surface)));
            return !result.isEmpty() && result.first().contains("success");
        }
    }
    
    return false;
}

bool WayfireWindowManager::closeWindow(const QString& appId)
{
    if (!m_initialized) return false;
    
    // Find window by app ID
    QList<WaylandWindow> windows = getAllWindows();
    for (const WaylandWindow& window : windows) {
        if (window.appId == appId) {
            QStringList result = executeWayfireCommand(QStringList() << "close-view" << QString::number(reinterpret_cast<quintptr>(window.surface)));
            return !result.isEmpty() && result.first().contains("success");
        }
    }
    
    return false;
}

bool WayfireWindowManager::minimizeWindow(const QString& appId)
{
    // Wayfire doesn't have minimize concept, use close instead
    return closeWindow(appId);
}

bool WayfireWindowManager::maximizeWindow(const QString& appId)
{
    if (!m_initialized) return false;
    
    // Find window by app ID
    QList<WaylandWindow> windows = getAllWindows();
    for (const WaylandWindow& window : windows) {
        if (window.appId == appId) {
            QStringList result = executeWayfireCommand(QStringList() << "maximize-view" << QString::number(reinterpret_cast<quintptr>(window.surface)));
            return !result.isEmpty() && result.first().contains("success");
        }
    }
    
    return false;
}

bool WayfireWindowManager::unmaximizeWindow(const QString& appId)
{
    if (!m_initialized) return false;
    
    // Find window by app ID
    QList<WaylandWindow> windows = getAllWindows();
    for (const WaylandWindow& window : windows) {
        if (window.appId == appId) {
            QStringList result = executeWayfireCommand(QStringList() << "unmaximize-view" << QString::number(reinterpret_cast<quintptr>(window.surface)));
            return !result.isEmpty() && result.first().contains("success");
        }
    }
    
    return false;
}

bool WayfireWindowManager::moveWindowToWorkspace(const QString& appId, int workspace)
{
    if (!m_initialized) return false;
    
    // Find window by app ID
    QList<WaylandWindow> windows = getAllWindows();
    for (const WaylandWindow& window : windows) {
        if (window.appId == appId) {
            QStringList result = executeWayfireCommand(QStringList() << "move-view-to-workspace" << QString::number(reinterpret_cast<quintptr>(window.surface)) << QString::number(workspace));
            return !result.isEmpty() && result.first().contains("success");
        }
    }
    
    return false;
}

bool WayfireWindowManager::moveWindowToMonitor(const QString& appId, int monitor)
{
    if (!m_initialized) return false;
    
    // Find window by app ID
    QList<WaylandWindow> windows = getAllWindows();
    for (const WaylandWindow& window : windows) {
        if (window.appId == appId) {
            QStringList result = executeWayfireCommand(QStringList() << "move-view-to-output" << QString::number(reinterpret_cast<quintptr>(window.surface)) << QString::number(monitor));
            return !result.isEmpty() && result.first().contains("success");
        }
    }
    
    return false;
}

QString WayfireWindowManager::getVersion() const
{
    if (!m_initialized) return "Unknown";
    
    QStringList result = executeWayfireCommand(QStringList() << "version");
    if (!result.isEmpty()) {
        return result.first();
    }
    
    return "Unknown";
}

int WayfireWindowManager::getCurrentWorkspace() const
{
    if (!m_initialized) return 0;
    
    QStringList result = executeWayfireCommand(QStringList() << "get-workspace");
    if (!result.isEmpty()) {
        return result.first().toInt();
    }
    
    return 0;
}

int WayfireWindowManager::getWorkspaceCount() const
{
    if (!m_initialized) return 1;
    
    QStringList result = executeWayfireCommand(QStringList() << "get-workspace-count");
    if (!result.isEmpty()) {
        return result.first().toInt();
    }
    
    return 1;
}

QStringList WayfireWindowManager::getWorkspaceNames() const
{
    QStringList names;
    
    if (!m_initialized) return names;
    
    int count = getWorkspaceCount();
    for (int i = 0; i < count; ++i) {
        names << QString("Workspace %1").arg(i + 1);
    }
    
    return names;
}

int WayfireWindowManager::getMonitorCount() const
{
    if (!m_initialized) return 1;
    
    QStringList result = executeWayfireCommand(QStringList() << "list-outputs");
    return result.size();
}

QStringList WayfireWindowManager::getMonitorNames() const
{
    QStringList names;
    
    if (!m_initialized) return names;
    
    QStringList result = executeWayfireCommand(QStringList() << "list-outputs");
    for (const QString& output : result) {
        names << output;
    }
    
    return names;
}

void WayfireWindowManager::updateWindows()
{
    QList<WaylandWindow> windows = getAllWindows();
    emit windowsUpdated(windows);
}

QStringList WayfireWindowManager::executeWayfireCommand(const QStringList& args) const
{
    QStringList result;
    
    if (m_socketPath.isEmpty()) {
        return result;
    }
    
    QProcess process;
    process.start("wayfire-socket", args);
    process.waitForFinished(5000); // 5 second timeout
    
    if (process.exitCode() == 0) {
        QString output = process.readAllStandardOutput();
        result = output.split('\n', Qt::SkipEmptyParts);
    } else {
        qDebug() << "WayfireWindowManager: Command failed:" << args << "Exit code:" << process.exitCode();
        qDebug() << "WayfireWindowManager: Error:" << process.readAllStandardError();
    }
    
    return result;
}

QJsonDocument WayfireWindowManager::executeWayfireJsonCommand(const QStringList& args) const
{
    QJsonDocument doc;
    
    if (m_socketPath.isEmpty()) {
        return doc;
    }
    
    QProcess process;
    process.start("wayfire-socket", args);
    process.waitForFinished(5000); // 5 second timeout
    
    if (process.exitCode() == 0) {
        QByteArray output = process.readAllStandardOutput();
        QJsonParseError error;
        doc = QJsonDocument::fromJson(output, &error);
        
        if (error.error != QJsonParseError::NoError) {
            qDebug() << "WayfireWindowManager: JSON parse error:" << error.errorString();
        }
    } else {
        qDebug() << "WayfireWindowManager: JSON command failed:" << args << "Exit code:" << process.exitCode();
    }
    
    return doc;
}

WaylandWindow WayfireWindowManager::parseWindowFromJson(const QJsonObject& obj) const
{
    WaylandWindow window;
    
    window.title = obj["title"].toString();
    window.appId = obj["app_id"].toString();
    window.workspaceIndex = obj["workspace"].toInt();
    window.monitorIndex = obj["output"].toInt();
    window.visible = obj["visible"].toBool();
    window.focused = obj["focused"].toBool();
    window.isWayland = true;
    window.isX11 = false;
    window.surface = reinterpret_cast<void*>(obj["id"].toVariant().toULongLong());
    
    return window;
}

#include "moc_wayfirewindowmanager.cpp"
