/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * Copyright: 2015-2025 Haydar Alkaduhimi
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
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#ifndef BATTERYAPPLET_H
#define BATTERYAPPLET_H

#include "applet.h"

class QTimer;
class TextGraphicsItem;
class QGraphicsPixmapItem;

class BatteryApplet: public Applet
{
	Q_OBJECT
public:
    BatteryApplet(PanelWindow* panelWindow = 0);
	~BatteryApplet();
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
    void updateBatteryInfo();
    QString getBatteryIconName(int percentage, bool charging, bool present);
    QString findBatteryDevice();
    bool readBatteryFile(const QString& path, QString& content);

	QTimer* m_timer = nullptr;
	QString m_text;
	TextGraphicsItem* m_textItem = nullptr;
    QGraphicsPixmapItem* m_iconItem = nullptr;
    
    // Battery info
    int m_batteryPercentage = 0;
    bool m_batteryCharging = false;
    bool m_batteryPresent = false;
    bool m_showPercentage = true;
    bool m_showIcon = true;
    
    // Battery device path (sysfs)
    QString m_batteryDevicePath;

    // Cache icon lookups to avoid expensive theme resolution on every update/layout
    QString m_lastIconName;
    int m_lastIconSize = 0;
};



class BatteryAppletPlugin: public QObject, public AppletPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "hde.panel.appletplugin")
    Q_INTERFACES(AppletPlugin)

public:
    BatteryAppletPlugin(){}
    ~BatteryAppletPlugin(){}

    Applet* createApplet(PanelWindow* panelWindow) {return new BatteryApplet(panelWindow);}
    QString name() const override { return tr("Battery"); }
};
#endif

