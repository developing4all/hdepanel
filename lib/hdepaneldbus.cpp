/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
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
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "hdepaneldbus.h"
#include "panelapplication.h"
#include "panelwindow.h"
#include "applet.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>

// Forward declaration - we'll need to access StartApplet
class StartApplet;

HDEPanelDBus::HDEPanelDBus(PanelApplication* app, QObject* parent)
    : QObject(parent)
    , m_app(app)
    , m_registered(false)
{
}

HDEPanelDBus::~HDEPanelDBus()
{
    unregisterService();
}

bool HDEPanelDBus::registerService()
{
    if (m_registered) {
        return true;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    
    // Register the object path
    if (!bus.registerObject("/com/developing4all/hdepanel", this, QDBusConnection::ExportAllSlots)) {
        qWarning() << "Failed to register D-Bus object:" << bus.lastError().message();
        return false;
    }

    // Register the service name
    if (!bus.registerService("com.developing4all.hdepanel")) {
        qWarning() << "Failed to register D-Bus service:" << bus.lastError().message();
        // Unregister object if service registration failed
        bus.unregisterObject("/com/developing4all/hdepanel");
        return false;
    }

    m_registered = true;
    qDebug() << "HDEPanel D-Bus service registered: com.developing4all.hdepanel";
    return true;
}

void HDEPanelDBus::unregisterService()
{
    if (!m_registered) {
        return;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject("/com/developing4all/hdepanel");
    bus.unregisterService("com.developing4all.hdepanel");
    
    m_registered = false;
    qDebug() << "HDEPanel D-Bus service unregistered";
}

bool HDEPanelDBus::ShowStartMenu()
{
    if (!m_app) {
        return false;
    }

    // Get all panels
    QVector<PanelWindow*> panels = m_app->panelWindows();
    
    // Find all StartApplet instances across all panels
    for (PanelWindow* panel : panels) {
        if (!panel) continue;
        
        QVector<Applet*> applets = panel->applets();
        for (Applet* applet : applets) {
            if (!applet) continue;
            
            // Check if this is a StartApplet by object name
            // StartApplet sets objectName("Start") in its constructor
            if (applet->objectName() == "Start") {
                // Call clicked() slot via QMetaObject
                // Use DirectConnection to ensure it executes immediately
                bool success = QMetaObject::invokeMethod(applet, "clicked", Qt::DirectConnection);
                if (success) {
                    qDebug() << "HDEPanelDBus: Start menu shown via D-Bus";
                    return true;
                } else {
                    qWarning() << "HDEPanelDBus: Failed to invoke clicked() on StartApplet";
                }
            }
        }
    }

    qDebug() << "HDEPanelDBus: No StartApplet found";
    return false;
}

bool HDEPanelDBus::ToggleStartMenu()
{
    // For now, just show the menu (toggle can be implemented later
    // by checking if the menu is visible)
    return ShowStartMenu();
}

bool HDEPanelDBus::HideStartMenu()
{
    // This would require tracking StartWindow instances
    // For now, return false as hiding is not critical
    // The menu will close on focus loss anyway
    return false;
}

