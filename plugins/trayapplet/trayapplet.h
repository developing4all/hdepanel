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

#ifndef TRAYAPPLET_H
#define TRAYAPPLET_H

#include <QtCore/QVector>
#include <QtCore/QSize>
#include "applet.h"

class TrayApplet;

class TrayItem: public QObject, public QGraphicsItem
{
	Q_OBJECT
	Q_INTERFACES(QGraphicsItem)
public:
	TrayItem(TrayApplet* trayApplet, unsigned long window);
	~TrayItem();

	void setPosition(const QPoint& position);
	void setSize(const QSize& size);

	QRectF boundingRect() const;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

	unsigned long window() const
	{
		return m_window;
	}

private:
	QSize m_size;
	TrayApplet* m_trayApplet;
	unsigned long m_window;
};

class TrayApplet: public Applet
{
	Q_OBJECT
public:
    TrayApplet(PanelWindow* panelWindow = 0);
	~TrayApplet();
    void close();
    virtual void setPanelWindow(PanelWindow* panelWindow);

	bool init();
    void startPlugin(){}


	QSize desiredSize();

	void registerTrayItem(TrayItem* trayItem);
	void unregisterTrayItem(TrayItem* trayItem);

	int iconSize() const { return m_iconSize; }

protected:
	void layoutChanged();

public slots:
    void fontChanged(){}

private slots:
    void clientMessageReceived(unsigned long window, unsigned long atom, void* data);
	void windowClosed(unsigned long window);
	void windowReconfigured(unsigned long window, int x, int y, int width, int height);
	void windowDamaged(unsigned long window);

private:
	void updateLayout();

	bool m_initialized;
	QVector<TrayItem*> m_trayItems;
	int m_iconSize;
	int m_spacing;
};


class TrayAppletPlugin: public QObject, public AppletPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "hde.panel.appletplugin")
    Q_INTERFACES(AppletPlugin)

public:
    TrayAppletPlugin(){}
    ~TrayAppletPlugin(){}

    Applet* createApplet(PanelWindow* panelWindow) {return new TrayApplet(panelWindow);}
};
#endif
