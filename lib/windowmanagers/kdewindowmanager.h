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

#ifndef KDEWINDOWMANAGER_H
#define KDEWINDOWMANAGER_H

#include "windowmanager.h"
#include <QDBusInterface>
#include <QDBusReply>

/**
 * @brief KDE Plasma (KWin) window manager implementation
 * 
 * Supports both Wayland and X11 on KDE desktop environment.
 * Uses KWin D-Bus interface for window management.
 */
class KDEWindowManager : public WindowManager
{
    Q_OBJECT

public:
    explicit KDEWindowManager(QObject* parent = nullptr);
    ~KDEWindowManager() override;

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

    // KDE-specific methods
    QString getWindowManagerName() const override { return "KDE Plasma (KWin)"; }
    Type getType() const override { return Type::KDE; }
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
    // D-Bus interfaces
    QDBusInterface* m_kwinInterface;
    QDBusInterface* m_activitiesInterface;
    
    // KWin specific methods
    QList<WaylandWindow> getWindowsFromKWin();
    bool callKWinMethod(const QString& method, const QVariantList& args = QVariantList());
    
    // Window manipulation helpers
    QString findWindowId(const QString& appId);
    bool executeWindowAction(const QString& windowId, const QString& action);
    
    // Workspace management
    bool moveWindowToWorkspaceKWin(const QString& windowId, int workspace);
    bool moveWindowToMonitorKWin(const QString& windowId, int monitor);
};

#endif // KDEWINDOWMANAGER_H
