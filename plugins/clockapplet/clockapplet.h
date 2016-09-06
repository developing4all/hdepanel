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

#ifndef CLOCKAPPLET_H
#define CLOCKAPPLET_H

#include "applet.h"

class QTimer;
class TextGraphicsItem;

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

protected:
	void layoutChanged();

private slots:
	void updateContent();

private:
	void scheduleUpdate();

	QTimer* m_timer;
	QString m_text;
	TextGraphicsItem* m_textItem;
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
