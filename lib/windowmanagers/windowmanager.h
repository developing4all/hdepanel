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

#ifndef WINDOWMANAGER_H
#define WINDOWMANAGER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QTimer>
#include "../waylandwindow.h"

/**
 * @brief Abstract base class for window manager implementations
 * 
 * This interface defines the common methods that all window manager
 * implementations must provide for HDEPanel to function properly.
 * 
 * Note: In modern Linux desktops, most "window managers" are actually
 * compositors that handle both window management and rendering (e.g.,
 * Hyprland, River, Wayfire, Labwc, Mutter/GNOME, KWin/KDE). This
 * interface focuses on the window management aspects needed by panels.
 */
class WindowManager : public QObject
{
    Q_OBJECT

public:
    enum class Type {
        Unknown,
        GNOME,      // GNOME Shell (Wayland + X11)
        KDE,        // KDE Plasma (KWin)
        Weston,     // Weston compositor
        Hyprland,   // Hyprland compositor
        River,      // River compositor
        Wayfire,    // Wayfire compositor
        Labwc,      // Labwc compositor
        X11         // Generic X11 (fallback)
    };

    explicit WindowManager(QObject* parent = nullptr);
    virtual ~WindowManager() = default;

    // Pure virtual methods that must be implemented by each WM
    virtual bool initialize() = 0;
    virtual bool isAvailable() = 0;
    virtual QList<WaylandWindow> getAllWindows() = 0;
    virtual bool activateWindow(const QString& appId) = 0;
    virtual bool closeWindow(const QString& appId) = 0;
    virtual bool minimizeWindow(const QString& appId) = 0;
    virtual bool maximizeWindow(const QString& appId) = 0;
    virtual bool unmaximizeWindow(const QString& appId) = 0;
    virtual bool moveWindowToWorkspace(const QString& appId, int workspace) = 0;
    virtual bool moveWindowToMonitor(const QString& appId, int monitor) = 0;

    // Virtual methods with default implementations
    virtual QString getWindowManagerName() const = 0;
    virtual Type getType() const = 0;
    virtual QString getVersion() const { return "Unknown"; }
    virtual bool supportsWorkspaces() const { return false; }
    virtual bool supportsMultipleMonitors() const { return false; }
    virtual int getCurrentWorkspace() const { return 0; }
    virtual int getWorkspaceCount() const { return 1; }
    virtual QStringList getWorkspaceNames() const { return QStringList(); }
    virtual int getMonitorCount() const { return 1; }
    virtual QStringList getMonitorNames() const { return QStringList(); }

    // Common utility methods
    virtual void startMonitoring() { m_monitoring = true; }
    virtual void stopMonitoring() { m_monitoring = false; }
    virtual bool isMonitoring() const { return m_monitoring; }

signals:
    void windowsUpdated(const QList<WaylandWindow>& windows);
    void windowCreated(const WaylandWindow& window);
    void windowClosed(const WaylandWindow& window);
    void windowActivated(const WaylandWindow& window);
    void workspaceChanged(int workspace);
    void monitorChanged(int monitor);

protected:
    // Common properties
    bool m_initialized;
    bool m_monitoring;
    QTimer* m_updateTimer;

    // Helper methods for subclasses
    virtual void setupUpdateTimer(int intervalMs = 500);
    virtual void emitWindowsUpdated(const QList<WaylandWindow>& windows);
};

/**
 * @brief Window Manager Factory
 * 
 * Factory class for creating appropriate WindowManager instances
 * based on the current desktop environment and available services.
 */
class WindowManagerFactory
{
public:
    static WindowManager* createWindowManager(QObject* parent = nullptr);
    static WindowManager::Type detectWindowManager();
    static QStringList getSupportedWindowManagers();
    static bool isWindowManagerSupported(WindowManager::Type type);
};

#endif // WINDOWMANAGER_H
