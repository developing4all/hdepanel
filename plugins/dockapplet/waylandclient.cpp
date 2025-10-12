/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * This Files has been imported to hde from qtpanel
 *
 * Copyright: 2015-2025 Haydar Alkaduhimi
 * Copyright: 2014 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "waylandclient.h"
#include "dockapplet.h"
#include "dockitem.h"
#include "waylandsupport.h"
#include <QtCore/QDebug>

// WaylandClient implementation
WaylandClient::WaylandClient(DockApplet* dockApplet, const WaylandWindow& window)
    : m_dockApplet(dockApplet)
    , m_surface(window.surface)
    , m_isUrgent(false)
    , m_visible(window.visible)
    , m_dockItem(nullptr)
{
    updateFromWindow(window);
    
    // Create a dock item for this Wayland client
    m_dockItem = new DockItem(dockApplet);
    m_dockItem->setWaylandClient(this);
    dockApplet->registerDockItem(m_dockItem);
}

WaylandClient::~WaylandClient()
{
    if (m_dockItem) {
        m_dockItem->removeClient(nullptr); // Remove from dock item
        delete m_dockItem; // Delete the dock item
        m_dockItem = nullptr;
    }
}

void WaylandClient::updateFromWindow(const WaylandWindow& window)
{
    QString oldName = m_name;
    m_surface = window.surface;
    m_visible = window.visible;
    
    // Set name, app ID, and icon from the window data
    m_name = window.title;
    m_appId = window.appId;
    m_icon = QIcon::fromTheme(window.iconName);
    
    // Title change detection removed for cleaner output
    
    updateUrgency();
    
    // Update the dock item if it exists
    if (m_dockItem) {
        m_dockItem->setWaylandClient(this);
    }
}

void WaylandClient::updateVisibility()
{
    // Wayland visibility is handled by the compositor
    // This is a placeholder for future implementation
}

void WaylandClient::updateName()
{
    // Use the window title from the Wayland window
    // This will be set in updateFromWindow()
    if (m_name.isEmpty()) {
        m_name = "Wayland Window";
    }
}

void WaylandClient::updateIcon()
{
    // Use the icon name from the Wayland window
    // This will be set in updateFromWindow()
    if (m_icon.isNull()) {
        m_icon = QIcon::fromTheme("application-x-executable");
    }
}

void WaylandClient::updateUrgency()
{
    // Wayland urgency handling would need to be implemented
    // This is a placeholder
    m_isUrgent = false;
}
