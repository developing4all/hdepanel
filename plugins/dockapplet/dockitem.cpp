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

#include "dockitem.h"
#include "dockapplet.h"
#include "client.h"
#include "waylandclient.h"
#include "../../lib/waylandsupport.h"
#include "textgraphicsitem.h"
#include "x11support.h"
#include "hpopupmenu.h"
#include "animationutils.h"
#include "dpisupport.h"
#include "panelwindow.h"
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#if QT_VERSION >= 0x050000
#include <QGraphicsPixmapItem>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QFontMetrics>
#include <QApplication>
#else
#include <QtGui/QGraphicsPixmapItem>
#include <QtGui/QPainter>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QGraphicsSceneHoverEvent>
#include <QtGui/QFontMetrics>
#include <QtWidgets/QApplication>
#endif
#include <X11/Xlib.h>

DockItem::DockItem(DockApplet* dockApplet)
{
    m_dragging = false;
    m_highlightIntensity = 0.0;
    m_focusHighlightIntensity = 0.0;
    m_urgencyHighlightIntensity = 0.0;
    m_isMinimized = false;
    m_waylandClient = nullptr;
    m_waylandText = QString();
    m_shouldDelete = false;
    m_buttonColor = QColor(255, 255, 255);
    m_buttonColorTransparency = 80;
    m_focusColor = QColor(0, 0, 0);
    m_focusColorTransparency = 128;

	m_dockApplet = dockApplet;

	m_animationTimer = new QTimer();

	m_animationTimer->setInterval(20);
	m_animationTimer->setSingleShot(true);
	connect(m_animationTimer, SIGNAL(timeout()), this, SLOT(animate()));

	setParentItem(m_dockApplet);
#if QT_VERSION >= 0x050000
    setAcceptHoverEvents(true);
#else
    setAcceptsHoverEvents(true);
#endif
	setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);

	m_textItem = new TextGraphicsItem(this);
	m_textItem->setColor(Qt::white);
	m_textItem->setFont(m_dockApplet->panelWindow()->font());

	m_iconItem = new QGraphicsPixmapItem(this);

    // Don't auto-register - let the caller decide when to register
    // if (m_dockApplet && m_dockApplet->panelWindow() && m_dockApplet->panelWindow()->panelItem())
    //     m_dockApplet->registerDockItem(this);
}

DockItem::~DockItem()
{
	// Clear the dock applet pointer to prevent any access during destruction
	DockApplet* applet = m_dockApplet;
	m_dockApplet = nullptr;
	
	// Unregister first, before deleting child items
	// (scene removal already handled by DockApplet::close during shutdown)
	if (applet && !applet->isDestroying()) {
		applet->unregisterDockItem(this);
	}

	delete m_iconItem;
	m_iconItem = nullptr;
	delete m_textItem;
	m_textItem = nullptr;
	delete m_animationTimer;
	m_animationTimer = nullptr;
}

void DockItem::updateContent()
{
    // Safety checks
    if (!m_textItem || !m_iconItem || !m_dockApplet || !m_dockApplet->panelWindow()) {
        return;
    }
    
    m_textItem->setFont(m_dockApplet->panelWindow()->font());
    QFontMetrics fontMetrics(m_textItem->font());
    
    QString displayText;
    QIcon displayIcon;
    
    if (!m_clients.isEmpty()) {
        // X11 applications - use client data
        displayText = m_clients[0]->name();
        displayIcon = m_clients[0]->icon();
    } else if (!m_waylandText.isEmpty()) {
        // Wayland applications - use stored text
        displayText = m_waylandText;
        // For Wayland, we need to get the icon from the pixmap that was already set
        if (m_iconItem && !m_iconItem->pixmap().isNull()) {
            displayIcon = QIcon(m_iconItem->pixmap());
        }
    } else {
        // Fallback - use text item text
        displayText = m_textItem->text();
        // For Wayland, we need to get the icon from the pixmap that was already set
        if (m_iconItem && !m_iconItem->pixmap().isNull()) {
            displayIcon = QIcon(m_iconItem->pixmap());
        }
    }
    
    // Apply text eliding and positioning
    int availableWidth = m_targetSize.width() > 0 ? m_targetSize.width() - adjustHardcodedPixelSize(36) : 200; // Fallback width
    QString shortName = fontMetrics.elidedText(displayText, Qt::ElideRight, availableWidth);
    m_textItem->setText(shortName);
    
    // Position text to the right of the icon with proper spacing
    int textX = adjustHardcodedPixelSize(28); // 8 (icon) + 16 (icon width) + 4 (spacing)
    int textY = m_dockApplet->panelWindow()->textBaseLine();
    m_textItem->setPos(textX, textY);

    // Apply icon sizing and positioning
    if (!displayIcon.isNull()) {
        m_iconItem->setPixmap(displayIcon.pixmap(adjustHardcodedPixelSize(16)));
    }
    // Center icon vertically in the dock item
    int iconY = m_targetSize.height() > 0 ? m_targetSize.height()/2 - adjustHardcodedPixelSize(8) : 16; // Fallback height
    m_iconItem->setPos(adjustHardcodedPixelSize(8), iconY);

	update();
}

void DockItem::fontChanged()
{
    m_textItem->setFont(m_dockApplet->panelWindow()->font());
    update();
}

void DockItem::addClient(Client* client)
{
	m_clients.append(client);
	updateClientsIconGeometry();
	updateContent();
}

void DockItem::removeClient(Client* client)
{
	int index = m_clients.indexOf(client);
	if (index >= 0) {
		m_clients.remove(index);
	}
	if(m_clients.isEmpty())
	{
		// Mark for deletion - the DockApplet will handle the actual deletion
		// Don't call unregisterDockItem here as it will be called from destructor
		// Just mark that this item should be deleted
		m_shouldDelete = true;
	}
	else
	{
		updateContent();
	}
}

void DockItem::setWaylandClient(WaylandClient* waylandClient)
{
    if (!waylandClient || !m_textItem || !m_iconItem) {
        return;
    }
    
    m_waylandClient = waylandClient;
    
    try {
        // Store Wayland client text separately
        m_waylandText = waylandClient->name();
        
        // Set up display using the actual Wayland client data
        setText(waylandClient->name());
        setIcon(waylandClient->icon());
        
        updateContent();
    } catch (...) {
        // Silently handle any errors
    }
}

void DockItem::setText(const QString& text)
{
    if (m_textItem) {
        m_textItem->setText(text);
        // Update stored Wayland text if this is a Wayland client
        if (m_clients.isEmpty()) {
            m_waylandText = text;
        }
        // Use updateContent() for consistent styling with X11 applications
        updateContent();
    } else {
    }
}

void DockItem::setIcon(const QIcon& icon)
{
    if (m_iconItem) {
        m_iconItem->setPixmap(icon.pixmap(16, 16));
        // Use updateContent() for consistent styling with X11 applications
        updateContent();
    } else {
    }
}

QString DockItem::text() const
{
    if (m_textItem) {
        return m_textItem->text();
    }
    return QString();
}

void DockItem::setTargetPosition(const QPoint& targetPosition)
{
	m_targetPosition = targetPosition;
	updateClientsIconGeometry();
}

void DockItem::setTargetSize(const QSize& targetSize)
{
	m_targetSize = targetSize;
	updateClientsIconGeometry();
	updateContent();
}

void DockItem::moveInstantly()
{
	m_position = m_targetPosition;
	m_size = m_targetSize;
	setPos(m_position.x(), m_position.y());
	update();
}

void DockItem::startAnimation()
{
	if(!m_animationTimer->isActive())
		m_animationTimer->start();
}

void DockItem::animate()
{
	bool needAnotherStep = false;

	static const qreal highlightAnimationSpeed = 0.15;
	qreal targetIntensity = isUnderMouse() ? 1.0 : 0.0;
	m_highlightIntensity = AnimationUtils::animate(m_highlightIntensity, targetIntensity, highlightAnimationSpeed, needAnotherStep);

	static const qreal focusHighlightAnimationSpeed = 0.15;
	qreal targetFocusIntensity = isFocused() ? 1.0 : 0.0;
	m_focusHighlightIntensity = AnimationUtils::animate(m_focusHighlightIntensity, targetFocusIntensity, focusHighlightAnimationSpeed, needAnotherStep);

	static const qreal urgencyHighlightAnimationSpeed = 0.015;
	qreal targetUrgencyIntensity = 0.0;
	if(isUrgent())
	{
		qint64 msecs = QDateTime::currentMSecsSinceEpoch() % 3000;
		if(msecs < 1500)
			targetUrgencyIntensity = 1.0;
		else
			targetUrgencyIntensity = 0.5;
		needAnotherStep = true;
	}
	m_urgencyHighlightIntensity = AnimationUtils::animate(m_urgencyHighlightIntensity, targetUrgencyIntensity, urgencyHighlightAnimationSpeed, needAnotherStep);

	if(!m_dragging)
	{
		static const int positionAnimationSpeed = 24;
		static const int sizeAnimationSpeed = 24;
		m_position.setX(AnimationUtils::animateExponentially(m_position.x(), m_targetPosition.x(), 0.2, positionAnimationSpeed, needAnotherStep));
		m_position.setY(AnimationUtils::animateExponentially(m_position.y(), m_targetPosition.y(), 0.2, positionAnimationSpeed, needAnotherStep));
		m_size.setWidth(AnimationUtils::animate(m_size.width(), m_targetSize.width(), sizeAnimationSpeed, needAnotherStep));
		m_size.setHeight(AnimationUtils::animate(m_size.height(), m_targetSize.height(), sizeAnimationSpeed, needAnotherStep));
		setPos(m_position.x(), m_position.y());
	}

	update();

	if(needAnotherStep)
		m_animationTimer->start();
}

void DockItem::close()
{
	// Close X11 windows
	for(int i = 0; i < m_clients.size(); i++)
	{
		X11Support::closeWindow(m_clients[i]->handle());
	}
	
	// Close Wayland windows
	if (m_waylandClient) {
		// Get the WaylandSupport instance from the DockApplet
		WaylandSupport* waylandSupport = m_dockApplet->waylandSupport();
		if (waylandSupport) {
			QString appId = m_waylandClient->appId();
			if (!appId.isEmpty()) {
				waylandSupport->closeWindow(appId);
			}
		}
	}
}

QRectF DockItem::boundingRect() const
{
	return QRectF(0.0, 0.0, m_size.width() - 1, m_size.height() - 1);
}

void DockItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(widget)
    Q_UNUSED(option)
    painter->setPen(Qt::NoPen);
	QPointF center(m_size.width()/2.0, m_size.height() + adjustHardcodedPixelSize(32));
	QRectF rect(0.0, adjustHardcodedPixelSize(4), m_size.width(), m_size.height() - adjustHardcodedPixelSize(8));
	static const qreal roundRadius = adjustHardcodedPixelSize(3);

	{
		QRadialGradient gradient(center, adjustHardcodedPixelSize(200), center);
		QColor buttonColorStart = m_buttonColor;
		buttonColorStart.setAlpha(m_buttonColorTransparency + static_cast<int>(m_buttonColorTransparency*m_highlightIntensity));
		QColor buttonColorEnd = m_buttonColor;
		buttonColorEnd.setAlpha(0);
		gradient.setColorAt(0.0, buttonColorStart);
		gradient.setColorAt(1.0, buttonColorEnd);
		painter->setBrush(QBrush(gradient));
		painter->drawRoundedRect(rect, roundRadius, roundRadius);
	}

	// Draw focus highlight (stronger than hover, different color)
	if(m_focusHighlightIntensity > 0.001)
	{
		// Draw a solid border for focused windows using configurable color
		QColor focusPenColor = m_focusColor;
		focusPenColor.setAlpha(static_cast<int>(m_focusColorTransparency*m_focusHighlightIntensity));
		QPen focusPen(focusPenColor);
		focusPen.setWidth(adjustHardcodedPixelSize(2));
		painter->setPen(focusPen);
		painter->setBrush(Qt::NoBrush);
		painter->drawRoundedRect(rect.adjusted(1, 1, -1, -1), roundRadius, roundRadius);
		
		// Also add a gradient overlay using configurable color
		QRadialGradient gradient(center, adjustHardcodedPixelSize(200), center);
		QColor focusColorStart = m_focusColor;
		focusColorStart.setAlpha(static_cast<int>(60*m_focusHighlightIntensity));
		QColor focusColorEnd = m_focusColor;
		focusColorEnd.setAlpha(0);
		gradient.setColorAt(0.0, focusColorStart);
		gradient.setColorAt(1.0, focusColorEnd);
		painter->setPen(Qt::NoPen);
		painter->setBrush(QBrush(gradient));
		painter->drawRoundedRect(rect, roundRadius, roundRadius);
	}

	if(m_urgencyHighlightIntensity > 0.001)
	{
		QRadialGradient gradient(center, adjustHardcodedPixelSize(200), center);
		gradient.setColorAt(0.0, QColor(255, 100, 0, static_cast<int>(160*m_urgencyHighlightIntensity)));
		gradient.setColorAt(1.0, QColor(255, 255, 255, 0));
		painter->setBrush(QBrush(gradient));
		painter->drawRoundedRect(rect, roundRadius, roundRadius);
	}
}

void DockItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event)
    startAnimation();
}

void DockItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event)
    startAnimation();
}

void DockItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
	if(event->button() == Qt::LeftButton)
	{
		m_dragging = true;
		m_mouseDownPosition = event->scenePos();
		m_dragStartPosition = m_position;
		m_dockApplet->draggingStarted();
		setZValue(1.0); // Be on top when dragging.
	}
}

void DockItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	if (event->button() == Qt::LeftButton) {
		m_dragging = false;
		m_dockApplet->draggingStopped();
		setZValue(0.0); // No more on top.
		startAnimation(); // Item can be out of it's regular, start animation to bring it back.
	}

	if (isUnderMouse()) {
		if (m_clients.isEmpty() && !m_waylandClient) return;

		if (event->button() == Qt::LeftButton) {
			static const qreal clickMouseMoveTolerance = 10.0;

			if ((event->scenePos() - m_mouseDownPosition).manhattanLength() < 
                clickMouseMoveTolerance) {
                
                // Handle Wayland clients
                if (m_waylandClient) {
                    DockApplet* dockApplet = qobject_cast<DockApplet*>(m_dockApplet);
                    if (dockApplet && dockApplet->waylandSupport()) {
                        QString appId = m_waylandClient->appId();
                        if (!appId.isEmpty()) {
                            dockApplet->waylandSupport()->activateWindow(appId);
                        }
                    }
                }
                // Handle X11 clients
                else if (!m_clients.isEmpty()) {
                    if (m_dockApplet->activeWindow() == m_clients[0]->handle()) {
#if QT_VERSION >= 0x050000
                        if (m_isMinimized) {
                            X11Support::activateWindow(m_clients[0]->handle());
                            m_isMinimized = false;
                        } else {
                            X11Support::minimizeWindow(m_clients[0]->handle());
                            m_isMinimized = true;
                        }
#else
                        X11Support::minimizeWindow(m_clients[0]->handle());
#endif
                    } else 
                        X11Support::activateWindow(m_clients[0]->handle());
                }
			}
		}

        if (event->button() == Qt::RightButton && !m_dragging) {
            HPopupMenu menu;

            menu.addTitle(tr("Application"));
            menu.addAction(QIcon::fromTheme("window-close"), tr("Close"), this, SLOT(close()));
            menu.addTitle(tr("Dock Applet"));
            menu.addAction(QIcon::fromTheme("preferences-other"), tr("Configure Dock Applet"), m_dockApplet, SLOT(showConfigurationDialog()));

            menu.addTitle(tr("Panel"));
            menu.addAction(QIcon::fromTheme("preferences-desktop"), tr("Configure Panel"), m_dockApplet->panelWindow(), SLOT(showConfigurationDialog()));

            menu.addAction(QIcon::fromTheme("list-add"), tr("Add Panel"), QApplication::instance(), SLOT(addPanel()));
            menu.addAction(QIcon::fromTheme("list-remove"), tr("Remove Panel"), m_dockApplet->panelWindow(), SLOT(removePanel()));

            menu.exec(event->screenPos());
        }
	}
}

void DockItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	// Mouse events are sent only when mouse button is pressed.
	if(!m_dragging)
		return;

	// TODO: Vertical orientation support.

	QPointF delta = event->scenePos() - m_mouseDownPosition;
	m_position.setX(m_dragStartPosition.x() + static_cast<int>(delta.x()));
	if(m_position.x() < 0)
		m_position.setX(0);
	if(m_position.x() >= m_dockApplet->size().width() - m_targetSize.width())
		m_position.setX(m_dockApplet->size().width() - m_targetSize.width());
	setPos(m_position.x(), m_position.y());

	int criticalShift = m_targetSize.width()*55/100;

	if(m_position.x() < m_targetPosition.x() - criticalShift)
		m_dockApplet->moveItem(this, false);

	if(m_position.x() > m_targetPosition.x() + criticalShift)
		m_dockApplet->moveItem(this, true);

	update();
}

void DockItem::updateClientsIconGeometry()
{
	QPointF topLeft = m_dockApplet->mapToScene(m_targetPosition);
	QVector<unsigned long> values;
	values.resize(4);
	values[0] = static_cast<unsigned long>(topLeft.x()) + m_dockApplet->panelWindow()->pos().x();
	values[1] = static_cast<unsigned long>(topLeft.y()) + m_dockApplet->panelWindow()->pos().y();
	values[2] = m_targetSize.width();
	values[3] = m_targetSize.height();
	for(int i = 0; i < m_clients.size(); i++)
	{
		X11Support::setWindowPropertyCardinalArray(m_clients[i]->handle(), "_NET_WM_ICON_GEOMETRY", values);
	}
}

bool DockItem::isUrgent()
{
	for(int i = 0; i < m_clients.size(); i++)
	{
		if(m_clients[i]->isUrgent())
			return true;
	}
	return false;
}

bool DockItem::isFocused() const
{
	if (!m_dockApplet)
		return false;
	
	// Check X11 clients
	for(int i = 0; i < m_clients.size(); i++)
	{
		if(m_dockApplet->activeWindow() == m_clients[i]->handle())
			return true;
	}
	
	// Check Wayland client
	if (m_waylandClient && m_waylandClient->isFocused())
		return true;
	
	return false;
}

void DockItem::setButtonColor(const QColor& color, int transparency)
{
    m_buttonColor = color;
    m_buttonColorTransparency = transparency;
    update();
}

void DockItem::setFocusColor(const QColor& color, int transparency)
{
    m_focusColor = color;
    m_focusColorTransparency = transparency;
    update();
}

bool DockItem::hasClient(Client* client) const
{
	return m_clients.contains(client);
}

bool DockItem::hasWaylandClient(WaylandClient* client) const
{
	return m_waylandClient == client;
}
