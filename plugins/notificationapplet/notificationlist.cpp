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
#include <QPaintEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRegion>
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
    setAttribute(Qt::WA_TranslucentBackground, true);
    
    setMinimumWidth(380);
    setMaximumWidth(420);
    setMinimumHeight(200);
    setMaximumHeight(600);
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // Header with "Clear All" button
    QWidget* headerWidget = new QWidget(this);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(16, 12, 16, 12);
    
    QLabel* titleLabel = new QLabel(tr("Notifications"), headerWidget);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 1);
    titleFont.setWeight(QFont::Bold);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: rgba(0, 0, 0, 0.87);");
    
    m_clearAllButton = new QPushButton(tr("Clear All"), headerWidget);
    m_clearAllButton->setFlat(true);
    QFont buttonFont = m_clearAllButton->font();
    buttonFont.setPointSize(buttonFont.pointSize() - 1);
    m_clearAllButton->setFont(buttonFont);
    m_clearAllButton->setStyleSheet(
        "QPushButton { color: rgba(0, 0, 0, 0.6); padding: 4px 8px; border-radius: 4px; }"
        "QPushButton:hover { background-color: rgba(0, 0, 0, 0.08); color: rgba(0, 0, 0, 0.87); }"
    );
    connect(m_clearAllButton, &QPushButton::clicked, this, &NotificationList::onClearAllClicked);
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_clearAllButton);
    
    // Scroll area for notifications
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    
    m_scrollContent = new QWidget();
    m_notificationsLayout = new QVBoxLayout(m_scrollContent);
    m_notificationsLayout->setContentsMargins(8, 8, 8, 8);
    m_notificationsLayout->setSpacing(6);
    m_notificationsLayout->addStretch();
    
    m_scrollArea->setWidget(m_scrollContent);
    
    // Empty label - add it to the scroll content so it appears in the scroll area
    m_emptyLabel = new QLabel(tr("No notifications"), m_scrollContent);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: rgba(0, 0, 0, 0.5); padding: 40px; font-size: 14px;");
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
        "QWidget { background: transparent; border: none; }"
        "QScrollArea { background: transparent; }"
    );
    
    // Enable transparency for rounded corners
    setAttribute(Qt::WA_TranslucentBackground, true);
}

NotificationList::~NotificationList()
{
}

void NotificationList::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect rect = this->rect();
    
    QColor bgColor(246, 245, 244); // #f6f5f4
    
    // Draw shadow (subtle, multiple layers for depth) - only within rounded bounds
    QPainterPath shadowPath1;
    shadowPath1.addRoundedRect(rect.adjusted(0, 2, 0, 4), 16, 16);
    QColor shadowColor1(0, 0, 0, 30);
    painter.fillPath(shadowPath1, shadowColor1);
    
    QPainterPath shadowPath2;
    shadowPath2.addRoundedRect(rect.adjusted(0, 1, 0, 2), 16, 16);
    QColor shadowColor2(0, 0, 0, 15);
    painter.fillPath(shadowPath2, shadowColor2);
    
    // Draw background
    QPainterPath bgPath;
    bgPath.addRoundedRect(rect.adjusted(0, 0, 0, -2), 16, 16);
    painter.fillPath(bgPath, bgColor);
    
    // Subtle border (very light)
    QColor borderColor(0, 0, 0, 8);
    painter.setPen(QPen(borderColor, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect.adjusted(0, 0, -1, -3), 16, 16);
}

void NotificationList::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // Update mask for transparent rounded corners
    QPainterPath path;
    path.addRoundedRect(rect(), 16, 16);
    QRegion mask = QRegion(path.toFillPolygon().toPolygon());
    setMask(mask);
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
                "QFrame { background-color: rgba(255, 255, 255, 0.6); border: none; "
                "border-radius: 12px; padding: 12px; }"
                "QFrame:hover { background-color: rgba(255, 255, 255, 0.8); }"
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
    frame->setFrameShape(QFrame::NoFrame);
    
    // rounded items with light background (more compact)
    if (notification.isRead) {
        frame->setStyleSheet(
            "QFrame { background-color: rgba(255, 255, 255, 0.6); border: none; "
            "border-radius: 12px; }"
            "QFrame:hover { background-color: rgba(255, 255, 255, 0.8); }"
        );
    } else {
        frame->setStyleSheet(
            "QFrame { background-color: rgba(255, 255, 255, 0.9); border: none; "
            "border-radius: 12px; }"
            "QFrame:hover { background-color: rgba(255, 255, 255, 1.0); }"
        );
    }
    
    QHBoxLayout* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(10);
    
    // Icon Container to keep layout stable during resize
    QWidget* iconContainer = new QWidget(frame);
    iconContainer->setFixedSize(48, 48);
    QVBoxLayout* iconContainerLayout = new QVBoxLayout(iconContainer);
    iconContainerLayout->setContentsMargins(0, 0, 0, 0);
    iconContainerLayout->setSpacing(0);
    iconContainerLayout->setAlignment(Qt::AlignCenter);

    QLabel* iconLabel = new QLabel(iconContainer);
    iconLabel->setObjectName("iconLabel");
    iconLabel->setFixedSize(48, 48);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setScaledContents(true); // Ensure icon always fits perfectly without clipping
    iconContainerLayout->addWidget(iconLabel);
    
    if (!notification.icon.isNull()) {
        // Store the original icon for later
        iconLabel->setProperty("originalIcon", QVariant::fromValue(notification.icon));
        
        // Use a high-quality pixmap as source
        QPixmap iconPixmap = notification.icon.pixmap(96, 96);
        if (!iconPixmap.isNull()) {
            iconLabel->setPixmap(iconPixmap);
        } else {
            iconLabel->setStyleSheet("background-color: rgba(0, 0, 0, 0.1); border-radius: 8px;");
        }
    } else {
        iconLabel->setStyleSheet("background-color: rgba(0, 0, 0, 0.1); border-radius: 8px;");
    }
    
    // Content
    QVBoxLayout* contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(2);
    
    // Summary (main text) - compact, single line if possible
    QLabel* summaryLabel = new QLabel(notification.summary, frame);
    QFont summaryFont = summaryLabel->font();
    summaryFont.setWeight(QFont::Medium);
    summaryLabel->setFont(summaryFont);
    summaryLabel->setStyleSheet("color: rgba(0, 0, 0, 0.87);");
    summaryLabel->setWordWrap(true);
    summaryLabel->setMaximumHeight(40); // Limit height to prevent excessive expansion
    
    // Body (if present) - expandable on click
    QLabel* bodyLabel = nullptr;
    if (!notification.body.isEmpty()) {
        bodyLabel = new QLabel(notification.body, frame);
        bodyLabel->setObjectName("bodyLabel");
        QFont bodyFont = bodyLabel->font();
        bodyFont.setPointSize(bodyFont.pointSize() - 1);
        bodyLabel->setFont(bodyFont);
        bodyLabel->setStyleSheet("color: rgba(0, 0, 0, 0.6);");
        bodyLabel->setWordWrap(true);
        bodyLabel->setMaximumHeight(48); // Initially collapsed
        bodyLabel->setProperty("collapsed", true);
        bodyLabel->setProperty("fullText", notification.body);
        
        // Show "..." if text is truncated
        QFontMetrics fm(bodyFont);
        if (fm.boundingRect(notification.body).height() > 48) {
            bodyLabel->setToolTip(tr("Click to expand"));
        }
    }
    
    // App name (smaller, at bottom) - single line
    QLabel* appLabel = new QLabel(notification.appName, frame);
    QFont appFont = appLabel->font();
    appFont.setPointSize(appFont.pointSize() - 2);
    appFont.setWeight(QFont::Normal);
    appLabel->setFont(appFont);
    appLabel->setStyleSheet("color: rgba(0, 0, 0, 0.5);");
    appLabel->setMaximumHeight(16); // Single line
    
    contentLayout->addWidget(summaryLabel);
    if (bodyLabel) {
        contentLayout->addWidget(bodyLabel);
    }
    contentLayout->addWidget(appLabel);
    // Remove stretch to make items more compact
    
    // Dismiss button
    QPushButton* dismissButton = new QPushButton("×", frame);
    dismissButton->setFixedSize(24, 24);
    dismissButton->setFlat(true);
    dismissButton->setStyleSheet(
        "QPushButton { color: rgba(0, 0, 0, 0.5); font-size: 18px; font-weight: bold; "
        "border-radius: 12px; }"
        "QPushButton:hover { background-color: rgba(0, 0, 0, 0.1); color: rgba(0, 0, 0, 0.87); }"
    );
    connect(dismissButton, &QPushButton::clicked, this, [this, notification]() {
        emit notificationDismissed(notification.id);
    });
    
    layout->addWidget(iconContainer);
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
            
            // Toggle expand/collapse for body text and icon
            QLabel* bodyLabel = frame->findChild<QLabel*>("bodyLabel");
            QLabel* iconLabel = frame->findChild<QLabel*>("iconLabel");
            
            if (bodyLabel) {
                bool collapsed = bodyLabel->property("collapsed").toBool();
                if (collapsed) {
                    // Expand - make icon smaller (32px)
                    bodyLabel->setMaximumHeight(QWIDGETSIZE_MAX);
                    bodyLabel->setProperty("collapsed", false);
                    bodyLabel->setToolTip(tr("Click to collapse"));
                    
                    if (iconLabel) {
                        iconLabel->setFixedSize(32, 32);
                    }
                } else {
                    // Collapse - restore icon to original size (48px)
                    bodyLabel->setMaximumHeight(48);
                    bodyLabel->setProperty("collapsed", true);
                    bodyLabel->setToolTip(tr("Click to expand"));
                    
                    if (iconLabel) {
                        iconLabel->setFixedSize(48, 48);
                    }
                }
                // Force complete layout recalculation
                if (frame->layout()) {
                    frame->layout()->invalidate();
                    frame->layout()->activate();
                }
                frame->updateGeometry();
                m_scrollContent->updateGeometry();
                m_scrollContent->adjustSize();
                update();
            }
            
            // Only mark as read if not already read
            if (!isRead && id > 0) {
                emit notificationClicked(id);
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

