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

#ifndef DOCKAPPLET_H
#define DOCKAPPLET_H

#include <QtCore/QVector>
#include <QtCore/QMap>
#include <QtCore/QList>
#include <QtGui/QIcon>
#include "applet.h"

// Forward declarations
struct WaylandWindow;
class WaylandSupport;
class WaylandClient;
class TaskBarItem;
class Client;
class TaskBarConfigurationDialog;

class TaskBarApplet: public Applet
{
	Q_OBJECT
public:
	TaskBarApplet(PanelWindow* panelWindow);
	~TaskBarApplet();
    void close();
    virtual void setPanelWindow(PanelWindow* panelWindow);

	bool init();
	QSize desiredSize();
    void startPlugin(){}

	void registerTaskBarItem(TaskBarItem* dockItem);
	void unregisterTaskBarItem(TaskBarItem* dockItem);

	TaskBarItem* dockItemForClient(Client* client);
	TaskBarItem* dockItemForWaylandClient(WaylandClient* client);

	void updateLayout();

	unsigned long activeWindow() const { return m_activeWindow; }

	void draggingStarted();
	void draggingStopped();
	void moveItem(TaskBarItem* dockItem, bool right);
	
	// Public accessor for WaylandSupport
	WaylandSupport* waylandSupport() const { return m_waylandSupport; }
	
	// Check if applet is being destroyed
	bool isDestroying() const { return m_destroying; }

public slots:
    void fontChanged();

protected:
	void layoutChanged();

private slots:
	void windowPropertyChanged(unsigned long window, unsigned long atom);
    void windowReconfigured(unsigned long window, int x, int y, int width, int height);
    void windowClosed(unsigned long window);
    /**
     * @brief show keyboard layout configuration dialog
     */
	void showConfigurationDialog();

private:
    void updateClientList();
    void updateWaylandClientList(const QList<WaylandWindow>& windows);
    void updateX11ClientList();
    void updateActiveWindow();
    void readSettings();
    void deduplicateTaskBarItems();
    TaskBarItem* createTaskBarItem(const QString& name, const QIcon& icon, const QString& objectName = QString());

	QMap<unsigned long, Client*> m_clients;
	QMap<void*, WaylandClient*> m_waylandClients;
    QList <Client*> m_in_loop;
	QVector<TaskBarItem*> m_dockItems;
	unsigned long m_activeWindow;
	bool m_dragging;
    bool m_only_minimized;
    bool m_only_current_screen;
    bool m_only_current_desktop;
    bool m_initialized;
    bool m_destroying;
    WaylandSupport* m_waylandSupport;
    
    // Color settings
    QColor m_buttonColor;
    int m_buttonColorTransparency;
    QColor m_focusColor;
    int m_focusColorTransparency;
};

#endif