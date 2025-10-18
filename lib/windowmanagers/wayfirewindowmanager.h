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

#ifndef WAYFIREWINDOWMANAGER_H
#define WAYFIREWINDOWMANAGER_H

#include "windowmanager.h"
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief Wayfire compositor window manager implementation
 * 
 * Uses Wayfire's socket-based IPC for window management.
 * Communicates with Wayfire via wayfire-socket commands.
 */
class WayfireWindowManager : public WindowManager
{
    Q_OBJECT

public:
    explicit WayfireWindowManager(QObject* parent = nullptr);
    ~WayfireWindowManager() override;

    // WindowManager interface implementation
    bool initialize() override;
    bool isAvailable() override;
    QList<WaylandWindow> getAllWindows() override;
    bool activateWindow(const QString& appId) override;
    bool closeWindow(const QString& appId) override;
    bool minimizeWindow(const QString& appId) override;
    bool maximizeWindow(const QString& appId) override;
    bool unmaximizeWindow(const QString& appId) override;
    bool moveWindowToWorkspace(const QString& appId, int workspace) override;
    bool moveWindowToMonitor(const QString& appId, int monitor) override;

    // Wayfire-specific methods
    QString getWindowManagerName() const override { return "Wayfire"; }
    Type getType() const override { return Type::Wayfire; }
    QString getVersion() const override;
    bool supportsWorkspaces() const override { return true; }
    bool supportsMultipleMonitors() const override { return true; }
    int getCurrentWorkspace() const override;
    int getWorkspaceCount() const override;
    QStringList getWorkspaceNames() const override;
    int getMonitorCount() const override;
    QStringList getMonitorNames() const override;

private slots:
    void updateWindows();

private:
    QTimer* m_updateTimer;
    bool m_initialized;
    QString m_socketPath;
    
    // Helper methods
    QStringList executeWayfireCommand(const QStringList& args) const;
    QJsonDocument executeWayfireJsonCommand(const QStringList& args) const;
    WaylandWindow parseWindowFromJson(const QJsonObject& obj) const;
};

#endif // WAYFIREWINDOWMANAGER_H
