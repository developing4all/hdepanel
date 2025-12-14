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

#ifndef DOCKITEM_H
#define DOCKITEM_H

#include <QtCore/QVector>
#include <QtCore/QTimer>
#include <QtGui/QIcon>
#if QT_VERSION >= 0x050000
#include <QGraphicsItem>
#else
#include <QtGui/QGraphicsItem>
#endif

// Forward declarations
class DockApplet;
class Client;
class WaylandClient;
class QGraphicsPixmapItem;
class TextGraphicsItem;

// Represents a single item in a dock.
// There isn't one to one relationship between window (client) and dock item, that's why
// it's separate entity. One dock item can represent pinned launcher and one or more opened
// windows of that application.
class DockItem: public QObject, public QGraphicsItem
{
	Q_OBJECT
public:
	DockItem(DockApplet* dockApplet);
	~DockItem();

	void updateContent();

	void addClient(Client* client);
	void removeClient(Client* client);
	void setWaylandClient(WaylandClient* waylandClient);
	bool hasClient(Client* client) const;
	bool hasWaylandClient(WaylandClient* client) const;
	void setText(const QString& text);
	void setIcon(const QIcon& icon);
	QString text() const;
	bool shouldDelete() const { return m_shouldDelete; }

	void setTargetPosition(const QPoint& targetPosition);
	void setTargetSize(const QSize& targetSize);
	void moveInstantly();
	void startAnimation();
	void setButtonColor(const QColor& color, int transparency);
	void setFocusColor(const QColor& color, int transparency);

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
	bool isFocused() const;

	QTimer* m_animationTimer;
	DockApplet* m_dockApplet;
	TextGraphicsItem* m_textItem;
	QGraphicsPixmapItem* m_iconItem;
	QVector<Client*> m_clients;
	WaylandClient* m_waylandClient;
	QString m_waylandText;  // Store Wayland client text separately
	QPoint m_position;
	QPoint m_targetPosition;
	QSize m_size;
	QSize m_targetSize;
	qreal m_highlightIntensity;
	qreal m_focusHighlightIntensity;
	qreal m_urgencyHighlightIntensity;
    bool m_dragging;
	QPointF m_mouseDownPosition;
	QPoint m_dragStartPosition;
    bool m_isMinimized;
    bool m_shouldDelete;
    
    // Color settings
    QColor m_buttonColor;
    int m_buttonColorTransparency;
    QColor m_focusColor;
    int m_focusColorTransparency;
};

#endif
