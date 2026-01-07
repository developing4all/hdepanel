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

#include "notificationpopup.h"
#include "notificationapplet.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QTimer>
#include <QDebug>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
#include <QWindow>
#else
#include <QEvent>
#endif

NotificationPopup::NotificationPopup(QWidget *parent)
    : QWidget(parent)
    , m_notification(nullptr)
    , m_opacity(1.0)
    , m_timer(nullptr)
    , m_fadeAnimation(nullptr)
    , m_hovered(false)
{
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    
    setMinimumWidth(340);
    setMaximumWidth(420);
    setMinimumHeight(80);
    
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &NotificationPopup::onTimeout);
    
    m_fadeAnimation = new QPropertyAnimation(this, "opacity", this);
    m_fadeAnimation->setDuration(200);
    m_fadeAnimation->setStartValue(1.0);
    m_fadeAnimation->setEndValue(0.0);
    m_fadeAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_fadeAnimation, &QPropertyAnimation::finished, this, &NotificationPopup::onFadeOutFinished);
}

NotificationPopup::~NotificationPopup()
{
    if (m_notification) {
        delete m_notification;
    }
}

void NotificationPopup::setOpacity(qreal opacity)
{
    m_opacity = opacity;
    if (windowHandle()) {
        windowHandle()->setOpacity(opacity);
    }
    update();
}

void NotificationPopup::showNotification(const Notification &notification, int timeoutMs)
{
    // Delete old notification if exists
    if (m_notification) {
        delete m_notification;
    }
    
    // Copy notification
    m_notification = new Notification(notification);
    
    // Clear existing layout
    QLayout* oldLayout = layout();
    if (oldLayout) {
        QLayoutItem* item;
        while ((item = oldLayout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
        delete oldLayout;
    }
    
    // Create new layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(0);
    
    // Content layout (icon + text)
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);
    
    // Icon (48px)
    QLabel* iconLabel = new QLabel(this);
    iconLabel->setFixedSize(48, 48);
    iconLabel->setScaledContents(true);
    iconLabel->setAlignment(Qt::AlignCenter);
    
    if (!m_notification->icon.isNull()) {
        QPixmap iconPixmap = m_notification->icon.pixmap(48, 48, QIcon::Normal, QIcon::On);
        if (iconPixmap.isNull()) {
            iconPixmap = m_notification->icon.pixmap(48, 48);
        }
        if (!iconPixmap.isNull()) {
            iconLabel->setPixmap(iconPixmap);
        } else {
            // Fallback icon
            iconLabel->setText("");
            iconLabel->setStyleSheet("background-color: rgba(150, 150, 150, 0.2); border-radius: 8px;");
        }
    } else {
        iconLabel->setText("");
        iconLabel->setStyleSheet("background-color: rgba(150, 150, 150, 0.2); border-radius: 8px;");
    }
    
    // Text content layout
    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);
    
    // Summary (main text - larger, bold)
    QLabel* summaryLabel = new QLabel(m_notification->summary, this);
    summaryLabel->setWordWrap(true);
    summaryLabel->setTextFormat(Qt::PlainText);
    QFont summaryFont = summaryLabel->font();
    summaryFont.setPointSize(summaryFont.pointSize());
    summaryFont.setWeight(QFont::Medium);
    summaryLabel->setFont(summaryFont);
    summaryLabel->setStyleSheet("color: rgba(0, 0, 0, 0.87);");
    
    // Body (if present) - smaller, lighter
    QLabel* bodyLabel = nullptr;
    if (!m_notification->body.isEmpty()) {
        bodyLabel = new QLabel(m_notification->body, this);
        bodyLabel->setWordWrap(true);
        bodyLabel->setTextFormat(Qt::PlainText);
        QFont bodyFont = bodyLabel->font();
        bodyFont.setPointSize(bodyFont.pointSize() - 1);
        bodyLabel->setFont(bodyFont);
        bodyLabel->setStyleSheet("color: rgba(0, 0, 0, 0.6);");
    }
    
    // App name (smaller, at bottom)
    QLabel* appLabel = new QLabel(m_notification->appName, this);
    QFont appFont = appLabel->font();
    appFont.setPointSize(appFont.pointSize() - 2);
    appFont.setWeight(QFont::Normal);
    appLabel->setFont(appFont);
    appLabel->setStyleSheet("color: rgba(0, 0, 0, 0.5);");
    
    textLayout->addWidget(summaryLabel);
    if (bodyLabel) {
        textLayout->addWidget(bodyLabel);
    }
    textLayout->addWidget(appLabel);
    textLayout->addStretch();
    
    contentLayout->addWidget(iconLabel);
    contentLayout->addLayout(textLayout, 1);
    
    mainLayout->addLayout(contentLayout);
    
    adjustSize();
    
    // Use provided timeout or default to 1 second
    int timeout = timeoutMs > 0 ? timeoutMs : 1000;
    m_timer->start(timeout);
    
    // Reset opacity and show
    setOpacity(1.0);
    show();
    raise();
    activateWindow();
}

void NotificationPopup::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect rect = this->rect();
    
    // Light background with subtle shadow
    // Background color (light gray/white with transparency)
    QColor bgColor(242, 242, 242, static_cast<int>(255 * m_opacity));
    
    // Draw shadow (subtle, multiple layers for depth)
    if (m_opacity > 0.3) {
        // Outer shadow
        QColor shadowColor1(0, 0, 0, static_cast<int>(40 * m_opacity));
        painter.setBrush(shadowColor1);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rect.adjusted(0, 2, 0, 4), 16, 16);
        
        // Inner shadow
        QColor shadowColor2(0, 0, 0, static_cast<int>(20 * m_opacity));
        painter.setBrush(shadowColor2);
        painter.drawRoundedRect(rect.adjusted(0, 1, 0, 2), 16, 16);
    }
    
    // Draw background (light with rounded corners)
    painter.setBrush(bgColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect.adjusted(0, 0, 0, -2), 16, 16);
    
    // Subtle border (very light)
    QColor borderColor(0, 0, 0, static_cast<int>(10 * m_opacity));
    painter.setPen(QPen(borderColor, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect.adjusted(0, 0, -1, -3), 16, 16);
}

void NotificationPopup::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
        hide();
    }
    QWidget::mousePressEvent(event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void NotificationPopup::enterEvent(QEnterEvent *event)
#else
void NotificationPopup::enterEvent(QEvent *event)
#endif
{
    Q_UNUSED(event)
    m_hovered = true;
    // Pause timer when hovered
    if (m_timer && m_timer->isActive()) {
        m_timer->stop();
    }
}

void NotificationPopup::leaveEvent(QEvent *event)
{
    Q_UNUSED(event)
    m_hovered = false;
    // Resume timer when not hovered
    if (m_notification && m_notification->timeout > 0) {
        m_timer->start(m_notification->timeout);
    }
}

void NotificationPopup::onTimeout()
{
    // Fade out animation
    m_fadeAnimation->start();
}

void NotificationPopup::onFadeOutFinished()
{
    hide();
    emit dismissed();
}

