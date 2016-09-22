/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * This Files has been imported to hde from qtpanel
 *
 * Copyright: 2015-2016 Haydar Alkaduhimi
 * Authors:
 *   Haydar Alkaduhimi <haydar@hosting4all.com>
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

#include "applet.h"

#include <QtCore/QTimer>
#include <QtGui/QPainter>
#if QT_VERSION >= 0x050000
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#else
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsSceneMouseEvent>
#endif
#include "panelwindow.h"
#include "animationutils.h"

Applet::Applet(PanelWindow* panelWindow)
{
    m_highlightIntensity = 0.0;
    m_interactive = false;

	setZValue(-1.0);
	setAcceptedMouseButtons(Qt::RightButton);
    if(panelWindow != 0)
    {
        this->setPanelWindow(panelWindow);
    }
}

void Applet::setPanelWindow(PanelWindow *panelWindow)
{
    m_panelWindow = panelWindow;
    setParentItem(m_panelWindow->panelItem());
}

Applet::~Applet()
{
}

bool Applet::init()
{
	m_panelWindow->updateLayout();
	return true;
}

void Applet::setPosition(const QPoint& position)
{
	m_position = position;
	setPos(m_position);
}

void Applet::setSize(const QSize& size)
{
	m_size = size;
	layoutChanged();
}

void Applet::setInteractive(bool interactive)
{
	m_interactive = interactive;

	if(m_interactive)
	{
#if QT_VERSION >= 0x050000
        setAcceptHoverEvents(true);
#else
        setAcceptsHoverEvents(true);
#endif
		setAcceptedMouseButtons(Qt::RightButton | Qt::LeftButton);
	}
	else
	{
#if QT_VERSION >= 0x050000
        setAcceptHoverEvents(false);
#else
		setAcceptsHoverEvents(false);
#endif
		setAcceptedMouseButtons(Qt::RightButton);
	}
}

QRectF Applet::boundingRect() const
{
	return QRectF(0.0, 0.0, m_size.width(), m_size.height());
}

void Applet::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

	if(m_size.width() < 32)
		return; // Too small to draw a background (don't want to deal with weird corner cases).

	if(!m_interactive)
		return; // Currently, background is only used for highlight on interactive applets.

	painter->setPen(Qt::NoPen);
	qreal radius = (m_size.width()*m_size.width() + m_size.height()*m_size.height()) / (4.0*m_size.height());
	QPointF center(m_size.width()/2.0, m_size.height() + radius - m_size.height()/2.0);
	static const qreal radiusInc = 10.0;
	QRadialGradient gradient(center, radius + radiusInc, center);
	QColor highlightColor(255, 255, 255, static_cast<int>(150*m_highlightIntensity));
	gradient.setColorAt(0.0, highlightColor);
	gradient.setColorAt((radius - m_size.height()/2.0)/(radius + radiusInc), highlightColor);
	gradient.setColorAt(1.0, QColor(255, 255, 255, 0));
	painter->setBrush(QBrush(gradient));
	painter->drawRect(boundingRect());
}

void Applet::animateHighlight()
{
	static const qreal highlightAnimationSpeed = 0.15;
	qreal targetIntensity = isHighlighted() ? 1.0 : 0.0;
	bool needAnotherStep = false;
	m_highlightIntensity = AnimationUtils::animate(m_highlightIntensity, targetIntensity, highlightAnimationSpeed, needAnotherStep);
	if(needAnotherStep)
		QTimer::singleShot(20, this, SLOT(animateHighlight()));
	update();
}

void Applet::clicked()
{
}

void Applet::layoutChanged()
{
}

QPoint Applet::localToScreen(const QPoint& point)
{
    return m_panelWindow->pos() + m_position + point;
}

bool Applet::isHighlighted()
{
	return isUnderMouse();
}

void Applet::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event)

	animateHighlight();
}

void Applet::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event)

    animateHighlight();
}

void Applet::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    Q_UNUSED(event)
}

void Applet::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	if(isUnderMouse())
	{
		if(event->button() == Qt::LeftButton)
		{
			// FIXME: Workaround.
			// For some weird reason, if clicked() function is called directly, and menu is opened,
			// this item will receive hover enter event on menu close. But it shouldn't (mouse is outside).
			// Probably somehow related to taking a mouse grab when one is already active.
            QTimer::singleShot(1, this, SLOT(clicked()));
		}
		if(event->button() == Qt::RightButton)
		{
			m_panelWindow->showPanelContextMenu(m_position + QPoint(static_cast<int>(event->pos().x()), static_cast<int>(event->pos().y())));
		}
	}
}
