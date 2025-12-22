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

#ifndef NOTIFICATIONSERVER_H
#define NOTIFICATIONSERVER_H

#include <QObject>
#include <QDBusContext>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QStringList>
#include <QVariantMap>

class NotificationApplet;

/**
 * D-Bus notification server implementing org.freedesktop.Notifications
 * 
 * This server receives notifications from applications and forwards
 * them to the NotificationApplet.
 */
class NotificationServer : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

public:
    explicit NotificationServer(QObject *parent = nullptr);
    ~NotificationServer();

    /**
     * Register the notification service on the session bus
     * @return true if registration was successful
     */
    bool registerService();

    /**
     * Unregister the notification service
     */
    void unregisterService();

    /**
     * Set the applet to forward notifications to
     */
    void setApplet(NotificationApplet* applet);

public slots:
    /**
     * FreeDesktop.Notifications.Notify method
     * Receives notifications from applications
     */
    uint Notify(const QString &app_name, uint replaces_id, const QString &app_icon,
                const QString &summary, const QString &body, const QStringList &actions,
                const QVariantMap &hints, int timeout);

    /**
     * FreeDesktop.Notifications.CloseNotification method
     */
    void CloseNotification(uint id);

    /**
     * FreeDesktop.Notifications.GetCapabilities method
     */
    QStringList GetCapabilities();

    /**
     * FreeDesktop.Notifications.GetServerInformation method
     */
    void GetServerInformation(QString &name, QString &vendor, QString &version, QString &spec_version);

signals:
    /**
     * FreeDesktop.Notifications.NotificationClosed signal
     */
    void NotificationClosed(uint id, uint reason);

    /**
     * FreeDesktop.Notifications.ActionInvoked signal
     */
    void ActionInvoked(uint id, const QString &action_key);

private:
    NotificationApplet* m_applet;
    bool m_registered;
    uint m_nextId;
    QMap<uint, QDBusMessage> m_pendingReplies;
};

#endif // NOTIFICATIONSERVER_H

