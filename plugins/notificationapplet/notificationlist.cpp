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

#include "notificationlist.h"
#include "notificationapplet.h"

#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDateTime>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QStyle>
#include <QApplication>
#include <QFocusEvent>
#include <QDebug>

NotificationList::NotificationList(QWidget *parent)
    : QWidget(parent)
    , m_scrollArea(nullptr)
    , m_scrollContent(nullptr)
    , m_notificationsLayout(nullptr)
    , m_clearAllButton(nullptr)
    , m_emptyLabel(nullptr)
    , m_notificationInterface(nullptr)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setAttribute(Qt::WA_TranslucentBackground, false);
    
    setMinimumWidth(350);
    setMaximumWidth(450);
    setMinimumHeight(200);
    setMaximumHeight(600);
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // Header with "Clear All" button
    QWidget* headerWidget = new QWidget(this);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(10, 5, 10, 5);
    
    QLabel* titleLabel = new QLabel(tr("Notifications"), headerWidget);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    
    m_clearAllButton = new QPushButton(tr("Clear All"), headerWidget);
    m_clearAllButton->setFlat(true);
    connect(m_clearAllButton, &QPushButton::clicked, this, &NotificationList::onClearAllClicked);
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_clearAllButton);
    
    // Scroll area for notifications
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    m_scrollContent = new QWidget();
    m_notificationsLayout = new QVBoxLayout(m_scrollContent);
    m_notificationsLayout->setContentsMargins(0, 0, 0, 0);
    m_notificationsLayout->setSpacing(2);
    m_notificationsLayout->addStretch();
    
    m_scrollArea->setWidget(m_scrollContent);
    
    // Empty label - add it to the scroll content so it appears in the scroll area
    m_emptyLabel = new QLabel(tr("No notifications"), m_scrollContent);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: gray; padding: 40px; font-size: 14px;");
    m_emptyLabel->hide();
    
    // Add empty label to the notifications layout
    m_notificationsLayout->addWidget(m_emptyLabel, 0, Qt::AlignCenter);
    
    m_mainLayout->addWidget(headerWidget);
    m_mainLayout->addWidget(m_scrollArea);
    
    // Connect to notification service for dismissing
    m_notificationInterface = new QDBusInterface(
        "org.freedesktop.Notifications",
        "/org/freedesktop/Notifications",
        "org.freedesktop.Notifications",
        QDBusConnection::sessionBus(),
        this
    );
    
    setStyleSheet(
        "QWidget { background-color: rgba(40, 40, 40, 240); border: 1px solid rgba(100, 100, 100, 200); }"
        "QScrollArea { background: transparent; }"
        "QPushButton { color: white; padding: 4px 8px; }"
        "QPushButton:hover { background-color: rgba(255, 255, 255, 30); }"
    );
}

NotificationList::~NotificationList()
{
}

void NotificationList::addNotification(const Notification &notification)
{
    QWidget* notifWidget = createNotificationWidget(notification);
    if (notifWidget) {
        m_notificationWidgets.append(notifWidget);
        // Insert before the stretch at the end
        int insertIndex = m_notificationsLayout->count() - 1;
        if (insertIndex < 0) insertIndex = 0;
        m_notificationsLayout->insertWidget(insertIndex, notifWidget);
        updateLayout();
    }
}

void NotificationList::removeNotification(uint id)
{
    for (int i = 0; i < m_notificationWidgets.size(); ++i) {
        QWidget* widget = m_notificationWidgets[i];
        uint widgetId = widget->property("notificationId").toUInt();
        if (widgetId == id) {
            m_notificationsLayout->removeWidget(widget);
            m_notificationWidgets.removeAt(i);
            delete widget;
            updateLayout();
            return;
        }
    }
}

void NotificationList::clearNotifications()
{
    // Remove all widgets but keep the layout structure
    QList<QWidget*> widgetsToDelete = m_notificationWidgets;
    m_notificationWidgets.clear();
    
    foreach (QWidget* widget, widgetsToDelete) {
        m_notificationsLayout->removeWidget(widget);
        widget->deleteLater();
    }
    
    // Ensure stretch is at the end
    if (m_notificationsLayout->count() == 0 || 
        m_notificationsLayout->itemAt(m_notificationsLayout->count() - 1)->spacerItem() == nullptr) {
        m_notificationsLayout->addStretch();
    }
    
    updateLayout();
}

int NotificationList::notificationCount() const
{
    return m_notificationWidgets.size();
}

void NotificationList::updateLayout()
{
    if (m_notificationWidgets.isEmpty()) {
        // Show empty label and hide scroll area content
        m_emptyLabel->show();
        m_emptyLabel->raise();
        m_clearAllButton->setEnabled(false);
    } else {
        // Hide empty label and show notifications
        m_emptyLabel->hide();
        m_clearAllButton->setEnabled(true);
    }
    
    // Force update and resize
    m_scrollContent->adjustSize();
    adjustSize();
    update();
}

void NotificationList::markAsRead(uint id)
{
    for (int i = 0; i < m_notificationWidgets.size(); ++i) {
        QWidget* widget = m_notificationWidgets[i];
        uint widgetId = widget->property("notificationId").toUInt();
        if (widgetId == id) {
            // Update style to show as read (lighter background)
            widget->setStyleSheet(
                "QFrame { background-color: rgba(50, 50, 50, 150); border: 1px solid rgba(80, 80, 80, 100); "
                "border-radius: 4px; padding: 8px; }"
                "QFrame:hover { background-color: rgba(70, 70, 70, 150); }"
            );
            widget->setProperty("isRead", true);
            return;
        }
    }
}

QWidget* NotificationList::createNotificationWidget(const Notification &notification)
{
    QFrame* frame = new QFrame(m_scrollContent);
    frame->setProperty("notificationId", notification.id);
    frame->setFrameShape(QFrame::Box);
    
    // Different styling for read vs unread
    if (notification.isRead) {
        frame->setStyleSheet(
            "QFrame { background-color: rgba(50, 50, 50, 150); border: 1px solid rgba(80, 80, 80, 100); "
            "border-radius: 4px; padding: 8px; }"
            "QFrame:hover { background-color: rgba(70, 70, 70, 150); }"
        );
    } else {
        frame->setStyleSheet(
            "QFrame { background-color: rgba(60, 60, 60, 200); border: 1px solid rgba(100, 100, 100, 150); "
            "border-radius: 4px; padding: 8px; }"
            "QFrame:hover { background-color: rgba(80, 80, 80, 200); }"
        );
    }
    
    QHBoxLayout* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    
    // Icon
    QLabel* iconLabel = new QLabel(frame);
    if (!notification.icon.isNull()) {
        QPixmap iconPixmap = notification.icon.pixmap(32, 32);
        iconLabel->setPixmap(iconPixmap);
    } else {
        iconLabel->setText("📢");
        iconLabel->setStyleSheet("font-size: 24px;");
    }
    iconLabel->setFixedSize(32, 32);
    iconLabel->setScaledContents(true);
    
    // Content
    QVBoxLayout* contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(2);
    
    QLabel* appLabel = new QLabel(notification.appName, frame);
    QFont appFont = appLabel->font();
    appFont.setBold(true);
    appLabel->setFont(appFont);
    appLabel->setStyleSheet("color: white;");
    
    QLabel* summaryLabel = new QLabel(notification.summary, frame);
    summaryLabel->setStyleSheet("color: white;");
    summaryLabel->setWordWrap(true);
    
    if (!notification.body.isEmpty()) {
        QLabel* bodyLabel = new QLabel(notification.body, frame);
        bodyLabel->setStyleSheet("color: #cccccc; font-size: 11px;");
        bodyLabel->setWordWrap(true);
        contentLayout->addWidget(bodyLabel);
    }
    
    contentLayout->addWidget(appLabel);
    contentLayout->addWidget(summaryLabel);
    
    // Dismiss button
    QPushButton* dismissButton = new QPushButton("×", frame);
    dismissButton->setFixedSize(20, 20);
    dismissButton->setFlat(true);
    dismissButton->setStyleSheet(
        "QPushButton { color: white; font-size: 16px; font-weight: bold; }"
        "QPushButton:hover { background-color: rgba(255, 0, 0, 100); border-radius: 10px; }"
    );
    connect(dismissButton, &QPushButton::clicked, this, [this, notification]() {
        emit notificationDismissed(notification.id);
    });
    
    layout->addWidget(iconLabel);
    layout->addLayout(contentLayout, 1);
    layout->addWidget(dismissButton);
    
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    
    // Make the frame clickable to mark as read
    frame->setCursor(Qt::PointingHandCursor);
    frame->installEventFilter(this);
    
    return frame;
}

void NotificationList::onDismissClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (button) {
        QWidget* widget = button->parentWidget();
        if (widget) {
            uint id = widget->property("notificationId").toUInt();
            emit notificationDismissed(id);
        }
    }
}

void NotificationList::onClearAllClicked()
{
    if (m_notificationInterface) {
        foreach (QWidget* widget, m_notificationWidgets) {
            uint id = widget->property("notificationId").toUInt();
            m_notificationInterface->call("CloseNotification", id);
        }
    }
    clearNotifications();
    emit notificationDismissed(0); // Signal to parent
}

void NotificationList::focusOutEvent(QFocusEvent *event)
{
    Q_UNUSED(event)
    // Hide when focus is lost (user clicks outside)
    if (!geometry().contains(QCursor::pos())) {
        hide();
    }
}

bool NotificationList::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QFrame* frame = qobject_cast<QFrame*>(obj);
        if (frame) {
            uint id = frame->property("notificationId").toUInt();
            bool isRead = frame->property("isRead").toBool();
            
            // Only mark as read if not already read
            if (!isRead && id > 0) {
                emit notificationClicked(id);
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

