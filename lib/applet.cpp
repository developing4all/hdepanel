/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * This Files has been imported to hde from qtpanel
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
    // Defer parenting until panel window is fully initialized
    m_panelWindow = panelWindow;
}

void Applet::setPanelWindow(PanelWindow *panelWindow)
{
    m_panelWindow = panelWindow;
    // Note: Not calling setParentItem here to avoid Qt6 crashes
    // Applets are already added to the scene via PanelWindow's management
}

Applet::~Applet()
{
}

bool Applet::init()
{
	return true;
}

void Applet::setPosition(const QPoint& position)
{
    if (m_position == position)
        return;

    m_position = position;
    setPos(m_position);
    update();
}

void Applet::setSize(const QSize& size)
{
    if (m_size == size)
        return;

    // IMPORTANT: tell QGraphicsScene our geometry is about to change
    prepareGeometryChange();

    m_size = size;

    layoutChanged();
    update();
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

    if (m_size.width() < 32 || m_size.height() <= 0)
        return;

    if (!m_interactive)
        return;

    // On vertical panels, the old radial glow looks like a big circle "below" the button.
    // Use a simple rounded-rect hover instead.
    bool verticalPanel = false;
    if (m_panelWindow) {
        PanelWindow::Position pos = m_panelWindow->position();
        verticalPanel =
            (m_panelWindow->orientation() == PanelWindow::Vertical) ||
            (pos == PanelWindow::Left || pos == PanelWindow::Right);
    }

    painter->setPen(Qt::NoPen);

    if (verticalPanel) {
        QColor c(255, 255, 255, static_cast<int>(70 * m_highlightIntensity));
        painter->setBrush(c);
        QRectF r = boundingRect().adjusted(1, 1, -1, -1);
        painter->drawRoundedRect(r, 6.0, 6.0);
        return;
    }

    // Keep the original glow for horizontal panels
    qreal radius = (m_size.width()*m_size.width() + m_size.height()*m_size.height()) / (4.0*m_size.height());
    QPointF center(m_size.width()/2.0, m_size.height() + radius - m_size.height()/2.0);
    static const qreal radiusInc = 10.0;

    QRadialGradient gradient(center, radius + radiusInc, center);
    QColor highlightColor(255, 255, 255, static_cast<int>(150*m_highlightIntensity));
    gradient.setColorAt(0.0, highlightColor);

    qreal stop = (radius - m_size.height()/2.0) / (radius + radiusInc);
    if (stop < 0.0) stop = 0.0; else if (stop > 1.0) stop = 1.0;
    gradient.setColorAt(stop, highlightColor);
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
