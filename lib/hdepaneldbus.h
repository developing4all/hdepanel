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

#ifndef HDEPANELDBUS_H
#define HDEPANELDBUS_H

#include <QObject>
#include <QDBusContext>
#include <QDBusConnection>

class PanelApplication;

/**
 * D-Bus interface for HDEPanel
 * 
 * Allows external applications (like Openbox, Wayfire) to trigger
 * panel actions via keybindings.
 * 
 * Service: com.developing4all.hdepanel
 * Object Path: /com/developing4all/hdepanel
 * Interface: com.developing4all.hdepanel
 */
class HDEPanelDBus : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.developing4all.hdepanel")

public:
    explicit HDEPanelDBus(PanelApplication* app, QObject* parent = nullptr);
    ~HDEPanelDBus();

    /**
     * Register the D-Bus service on the session bus
     * @return true if registration was successful
     */
    bool registerService();

    /**
     * Unregister the D-Bus service
     */
    void unregisterService();

public slots:
    /**
     * Show the start menu
     * @return true if a start menu was found and shown
     */
    bool ShowStartMenu();

    /**
     * Toggle the start menu (show if hidden, hide if shown)
     * @return true if a start menu was found and toggled
     */
    bool ToggleStartMenu();

    /**
     * Hide the start menu if it's currently shown
     * @return true if a start menu was found and hidden
     */
    bool HideStartMenu();

private:
    PanelApplication* m_app;
    bool m_registered;
};

#endif // HDEPANELDBUS_H

