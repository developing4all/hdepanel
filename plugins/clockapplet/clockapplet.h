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

#ifndef CLOCKAPPLET_H
#define CLOCKAPPLET_H

#include "applet.h"

class QTimer;
class TextGraphicsItem;
class Calendar;

class ClockApplet: public Applet
{
	Q_OBJECT
public:
    ClockApplet(PanelWindow* panelWindow = 0);
	~ClockApplet();
    void close();
    void setPanelWindow(PanelWindow* panelWindow);

	bool init();
    void startPlugin(){}

    QSize desiredSize();

public slots:
    void fontChanged();
    void clicked();
    void showConfigurationDialog();

protected:
	void layoutChanged();
    bool isHighlighted(){ return isUnderMouse();}
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event);

private slots:
	void updateContent();

private:
	void scheduleUpdate();
    void readSettings();

	// IMPORTANT: initialize pointers to avoid random non-null garbage (Qt6 crash in scheduleUpdate)
	QTimer* m_timer = nullptr;
	QString m_text;
	TextGraphicsItem* m_textItem = nullptr;
    Calendar *m_calendar = nullptr;
    bool m_use24HourFormat = false;
};



class ClockAppletPlugin: public QObject, public AppletPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "hde.panel.appletplugin")
    Q_INTERFACES(AppletPlugin)

public:
    ClockAppletPlugin(){}
    ~ClockAppletPlugin(){}

    Applet* createApplet(PanelWindow* panelWindow) {return new ClockApplet(panelWindow);}
};
#endif
