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
#include "clockconfigurationdialog.h"

#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <settings.h>
#include <QMenu>
#include "hpopupmenu.h"
#include <QApplication>
#if QT_VERSION >= 0x050000
#include <QGraphicsSceneContextMenuEvent>
#else
#include <QtGui/QGraphicsSceneContextMenuEvent>
#endif
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
#include "../../lib/dpisupport.h"
#include <QDebug>

#include <QApplication>
#include <QScreen>

#include "calendar.h"

static QString formatClockText(const QDateTime& dt,
    int availableWidthPx,
    const QFontMetrics& fm,
    bool verticalPanel,
    bool use24HourFormat)
{
    if (use24HourFormat) {
        // 24-hour format
        if (!verticalPanel) {
            // Horizontal: single line
            return dt.toString("HH:mm");
        }
        
        // Vertical: can split across lines if needed
        const QString oneLine = dt.toString("HH:mm");      // "16:14"
        const QString hh      = dt.toString("HH");          // "16"
        const QString mm      = dt.toString("mm");          // "14"
        const QString twoLine = hh + "\n" + mm;             // "16\n14"
        
        const int oneW = fm.horizontalAdvance(oneLine);
        
        if (oneW <= availableWidthPx) {
            return oneLine;
        }
        
        // Split into two lines if one line doesn't fit
        return twoLine;
    } else {
        // 12-hour format - manually convert to ensure correct format
        QTime time = dt.time();
        int hour24 = time.hour();
        int hour12 = hour24 % 12;
        if (hour12 == 0) hour12 = 12;  // 0 or 12 both become 12
        QString ap = (hour24 < 12) ? "AM" : "PM";
        
        QString hourStr = QString::number(hour12);
        QString minuteStr = QString("%1").arg(time.minute(), 2, 10, QChar('0'));
        QString timeStr = hourStr + ":" + minuteStr;
        QString oneLine = timeStr + " " + ap;
        
        if (!verticalPanel) {
            // Horizontal: single line
            return oneLine;
        }
        
        const QString twoLine = timeStr + "\n" + ap;                // "4:14\nPM"
        const QString hColon  = hourStr + ":";                       // "4:"
        const QString threeLine = hColon + "\n" + minuteStr + "\n" + ap;// "4:\n14\nPM"
        
        // Measure required widths for each layout.
        const int oneW = fm.horizontalAdvance(oneLine);
        
        const int twoW = qMax(fm.horizontalAdvance(timeStr),
                              fm.horizontalAdvance(ap));
        
        if (oneW <= availableWidthPx) {
            return oneLine;
        }
        
        if (twoW <= availableWidthPx) {
            return twoLine;
        }
        
        // Three line format if two lines don't fit
        return threeLine;
    }
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
    readSettings();
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

    const QString newText = formatClockText(now, usableW, fm, verticalPanel, m_use24HourFormat);
    if (m_text != newText) {
        m_text = newText;
        m_textItem->setText(m_text);
    }

    const QRectF br = m_textItem->boundingRect();

    if (!verticalPanel) {
        // Horizontal: center vertically within applet height
        qreal appletHeight = static_cast<qreal>(m_size.height());
        if (appletHeight <= 0) {
            appletHeight = static_cast<qreal>(m_panelWindow->panelHeight());
        }
        
        const qreal textHeight = br.height();
        const qreal textTop = br.top();
        
        qreal x = (m_size.width() - br.width()) / 2.0;
        if (x < innerPad) x = innerPad;
        
        // Center the text block vertically, accounting for font ascent
        qreal y = (appletHeight - textHeight) / 2.0 - textTop;
        if (y < 0) y = 0;
        
        m_textItem->setPos(x, y);
    } else {
        // Vertical: center the multi-line block inside the tile
        // Use actual allocated size for centering, with fallback
        qreal appletHeight = static_cast<qreal>(m_size.height());
        if (appletHeight <= 0) {
            // Fallback if size not set yet
            appletHeight = static_cast<qreal>(m_panelWindow->panelWidth());
        }
        
        const qreal textHeight = br.height();
        const qreal textTop = br.top(); // May be negative due to font ascent
        
        qreal x = (m_size.width() > 0 ? m_size.width() : availW) - br.width();
        x = x / 2.0;
        
        // Center the text block: we want the visual center of the text bounding rect
        // to align with the center of the applet
        // The bounding rect's visual center is at: br.top() + br.height() / 2
        // We want: y + br.top() + br.height() / 2 = appletHeight / 2
        // So: y = appletHeight / 2 - br.top() - br.height() / 2
        // Which simplifies to: y = (appletHeight - br.height()) / 2 - br.top()
        qreal y = (appletHeight - textHeight) / 2.0 - textTop;
        
        // Only apply horizontal padding constraint, vertical should be truly centered
        if (x < innerPad) x = innerPad;
        // Ensure y is non-negative (text baseline can't be above 0)
        if (y < 0) y = 0;
        
        m_textItem->setPos(x, y);
    }
}

void ClockApplet::updateContent()
{
    if (!m_textItem || !m_panelWindow)
        return;

    // Update text + position together
    layoutChanged();

    // Update tooltip with current date and time
    const QDateTime now = QDateTime::currentDateTime();
    QString tooltipText;
    if (m_use24HourFormat) {
        tooltipText = now.toString("dddd, MMMM d, yyyy\nHH:mm:ss");
    } else {
        tooltipText = now.toString("dddd, MMMM d, yyyy\nh:mm:ss AP");
    }
    setToolTip(tooltipText);

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
        // Use appropriate format based on setting
        const QString sampleText = m_use24HourFormat ? QStringLiteral("88:88") : QStringLiteral("88:88 PM");
        const int w = fm.horizontalAdvance(sampleText) + 16;
        return QSize(w, m_panelWindow->panelHeight());
    }

    // Vertical panel:
    // Width is enforced by PanelWindow; here we return a *tile height* based on text content.
    const int w = qMax(10, m_panelWindow->panelWidth());
    int lines = 3;
    if (w >= 90) lines = 1;
    else if (w >= 55) lines = 2;

    const int textH     = lines * fm.height();
    const int paddingV  = 12;
    // Use text height + padding, with a reasonable minimum (don't use panelHeight which is for horizontal panels)
    const int minHeight = adjustHardcodedPixelSize(24);
    const int h = qMax(minHeight, textH + paddingV);
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

void ClockApplet::readSettings()
{
    m_use24HourFormat = Settings::value(m_id, "use24HourFormat", false).toBool();
}

void ClockApplet::showConfigurationDialog()
{
    ClockConfigurationDialog dialog(m_id, m_panelWindow);
    if (dialog.exec()) {
        readSettings();
        updateContent();
    }
}

void ClockApplet::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    HPopupMenu menu;

    menu.addTitle(tr("Clock Applet"));
    menu.addAction(QIcon::fromTheme("preferences-other"), tr("Configure Clock"), this, SLOT(showConfigurationDialog()));

    menu.addTitle(tr("Panel"));
    menu.addAction(QIcon::fromTheme("preferences-desktop"), tr("Configure Panel"), m_panelWindow, SLOT(showConfigurationDialog()));

    menu.addAction(QIcon::fromTheme("list-add"), tr("Add Panel"), QApplication::instance(), SLOT(addPanel()));
    menu.addAction(QIcon::fromTheme("list-remove"), tr("Remove Panel"), m_panelWindow, SLOT(removePanel()));

    menu.exec(event->screenPos());
}
