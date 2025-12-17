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

#ifndef CLIENT_H
#define CLIENT_H

#include <QtCore/QString>
#include <QtGui/QIcon>

// Forward declarations
class TaskBarApplet;
class TaskBarItem;

// Used for tracking connected windows (X11 clients).
// Client may have it's TaskBarItem, but not necessary (for example, special windows are not shown in dock).
class Client
{
public:
	Client(TaskBarApplet* dockApplet, unsigned long handle);
	~Client();

	unsigned long handle() const
	{
		return m_handle;
	}

	bool isVisible()
	{
		return m_visible;
	}

	const QString& name() const
	{
		return m_name;
	}

	const QIcon& icon() const
	{
		return m_icon;
	}

	bool isUrgent() const
	{
		return m_isUrgent;
	}

	void windowPropertyChanged(unsigned long atom);

private:
	void updateVisibility();
	void updateName();
	void updateIcon();
	void updateUrgency();

	TaskBarApplet* m_dockApplet;
	unsigned long m_handle;
	QString m_name;
	QIcon m_icon;
	bool m_isUrgent;
	bool m_visible;
	TaskBarItem* m_dockItem;
};

#endif
