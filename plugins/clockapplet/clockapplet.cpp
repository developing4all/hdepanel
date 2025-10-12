/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * This Files has been imported to hde from qtpanel
 *
 * Copyright: 2015-2025 Haydar Alkaduhimi
 * Copyright: 2014 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "clockapplet.h"

#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#if QT_VERSION >= 0x050000
#include <QGraphicsScene>
#else
#include <QtGui/QGraphicsScene>
#endif
#include "textgraphicsitem.h"
#include "panelwindow.h"
#include <QDebug>

#include <QApplication>
#include <QScreen>

#include "calendar.h"

ClockApplet::ClockApplet(PanelWindow* panelWindow)
	: Applet(panelWindow)
{
    setObjectName("Clock");
    m_calendar = new Calendar;
    
    // Create m_textItem early so desiredSize() doesn't crash
    m_textItem = new TextGraphicsItem(this);
    m_textItem->setColor(Qt::white);
}

void ClockApplet::setPanelWindow(PanelWindow *panelWindow)
{
    Applet::setPanelWindow(panelWindow);

    m_timer = new QTimer();
    m_timer->setSingleShot(true);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(updateContent()));
    m_textItem = new TextGraphicsItem(this);
    m_textItem->setColor(Qt::white);
    m_textItem->setFont(m_panelWindow->font());
}

ClockApplet::~ClockApplet()
{
    close();
}

void ClockApplet::close()
{
    delete m_textItem;
    delete m_timer;
    if(m_calendar != NULL)
    {
        delete m_calendar;
    }
}

void ClockApplet::fontChanged()
{
    m_textItem->setFont(m_panelWindow->font());
}

bool ClockApplet::init()
{
    setInteractive(true);
    updateContent();
    return true;
}

void ClockApplet::layoutChanged()
{
	m_textItem->setPos((m_size.width() - m_textItem->boundingRect().size().width())/2.0, m_panelWindow->textBaseLine());
}

void ClockApplet::updateContent()
{
	QDateTime dateTimeNow = QDateTime::currentDateTime();
	m_text = dateTimeNow.toString("h:mm AP");
	m_textItem->setText(m_text);
	update();
	scheduleUpdate();
}

QSize ClockApplet::desiredSize()
{
	if (!m_textItem) return QSize(100, 24);
	return QSize(m_textItem->boundingRect().width() + 16, m_textItem->boundingRect().height() + 16);
}

void ClockApplet::scheduleUpdate()
{
	m_timer->setInterval(1000 - QDateTime::currentDateTime().time().msec());
	m_timer->start();
}

void ClockApplet::clicked()
{
    // Show calender widget
    //int x = localToScreen(QPoint(0, m_size.height())).x();
    int x = localToScreen(QPoint(0, m_size.height())).x() - m_calendar->width() + m_size.width();
    int y = localToScreen(QPoint(0, m_size.height())).y();

    //qDebug() << "orig x: " << x;
    {
        const QList<QScreen*> screens = QGuiApplication::screens();
        const int sidx = m_panelWindow->screen();
        const QScreen* screen = (sidx >= 0 && sidx < screens.size()) ? screens[sidx] : QGuiApplication::primaryScreen();
        const QRect screenGeometry = screen ? screen->geometry() : QRect(0,0,1920,1080);

    if(y >= screenGeometry.height() )
    {
        y = y - m_calendar->height() - m_size.height();
    }

    if((x) < screenGeometry.x())
    {
        //qDebug() << "SIZE Error";
        x = screenGeometry.x() + m_position.x() ;
    }
    m_calendar->move(x,y);
    m_calendar->show();
    m_calendar->setFocused();
}
}
