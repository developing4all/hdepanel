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
#include <QDBusArgument>
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

// Helper function to extract image from QDBusArgument immediately
// This MUST be called while the D-Bus message is still being processed
static QImage extractImageFromDBusArgument(const QDBusArgument& arg)
{
    static const bool debugIcons = qEnvironmentVariableIsSet("HDE_NOTIFICATION_ICON_DEBUG");
    
    if (arg.currentType() != QDBusArgument::StructureType) {
        if (debugIcons) qDebug() << "NotificationServer: image arg is not a structure";
        return QImage();
    }
    
    qint32 width = 0, height = 0, rowstride = 0;
    bool hasAlpha = false;
    qint32 bitsPerSample = 8, channels = 4;
    QByteArray data;
    
    arg.beginStructure();
    arg >> width >> height >> rowstride >> hasAlpha >> bitsPerSample >> channels;
    
    // Extract byte array
    arg.beginArray();
    data.reserve(height * rowstride);
    while (!arg.atEnd()) {
        uchar byte;
        arg >> byte;
        data.append(static_cast<char>(byte));
    }
    arg.endArray();
    arg.endStructure();
    
    if (debugIcons) {
        qDebug() << "NotificationServer: extracted image data - width:" << width 
                 << "height:" << height << "rowstride:" << rowstride 
                 << "hasAlpha:" << hasAlpha << "channels:" << channels 
                 << "dataSize:" << data.size();
    }
    
    if (width <= 0 || height <= 0 || data.isEmpty() || channels < 3) {
        return QImage();
    }
    
    // Create QImage
    QImage::Format format = hasAlpha ? QImage::Format_ARGB32 : QImage::Format_RGB32;
    QImage image(width, height, format);
    
    if (image.isNull()) {
        return QImage();
    }
    
    int expectedDataSize = height * rowstride;
    if (data.size() < expectedDataSize || rowstride <= 0) {
        if (debugIcons) qDebug() << "NotificationServer: data size mismatch";
        return QImage();
    }
    
    const uchar* src = reinterpret_cast<const uchar*>(data.constData());
    
    for (int y = 0; y < height; ++y) {
        QRgb* destLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        const uchar* row = src + y * rowstride;
        
        if (channels == 4 && bitsPerSample == 8) {
            // RGBA format from D-Bus (R, G, B, A bytes in order)
            for (int x = 0; x < width; ++x) {
                int offset = x * 4;
                uchar r = row[offset + 0];
                uchar g = row[offset + 1];
                uchar b = row[offset + 2];
                uchar a = row[offset + 3];
                destLine[x] = qRgba(r, g, b, a);
            }
        } else if (channels == 3 && bitsPerSample == 8) {
            // RGB format (no alpha)
            for (int x = 0; x < width; ++x) {
                int offset = x * 3;
                uchar r = row[offset + 0];
                uchar g = row[offset + 1];
                uchar b = row[offset + 2];
                destLine[x] = qRgb(r, g, b);
            }
        }
    }
    
    if (debugIcons && !image.isNull()) {
        qDebug() << "NotificationServer: successfully created image" << image.size();
    }
    
    return image;
}

uint NotificationServer::Notify(const QString &app_name, uint replaces_id, const QString &app_icon,
                                 const QString &summary, const QString &body, const QStringList &actions,
                                 const QVariantMap &hints, int timeout)
{
    Q_UNUSED(app_icon)
    static const bool debugIcons = qEnvironmentVariableIsSet("HDE_NOTIFICATION_ICON_DEBUG");
    
    uint id = replaces_id;
    if (id == 0) {
        id = m_nextId++;
    }

    // Create a modified hints map where we extract image data immediately
    // QDBusArgument MUST be read while the D-Bus message is still being processed
    QVariantMap processedHints = hints;
    
    // Extract image-data (hyphen version)
    if (hints.contains("image-data")) {
        QVariant imageVar = hints.value("image-data");
        if (imageVar.canConvert<QDBusArgument>()) {
            QDBusArgument arg = imageVar.value<QDBusArgument>();
            QImage image = extractImageFromDBusArgument(arg);
            if (!image.isNull()) {
                processedHints["_extracted_image"] = image;
                processedHints.remove("image-data");
                if (debugIcons) qDebug() << "NotificationServer: extracted image-data successfully";
            }
        }
    }
    
    // Extract image_data (underscore version) - KDE Connect uses this
    if (hints.contains("image_data") && !processedHints.contains("_extracted_image")) {
        QVariant imageVar = hints.value("image_data");
        if (imageVar.canConvert<QDBusArgument>()) {
            QDBusArgument arg = imageVar.value<QDBusArgument>();
            QImage image = extractImageFromDBusArgument(arg);
            if (!image.isNull()) {
                processedHints["_extracted_image"] = image;
                processedHints.remove("image_data");
                if (debugIcons) qDebug() << "NotificationServer: extracted image_data successfully";
            }
        }
    }

    // Forward to applet with processed hints
    if (m_applet) {
        m_applet->onNotificationReceived(id, app_name, summary, body, actions, processedHints, timeout);
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

