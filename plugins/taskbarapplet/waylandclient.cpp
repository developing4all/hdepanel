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
#include "taskbarapplet.h"
#include "taskbaritem.h"
#include "waylandsupport.h"
#include "../../lib/unifiediconservice.h"
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QGraphicsScene>

// WaylandClient implementation
WaylandClient::WaylandClient(TaskBarApplet* dockApplet, const WaylandWindow& window)
    : m_dockApplet(dockApplet)
    , m_surface(window.surface)
    , m_isUrgent(false)
    , m_focused(false)
    , m_visible(window.visible)
    , m_dockItem(nullptr)
{
    updateFromWindow(window);
    
    // Create a dock item for this Wayland client
    try {
        m_dockItem = new TaskBarItem(dockApplet);
        if (m_dockItem) {
            m_dockItem->setWaylandClient(this);
            dockApplet->registerTaskBarItem(m_dockItem);
        }
    } catch (...) {
        qDebug() << "Failed to create or register TaskBarItem for" << window.appId;
        if (m_dockItem) {
            delete m_dockItem;
            m_dockItem = nullptr;
        }
        throw; // Re-throw to notify caller
    }
}

WaylandClient::~WaylandClient()
{
    if (m_dockItem) {
        TaskBarItem* item = m_dockItem;
        m_dockItem = nullptr;
        
        // Check if applet is being destroyed
        if (m_dockApplet && m_dockApplet->isDestroying()) {
            // Direct deletion during shutdown - scene removal handled by TaskBarApplet::close()
            delete item;
        } else {
            // Normal operation: remove from scene before deletion
            if (item->scene()) {
                item->scene()->removeItem(item);
            }
            
            // Use deferred deletion during normal operation
            QTimer::singleShot(0, [item]() {
                delete item;
            });
        }
    }
}

void WaylandClient::updateFromWindow(const WaylandWindow& window)
{
    // Safety check: bail out if applet is being destroyed
    if(m_dockApplet == NULL || m_dockApplet->isDestroying())
        return;

    QString oldName = m_name;
    m_surface = window.surface;
    m_visible = window.visible;
    m_focused = window.focused;
    
    // Set name, app ID, and icon from the window data
    m_name = window.title;
    m_appId = window.appId;
    m_icon = UnifiedIconService::instance()->loadApplicationIcon(window.appId, QString(), 32);
    
    // Title change detection removed for cleaner output
    
    updateUrgency();
    
    // Update the dock item if it exists
    // Safety check: ensure dock item and applet are still valid before updating
    if (m_dockItem && m_dockApplet && !m_dockApplet->isDestroying()) {
        m_dockItem->setWaylandClient(this);
        // Trigger animation update to show/hide focus highlight
        m_dockItem->startAnimation();
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
        m_icon = UnifiedIconService::instance()->loadIcon("application-x-executable", 32);
    }
}

void WaylandClient::updateUrgency()
{
    // Wayland urgency handling would need to be implemented
    // This is a placeholder
    m_isUrgent = false;
}
