/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * This Files has been imported to hde from qtpanel
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#ifndef APPLET_H
#define APPLET_H

#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtCore/QMargins>

#if QT_VERSION >= 0x050000
#include <QGraphicsItem>
#else
#include <QtGui/QGraphicsItem>
#endif

class PanelWindow;
class Applet;

class AppletPlugin
{
public:
	AppletPlugin(){}
	virtual ~AppletPlugin(){}
	virtual Applet* createApplet(PanelWindow* panelWindow) = 0;
	virtual QString name() const = 0;
};

Q_DECLARE_INTERFACE(AppletPlugin, "hde.panel.appletplugin")

class Applet: public QObject, public QGraphicsItem
{
	Q_OBJECT
	Q_INTERFACES(QGraphicsItem)

public:
	Applet(PanelWindow* panelWindow = 0);
	~Applet();

	virtual void setPanelWindow(PanelWindow* panelWindow);
	virtual void close() = 0;

	virtual bool init();

	 bool expandable() const { return m_expandable; }
	 virtual QMargins buttonMargins() const { return QMargins(0,0,0,0); }
 
	 void setId(QString id){ m_id = id; }
	 QString id() const { return m_id; }
 
	 void setPosition(const QPoint& position);
	 void setSize(const QSize& size);
 
	 const QSize& size() const { return m_size; }
 
	 virtual QSize desiredSize() = 0;
 
	 PanelWindow* panelWindow() { return m_panelWindow; }
 
	 void setInteractive(bool interactive);
 
	 QRectF boundingRect() const override;
	 void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
 
 public slots:
	 void animateHighlight();
	 virtual void clicked();
	 virtual void fontChanged() = 0;
 
 protected:
	 virtual void layoutChanged();
	 void setExpandable(bool on) { m_expandable = on; }
 
	 QPoint localToScreen(const QPoint& point);
 
	 QRectF buttonRect() const
	 {
		 const QMargins m = buttonMargins();
		 const qreal w = qMax<qreal>(0.0, m_size.width()  - m.left() - m.right());
		 const qreal h = qMax<qreal>(0.0, m_size.height() - m.top()  - m.bottom());
		 return QRectF(m.left(), m.top(), w, h);
	 }
 
	 virtual bool isHighlighted();
 
	 void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
	 void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
	 void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
	 void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
 
	 PanelWindow* m_panelWindow = nullptr;
	 QString m_id;
 
	 QPoint m_position;
	 QSize  m_size;
 
	 bool  m_interactive = false;
	 qreal m_highlightIntensity = 0.0;
 
	 bool m_expandable = false;
 
	 // We track hover ourselves so we don’t depend on isUnderMouse() being correct
	 // when children are involved.
	 bool m_hovered = false;
 };
 
 #endif
 