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

#ifndef GNOMEWINDOWMANAGER_H
#define GNOMEWINDOWMANAGER_H

#include "windowmanager.h"
#include <QDBusInterface>
#include <QDBusReply>

/**
 * @brief GNOME Shell window manager implementation
 * 
 * Supports both Wayland and X11 on GNOME desktop environment.
 * Uses GNOME Shell extensions and D-Bus for window management.
 */
class GNOMEWindowManager : public WindowManager
{
    Q_OBJECT

public:
    explicit GNOMEWindowManager(QObject* parent = nullptr);
    ~GNOMEWindowManager() override;

    // WindowManager interface implementation
    bool initialize() override;
    bool isAvailable() override;
    QList<WaylandWindow> getAllWindows() override;
    bool activateWindow(const QString& appId) override;
    bool closeWindow(const QString& appId) override;
    bool activateWindowById(quint64 id) override;
    bool closeWindowById(quint64 id) override;
    bool minimizeWindow(const QString& appId) override;
    bool maximizeWindow(const QString& appId) override;
    bool unmaximizeWindow(const QString& appId) override;
    bool moveWindowToWorkspace(const QString& appId, int workspace) override;
    bool moveWindowToMonitor(const QString& appId, int monitor) override;

    // GNOME-specific methods
    QString getWindowManagerName() const override { return "GNOME Shell"; }
    Type getType() const override { return Type::GNOME; }
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
    void onWindowClosedSignal(quint64 id, const QString& title, const QString& appId);

private:
    // D-Bus interfaces
    QDBusInterface* m_extensionInterface;
    QDBusInterface* m_shellInterface;
    
    // GNOME Shell specific methods
    QList<WaylandWindow> getWindowsFromExtension();
    QList<WaylandWindow> getWindowsFromGnomeShell();
    bool callExtensionMethod(const QString& method, const QVariant& arg = QVariant());
    bool callShellMethod(const QString& method, const QString& script);
    
    // Window manipulation helpers
    QString findWindowScript(const QString& appId, const QString& action);
    bool executeWindowAction(const QString& appId, const QString& action);
    
    // Workspace management
    bool moveWindowToWorkspaceScript(const QString& appId, int workspace);
    bool moveWindowToMonitorScript(const QString& appId, int monitor);
};

#endif // GNOMEWINDOWMANAGER_H
