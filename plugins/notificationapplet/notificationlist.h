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

#ifndef NOTIFICATIONLIST_H
#define NOTIFICATIONLIST_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QDBusInterface>
#include <QFocusEvent>

// Forward declaration - Notification struct is defined in notificationapplet.h
struct Notification;

class NotificationList : public QWidget
{
    Q_OBJECT

public:
    explicit NotificationList(QWidget *parent = nullptr);
    ~NotificationList();

    void addNotification(const Notification &notification);
    void removeNotification(uint id);
    void clearNotifications();
    int notificationCount() const;

signals:
    void notificationDismissed(uint id);
    void notificationClicked(uint id);

public:
    void markAsRead(uint id);

private slots:
    void onDismissClicked();
    void onClearAllClicked();

protected:
    void focusOutEvent(QFocusEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void updateLayout();
    QWidget* createNotificationWidget(const Notification &notification);

    QVBoxLayout* m_mainLayout;
    QScrollArea* m_scrollArea;
    QWidget* m_scrollContent;
    QVBoxLayout* m_notificationsLayout;
    QPushButton* m_clearAllButton;
    QLabel* m_emptyLabel;
    QList<QWidget*> m_notificationWidgets;
    QDBusInterface* m_notificationInterface;
};

#endif // NOTIFICATIONLIST_H

