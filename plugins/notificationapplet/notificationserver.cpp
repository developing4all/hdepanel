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

#include "notificationserver.h"
#include "notificationapplet.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusVariant>
#include <QDebug>
#include <QImage>
#include <QPixmap>
#include <QIcon>

NotificationServer::NotificationServer(QObject *parent)
    : QObject(parent)
    , m_applet(nullptr)
    , m_registered(false)
    , m_nextId(1)
{
}

NotificationServer::~NotificationServer()
{
    unregisterService();
}

void NotificationServer::setApplet(NotificationApplet* applet)
{
    m_applet = applet;
}

bool NotificationServer::registerService()
{
    if (m_registered) {
        return true;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    
    // Register the object path with the correct interface
    if (!bus.registerObject("/org/freedesktop/Notifications", this, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        qWarning() << "Failed to register notification server object:" << bus.lastError().message();
        return false;
    }

    // Try to register the service name
    // If another notification daemon is running, this will fail (which is fine)
    QDBusConnectionInterface* iface = bus.interface();
    if (iface) {
        QDBusReply<QString> reply = iface->serviceOwner("org.freedesktop.Notifications");
        if (reply.isValid() && !reply.value().isEmpty()) {
            qDebug() << "Another notification service is already running:" << reply.value();
            qDebug() << "HDEPanel notification applet will not receive notifications directly.";
            qDebug() << "Please stop the other notification daemon to use HDEPanel's notification server.";
            // Unregister object since we can't register the service
            bus.unregisterObject("/org/freedesktop/Notifications");
            return false;
        }
    }

    if (!bus.registerService("org.freedesktop.Notifications")) {
        qWarning() << "Failed to register notification service:" << bus.lastError().message();
        qWarning() << "Another notification daemon may be running. Notifications may not work.";
        // Unregister object if service registration failed
        bus.unregisterObject("/org/freedesktop/Notifications");
        return false;
    }

    m_registered = true;
    qDebug() << "Notification server registered: org.freedesktop.Notifications";
    return true;
}

void NotificationServer::unregisterService()
{
    if (!m_registered) {
        return;
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject("/org/freedesktop/Notifications");
    bus.unregisterService("org.freedesktop.Notifications");
    
    m_registered = false;
}

uint NotificationServer::Notify(const QString &app_name, uint replaces_id, const QString &app_icon,
                                 const QString &summary, const QString &body, const QStringList &actions,
                                 const QVariantMap &hints, int timeout)
{
    Q_UNUSED(app_icon)
    
    uint id = replaces_id;
    if (id == 0) {
        id = m_nextId++;
    }

    // Forward to applet
    if (m_applet) {
        m_applet->onNotificationReceived(id, app_name, summary, body, actions, hints, timeout);
    }

    // Store the reply message if we need to send it later
    QDBusMessage reply = message().createReply(id);
    QDBusConnection::sessionBus().send(reply);

    return id;
}

void NotificationServer::CloseNotification(uint id)
{
    // Forward to applet
    if (m_applet) {
        m_applet->onNotificationClosed(id, 2); // 2 = closed by user
    }

    // Emit signal
    emit NotificationClosed(id, 2);
}

QStringList NotificationServer::GetCapabilities()
{
    return QStringList() << "body" << "body-hyperlinks" << "body-images" << "body-markup"
                        << "icon-static" << "actions" << "action-icons" << "persistence";
}

void NotificationServer::GetServerInformation(QString &name, QString &vendor, QString &version, QString &spec_version)
{
    name = "HDEPanel Notification Server";
    vendor = "HDEPanel";
    version = "1.0";
    spec_version = "1.2";
}

