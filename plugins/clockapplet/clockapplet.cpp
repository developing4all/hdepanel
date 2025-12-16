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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <chrono>
#endif
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

static QString formatClockText(const QDateTime& dt,
    int availableWidthPx,
    const QFontMetrics& fm,
    bool verticalPanel)
{
// Horizontal panels always use one line.
if (!verticalPanel)
return dt.toString("h:mm AP");

const QString oneLine = dt.toString("h:mm AP");          // "12:25 PM"
const QString time    = dt.toString("h:mm");             // "12:25"
const QString ap      = dt.toString("AP");               // "PM"
const QString twoLine = time + "\n" + ap;                // "12:25\nPM"
const QString hColon  = dt.toString("h") + ":";          // "12:"
const QString mm      = dt.toString("mm");               // "25"
const QString threeLine = hColon + "\n" + mm + "\n" + ap;// "12:\n25\nPM"

// Measure required widths for each layout.
const int oneW = fm.horizontalAdvance(oneLine);

const int twoW = qMax(fm.horizontalAdvance(time),
fm.horizontalAdvance(ap));

const int threeW = qMax(fm.horizontalAdvance(hColon),
qMax(fm.horizontalAdvance(mm),
fm.horizontalAdvance(ap)));

if (oneW <= availableWidthPx)
return oneLine;

if (twoW <= availableWidthPx)
return twoLine;

return threeLine;
}

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

    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setSingleShot(true);
        connect(m_timer, SIGNAL(timeout()), this, SLOT(updateContent()));
    }

    // DO NOT recreate m_textItem here if ctor already created it
    if (!m_textItem) {
        m_textItem = new TextGraphicsItem(this);
        m_textItem->setColor(Qt::white);
    }

    m_textItem->setFont(m_panelWindow->font());
}

ClockApplet::~ClockApplet()
{
    close();
}

void ClockApplet::close()
{
    if (m_textItem) {
        delete m_textItem;
        m_textItem = nullptr;
    }
    
    if (m_timer) {
        delete m_timer;
        m_timer = nullptr;
    }
    
    if (m_calendar) {
        delete m_calendar;
        m_calendar = nullptr;
    }
}

void ClockApplet::fontChanged()
{
    if (m_textItem && m_panelWindow) {
        m_textItem->setFont(m_panelWindow->font());
    }
}

bool ClockApplet::init()
{
    setInteractive(true);
    updateContent();
    return true;
}

void ClockApplet::layoutChanged()
{
    if (!m_textItem || !m_panelWindow)
        return;

    const PanelWindow::Position pos = m_panelWindow->position();
    const bool verticalPanel = (pos == PanelWindow::Left || pos == PanelWindow::Right);

    const int availW = (m_size.width() > 0 ? m_size.width() : (verticalPanel ? m_panelWindow->panelWidth()
                                                                            : m_panelWindow->panelHeight()));

    // Keep a small inner padding so text never touches edges.
    const int innerPad = 6; // 3px left + 3px right (adjust if you want 5/5 later)
    const int usableW = qMax(0, availW - 2 * innerPad);

    const QDateTime now = QDateTime::currentDateTime();
    const QFontMetrics fm(m_panelWindow->font());

    const QString newText = formatClockText(now, usableW, fm, verticalPanel);
    if (m_text != newText) {
        m_text = newText;
        m_textItem->setText(m_text);
    }

    const QRectF br = m_textItem->boundingRect();

    if (!verticalPanel) {
        // Horizontal: classic baseline positioning
        qreal x = (m_size.width() - br.width()) / 2.0;
        if (x < innerPad) x = innerPad;
        m_textItem->setPos(x, m_panelWindow->textBaseLine());
    } else {
        // Vertical: center the multi-line block inside the tile
        qreal x = (m_size.width()  - br.width())  / 2.0;
        qreal y = (m_size.height() - br.height()) / 2.0;
        if (x < innerPad) x = innerPad;
        if (y < innerPad) y = innerPad;
        m_textItem->setPos(x, y);
    }
}

void ClockApplet::updateContent()
{
    if (!m_textItem || !m_panelWindow)
        return;

    // Update text + position together
    layoutChanged();

    update();
    scheduleUpdate();
}

QSize ClockApplet::desiredSize()
{
    if (!m_panelWindow)
        return QSize(100, 24);

    const PanelWindow::Position pos = m_panelWindow->position();
    const bool verticalPanel = (pos == PanelWindow::Left || pos == PanelWindow::Right);

    QFontMetrics fm(m_panelWindow->font());

    if (!verticalPanel) {
        // Stable width (avoid jitter when digits change)
        const int w = fm.horizontalAdvance(QStringLiteral("88:88 PM")) + 16;
        return QSize(w, m_panelWindow->panelHeight());
    }

    // Vertical panel:
    // Width is enforced by PanelWindow; here we return a *tile height* that does NOT scale with panel width.
    const int w = qMax(10, m_panelWindow->panelWidth());
    int lines = 3;
    if (w >= 90) lines = 1;
    else if (w >= 55) lines = 2;

    const int baseTileH = qMax(10, m_panelWindow->panelHeight());     // e.g. 48
    const int textH     = lines * fm.height();
    const int paddingV  = 12;

    const int h = qMax(baseTileH, textH + paddingV);
    return QSize(-1, h);
}


void ClockApplet::scheduleUpdate()
{
    if (m_timer) {
        const int msec = 1000 - QDateTime::currentDateTime().time().msec();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_timer->setInterval(std::chrono::milliseconds(msec));
#else
        m_timer->setInterval(msec);
#endif
        m_timer->start();
    }
}

void ClockApplet::clicked()
{
    if (!m_calendar || !m_panelWindow) return;
    
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
