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
 * version 3.0 of the License, or (at your option) any later version.
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

#ifndef HYPRLANDWINDOWMANAGER_H
#define HYPRLANDWINDOWMANAGER_H

#include "windowmanager.h"
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

/**
 * @brief Hyprland compositor window manager implementation
 * 
 * Uses Hyprland's socket-based IPC for window management.
 * Communicates with Hyprland via hyprctl commands.
 */
class HyprlandWindowManager : public WindowManager
{
    Q_OBJECT

public:
    explicit HyprlandWindowManager(QObject* parent = nullptr);
    ~HyprlandWindowManager() override;

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

    // Hyprland-specific methods
    QString getWindowManagerName() const override { return "Hyprland"; }
    Type getType() const override { return Type::Hyprland; }
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
    // Hyprland IPC methods
    QString executeHyprctl(const QString& command) const;
    QJsonArray getHyprctlJson(const QString& command) const;
    QJsonObject getHyprctlObject(const QString& command) const;
    
    // Window management
    QList<WaylandWindow> parseWindowsFromJson(const QJsonArray& windows) const;
    QString findWindowAddress(const QString& appId) const;
    bool executeWindowAction(const QString& windowAddress, const QString& action) const;
    
    // Workspace management
    bool moveWindowToWorkspaceHyprland(const QString& windowAddress, int workspace) const;
    bool moveWindowToMonitorHyprland(const QString& windowAddress, int monitor) const;
    
    // Monitor management
    QJsonArray getMonitors() const;
    QJsonArray getWorkspaces() const;
};

#endif // HYPRLANDWINDOWMANAGER_H
