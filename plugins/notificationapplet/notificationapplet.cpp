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

#include "notificationapplet.h"
#include "notificationlist.h"
#include "notificationconfigurationdialog.h"
#include "notificationserver.h"
#include "notificationpopup.h"

#include <QtCore/QTimer>
#include <settings.h>
#include "hpopupmenu.h"
#include <QApplication>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include "textgraphicsitem.h"
#include "panelwindow.h"
#include <QFontMetrics>
#include <QIcon>
#include <QPainter>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusMetaType>
#include <QDebug>
#include <QScreen>
#include "../../lib/dpisupport.h"
#include "../../lib/unifiediconservice.h"

NotificationApplet::NotificationApplet(PanelWindow* panelWindow)
    : Applet(panelWindow)
    , m_notificationInterface(nullptr)
    , m_notificationServer(nullptr)
    , m_notificationList(nullptr)
    , m_iconItem(nullptr)
    , m_countItem(nullptr)
    , m_unreadCount(0)
    , m_notificationPosition(5) // Default: AtApplet
    , m_showCount(true)
    , m_iconSize(22)
{
    setObjectName("Notifications");
    
    m_iconItem = new QGraphicsPixmapItem(this);
    m_countItem = new TextGraphicsItem(this);
    m_countItem->setColor(Qt::white);
}

void NotificationApplet::setPanelWindow(PanelWindow *panelWindow)
{
    Applet::setPanelWindow(panelWindow);

    // Make the applet interactive so it can be clicked
    setInteractive(true);

    if (m_countItem && m_panelWindow) {
        m_countItem->setFont(m_panelWindow->font());
    }
}

NotificationApplet::~NotificationApplet()
{
    close();
}

void NotificationApplet::close()
{
    if (m_notificationServer) {
        m_notificationServer->unregisterService();
        delete m_notificationServer;
        m_notificationServer = nullptr;
    }
    
    if (m_notificationInterface) {
        QDBusConnection::sessionBus().disconnect(
            "org.freedesktop.Notifications",
            "/org/freedesktop/Notifications",
            "org.freedesktop.Notifications",
            "NotificationClosed",
            this,
            SLOT(onNotificationClosed(uint,uint))
        );
        QDBusConnection::sessionBus().disconnect(
            "org.freedesktop.Notifications",
            "/org/freedesktop/Notifications",
            "org.freedesktop.Notifications",
            "ActionInvoked",
            this,
            SLOT(onActionInvoked(uint,QString))
        );
        delete m_notificationInterface;
        m_notificationInterface = nullptr;
    }
    
    if (m_notificationList) {
        delete m_notificationList;
        m_notificationList = nullptr;
    }
    
    if (m_iconItem) {
        delete m_iconItem;
        m_iconItem = nullptr;
    }
    
    if (m_countItem) {
        delete m_countItem;
        m_countItem = nullptr;
    }
}

bool NotificationApplet::init()
{
    readSettings();
    connectToNotificationService();
    updateIcon();
    return true;
}

void NotificationApplet::readSettings()
{
    m_notificationPosition = Settings::value(m_id, "notificationPosition", 5).toInt(); // 5 = AtApplet
    m_showCount = Settings::value(m_id, "showCount", true).toBool();
    m_iconSize = Settings::value(m_id, "iconSize", 22).toInt();
}

void NotificationApplet::connectToNotificationService()
{
    // Create and register our own notification server
    m_notificationServer = new NotificationServer(this);
    m_notificationServer->setApplet(this);
    
    if (!m_notificationServer->registerService()) {
        qWarning() << "Failed to register notification server. Trying to connect to existing service...";
        
        // Fallback: try to connect to existing notification service
        m_notificationInterface = new QDBusInterface(
            "org.freedesktop.Notifications",
            "/org/freedesktop/Notifications",
            "org.freedesktop.Notifications",
            QDBusConnection::sessionBus(),
            this
        );

        if (m_notificationInterface->isValid()) {
            // Connect to signals from existing service
            QDBusConnection::sessionBus().connect(
                "org.freedesktop.Notifications",
                "/org/freedesktop/Notifications",
                "org.freedesktop.Notifications",
                "NotificationClosed",
                this,
                SLOT(onNotificationClosed(uint,uint))
            );

            QDBusConnection::sessionBus().connect(
                "org.freedesktop.Notifications",
                "/org/freedesktop/Notifications",
                "org.freedesktop.Notifications",
                "ActionInvoked",
                this,
                SLOT(onActionInvoked(uint,QString))
            );
        } else {
            qWarning() << "No notification service available";
        }
    } else {
        qDebug() << "Notification server registered successfully";
    }
}

QSize NotificationApplet::desiredSize()
{
    if (!m_panelWindow)
        return QSize(32, 24);

    const PanelWindow::Position pos = m_panelWindow->position();
    const bool verticalPanel = (pos == PanelWindow::Left || pos == PanelWindow::Right);

    if (!verticalPanel) {
        int width = adjustHardcodedPixelSize(m_iconSize);
        if (m_showCount && m_unreadCount > 0) {
            QFontMetrics fm(m_panelWindow->font());
            width += fm.horizontalAdvance(QString::number(m_unreadCount)) + 8;
        }
        return QSize(width, m_panelWindow->panelHeight());
    }

    // Vertical panel
    int height = adjustHardcodedPixelSize(m_iconSize);
    if (m_showCount && m_unreadCount > 0) {
        QFontMetrics fm(m_panelWindow->font());
        height += fm.height() + adjustHardcodedPixelSize(4);
    }
    return QSize(-1, height);
}

void NotificationApplet::layoutChanged()
{
    if (!m_iconItem || !m_panelWindow)
        return;

    const PanelWindow::Position pos = m_panelWindow->position();
    const bool verticalPanel = (pos == PanelWindow::Left || pos == PanelWindow::Right);

    QPixmap iconPixmap = m_iconItem->pixmap();
    if (iconPixmap.isNull()) {
        updateIcon();
        iconPixmap = m_iconItem->pixmap();
    }

    if (!verticalPanel) {
        // Horizontal: center icon vertically
        qreal iconY = (m_size.height() - iconPixmap.height()) / 2.0;
        m_iconItem->setPos(0, iconY);

        // Position count text to the right of icon
        if (m_countItem && m_showCount && m_unreadCount > 0) {
            QFontMetrics fm(m_panelWindow->font());
            qreal countX = iconPixmap.width() + 4;
            qreal countY = (m_size.height() - fm.height()) / 2.0;
            m_countItem->setPos(countX, countY);
        }
    } else {
        // Vertical: center icon horizontally
        qreal iconX = (m_size.width() - iconPixmap.width()) / 2.0;
        m_iconItem->setPos(iconX, 0);

        // Position count text below icon
        if (m_countItem && m_showCount && m_unreadCount > 0) {
            QFontMetrics fm(m_panelWindow->font());
            qreal countX = (m_size.width() - fm.horizontalAdvance(QString::number(m_unreadCount))) / 2.0;
            qreal countY = iconPixmap.height() + adjustHardcodedPixelSize(4);
            m_countItem->setPos(countX, countY);
        }
    }
}

void NotificationApplet::updateIcon()
{
    if (!m_iconItem)
        return;

    QIcon icon = QIcon::fromTheme("notification", QIcon::fromTheme("preferences-desktop-notification"));
    if (icon.isNull()) {
        icon = QIcon::fromTheme("dialog-information");
    }

    QPixmap pixmap = icon.pixmap(adjustHardcodedPixelSize(m_iconSize), adjustHardcodedPixelSize(m_iconSize));
    m_iconItem->setPixmap(pixmap);

    if (m_countItem && m_showCount) {
        if (m_unreadCount > 0) {
            m_countItem->setText(QString::number(m_unreadCount));
        } else {
            m_countItem->setText("");
        }
    }

    layoutChanged();
    update();
}

void NotificationApplet::updateContent()
{
    updateIcon();
}

void NotificationApplet::clicked()
{
    qDebug() << "NotificationApplet: clicked() called, notifications count:" << m_notifications.size();
    
    if (!m_notificationList) {
        qDebug() << "NotificationApplet: Creating new notification list";
        m_notificationList = new NotificationList();
        m_notificationList->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        m_notificationList->setAttribute(Qt::WA_ShowWithoutActivating, false);
        m_notificationList->setAttribute(Qt::WA_TranslucentBackground, false);
        
        connect(m_notificationList, &NotificationList::notificationDismissed,
                this, [this](uint id) {
                    if (m_notificationInterface) {
                        m_notificationInterface->call("CloseNotification", id);
                    }
                    removeNotification(id);
                });
        
        connect(m_notificationList, &NotificationList::notificationClicked,
                this, [this](uint id) {
                    markNotificationAsRead(id);
                });
    }

    // Refresh the list with all current notifications
    refreshNotificationList();

    if (!m_panelWindow) return;

    // Ensure the list is properly sized before positioning
    m_notificationList->adjustSize();
    m_notificationList->update();
    m_notificationList->resize(m_notificationList->sizeHint().expandedTo(QSize(400, 200)));
    
    // Calculate position based on settings
    QSize listSize = m_notificationList->size();
    if (listSize.isEmpty()) {
        listSize = QSize(400, 300); // Default size if empty
        m_notificationList->resize(listSize);
    }
    QPoint pos = calculateNotificationPosition(listSize);
    
    m_notificationList->move(pos);
    m_notificationList->show();
    m_notificationList->raise();
    m_notificationList->activateWindow();
    m_notificationList->setFocus();
    
    // Force the window to be visible
    m_notificationList->setVisible(true);
    m_notificationList->repaint();
}

QPoint NotificationApplet::calculateNotificationPosition(const QSize &notificationSize)
{
    if (!m_panelWindow) {
        return QPoint(100, 100);
    }

    const QList<QScreen*> screens = QGuiApplication::screens();
    const int sidx = m_panelWindow->screen();
    const QScreen* screen = (sidx >= 0 && sidx < screens.size()) ? screens[sidx] : QGuiApplication::primaryScreen();
    const QRect screenGeometry = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);

    PanelWindow::Position panelPos = m_panelWindow->position();
    int x = 0, y = 0;
    const int spacing = 2;

    if (m_notificationPosition == 5) { // AtApplet
        // Position relative to applet
        QPoint appletTopLeft = localToScreen(QPoint(0, 0));
        QPoint appletBottomRight = localToScreen(QPoint(m_size.width(), m_size.height()));

        switch (panelPos) {
            case PanelWindow::Top:
                x = appletTopLeft.x();
                y = appletBottomRight.y() + spacing;
                break;
            case PanelWindow::Bottom:
                x = appletTopLeft.x();
                y = appletTopLeft.y() - notificationSize.height() - spacing;
                break;
            case PanelWindow::Left:
                x = appletBottomRight.x() + spacing;
                y = appletTopLeft.y();
                break;
            case PanelWindow::Right:
                x = appletTopLeft.x() - notificationSize.width() - spacing;
                y = appletTopLeft.y();
                break;
        }
    } else {
        // Position at screen corners/center
        switch (m_notificationPosition) {
            case 0: // TopLeft
                x = screenGeometry.left() + spacing;
                y = screenGeometry.top() + spacing;
                break;
            case 1: // TopRight
                x = screenGeometry.right() - notificationSize.width() - spacing;
                y = screenGeometry.top() + spacing;
                break;
            case 2: // BottomLeft
                x = screenGeometry.left() + spacing;
                y = screenGeometry.bottom() - notificationSize.height() - spacing;
                break;
            case 3: // BottomRight
                x = screenGeometry.right() - notificationSize.width() - spacing;
                y = screenGeometry.bottom() - notificationSize.height() - spacing;
                break;
            case 4: // Center
                x = screenGeometry.center().x() - notificationSize.width() / 2;
                y = screenGeometry.center().y() - notificationSize.height() / 2;
                break;
        }
    }

    // Ensure notification stays on screen
    if (x < screenGeometry.left()) x = screenGeometry.left() + spacing;
    if (x + notificationSize.width() > screenGeometry.right()) 
        x = screenGeometry.right() - notificationSize.width() - spacing;
    if (y < screenGeometry.top()) y = screenGeometry.top() + spacing;
    if (y + notificationSize.height() > screenGeometry.bottom()) 
        y = screenGeometry.bottom() - notificationSize.height() - spacing;

    return QPoint(x, y);
}

void NotificationApplet::onNotificationReceived(uint id, const QString &appName, const QString &summary,
                                                 const QString &body, const QStringList &actions,
                                                 const QVariantMap &hints, int timeout)
{
    // Check if this notification replaces an existing one
    bool replaces = false;
    for (int i = 0; i < m_notifications.size(); ++i) {
        if (m_notifications[i].id == id) {
            m_notifications[i].appName = appName;
            m_notifications[i].summary = summary;
            m_notifications[i].body = body;
            m_notifications[i].actions = actions;
            m_notifications[i].hints = hints;
            m_notifications[i].timeout = timeout;
            m_notifications[i].timestamp = QDateTime::currentDateTime();
            m_notifications[i].isRead = false; // Reset read status when updated
            replaces = true;
            
            // Update icon from hints
            if (hints.contains("image-data") || hints.contains("image_path")) {
                // Handle icon extraction
                m_notifications[i].icon = QIcon::fromTheme(appName.toLower());
            } else if (!hints.value("image_data").isNull()) {
                // Handle image_data variant
                m_notifications[i].icon = QIcon::fromTheme(appName.toLower());
            } else {
                QString iconName = hints.value("image_path").toString();
                if (iconName.isEmpty()) {
                    iconName = hints.value("icon-name").toString();
                }
                if (!iconName.isEmpty()) {
                    m_notifications[i].icon = QIcon::fromTheme(iconName, QIcon::fromTheme("dialog-information"));
                } else {
                    m_notifications[i].icon = QIcon::fromTheme(appName.toLower(), QIcon::fromTheme("dialog-information"));
                }
            }
            
            // Update list if visible
            if (m_notificationList && m_notificationList->isVisible()) {
                m_notificationList->removeNotification(id);
                m_notificationList->addNotification(m_notifications[i]);
            }
            
            updateContent();
            return;
        }
    }
    
    if (replaces) {
        return; // Already handled above
    }
    
    // New notification
    Notification notification;
    notification.id = id;
    notification.appName = appName;
    notification.summary = summary;
    notification.body = body;
    notification.actions = actions;
    notification.hints = hints;
    notification.timeout = timeout;
    notification.timestamp = QDateTime::currentDateTime();
    notification.isRead = false;

    // Try to extract icon from hints
    if (hints.contains("image-data") || hints.contains("image_path")) {
        // Handle icon extraction (simplified for now)
        notification.icon = QIcon::fromTheme(appName.toLower());
    } else if (!hints.value("image_data").isNull()) {
        // Handle image_data variant
        notification.icon = QIcon::fromTheme(appName.toLower());
    } else {
        QString iconName = hints.value("image_path").toString();
        if (iconName.isEmpty()) {
            iconName = hints.value("icon-name").toString();
        }
        if (!iconName.isEmpty()) {
            notification.icon = QIcon::fromTheme(iconName, QIcon::fromTheme("dialog-information"));
        } else {
            notification.icon = QIcon::fromTheme(appName.toLower(), QIcon::fromTheme("dialog-information"));
        }
    }

    m_notifications.append(notification);
    m_unreadCount++;
    
    // Add to notification list if it exists
    if (m_notificationList) {
        m_notificationList->addNotification(notification);
    }

    // Show notification popup according to settings
    showNotificationPopup(notification);

    updateContent();
}

void NotificationApplet::onNotificationClosed(uint id, uint reason)
{
    Q_UNUSED(reason)
    removeNotification(id);
}

void NotificationApplet::onActionInvoked(uint id, const QString &actionKey)
{
    Q_UNUSED(id)
    Q_UNUSED(actionKey)
    // Handle action if needed
}

void NotificationApplet::removeNotification(uint id)
{
    for (int i = 0; i < m_notifications.size(); ++i) {
        if (m_notifications[i].id == id) {
            m_notifications.removeAt(i);
            m_unreadCount = qMax(0, m_unreadCount - 1);
            if (m_notificationList) {
                m_notificationList->removeNotification(id);
            }
            updateContent();
            return;
        }
    }
}

void NotificationApplet::showNotificationPopup(const Notification &notification)
{
    // Create popup
    NotificationPopup* popup = new NotificationPopup();
    
    // Connect signals
    connect(popup, &NotificationPopup::dismissed, this, [this, popup]() {
        m_activePopups.removeAll(popup);
        popup->deleteLater();
    });
    
    connect(popup, &NotificationPopup::clicked, this, [this, notification]() {
        // Mark as read when clicked
        markNotificationAsRead(notification.id);
        // Open notification list
        clicked();
    });
    
    // Show notification
    popup->showNotification(notification);
    
    // Position the popup
    if (m_panelWindow) {
        QSize popupSize = popup->sizeHint();
        if (popupSize.isEmpty()) {
            popupSize = QSize(350, 100);
        }
        QPoint pos = calculateNotificationPosition(popupSize);
        popup->move(pos);
    }
    
    m_activePopups.append(popup);
}

void NotificationApplet::markNotificationAsRead(uint id)
{
    for (int i = 0; i < m_notifications.size(); ++i) {
        if (m_notifications[i].id == id && !m_notifications[i].isRead) {
            m_notifications[i].isRead = true;
            m_unreadCount = qMax(0, m_unreadCount - 1);
            updateContent();
            
            // Update the notification widget in the list
            if (m_notificationList) {
                m_notificationList->markAsRead(id);
            }
            return;
        }
    }
}

void NotificationApplet::refreshNotificationList()
{
    if (!m_notificationList) return;
    
    // Clear existing widgets
    m_notificationList->clearNotifications();
    
    // Add all notifications (newest first)
    for (int i = m_notifications.size() - 1; i >= 0; --i) {
        m_notificationList->addNotification(m_notifications[i]);
    }
}

void NotificationApplet::fontChanged()
{
    if (m_countItem && m_panelWindow) {
        m_countItem->setFont(m_panelWindow->font());
    }
    layoutChanged();
}

void NotificationApplet::showConfigurationDialog()
{
    NotificationConfigurationDialog dialog(m_id, m_panelWindow);
    if (dialog.exec()) {
        readSettings();
        updateContent();
    }
}

void NotificationApplet::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    HPopupMenu menu;

    menu.addTitle(tr("Notification Applet"));
    menu.addAction(QIcon::fromTheme("preferences-other"), tr("Configure Notifications"), this, SLOT(showConfigurationDialog()));

    menu.addTitle(tr("Panel"));
    menu.addAction(QIcon::fromTheme("preferences-desktop"), tr("Configure Panel"), m_panelWindow, SLOT(showConfigurationDialog()));

    menu.addAction(QIcon::fromTheme("list-add"), tr("Add Panel"), QApplication::instance(), SLOT(addPanel()));
    menu.addAction(QIcon::fromTheme("list-remove"), tr("Remove Panel"), m_panelWindow, SLOT(removePanel()));

    menu.exec(event->screenPos());
}

