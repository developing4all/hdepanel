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

#include "snitrayitem.h"
#include "trayapplet.h"
#include "sni.h"

SniTrayItem::SniTrayItem(TrayApplet* trayApplet, SniItemProxy* sniItem)
	: m_trayApplet(trayApplet), m_sniItem(sniItem)
{
	setParentItem(m_trayApplet);
	m_trayApplet->registerSniTrayItem(this);
	m_size = QSize(m_trayApplet->iconSize(), m_trayApplet->iconSize());

	if (m_sniItem) {
		connect(m_sniItem, &SniItemProxy::changed, this, &SniTrayItem::updateIcon);
		updateIcon();
	}
}

SniTrayItem::~SniTrayItem()
{
	// Avoid double-removal during TrayApplet destruction
	if (m_trayApplet && !m_trayApplet->isDestroying()) {
		m_trayApplet->unregisterSniTrayItem(this);
	}
}

void SniTrayItem::updateIcon()
{
	if (m_sniItem) {
		m_cachedIcon = m_sniItem->icon();
	}
	update();
}

void SniTrayItem::setPosition(const QPoint& position)
{
	setPos(position.x(), position.y());
}

void SniTrayItem::setSize(const QSize& size)
{
	m_size = size;
	update();
}

QRectF SniTrayItem::boundingRect() const
{
	return QRectF(0.0, 0.0, m_size.width() - 1, m_size.height() - 1);
}

void SniTrayItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Q_UNUSED(option)
	Q_UNUSED(widget)

	// Icon itself.
	if (m_sniItem) {
		if (!m_cachedIcon.isNull()) {
			QPixmap pix = m_cachedIcon.pixmap(m_trayApplet->iconSize(), m_trayApplet->iconSize());
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
	}
}

void SniTrayItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_sniItem) return;
    QPoint globalPos = event->screenPos();
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    if (trayDebug) {
        qDebug() << "SniTrayItem::mousePressEvent - button:" << event->button() << "pos:" << globalPos;
    }
    if (event->button() == Qt::LeftButton) {
        // Many Ayatana/indicator items only expose a DBusMenu; show it on left-click if available.
        if (!(m_sniItem->hasMenu() && m_sniItem->popupMenu(globalPos.x(), globalPos.y()))) {
            m_sniItem->activate(globalPos.x(), globalPos.y());
        }
    } else if (event->button() == Qt::RightButton) {
        // Prefer DBusMenu if exported; otherwise fall back to the SNI ContextMenu call.
        if (!(m_sniItem->hasMenu() && m_sniItem->popupMenu(globalPos.x(), globalPos.y()))) {
            m_sniItem->contextMenu(globalPos.x(), globalPos.y());
        }
    } else if (event->button() == Qt::MiddleButton) {
        m_sniItem->secondaryActivate(globalPos.x(), globalPos.y());
	}
    event->accept();
}

