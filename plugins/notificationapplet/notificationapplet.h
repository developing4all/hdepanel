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
 * version 3.0 of the License, or (at your option) any later version.
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

#ifndef NOTIFICATIONAPPLET_H
#define NOTIFICATIONAPPLET_H

#include "../../lib/applet.h"
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusArgument>
#include <QDateTime>
#include <QIcon>
#include <QVariantMap>

class QTimer;
class TextGraphicsItem;
class NotificationList;
class NotificationPopup;
class QGraphicsPixmapItem;
class NotificationServer;

struct Notification {
    uint id;
    QString appName;
    QString summary;
    QString body;
    QStringList actions;
    QVariantMap hints;
    int timeout;
    QDateTime timestamp;
    QIcon icon;
    bool isRead;
    
    Notification() : id(0), timeout(0), isRead(false) {}
};

class NotificationApplet: public Applet
{
    Q_OBJECT
public:
    NotificationApplet(PanelWindow* panelWindow = 0);
    ~NotificationApplet();
    void close();
    void setPanelWindow(PanelWindow* panelWindow);

    bool init();
    QSize desiredSize();

public slots:
    void fontChanged();
    void clicked();
    void showConfigurationDialog();

protected:
    void layoutChanged();
    bool isHighlighted() { return isUnderMouse(); }
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event);

private slots:
    void onNotificationReceived(uint id, const QString &appName, const QString &summary, 
                                const QString &body, const QStringList &actions, 
                                const QVariantMap &hints, int timeout);
    void onNotificationClosed(uint id, uint reason);
    void onActionInvoked(uint id, const QString &actionKey);
    void updateContent();
    void showNotificationPopup(const Notification &notification);

private:
    void readSettings();
    void connectToNotificationService();
    void updateIcon();
    void removeNotification(uint id);
    QPoint calculateNotificationPosition(const QSize &notificationSize);
    
    // Called by NotificationServer
    friend class NotificationServer;

    QDBusInterface* m_notificationInterface;
    NotificationServer* m_notificationServer;
    QList<Notification> m_notifications;
    NotificationList* m_notificationList;
    QList<NotificationPopup*> m_activePopups;
    QGraphicsPixmapItem* m_iconItem;
    TextGraphicsItem* m_countItem;
    int m_unreadCount;
    void markNotificationAsRead(uint id);
    void refreshNotificationList();
    int m_notificationPosition; // 0=TopLeft, 1=TopRight, 2=BottomLeft, 3=BottomRight, 4=Center, 5=AtApplet
    bool m_showCount;
    int m_iconSize;
};

class NotificationAppletPlugin: public QObject, public AppletPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "hde.panel.appletplugin")
    Q_INTERFACES(AppletPlugin)

public:
    NotificationAppletPlugin() {}
    ~NotificationAppletPlugin() {}

    Applet* createApplet(PanelWindow* panelWindow) { return new NotificationApplet(panelWindow); }
    QString name() const override { return tr("Notifications"); }
};

#endif // NOTIFICATIONAPPLET_H

