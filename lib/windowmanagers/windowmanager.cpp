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

#include "windowmanager.h"
#include "gnomewindowmanager.h"
#include "hyprlandwindowmanager.h"
#include "wayfirewindowmanager.h"
#include "../waylandsupport.h"
#include <QDebug>
#include <QCoreApplication>
#include <QProcess>
#include <QStandardPaths>

WindowManager::WindowManager(QObject* parent)
    : QObject(parent)
    , m_initialized(false)
    , m_monitoring(false)
    , m_updateTimer(nullptr)
{
}

void WindowManager::setupUpdateTimer(int intervalMs)
{
    if (!m_updateTimer) {
        m_updateTimer = new QTimer(this);
        m_updateTimer->setInterval(intervalMs);
        connect(m_updateTimer, &QTimer::timeout, this, [this]() {
            if (m_monitoring) {
                QList<WaylandWindow> windows = getAllWindows();
                emitWindowsUpdated(windows);
            }
        });
    }
}

void WindowManager::emitWindowsUpdated(const QList<WaylandWindow>& windows)
{
    emit windowsUpdated(windows);
}

// WindowManagerFactory implementation
WindowManager* WindowManagerFactory::createWindowManager(QObject* parent)
{
    WindowManager::Type type = detectWindowManager();
    qDebug() << "WindowManagerFactory: Detected type:" << static_cast<int>(type);
    
    switch (type) {
        case WindowManager::Type::GNOME:
            qDebug() << "WindowManagerFactory: Creating GNOME window manager";
            return new GNOMEWindowManager(parent);
            
        case WindowManager::Type::KDE:
            qDebug() << "WindowManagerFactory: Creating KDE window manager";
            return nullptr; // TODO: Implement KDEWindowManager
            
        case WindowManager::Type::Hyprland:
            qDebug() << "WindowManagerFactory: Creating Hyprland window manager";
            return new HyprlandWindowManager(parent);
            
        case WindowManager::Type::River:
            qDebug() << "WindowManagerFactory: Creating River window manager";
            return nullptr; // TODO: Implement RiverWindowManager
            
        case WindowManager::Type::Wayfire:
            qDebug() << "WindowManagerFactory: Creating Wayfire window manager";
            return new WayfireWindowManager(parent);
            
        case WindowManager::Type::Labwc:
            qDebug() << "WindowManagerFactory: Creating Labwc window manager";
            return nullptr; // TODO: Implement LabwcWindowManager
            
        case WindowManager::Type::Weston:
            qDebug() << "WindowManagerFactory: Creating Weston window manager";
            return nullptr; // TODO: Implement WestonWindowManager
            
        case WindowManager::Type::X11:
            qDebug() << "WindowManagerFactory: Creating X11 window manager";
            return nullptr; // TODO: Implement X11WindowManager
            
        default:
            qDebug() << "WindowManagerFactory: Unknown window manager, creating fallback";
            return nullptr; // TODO: Implement fallback
    }
}

WindowManager::Type WindowManagerFactory::detectWindowManager()
{
    // Check environment variables and running processes
    QString sessionType = qgetenv("XDG_SESSION_TYPE").toLower();
    QString currentDesktop = qgetenv("XDG_CURRENT_DESKTOP").toLower();
    QString waylandDisplay = qgetenv("WAYLAND_DISPLAY");
    QString x11Display = qgetenv("DISPLAY");
    
    qDebug() << "WindowManagerFactory: Detecting WM - Session:" << sessionType 
             << "Desktop:" << currentDesktop 
             << "Wayland:" << waylandDisplay 
             << "X11:" << x11Display;
    
    // Check for Wayland compositors
    if (sessionType == "wayland" || !waylandDisplay.isEmpty()) {
        // Check for specific compositors
        if (currentDesktop.contains("gnome") || currentDesktop.contains("unity")) {
            return WindowManager::Type::GNOME;
        }
        if (currentDesktop.contains("kde") || currentDesktop.contains("plasma")) {
            return WindowManager::Type::KDE;
        }
        
        // Check for Hyprland by desktop environment first
        if (currentDesktop.contains("hypr")) {
            qDebug() << "WindowManagerFactory: Found Hyprland by desktop environment";
            return WindowManager::Type::Hyprland;
        }
        
        // Check for standalone compositors
        QProcess process;
        process.start("pgrep", QStringList() << "-f" << "hyprland");
        process.waitForFinished(1000);
        qDebug() << "WindowManagerFactory: Checking for Hyprland - exit code:" << process.exitCode();
        if (process.exitCode() == 0) {
            qDebug() << "WindowManagerFactory: Found Hyprland process";
            return WindowManager::Type::Hyprland;
        }
        
        process.start("pgrep", QStringList() << "-f" << "river");
        process.waitForFinished(1000);
        if (process.exitCode() == 0) {
            return WindowManager::Type::River;
        }
        
        process.start("pgrep", QStringList() << "-f" << "wayfire");
        process.waitForFinished(1000);
        if (process.exitCode() == 0) {
            return WindowManager::Type::Wayfire;
        }
        
        process.start("pgrep", QStringList() << "-f" << "labwc");
        process.waitForFinished(1000);
        if (process.exitCode() == 0) {
            return WindowManager::Type::Labwc;
        }
        
        process.start("pgrep", QStringList() << "-f" << "weston");
        process.waitForFinished(1000);
        if (process.exitCode() == 0) {
            return WindowManager::Type::Weston;
        }
        
        // Default to GNOME for Wayland if no specific compositor detected
        return WindowManager::Type::GNOME;
    }
    
    // Check for X11
    if (sessionType == "x11" || !x11Display.isEmpty()) {
        if (currentDesktop.contains("gnome") || currentDesktop.contains("unity")) {
            return WindowManager::Type::GNOME;
        }
        if (currentDesktop.contains("kde") || currentDesktop.contains("plasma")) {
            return WindowManager::Type::KDE;
        }
        return WindowManager::Type::X11;
    }
    
    return WindowManager::Type::Unknown;
}

QStringList WindowManagerFactory::getSupportedWindowManagers()
{
    return QStringList() 
        << "GNOME" << "KDE" << "Hyprland" << "River" 
        << "Wayfire" << "Labwc" << "Weston" << "X11";
}

bool WindowManagerFactory::isWindowManagerSupported(WindowManager::Type type)
{
    return type != WindowManager::Type::Unknown;
}
