/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * This Files has been imported to hde from qtpanel
 *
 * Copyright: 2015-2016 Haydar Alkaduhimi
 * Copyright: 2014 Leslie Zhai <xiang.zhai@i-soft.com.cn>
 * Authors:
 *   Haydar Alkaduhimi <haydar@hosting4all.com>
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
#include <QtGui/QIcon>
#if QT_VERSION >= 0x050000
#include <QGraphicsItem>
#else
#include <QtGui/QGraphicsItem>
#endif
#include "applet.h"

class QGraphicsPixmapItem;
class TextGraphicsItem;
class DockApplet;
class Client;
class DockConfigurationDialog;

// Represents a single item in a dock.
// There isn't one to one relationship between window (client) and dock item, that's why
// it's separate entity. One dock item can represent pinned launcher and one or more opened
// windows of that application.
class DockItem: public QObject, public QGraphicsItem
{
	Q_OBJECT
	Q_INTERFACES(QGraphicsItem)
public:
	DockItem(DockApplet* dockApplet);
	~DockItem();

	void updateContent();

	void addClient(Client* client);
	void removeClient(Client* client);

	void setTargetPosition(const QPoint& targetPosition);
	void setTargetSize(const QSize& targetSize);
	void moveInstantly();
	void startAnimation();

	const QVector<Client*>& clients() const
	{
		return m_clients;
	}

	QRectF boundingRect() const;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

public slots:
	void animate();
	void close();
    void fontChanged();

protected:
	void hoverEnterEvent(QGraphicsSceneHoverEvent* event);
	void hoverLeaveEvent(QGraphicsSceneHoverEvent* event);
	void mousePressEvent(QGraphicsSceneMouseEvent* event);
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);
	void mouseMoveEvent(QGraphicsSceneMouseEvent* event);

private:
	void updateClientsIconGeometry();
	bool isUrgent();

	QTimer* m_animationTimer;
	DockApplet* m_dockApplet;
	TextGraphicsItem* m_textItem;
	QGraphicsPixmapItem* m_iconItem;
	QVector<Client*> m_clients;
	QPoint m_position;
	QPoint m_targetPosition;
	QSize m_size;
	QSize m_targetSize;
	qreal m_highlightIntensity;
	qreal m_urgencyHighlightIntensity;
    bool m_dragging;
	QPointF m_mouseDownPosition;
	QPoint m_dragStartPosition;
    bool m_isMinimized;
};

// Used for tracking connected windows (X11 clients).
// Client may have it's DockItem, but not necessary (for example, special windows are not shown in dock).
class Client
{
public:
	Client(DockApplet* dockApplet, unsigned long handle);
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

	DockApplet* m_dockApplet;
	unsigned long m_handle;
	QString m_name;
	QIcon m_icon;
	bool m_isUrgent;
	bool m_visible;
	DockItem* m_dockItem;
};

class DockApplet: public Applet
{
	Q_OBJECT
public:
	DockApplet(PanelWindow* panelWindow);
	~DockApplet();
    void close();
    virtual void setPanelWindow(PanelWindow* panelWindow);

	bool init();
	QSize desiredSize();
    void startPlugin(){}


	void registerDockItem(DockItem* dockItem);
	void unregisterDockItem(DockItem* dockItem);

	DockItem* dockItemForClient(Client* client);

	void updateLayout();

	unsigned long activeWindow() const { return m_activeWindow; }

	void draggingStarted();
	void draggingStopped();
	void moveItem(DockItem* dockItem, bool right);

public slots:
    void fontChanged();

protected:
	void layoutChanged();
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);

private slots:
	void windowPropertyChanged(unsigned long window, unsigned long atom);
    void windowReconfigured(unsigned long window, int x, int y, int width, int height);
    /**
     * @brief show keyboard layout configuration dialog
     */
    void showConfigurationDialog();

private:
    void updateClientList();
    void updateActiveWindow();
    void readSettings();

	QMap<unsigned long, Client*> m_clients;
    QList <Client*> m_in_loop;
	QVector<DockItem*> m_dockItems;
	unsigned long m_activeWindow;
	bool m_dragging;
    bool m_only_minimized;
    bool m_only_current_screen;
    bool m_only_current_desktop;
};


class DockAppletPlugin: public QObject, public AppletPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "hde.panel.appletplugin")
    Q_INTERFACES(AppletPlugin)

public:
    DockAppletPlugin(){}
    ~DockAppletPlugin(){}

    Applet* createApplet(PanelWindow* panelWindow) {return new DockApplet(panelWindow);}
};

#endif
