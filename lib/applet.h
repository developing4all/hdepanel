/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * This Files has been imported to hde from qtpanel
 *
 * Copyright: 2015-2016 Haydar Alkaduhimi
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

#ifndef APPLET_H
#define APPLET_H

#include <QtCore/QObject>
#include <QtCore/QPoint>
#include <QtCore/QSize>
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

	void setPosition(const QPoint& position);
	void setSize(const QSize& size);

	const QSize& size() const
	{
		return m_size;
	}

	virtual QSize desiredSize() = 0;


	PanelWindow* panelWindow()
	{
		return m_panelWindow;
	}

	void setInteractive(bool interactive);

	QRectF boundingRect() const;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

public slots:
	void animateHighlight();
	virtual void clicked();
    virtual void fontChanged()=0;

protected:
	virtual void layoutChanged();
	QPoint localToScreen(const QPoint& point);

	virtual bool isHighlighted();

	void hoverEnterEvent(QGraphicsSceneHoverEvent* event);
	void hoverLeaveEvent(QGraphicsSceneHoverEvent* event);
	void mousePressEvent(QGraphicsSceneMouseEvent* event);
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);

	PanelWindow* m_panelWindow;
	QPoint m_position;
	QSize m_size;
	bool m_interactive;
	qreal m_highlightIntensity;
};
#endif
