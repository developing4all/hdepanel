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
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include <QtGui/QPainter>
#if QT_VERSION >= 0x050000
#include <QGraphicsSceneMouseEvent>
#else
#include <QtGui/QGraphicsSceneMouseEvent>
#endif

#include "trayitem.h"
#include "trayapplet.h"
#include "panelwindow.h"
#include "x11support.h"

TrayItem::TrayItem(TrayApplet* trayApplet, unsigned long window)
	: m_trayApplet(trayApplet), m_window(window)
{
	setParentItem(m_trayApplet);

	// This is needed for non-composited tray icons, otherwise we'll get a BadMatch on reparent attempt.
	// Doesn't affect composited icons.
	X11Support::setWindowBackgroundBlack(m_window);

	X11Support::registerForTrayIconUpdates(m_window);
	X11Support::reparentWindow(m_window, m_trayApplet->panelWindow()->winId());
	X11Support::resizeWindow(m_window, m_trayApplet->iconSize(), m_trayApplet->iconSize());
	X11Support::redirectWindow(m_window);
	X11Support::mapWindow(m_window);
	

	m_trayApplet->registerTrayItem(this);
}

TrayItem::~TrayItem()
{
    // Guard X11 calls during shutdown or on non-X11 platforms
#if QT_VERSION >= 0x050000
    const bool isX11 = qApp && qApp->platformName().toLower().contains("xcb");
#else
    const bool isX11 = true;
#endif
    if (isX11 && m_window) {
        X11Support::reparentWindow(m_window, X11Support::rootWindow());
    }

    // Avoid double-removal during TrayApplet destruction
    if (m_trayApplet && !m_trayApplet->isDestroying()) {
        m_trayApplet->unregisterTrayItem(this);
    }
}

void TrayItem::setPosition(const QPoint& position)
{
	setPos(position.x(), position.y());
	
	// Only update X11 window position if we have a valid size
	if (m_size.width() > 0 && m_size.height() > 0) {
		X11Support::moveWindow(m_window,
			static_cast<int>(m_trayApplet->pos().x()) + position.x() + m_size.width()/2 - m_trayApplet->iconSize()/2,
			static_cast<int>(m_trayApplet->pos().y()) + position.y() + m_size.height()/2 - m_trayApplet->iconSize()/2
		);
	}
}

void TrayItem::setSize(const QSize& size)
{
	if (m_size == size)
		return;
		
	m_size = size;
	update();
}

QRectF TrayItem::boundingRect() const
{
	return QRectF(0.0, 0.0, m_size.width() - 1, m_size.height() - 1);
}

void TrayItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

	// Icon itself.
    QPixmap pix = X11Support::getWindowPixmap(m_window);
    if (!pix.isNull()) {
        // Background - only if icon exists.
	painter->setPen(Qt::NoPen);
	QPointF center(m_size.width()/2.0, m_size.height()/2.0);
	QRadialGradient gradient(center, m_size.width()/2.0, center);
	gradient.setColorAt(0.0, QColor(255, 255, 255, 80));
	gradient.setColorAt(1.0, QColor(255, 255, 255, 0));
	painter->setBrush(QBrush(gradient));
	painter->drawRect(boundingRect());

        // Center the icon using its actual size
        int iconX = m_size.width()/2 - pix.width()/2;
        int iconY = m_size.height()/2 - pix.height()/2;
        painter->drawPixmap(iconX, iconY, pix);
    }
}

void TrayItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    event->ignore();
}

