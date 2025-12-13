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

#include "client.h"
#include "dockapplet.h"
#include "dockitem.h"
#include "x11support.h"
#include <X11/Xlib.h>

Client::Client(DockApplet* dockApplet, unsigned long handle)
	: m_dockItem(NULL)
{
	m_dockApplet = dockApplet;
	m_handle = handle;

    // Select events on the same X connection used by Qt/X11Support, otherwise
    // PropertyNotify (title changes like Firefox tab) won't reach our event filter.
    X11Support::selectInput(m_handle, PropertyChangeMask | StructureNotifyMask);
    updateVisibility();
	updateName();
	updateIcon();
	updateUrgency();
	
	// Get or create dock item for this client (after name is set)
	m_dockItem = dockApplet->dockItemForClient(this);
}

Client::~Client()
{
	if(m_dockItem != NULL)
	{
		m_dockItem->removeClient(this);
	}
}

void Client::windowPropertyChanged(unsigned long atom)
{

    if(atom == X11Support::atom("_NET_WM_WINDOW_TYPE") || atom == X11Support::atom("_NET_WM_STATE"))
	{
        updateVisibility();
    }

	if(atom == X11Support::atom("_NET_WM_VISIBLE_NAME") || atom == X11Support::atom("_NET_WM_NAME") || atom == X11Support::atom("WM_NAME"))
    {
		updateName();
	}

	if(atom == X11Support::atom("_NET_WM_ICON"))
	{
        updateIcon();
	}

	if(atom == X11Support::atom("WM_HINTS"))
	{
		updateUrgency();
    }
}

void Client::updateVisibility()
{
	QVector<unsigned long> windowTypes = X11Support::getWindowPropertyAtomsArray(m_handle, "_NET_WM_WINDOW_TYPE");
	QVector<unsigned long> windowStates = X11Support::getWindowPropertyAtomsArray(m_handle, "_NET_WM_STATE");

	// Show only regular windows in dock.
	// When no window type is set, assume it's normal window.
	m_visible = (windowTypes.size() == 0) || (windowTypes.size() == 1 && windowTypes[0] == X11Support::atom("_NET_WM_WINDOW_TYPE_NORMAL"));
	// Don't show window if requested explicitly in window states.
	if(windowStates.contains(X11Support::atom("_NET_WM_STATE_SKIP_TASKBAR")))
		m_visible = false;

	if(m_dockItem == NULL && m_visible)
	{
		m_dockItem = m_dockApplet->dockItemForClient(this);
	}

	if(m_dockItem != NULL && !m_visible)
	{
        m_dockItem->removeClient(this);
        m_dockItem = NULL;
	}
}

void Client::updateName()
{
	m_name = X11Support::getWindowName(m_handle);
	if(m_dockItem != NULL)
		m_dockItem->updateContent();
}

void Client::updateIcon()
{
	m_icon = X11Support::getWindowIcon(m_handle);
	if(m_dockItem != NULL)
		m_dockItem->updateContent();
}

void Client::updateUrgency()
{
	m_isUrgent = X11Support::getWindowUrgency(m_handle);
	if(m_dockItem != NULL)
		m_dockItem->startAnimation();
}
