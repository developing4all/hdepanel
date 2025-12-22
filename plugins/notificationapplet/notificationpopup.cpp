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
    
    setMinimumWidth(300);
    setMaximumWidth(400);
    setMinimumHeight(80);
    
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &NotificationPopup::onTimeout);
    
    m_fadeAnimation = new QPropertyAnimation(this, "opacity", this);
    m_fadeAnimation->setDuration(300);
    m_fadeAnimation->setStartValue(1.0);
    m_fadeAnimation->setEndValue(0.0);
    connect(m_fadeAnimation, &QPropertyAnimation::finished, this, &NotificationPopup::onFadeOutFinished);
    
    setStyleSheet(
        "QWidget { background-color: rgba(40, 40, 40, 240); border: 1px solid rgba(100, 100, 100, 200); border-radius: 8px; }"
        "QLabel { color: white; }"
        "QPushButton { color: white; background: transparent; border: none; font-size: 16px; font-weight: bold; }"
        "QPushButton:hover { background-color: rgba(255, 255, 255, 30); border-radius: 10px; }"
    );
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

void NotificationPopup::showNotification(const Notification &notification)
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
    mainLayout->setSpacing(8);
    
    // Header with app name and close button
    QHBoxLayout* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);
    
    QLabel* appLabel = new QLabel(m_notification->appName, this);
    QFont appFont = appLabel->font();
    appFont.setBold(true);
    appFont.setPointSize(appFont.pointSize() + 1);
    appLabel->setFont(appFont);
    
    QPushButton* closeButton = new QPushButton("×", this);
    closeButton->setFixedSize(20, 20);
    closeButton->setFlat(true);
    connect(closeButton, &QPushButton::clicked, this, &NotificationPopup::dismissed);
    
    headerLayout->addWidget(appLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(closeButton);
    
    // Summary
    QLabel* summaryLabel = new QLabel(m_notification->summary, this);
    summaryLabel->setWordWrap(true);
    summaryLabel->setTextFormat(Qt::PlainText);
    
    // Body (if present)
    QLabel* bodyLabel = nullptr;
    if (!m_notification->body.isEmpty()) {
        bodyLabel = new QLabel(m_notification->body, this);
        bodyLabel->setWordWrap(true);
        bodyLabel->setTextFormat(Qt::PlainText);
        QFont bodyFont = bodyLabel->font();
        bodyFont.setPointSize(bodyFont.pointSize() - 1);
        bodyLabel->setFont(bodyFont);
        bodyLabel->setStyleSheet("color: #cccccc;");
    }
    
    // Icon and content layout
    QHBoxLayout* contentLayout = new QHBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(12);
    
    // Icon
    QLabel* iconLabel = new QLabel(this);
    if (!m_notification->icon.isNull()) {
        QPixmap iconPixmap = m_notification->icon.pixmap(48, 48);
        iconLabel->setPixmap(iconPixmap);
    } else {
        iconLabel->setText("📢");
        iconLabel->setStyleSheet("font-size: 32px;");
    }
    iconLabel->setFixedSize(48, 48);
    iconLabel->setScaledContents(true);
    
    // Text content
    QVBoxLayout* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);
    textLayout->addWidget(summaryLabel);
    if (bodyLabel) {
        textLayout->addWidget(bodyLabel);
    }
    
    contentLayout->addWidget(iconLabel);
    contentLayout->addLayout(textLayout, 1);
    
    mainLayout->addLayout(headerLayout);
    mainLayout->addLayout(contentLayout);
    
    adjustSize();
    
    // Set timeout
    int timeout = m_notification->timeout;
    if (timeout <= 0) {
        timeout = 5000; // Default 5 seconds
    }
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
    
    // Draw background with opacity
    QColor bgColor(40, 40, 40, static_cast<int>(240 * m_opacity));
    QColor borderColor(100, 100, 100, static_cast<int>(200 * m_opacity));
    
    QRect rect = this->rect();
    painter.setBrush(bgColor);
    painter.setPen(QPen(borderColor, 1));
    painter.drawRoundedRect(rect.adjusted(0, 0, -1, -1), 8, 8);
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

