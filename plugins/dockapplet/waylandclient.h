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

#ifndef WAYLANDCLIENT_H
#define WAYLANDCLIENT_H

#include <QtCore/QString>
#include <QtGui/QIcon>

// Forward declarations
struct WaylandWindow;
class DockApplet;
class DockItem;

// Used for tracking Wayland windows
class WaylandClient
{
public:
	WaylandClient(DockApplet* dockApplet, const WaylandWindow& window);
	~WaylandClient();

	void* surface() const
	{
		return m_surface;
	}

	bool isVisible() const
	{
		return m_visible;
	}

	const QString& name() const
	{
		return m_name;
	}

	const QString& appId() const
	{
		return m_appId;
	}

	const QIcon& icon() const
	{
		return m_icon;
	}

	bool isUrgent() const
	{
		return m_isUrgent;
	}

	void updateFromWindow(const WaylandWindow& window);

private:
	void updateVisibility();
	void updateName();
	void updateIcon();
	void updateUrgency();

	DockApplet* m_dockApplet;
	void* m_surface;
	QString m_name;
	QString m_appId;
	QIcon m_icon;
	bool m_isUrgent;
	bool m_visible;
	DockItem* m_dockItem;
};

#endif
