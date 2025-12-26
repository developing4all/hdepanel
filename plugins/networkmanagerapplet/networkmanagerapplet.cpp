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

#include "networkmanagerapplet.h"
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <settings.h>
#include <QMenu>
#include "hpopupmenu.h"
#include <QApplication>
#include <QAction>
#include <QActionGroup>
#include <QCursor>
#include <algorithm>
#if QT_VERSION >= 0x050000
#include <QGraphicsSceneContextMenuEvent>
#else
#include <QtGui/QGraphicsSceneContextMenuEvent>
#endif
#if QT_VERSION >= 0x050000
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#else
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsPixmapItem>
#endif
#include "textgraphicsitem.h"
#include "panelwindow.h"
#include <QFontMetrics>
#include <QIcon>
#include <QPainter>
#include "../../lib/dpisupport.h"
#include <QDBusInterface>
#include <QDBusArgument>
#include <QDBusReply>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QVariant>
#include <QVariantMap>
#include <QProcess>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <functional>
#include <memory>

namespace {
static QString decodeSsidFromDbusVariant(const QVariant& v)
{
    // NetworkManager AccessPoint.Ssid is a D-Bus "ay" (array of bytes).
    // Depending on Qt/D-Bus conversion, it can come as QByteArray or QDBusArgument.
    QByteArray bytes;

    if (v.canConvert<QByteArray>()) {
        bytes = v.toByteArray();
    } else if (v.canConvert<QDBusArgument>()) {
        QDBusArgument arg = v.value<QDBusArgument>();
        QByteArray out;
        arg.beginArray();
        while (!arg.atEnd()) {
            quint8 b = 0;
            arg >> b;
            out.append(static_cast<char>(b));
        }
        arg.endArray();
        bytes = out;
    }

    QString ssid = QString::fromUtf8(bytes).trimmed();
    return ssid;
}
} // namespace

NetworkManagerApplet::NetworkManagerApplet(PanelWindow* panelWindow)
	: Applet(panelWindow)
{
    setObjectName("NetworkManager");
    
    // Create text and icon items early
    m_textItem = new TextGraphicsItem(this);
    m_textItem->setColor(Qt::white);
    
    m_iconItem = new QGraphicsPixmapItem(this);
    
    // Initialize NetworkManager D-Bus interface
    m_managerInterface = new QDBusInterface("org.freedesktop.NetworkManager",
                                          "/org/freedesktop/NetworkManager",
                                          "org.freedesktop.NetworkManager",
                                          QDBusConnection::systemBus(), this);
}

void NetworkManagerApplet::setPanelWindow(PanelWindow *panelWindow)
{
    Applet::setPanelWindow(panelWindow);

    if (m_timer) {
        m_timer->setSingleShot(false);
        m_timer->setInterval(5000); // Update every 5 seconds
        connect(m_timer, SIGNAL(timeout()), this, SLOT(updateContent()));
    } else {
        m_timer = new QTimer(this);
        m_timer->setSingleShot(false);
        m_timer->setInterval(5000); // Update every 5 seconds
        connect(m_timer, SIGNAL(timeout()), this, SLOT(updateContent()));
    }

    if (m_textItem && m_panelWindow) {
        m_textItem->setFont(m_panelWindow->font());
    }
}

NetworkManagerApplet::~NetworkManagerApplet()
{
    close();
}

void NetworkManagerApplet::close()
{
    if (m_textItem) {
        delete m_textItem;
        m_textItem = nullptr;
    }
    
    if (m_iconItem) {
        delete m_iconItem;
        m_iconItem = nullptr;
    }
    
    if (m_timer) {
        m_timer->stop();
        delete m_timer;
        m_timer = nullptr;
    }
    
    if (m_managerInterface) {
        delete m_managerInterface;
        m_managerInterface = nullptr;
    }
    
    if (m_deviceInterface) {
        delete m_deviceInterface;
        m_deviceInterface = nullptr;
    }
    
    if (m_activeConnectionInterface) {
        delete m_activeConnectionInterface;
        m_activeConnectionInterface = nullptr;
    }
}

void NetworkManagerApplet::fontChanged()
{
    if (m_textItem && m_panelWindow) {
        m_textItem->setFont(m_panelWindow->font());
        updateContent();
    }
}

bool NetworkManagerApplet::init()
{
    setInteractive(true);
    readSettings();
    
    // Check if NetworkManager is available
    if (!m_managerInterface || !m_managerInterface->isValid()) {
        m_wifiEnabled = false;
        m_wifiConnected = false;
        m_text = tr("No NetworkManager");
        updateContent();
        return true; // Still return true so applet shows "not available" state
    }
    
    // Connect to NetworkManager signals
    QDBusConnection::systemBus().connect("org.freedesktop.NetworkManager",
                                        "/org/freedesktop/NetworkManager",
                                        "org.freedesktop.NetworkManager",
                                        "StateChanged",
                                        this, SLOT(onNetworkStateChanged()));
    
    // Monitor WirelessEnabled property changes
    QDBusConnection::systemBus().connect("org.freedesktop.NetworkManager",
                                        "/org/freedesktop/NetworkManager",
                                        "org.freedesktop.DBus.Properties",
                                        "PropertiesChanged",
                                        this, SLOT(onNetworkManagerPropertiesChanged(QString,QVariantMap,QStringList)));
    
    // Find WiFi device and get initial state
    findWifiDevice();
    updateContent();
    
    if (m_timer) {
        m_timer->start();
    }
    return true;
}

void NetworkManagerApplet::layoutChanged()
{
    if (!m_textItem || !m_panelWindow)
        return;

    const PanelWindow::Position pos = m_panelWindow->position();
    const bool verticalPanel = (pos == PanelWindow::Left || pos == PanelWindow::Right);

    const int availW = (m_size.width() > 0 ? m_size.width() : (verticalPanel ? m_panelWindow->panelWidth()
                                                                            : m_panelWindow->panelHeight()));
    const int availH = (m_size.height() > 0 ? m_size.height() : (verticalPanel ? m_panelWindow->panelHeight()
                                                                               : m_panelWindow->panelWidth()));

    updateContent();
}

void NetworkManagerApplet::readSettings()
{
    // Read settings from panel configuration
    // For now, use defaults
    m_showSSID = true;
    m_showIcon = true;
    m_showSignalStrength = false;
}

void NetworkManagerApplet::findWifiDevice()
{
    if (!m_managerInterface || !m_managerInterface->isValid()) {
        return;
    }
    
    // Get all devices asynchronously
    QDBusPendingCall asyncCall = m_managerInterface->asyncCall("GetDevices");
    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(asyncCall, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), this, SLOT(onGetDevicesFinished(QDBusPendingCallWatcher*)));
}

void NetworkManagerApplet::onGetDevicesFinished(QDBusPendingCallWatcher* watcher)
{
    QDBusPendingReply<QList<QDBusObjectPath>> reply = *watcher;
    watcher->deleteLater();
    
    if (reply.isError()) {
        return;
    }
    
    QList<QDBusObjectPath> devices = reply.value();
    
    // Find WiFi device (type 2 = NM_DEVICE_TYPE_WIFI) and LAN device (type 1 = NM_DEVICE_TYPE_ETHERNET)
    for (const QDBusObjectPath& devicePath : devices) {
        QDBusInterface deviceInterface("org.freedesktop.NetworkManager",
                                      devicePath.path(),
                                      "org.freedesktop.DBus.Properties",
                                      QDBusConnection::systemBus(), this);
        
        QDBusReply<QVariant> deviceTypeReply = deviceInterface.call("Get",
                                                                    "org.freedesktop.NetworkManager.Device",
                                                                    "DeviceType");
        if (deviceTypeReply.isValid()) {
            uint deviceType = deviceTypeReply.value().toUInt();
            
            if (deviceType == 2) {
                // Found WiFi device
                m_wifiDevicePath = devicePath.path();
                
                // Get device state and enabled status
                QDBusReply<QVariant> stateReply = deviceInterface.call("Get",
                                                                       "org.freedesktop.NetworkManager.Device",
                                                                       "State");
                QDBusReply<QVariant> enabledReply = deviceInterface.call("Get",
                                                                         "org.freedesktop.NetworkManager.Device",
                                                                         "State");
                if (stateReply.isValid()) {
                    uint state = stateReply.value().toUInt();
                    // State 30-100 = activated/connected states
                    // State 20 = NM_DEVICE_STATE_DISCONNECTED (but device is enabled)
                    // State 10 = NM_DEVICE_STATE_UNAVAILABLE (device disabled)
                    m_wifiEnabled = (state >= 20); // Device is available/enabled if state >= 20
                }
                
                // Also check the wireless interface state
                QDBusInterface wirelessProps("org.freedesktop.NetworkManager",
                                            m_wifiDevicePath,
                                            "org.freedesktop.DBus.Properties",
                                            QDBusConnection::systemBus(), this);
                QDBusReply<QVariant> wirelessEnabledReply = wirelessProps.call("Get",
                                                                               "org.freedesktop.NetworkManager.Device.Wireless",
                                                                               "WirelessEnabled");
                if (wirelessEnabledReply.isValid()) {
                    m_wifiEnabled = wirelessEnabledReply.value().toBool();
                }
                
                // Create device interface for signal monitoring
                if (m_deviceInterface) {
                    delete m_deviceInterface;
                }
                m_deviceInterface = new QDBusInterface("org.freedesktop.NetworkManager",
                                                       m_wifiDevicePath,
                                                       "org.freedesktop.NetworkManager.Device.Wireless",
                                                       QDBusConnection::systemBus(), this);
                
                // Monitor property changes
                QDBusConnection::systemBus().connect("org.freedesktop.NetworkManager",
                                                     m_wifiDevicePath,
                                                     "org.freedesktop.DBus.Properties",
                                                     "PropertiesChanged",
                                                     this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
                
                // Get active connection
                getActiveConnection();
            } else if (deviceType == 1) {
                // Found Ethernet/LAN device
                m_lanDevicePath = devicePath.path();
                m_lanEnabled = isLanEnabled();
            }
        }
    }
}

void NetworkManagerApplet::getActiveConnection()
{
    if (!m_managerInterface || !m_managerInterface->isValid()) {
        return;
    }
    
    // Get active connections using Properties interface
    QDBusInterface propsInterface("org.freedesktop.NetworkManager",
                                  "/org/freedesktop/NetworkManager",
                                  "org.freedesktop.DBus.Properties",
                                  QDBusConnection::systemBus(), this);
    
    QDBusPendingCall asyncCall = propsInterface.asyncCall("Get",
                                                          "org.freedesktop.NetworkManager",
                                                          "ActiveConnections");
    QDBusPendingCallWatcher* watcher = new QDBusPendingCallWatcher(asyncCall, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), this, SLOT(onGetActiveConnectionFinished(QDBusPendingCallWatcher*)));
}

void NetworkManagerApplet::getActiveConnectionSync()
{
    if (!m_managerInterface || !m_managerInterface->isValid()) {
        return;
    }
    
    // Get active connections synchronously for menu display
    QDBusInterface propsInterface("org.freedesktop.NetworkManager",
                                  "/org/freedesktop/NetworkManager",
                                  "org.freedesktop.DBus.Properties",
                                  QDBusConnection::systemBus(), this);
    
    QDBusReply<QVariant> reply = propsInterface.call("Get",
                                                     "org.freedesktop.NetworkManager",
                                                     "ActiveConnections");
    
    if (!reply.isValid()) {
        m_wifiConnected = false;
        return;
    }
    
    QVariant activeConnectionsVar = reply.value();
    QList<QDBusObjectPath> activeConnections;
    
    // Convert variant to list of paths
    if (activeConnectionsVar.canConvert<QDBusArgument>()) {
        QDBusArgument arg = activeConnectionsVar.value<QDBusArgument>();
        arg >> activeConnections;
    } else if (activeConnectionsVar.type() == QVariant::List) {
        QList<QVariant> list = activeConnectionsVar.toList();
        for (const QVariant& v : list) {
            if (v.canConvert<QDBusObjectPath>()) {
                activeConnections.append(v.value<QDBusObjectPath>());
            }
        }
    }
    
    // Find WiFi connection
    m_activeConnectionPath.clear();
    m_wifiConnected = false;
    m_ssid.clear();
    
    for (const QDBusObjectPath& connPath : activeConnections) {
        QDBusInterface connInterface("org.freedesktop.NetworkManager",
                                    connPath.path(),
                                    "org.freedesktop.DBus.Properties",
                                    QDBusConnection::systemBus(), this);
        
        QDBusReply<QVariant> typeReply = connInterface.call("Get",
                                                             "org.freedesktop.NetworkManager.Connection.Active",
                                                             "Type");
        if (typeReply.isValid() && typeReply.value().toString() == "802-11-wireless") {
            m_activeConnectionPath = connPath.path();
            m_wifiConnected = true;

            // Primary SSID source: Connection.Active.Id (string)
            QDBusReply<QVariant> idReply = connInterface.call("Get",
                                                             "org.freedesktop.NetworkManager.Connection.Active",
                                                             "Id");
            if (idReply.isValid()) {
                const QString id = idReply.value().toString().trimmed();
                if (!id.isEmpty()) {
                    m_ssid = id;
                }
            }

            // Get connection info synchronously - this will set m_ssid
            getConnectionInfo(connPath.path());
            break;
        }
    }
}

void NetworkManagerApplet::onGetActiveConnectionFinished(QDBusPendingCallWatcher* watcher)
{
    QDBusPendingReply<QVariant> reply = *watcher;
    watcher->deleteLater();
    
    if (reply.isError()) {
        m_wifiConnected = false;
        updateContent();
        return;
    }
    
    QVariant activeConnectionsVar = reply.value();
    QList<QDBusObjectPath> activeConnections;
    
    // Convert variant to list of paths
    if (activeConnectionsVar.canConvert<QDBusArgument>()) {
        QDBusArgument arg = activeConnectionsVar.value<QDBusArgument>();
        arg >> activeConnections;
    } else if (activeConnectionsVar.type() == QVariant::List) {
        QList<QVariant> list = activeConnectionsVar.toList();
        for (const QVariant& v : list) {
            if (v.canConvert<QDBusObjectPath>()) {
                activeConnections.append(v.value<QDBusObjectPath>());
            }
        }
    }
    
    // Find WiFi connection
    m_activeConnectionPath.clear();
    bool wasConnected = m_wifiConnected;
    m_wifiConnected = false;
    m_ssid.clear();
    
    for (const QDBusObjectPath& connPath : activeConnections) {
        QDBusInterface connInterface("org.freedesktop.NetworkManager",
                                    connPath.path(),
                                    "org.freedesktop.DBus.Properties",
                                    QDBusConnection::systemBus(), this);
        
        QDBusReply<QVariant> typeReply = connInterface.call("Get",
                                                             "org.freedesktop.NetworkManager.Connection.Active",
                                                             "Type");
        if (typeReply.isValid() && typeReply.value().toString() == "802-11-wireless") {
            m_activeConnectionPath = connPath.path();
            m_wifiConnected = true;

            // Primary SSID source: Connection.Active.Id (string)
            QDBusReply<QVariant> idReply = connInterface.call("Get",
                                                             "org.freedesktop.NetworkManager.Connection.Active",
                                                             "Id");
            if (idReply.isValid()) {
                const QString id = idReply.value().toString().trimmed();
                if (!id.isEmpty()) {
                    m_ssid = id;
                }
            }

            // Clear icon cache to force icon update when connection state changes
            if (!wasConnected) {
                m_lastIconName.clear();
                m_lastIconSize = 0;
            }
            // Update content immediately to show connected state
            updateContent();
            getConnectionInfo(connPath.path());
            break;
        }
    }
    
    if (!m_wifiConnected) {
        // Clear icon cache when disconnected
        if (wasConnected) {
            m_lastIconName.clear();
            m_lastIconSize = 0;
        }
        updateContent();
    }
}

void NetworkManagerApplet::getConnectionInfo(const QString& connectionPath)
{
    if (m_activeConnectionInterface) {
        delete m_activeConnectionInterface;
    }
    
    m_activeConnectionInterface = new QDBusInterface("org.freedesktop.NetworkManager",
                                                     connectionPath,
                                                     "org.freedesktop.DBus.Properties",
                                                     QDBusConnection::systemBus(), this);
    
    // Get connection info synchronously (should be fast)
    QDBusReply<QVariant> devicesReply = m_activeConnectionInterface->call("Get",
                                                                         "org.freedesktop.NetworkManager.Connection.Active",
                                                                         "Devices");
    if (devicesReply.isValid()) {
        QList<QDBusObjectPath> devices;
        QVariant devicesVar = devicesReply.value();
        if (devicesVar.canConvert<QDBusArgument>()) {
            QDBusArgument arg = devicesVar.value<QDBusArgument>();
            arg >> devices;
        }
        
        if (!devices.isEmpty()) {
            // Get access point info from the device - this is the most reliable way to get SSID
            QDBusInterface deviceInterface("org.freedesktop.NetworkManager",
                                          devices.first().path(),
                                          "org.freedesktop.NetworkManager.Device.Wireless",
                                          QDBusConnection::systemBus(), this);
            
            QDBusReply<QDBusObjectPath> apReply = deviceInterface.call("GetActiveAccessPoint");
            if (apReply.isValid()) {
                QDBusObjectPath apPath = apReply.value();
                
                // Only proceed if we have a valid access point path (not "/")
                if (apPath.path() != "/") {
                    QDBusInterface apInterface("org.freedesktop.NetworkManager",
                                              apPath.path(),
                                              "org.freedesktop.DBus.Properties",
                                              QDBusConnection::systemBus(), this);
                    
                    // Get SSID from access point (most reliable when connected)
                    QDBusReply<QVariant> ssidReply = apInterface.call("Get",
                                                                     "org.freedesktop.NetworkManager.AccessPoint",
                                                                     "Ssid");
                    if (ssidReply.isValid()) {
                        const QString ssid = decodeSsidFromDbusVariant(ssidReply.value());
                        if (!ssid.isEmpty()) {
                            m_ssid = ssid;
                        }
                    }
                    
                    // Get signal strength (NetworkManager returns 0-100 directly)
                    QDBusReply<QVariant> strengthReply = apInterface.call("Get",
                                                                         "org.freedesktop.NetworkManager.AccessPoint",
                                                                         "Strength");
                    if (strengthReply.isValid()) {
                        uint strength = strengthReply.value().toUInt();
                        m_signalStrength = qBound(0, static_cast<int>(strength), 100); // Already 0-100, just clamp
                    }
                } else {
                    // No active access point, but we're still connected
                    // Try to get SSID from connection settings as fallback
                    QDBusReply<QVariant> connectionReply = m_activeConnectionInterface->call("Get",
                                                                                              "org.freedesktop.NetworkManager.Connection.Active",
                                                                                              "Connection");
                    if (connectionReply.isValid()) {
                        QDBusObjectPath connectionSettingsPath = connectionReply.value().value<QDBusObjectPath>();
                        if (connectionSettingsPath.path() != "/") {
                            QDBusInterface settingsInterface("org.freedesktop.NetworkManager",
                                                           connectionSettingsPath.path(),
                                                           "org.freedesktop.NetworkManager.Settings.Connection",
                                                           QDBusConnection::systemBus(), this);
                            
                            QDBusReply<QVariantMap> settingsReply = settingsInterface.call("GetSettings");
                            if (settingsReply.isValid()) {
                                QVariantMap settings = settingsReply.value();
                                if (settings.contains("802-11-wireless")) {
                                    QVariantMap wirelessSettings = settings["802-11-wireless"].toMap();
                                    if (wirelessSettings.contains("ssid")) {
                                        QByteArray ssidBytes = wirelessSettings["ssid"].toByteArray();
                                        m_ssid = QString::fromUtf8(ssidBytes);
                                    }
                                }
                            }
                        }
                    }
                    // Set a default signal strength so we show connected icon
                    if (m_signalStrength == 0) {
                        m_signalStrength = 50; // Default to medium signal
                    }
                }
            }
        }
    }
    
    // Clear icon cache to force update with new SSID
    m_lastIconName.clear();
    m_lastIconSize = 0;
    updateContent();
}

void NetworkManagerApplet::onPropertiesChanged(QString interface, QVariantMap changed, QStringList invalidated)
{
    Q_UNUSED(interface)
    Q_UNUSED(changed)
    Q_UNUSED(invalidated)
    
    // Properties changed, update info
    scheduleUpdate();
}

void NetworkManagerApplet::onNetworkStateChanged()
{
    // NetworkManager state changed, refresh
    scheduleUpdate();
}

void NetworkManagerApplet::onNetworkManagerPropertiesChanged(QString interface, QVariantMap changed, QStringList invalidated)
{
    Q_UNUSED(invalidated)
    
    // Check if WirelessEnabled property changed
    if (interface == "org.freedesktop.NetworkManager" && changed.contains("WirelessEnabled")) {
        bool newState = changed["WirelessEnabled"].toBool();
        if (newState != m_wifiEnabled) {
            m_wifiEnabled = newState;
            // Clear icon cache to force icon update
            m_lastIconName.clear();
            m_lastIconSize = 0;
            // Clear access points if disabled
            if (!m_wifiEnabled) {
                m_accessPoints.clear();
                m_wifiConnected = false;
                m_ssid.clear();
            } else {
                // Refresh device state when enabled
                findWifiDevice();
            }
            updateContent();
        }
    }
}

void NetworkManagerApplet::scheduleUpdate()
{
    // Use singleShot to batch updates and avoid rapid-fire refreshes
    if (m_timer) {
        m_timer->setSingleShot(true);
        m_timer->start(500); // Wait 500ms before updating
    }
}

void NetworkManagerApplet::updateNetworkInfo()
{
    if (!m_managerInterface || !m_managerInterface->isValid()) {
        m_wifiEnabled = false;
        m_wifiConnected = false;
        return;
    }
    
    // Only refresh if we don't have device info yet, or periodically
    if (m_wifiDevicePath.isEmpty()) {
        findWifiDevice();
    } else {
        // Just refresh active connection
        getActiveConnection();
    }
}

void NetworkManagerApplet::updateContent()
{
    if (!m_textItem || !m_panelWindow)
        return;
    
    const PanelWindow::Position pos = m_panelWindow->position();
    const bool verticalPanel = (pos == PanelWindow::Left || pos == PanelWindow::Right);

    const int availW = (m_size.width() > 0 ? m_size.width() : (verticalPanel ? m_panelWindow->panelWidth()
                                                                            : m_panelWindow->panelHeight()));
    const int availH = (m_size.height() > 0 ? m_size.height() : (verticalPanel ? m_panelWindow->panelHeight()
                                                                               : m_panelWindow->panelWidth()));

    // Build tooltip text with SSID (not shown on button)
    QString tooltipText;
    if (m_wifiConnected) {
        if (!m_ssid.isEmpty()) {
            tooltipText = m_ssid;
            if (m_showSignalStrength) {
                tooltipText += QString(" (%1%)").arg(m_signalStrength);
            }
        } else {
            // Connected but SSID not retrieved yet - show generic message
            tooltipText = tr("Connected");
        }
    } else if (!m_wifiEnabled) {
        tooltipText = tr("WiFi Off");
    } else if (!m_wifiConnected) {
        tooltipText = tr("Not Connected");
    } else {
        tooltipText = tr("Network Manager");
    }
    
    // Set tooltip instead of showing text on button
    setToolTip(tooltipText);
    
    // Hide text item - only show icon
    if (m_textItem) {
        m_textItem->setText(QString());
    }
    
    // Update icon
    if (m_showIcon) {
        QString iconName = getNetworkIconName(m_signalStrength, m_wifiConnected);
        int iconSize = qMin(availW, availH) - adjustHardcodedPixelSize(4);
        if (iconSize < adjustHardcodedPixelSize(16)) iconSize = adjustHardcodedPixelSize(16);
        if (iconSize > adjustHardcodedPixelSize(32)) iconSize = adjustHardcodedPixelSize(32);
        
        // Always update icon if cache is cleared (empty string) or icon name/size changed
        // Cache icon lookups to avoid expensive theme resolution
        if (iconName != m_lastIconName || iconSize != m_lastIconSize || m_lastIconName.isEmpty()) {
            m_lastIconName = iconName;
            m_lastIconSize = iconSize;
            QIcon icon = QIcon::fromTheme(iconName);
            if (icon.isNull()) {
                // Fallback icons based on WiFi state
                if (m_wifiEnabled && m_wifiConnected) {
                    icon = QIcon::fromTheme("network-wireless");
                } else if (!m_wifiEnabled) {
                    icon = QIcon::fromTheme("network-wireless-disconnected");
                } else {
                    icon = QIcon::fromTheme("network-wireless-disconnected");
                }
            }
            
            QPixmap pixmap = icon.pixmap(iconSize, iconSize);
            if (m_iconItem) {
                m_iconItem->setPixmap(pixmap);
            }
        }
    }
    
    // Layout icon only (centered) - no text on button
    int iconW = m_showIcon && m_iconItem ? m_iconItem->pixmap().width() : 0;
    int iconH = m_showIcon && m_iconItem ? m_iconItem->pixmap().height() : 0;
    
    if (m_showIcon && m_iconItem) {
        // Center icon in available space
        int x = (availW - iconW) / 2;
        int y = (availH - iconH) / 2;
        m_iconItem->setPos(x, y - adjustHardcodedPixelSize(4));
    }
    
    update();
}

QString NetworkManagerApplet::getNetworkIconName(int strength, bool connected)
{
    // If not connected or WiFi disabled, show disconnected icon
    if (!connected || !m_wifiEnabled) {
        return "network-wireless-disconnected";
    }
    
    // If connected, always show a signal icon (even if strength is 0, show weak)
    // Choose icon based on signal strength
    if (strength >= 75) {
        return "network-wireless-signal-excellent";
    } else if (strength >= 50) {
        return "network-wireless-signal-good";
    } else if (strength >= 25) {
        return "network-wireless-signal-ok";
    } else {
        // Even if strength is 0, if we're connected, show weak signal
        return "network-wireless-signal-weak";
    }
}

void NetworkManagerApplet::clicked()
{
    showNetworkMenu();
}

void NetworkManagerApplet::showConfigurationDialog()
{
    // TODO: Implement configuration dialog
    clicked();
}

void NetworkManagerApplet::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    HPopupMenu menu;
    
    menu.addTitle(tr("Network Manager Applet"));
    menu.addAction(QIcon::fromTheme("preferences-other"), tr("Configure..."), this, SLOT(showConfigurationDialog()));
    
    menu.exec(event->screenPos());
    event->accept();
}

void NetworkManagerApplet::showNetworkMenu()
{
    HPopupMenu menu;
    
    // WiFi section
    menu.addTitle(tr("WiFi"));
    
    // Toggle WiFi on/off - button text changes based on state
    if (m_wifiEnabled) {
        menu.addAction(QIcon::fromTheme("network-wireless"), tr("Disable WiFi"), this, SLOT(toggleWifi()));
    } else {
        menu.addAction(QIcon::fromTheme("network-wireless-disconnected"), tr("Enable WiFi"), this, SLOT(toggleWifi()));
    }
    
    menu.addSeparator();
    
    // Show available WiFi networks only if WiFi is enabled
    // When disabled, the list is empty (no networks shown)
    if (m_wifiEnabled && !m_wifiDevicePath.isEmpty()) {
        // Refresh connection info synchronously to ensure m_ssid and m_wifiConnected are up to date
        getActiveConnectionSync();
        
        // Refresh the access points list
        getAvailableAccessPoints();
        
        if (!m_accessPoints.isEmpty()) {
            QActionGroup* wifiGroup = new QActionGroup(&menu);
            wifiGroup->setExclusive(false);
            
            for (const AccessPoint& ap : m_accessPoints) {
                QString iconName;
                if (ap.strength >= 75) {
                    iconName = "network-wireless-signal-excellent";
                } else if (ap.strength >= 50) {
                    iconName = "network-wireless-signal-good";
                } else if (ap.strength >= 25) {
                    iconName = "network-wireless-signal-ok";
                } else {
                    iconName = "network-wireless-signal-weak";
                }
                
                QString displayName = ap.ssid;
                if (ap.secured) {
                    displayName += " " + tr("(Secured)");
                }
                
                QAction* action = menu.addAction(QIcon::fromTheme(iconName), displayName);
                action->setCheckable(true);
                // Check the connected WiFi network - compare SSID (case-insensitive for robustness)
                bool isConnected = m_wifiConnected && !m_ssid.isEmpty() && 
                                  ap.ssid.compare(m_ssid, Qt::CaseInsensitive) == 0;
                action->setChecked(isConnected);
                action->setData(ap.path);
                wifiGroup->addAction(action);
                
                connect(action, &QAction::triggered, this, [this, ap]() {
                    connectToAccessPoint(ap.ssid, ap.path);
                });
            }
        } else {
            menu.addAction(tr("No networks found"))->setEnabled(false);
        }
    }
    // When WiFi is disabled, don't show any networks (empty list)
    
    menu.addSeparator();
    
    // LAN section
    menu.addTitle(tr("Ethernet"));
    
    if (m_lanEnabled) {
        menu.addAction(QIcon::fromTheme("network-wired"), tr("Disable LAN"), this, SLOT(toggleLan()));
    } else {
        menu.addAction(QIcon::fromTheme("network-wired-disconnected"), tr("Enable LAN"), this, SLOT(toggleLan()));
    }
    
    // Show menu at mouse position or applet position
    QPoint pos = QCursor::pos();
    if (m_panelWindow) {
        QPointF scenePos = mapToScene(boundingRect().center());
        QPoint globalPos = m_panelWindow->mapToGlobal(scenePos.toPoint());
        pos = globalPos;
    }
    
    menu.exec(pos);
}

void NetworkManagerApplet::getAvailableAccessPoints()
{
    m_accessPoints.clear();
    
    if (m_wifiDevicePath.isEmpty() || !m_deviceInterface) {
        return;
    }
    
    // Get access points synchronously (should be fast)
    QDBusReply<QList<QDBusObjectPath>> reply = m_deviceInterface->call("GetAccessPoints");
    if (!reply.isValid()) {
        return;
    }
    
    QList<QDBusObjectPath> apPaths = reply.value();

    // Deduplicate SSIDs: NetworkManager returns one AP per BSSID, which causes duplicates in UI.
    // Keep the strongest signal per SSID.
    QHash<QString, AccessPoint> bestBySsid;

    for (const QDBusObjectPath& apPath : apPaths) {
        QDBusInterface apInterface("org.freedesktop.NetworkManager",
                                  apPath.path(),
                                  "org.freedesktop.DBus.Properties",
                                  QDBusConnection::systemBus(), this);
        
        AccessPoint ap;
        ap.path = apPath.path();
        ap.strength = 0;
        ap.secured = false;
        
        // Get SSID
        QDBusReply<QVariant> ssidReply = apInterface.call("Get",
                                                          "org.freedesktop.NetworkManager.AccessPoint",
                                                          "Ssid");
        if (ssidReply.isValid()) {
            ap.ssid = decodeSsidFromDbusVariant(ssidReply.value());
        }
        
        // Get signal strength
        QDBusReply<QVariant> strengthReply = apInterface.call("Get",
                                                             "org.freedesktop.NetworkManager.AccessPoint",
                                                             "Strength");
        if (strengthReply.isValid()) {
            ap.strength = strengthReply.value().toUInt();
        }
        
        // Check if secured
        QDBusReply<QVariant> flagsReply = apInterface.call("Get",
                                                          "org.freedesktop.NetworkManager.AccessPoint",
                                                          "Flags");
        QDBusReply<QVariant> wpaFlagsReply = apInterface.call("Get",
                                                              "org.freedesktop.NetworkManager.AccessPoint",
                                                              "WpaFlags");
        if (flagsReply.isValid() || wpaFlagsReply.isValid()) {
            uint flags = flagsReply.isValid() ? flagsReply.value().toUInt() : 0;
            uint wpaFlags = wpaFlagsReply.isValid() ? wpaFlagsReply.value().toUInt() : 0;
            ap.secured = (flags & 0x1) || (wpaFlags != 0); // NM_802_11_AP_FLAGS_PRIVACY
        }
        
        if (!ap.ssid.isEmpty()) {
            const QString key = ap.ssid.toLower();
            auto it = bestBySsid.find(key);
            if (it == bestBySsid.end()) {
                bestBySsid.insert(key, ap);
            } else {
                // prefer strongest signal; preserve "secured" if any entry is secured
                if (ap.strength > it->strength) {
                    const bool secured = it->secured || ap.secured;
                    *it = ap;
                    it->secured = secured;
                } else if (ap.secured) {
                    it->secured = true;
                }
            }
        }
    }

    m_accessPoints = bestBySsid.values();
    
    // Sort by signal strength (strongest first)
    std::sort(m_accessPoints.begin(), m_accessPoints.end(), 
              [](const AccessPoint& a, const AccessPoint& b) {
                  return a.strength > b.strength;
              });
}

void NetworkManagerApplet::connectToAccessPoint(const QString& ssid, const QString& path)
{
    Q_UNUSED(path)

    if (m_wifiDevicePath.isEmpty() || ssid.isEmpty()) {
        return;
    }

    // Determine if this SSID is secured from our current scan list (deduped)
    bool secured = false;
    for (const AccessPoint& ap : m_accessPoints) {
        if (ap.ssid.compare(ssid, Qt::CaseInsensitive) == 0) {
            secured = ap.secured;
            break;
        }
    }

    // Get WiFi device interface name (ifname) to target the correct adapter
    QString ifname;
    {
        QDBusInterface devProps("org.freedesktop.NetworkManager",
                                m_wifiDevicePath,
                                "org.freedesktop.DBus.Properties",
                                QDBusConnection::systemBus(), this);
        QDBusReply<QVariant> ifReply = devProps.call("Get",
                                                    "org.freedesktop.NetworkManager.Device",
                                                    "Interface");
        if (ifReply.isValid()) {
            ifname = ifReply.value().toString().trimmed();
        }
    }

    auto connectionNameForSsid = [ssid]() -> QString {
        // Make a deterministic, nmcli-friendly connection profile name to avoid reusing broken saved profiles.
        QString name = ssid.trimmed();
        name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]+")), QStringLiteral("_"));
        if (name.isEmpty()) {
            name = QStringLiteral("wifi");
        }
        name = QStringLiteral("hdepanel_%1").arg(name);
        if (name.size() > 64) {
            name = name.left(64);
        }
        return name;
    };

    auto needsSecrets = [](const QString& msg) -> bool {
        const QString m = msg.toLower();
        return m.contains("secrets were required") ||
               m.contains("password") ||
               m.contains("psk") ||
               m.contains("802-11-wireless-security") ||
               m.contains("cannot ask without") ||
               m.contains("passwd-file");
    };

    auto needsNewProfile = [](const QString& msg) -> bool {
        const QString m = msg.toLower();
        return m.contains("key-mgmt") || m.contains("property is missing");
    };

    // IMPORTANT: this must outlive connectToAccessPoint() because it is used in async callbacks.
    // Use shared_ptr to avoid capturing references to stack variables (which caused crashes).
    auto startNmcli = std::make_shared<std::function<void(const QString&, bool, bool)>>();
    *startNmcli = [this, ssid, ifname, connectionNameForSsid, secured, needsSecrets, needsNewProfile, startNmcli]
        (const QString& password, bool forceNewProfile, bool allowPrompt) {
        QProcess* proc = new QProcess(this);
        proc->setProgram("nmcli");
        QStringList args;
        args << "--wait" << "15" << "dev" << "wifi" << "connect" << ssid;
        if (!password.isEmpty()) {
            args << "password" << password;
        }
        if (!ifname.isEmpty()) {
            args << "ifname" << ifname;
        }
        if (forceNewProfile) {
            args << "name" << connectionNameForSsid();
        }
        proc->setArguments(args);

        connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, [this, proc, ssid, password, forceNewProfile, allowPrompt, secured, needsSecrets, needsNewProfile, startNmcli](int exitCode, QProcess::ExitStatus) {
            const QByteArray out = proc->readAllStandardOutput();
            const QByteArray err = proc->readAllStandardError();
            proc->deleteLater();

            if (exitCode == 0) {
                scheduleUpdate();
                return;
            }

            const QString msg = QString::fromUtf8(err.isEmpty() ? out : err).trimmed();

            // If the first attempt failed because secrets are required, prompt for password and retry.
            if (allowPrompt && password.isEmpty() && (needsSecrets(msg) || (secured && needsNewProfile(msg)))) {
                QWidget* parent = m_panelWindow ? static_cast<QWidget*>(m_panelWindow) : nullptr;
                bool ok = false;
                const QString prompt = tr("Enter WiFi password for %1").arg(ssid);
                const QString pw = QInputDialog::getText(parent,
                                                         tr("Network Manager"),
                                                         prompt,
                                                         QLineEdit::Password,
                                                         QString(),
                                                         &ok);
                if (!ok || pw.isEmpty()) {
                    return; // user cancelled
                }

                // Force a new profile on retry to avoid broken saved connection profiles.
                (*startNmcli)(pw, true, false);
                return;
            }

            // If key-mgmt property is missing, retry once with a fresh profile even for open networks.
            if (allowPrompt && password.isEmpty() && !forceNewProfile && needsNewProfile(msg)) {
                (*startNmcli)(QString(), true, false);
                return;
            }

            if (!msg.isEmpty()) {
                QWidget* parent = m_panelWindow ? static_cast<QWidget*>(m_panelWindow) : nullptr;
                QMessageBox::warning(parent, tr("Network Manager"), tr("Failed to connect: %1").arg(msg));
            }
            scheduleUpdate();
        });

        proc->start();
    };

    // First attempt: try without password (works for open networks and for secured networks with saved secrets).
    // If it fails due to secrets missing, we'll prompt and retry.
    (*startNmcli)(QString(), false, true);
}

void NetworkManagerApplet::toggleWifi()
{
    if (m_wifiDevicePath.isEmpty()) {
        return;
    }
    
    // Verify the device path is still valid by checking if we can access it
    QDBusInterface testInterface("org.freedesktop.NetworkManager",
                                  m_wifiDevicePath,
                                  "org.freedesktop.NetworkManager.Device",
                                  QDBusConnection::systemBus(), this);
    if (!testInterface.isValid()) {
        qDebug() << "WiFi device path is invalid:" << m_wifiDevicePath;
        // Try to find the device again
        findWifiDevice();
        if (m_wifiDevicePath.isEmpty()) {
            return;
        }
    }
    
    QDBusInterface wirelessInterface("org.freedesktop.NetworkManager",
                                    m_wifiDevicePath,
                                    "org.freedesktop.NetworkManager.Device.Wireless",
                                    QDBusConnection::systemBus(), this);
    
    QDBusInterface propsInterface("org.freedesktop.NetworkManager",
                                m_wifiDevicePath,
                                "org.freedesktop.DBus.Properties",
                                QDBusConnection::systemBus(), this);
    
    if (!propsInterface.isValid()) {
        qDebug() << "Cannot access Properties interface on device:" << m_wifiDevicePath;
        return;
    }
    
    // WirelessEnabled is a property on the main NetworkManager interface, not on individual devices
    QDBusInterface nmPropsInterface("org.freedesktop.NetworkManager",
                                   "/org/freedesktop/NetworkManager",
                                   "org.freedesktop.DBus.Properties",
                                   QDBusConnection::systemBus(), this);
    
    if (!nmPropsInterface.isValid()) {
        qDebug() << "Cannot access NetworkManager Properties interface";
        return;
    }
    
    if (m_wifiEnabled) {
        // Disable WiFi - first disconnect any active connections
        if (m_wifiConnected && !m_activeConnectionPath.isEmpty()) {
            QDBusInterface activeConnInterface("org.freedesktop.NetworkManager",
                                              m_activeConnectionPath,
                                              "org.freedesktop.NetworkManager.Connection.Active",
                                              QDBusConnection::systemBus(), this);
            activeConnInterface.call("Disconnect");
        }
        
        // Set WirelessEnabled to false on the main NetworkManager interface
        QDBusReply<void> reply = nmPropsInterface.call("Set",
                          "org.freedesktop.NetworkManager",
                          "WirelessEnabled",
                          QVariant::fromValue(QDBusVariant(false)));
        
        if (reply.isValid()) {
            // WiFi is being disabled - update local state immediately
            m_wifiEnabled = false;
            m_wifiConnected = false;
            m_ssid.clear();
            m_accessPoints.clear(); // Clear the list when disabled
            
            // Clear icon cache to force icon update
            m_lastIconName.clear();
            m_lastIconSize = 0;
            
            // Update content immediately to show disabled icon
            updateContent();
        } else {
            qDebug() << "Failed to disable WiFi:" << reply.error().message();
        }
    } else {
        // Enable WiFi - set WirelessEnabled to true on the main NetworkManager interface
        QDBusReply<void> reply = nmPropsInterface.call("Set",
                          "org.freedesktop.NetworkManager",
                          "WirelessEnabled",
                          QVariant::fromValue(QDBusVariant(true)));
        
        if (reply.isValid()) {
            // WiFi is being enabled - update local state immediately
            m_wifiEnabled = true;
            
            // Clear icon cache to force icon update
            m_lastIconName.clear();
            m_lastIconSize = 0;
            
            // Update content immediately to show enabled icon
            updateContent();
            
            // Wait a moment for the device to enable, then request scan
            QTimer::singleShot(500, this, [this]() {
                if (m_deviceInterface && m_wifiEnabled) {
                    m_deviceInterface->call("RequestScan", QVariantMap());
                }
                // Refresh device state and connection info
                findWifiDevice();
            });
        } else {
            qDebug() << "Failed to enable WiFi:" << reply.error().message();
        }
    }
    
    // Update display immediately
    updateContent();
    scheduleUpdate();
}

void NetworkManagerApplet::toggleLan()
{
    setLanEnabled(!m_lanEnabled);
    scheduleUpdate();
}

bool NetworkManagerApplet::isLanEnabled()
{
    if (m_lanDevicePath.isEmpty()) {
        return false;
    }
    
    QDBusInterface deviceInterface("org.freedesktop.NetworkManager",
                                  m_lanDevicePath,
                                  "org.freedesktop.DBus.Properties",
                                  QDBusConnection::systemBus(), this);
    
    QDBusReply<QVariant> stateReply = deviceInterface.call("Get",
                                                           "org.freedesktop.NetworkManager.Device",
                                                           "State");
    if (stateReply.isValid()) {
        uint state = stateReply.value().toUInt();
        return (state >= 30 && state <= 100); // 30-100 = activated states
    }
    
    return false;
}

void NetworkManagerApplet::setLanEnabled(bool enabled)
{
    if (m_lanDevicePath.isEmpty()) {
        return;
    }
    
    QDBusInterface deviceInterface("org.freedesktop.NetworkManager",
                                  m_lanDevicePath,
                                  "org.freedesktop.NetworkManager.Device",
                                  QDBusConnection::systemBus(), this);
    
    if (enabled) {
        // Enable - try to connect
        // NetworkManager will automatically connect if there's a saved connection
        deviceInterface.call("Reapply", QVariantMap(), 0);
    } else {
        // Disable
        deviceInterface.call("Disconnect");
    }
    
    m_lanEnabled = enabled;
}

QSize NetworkManagerApplet::desiredSize()
{
    if (!m_panelWindow)
        return QSize(0, -1);

    const PanelWindow::Position pos = m_panelWindow->position();
    const bool horizontal = (pos == PanelWindow::Top || pos == PanelWindow::Bottom);

    // Only show icon, no text
    int iconW = adjustHardcodedPixelSize(24);
    int padding = adjustHardcodedPixelSize(8);
    int totalW = iconW + padding;
    
    if (horizontal) {
        // Horizontal panel: fixed width based on icon size
        return QSize(totalW, -1);
    } else {
        // Vertical panel: fixed width (panel width), fixed height based on icon
        const int w = qMax(iconW + padding, m_panelWindow->panelWidth());
        const int h = iconW + padding;
        return QSize(w, h);
    }
}

