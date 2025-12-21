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

#ifndef NETWORKMANAGERAPPLET_H
#define NETWORKMANAGERAPPLET_H

#include "applet.h"

class QTimer;
class TextGraphicsItem;
class QGraphicsPixmapItem;
class QDBusInterface;
class QDBusPendingCallWatcher;

class NetworkManagerApplet: public Applet
{
	Q_OBJECT
public:
    NetworkManagerApplet(PanelWindow* panelWindow = 0);
	~NetworkManagerApplet();
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
	void onNetworkStateChanged();
	void onPropertiesChanged(QString interface, QVariantMap changed, QStringList invalidated);
	void onNetworkManagerPropertiesChanged(QString interface, QVariantMap changed, QStringList invalidated);
	void onGetDevicesFinished(QDBusPendingCallWatcher* watcher);
	void onGetActiveConnectionFinished(QDBusPendingCallWatcher* watcher);
	void onGetConnectionInfoFinished(QDBusPendingCallWatcher* watcher);
	void onGetAccessPointsFinished(QDBusPendingCallWatcher* watcher);
	void connectToWifi();
	void toggleWifi();
	void toggleLan();

private:
	void scheduleUpdate();
    void readSettings();
    void updateNetworkInfo();
    QString getNetworkIconName(int strength, bool connected);
    void findWifiDevice();
    void getActiveConnection();
    void getActiveConnectionSync(); // Synchronous version for menu
    void getConnectionInfo(const QString& connectionPath);
    void getAvailableAccessPoints();
    void showNetworkMenu();
    void connectToAccessPoint(const QString& ssid, const QString& path);
    void findLanDevice();
    bool isLanEnabled();
    void setLanEnabled(bool enabled);

	QTimer* m_timer = nullptr;
	QString m_text;
	TextGraphicsItem* m_textItem = nullptr;
    QGraphicsPixmapItem* m_iconItem = nullptr;
    
    // NetworkManager D-Bus interfaces
    QDBusInterface* m_managerInterface = nullptr;
    QDBusInterface* m_deviceInterface = nullptr;
    QDBusInterface* m_activeConnectionInterface = nullptr;
    
    // Network info
    bool m_wifiConnected = false;
    bool m_wifiEnabled = false;
    QString m_ssid;
    int m_signalStrength = 0; // 0-100
    QString m_wifiDevicePath;
    QString m_activeConnectionPath;
    QString m_lanDevicePath;
    bool m_lanEnabled = false;
    
    // WiFi access points list
    struct AccessPoint {
        QString ssid;
        QString path;
        int strength;
        bool secured;
    };
    QList<AccessPoint> m_accessPoints;
    QString m_selectedAccessPointPath;
    
    // Display options
    bool m_showSSID = true;
    bool m_showIcon = true;
    bool m_showSignalStrength = false;
    
    // Cache icon lookups to avoid expensive theme resolution on every update/layout
    QString m_lastIconName;
    int m_lastIconSize = 0;
};

class NetworkManagerAppletPlugin: public QObject, public AppletPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "hde.panel.appletplugin")
    Q_INTERFACES(AppletPlugin)

public:
    NetworkManagerAppletPlugin(){}
    ~NetworkManagerAppletPlugin(){}

    Applet* createApplet(PanelWindow* panelWindow) {return new NetworkManagerApplet(panelWindow);}
    QString name() const override { return tr("Network Manager"); }
};
#endif

