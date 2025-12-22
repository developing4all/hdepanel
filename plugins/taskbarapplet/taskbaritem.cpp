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

#include "taskbaritem.h"
#include "taskbarapplet.h"
#include "client.h"
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <chrono>
#endif
#include "waylandclient.h"
#include "../../lib/waylandsupport.h"
#include "textgraphicsitem.h"
#include "x11support.h"
#include "hpopupmenu.h"
#include "animationutils.h"
#include "dpisupport.h"
#include "panelwindow.h"
#include "../../lib/settings.h"
#include "../../lib/desktopapplications.h"
#include "../../lib/desktopdatastore.h"
#include "../../lib/unifiediconservice.h"
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#if QT_VERSION >= 0x050000
#include <QGraphicsPixmapItem>
#include <QPainter>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsScene>
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

TaskBarItem::TaskBarItem(TaskBarApplet* dockApplet)
{
    m_dragging = false;
    m_highlightIntensity = 0.0;
    m_focusHighlightIntensity = 0.0;
    m_urgencyHighlightIntensity = 0.0;
    m_isMinimized = false;
    m_waylandClient = nullptr;
    m_waylandText = QString();
    m_shouldDelete = false;
    m_lastClickedIndex = -1;
    m_buttonColor = QColor(255, 255, 255);
    m_buttonColorTransparency = 80;
    m_focusColor = QColor(0, 0, 0);
    m_focusColorTransparency = 128;

	m_dockApplet = dockApplet;

	m_animationTimer = new QTimer();

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	m_animationTimer->setInterval(std::chrono::milliseconds(20));
#else
	m_animationTimer->setInterval(20);
#endif
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
	// Safety check: only set font if panel window is available
	if (m_dockApplet && m_dockApplet->panelWindow()) {
		m_textItem->setFont(m_dockApplet->panelWindow()->font());
	}

	m_iconItem = new QGraphicsPixmapItem(this);
}

TaskBarItem::~TaskBarItem()
{
	// Clear the dock applet pointer to prevent any access during destruction
	TaskBarApplet* applet = m_dockApplet;
	m_dockApplet = nullptr;
	
	// Unregister first, before deleting child items
	// (scene removal already handled by TaskBarApplet::close during shutdown)
	if (applet && !applet->isDestroying()) {
		applet->unregisterTaskBarItem(this);
	}

	// NOTE: m_iconItem and m_textItem are QGraphicsItem children of this item
	// (constructed with `this` as parent item). QGraphicsItem will delete its
	// children automatically, so deleting them manually here causes double-free.
	m_iconItem = nullptr;
	m_textItem = nullptr;
	delete m_animationTimer;
	m_animationTimer = nullptr;
}

void TaskBarItem::updateContent()
{
    if (!m_textItem ||
        !m_iconItem ||
        !m_dockApplet ||
        m_dockApplet->isDestroying() ||
        !m_dockApplet->panelWindow()) {
        return;
    }

    PanelWindow* panelWindow = m_dockApplet->panelWindow();
    PanelWindow::Orientation orientation = panelWindow->orientation();
    PanelWindow::Position position = panelWindow->position();

    const bool isVerticalPanel =
        (orientation == PanelWindow::Vertical) ||
        (position == PanelWindow::Left || position == PanelWindow::Right);

    const int cellW = (m_size.width()  > 0) ? m_size.width()  : m_targetSize.width();
    const int cellH = (m_size.height() > 0) ? m_size.height() : m_targetSize.height();

    bool showText = true;
    if (isVerticalPanel) {
        if (panelWindow->panelWidth() < 100)
            showText = false;
    }

    m_textItem->setFont(panelWindow->font());
    QFontMetrics fm(m_textItem->font());

    QString displayText;
    QIcon displayIcon;

    if (!m_clients.isEmpty()) {
        displayText = m_clients[0]->name();
        displayIcon = m_clients[0]->icon();
        
        // Always try to load from desktop file if available (more reliable than window icon)
        // Only use window icon if desktop file is not found
        QString desktopFile = findDesktopFile();
        if (!desktopFile.isEmpty()) {
            DesktopDataStore* dataStore = DesktopDataStore::instance();
            if (dataStore) {
                DesktopEntryData entry = dataStore->getDesktopEntry(desktopFile);
                if (!entry.isValid) {
                    entry = dataStore->parseDesktopFile(desktopFile);
                }
                if (entry.isValid && !entry.icon.isEmpty()) {
                    // Use loadIcon to load by icon name (like the menu does)
                    QIcon desktopIcon = UnifiedIconService::instance()->loadIcon(entry.icon, 32);
                    if (!desktopIcon.isNull() && !desktopIcon.availableSizes().isEmpty()) {
                        displayIcon = desktopIcon;
                    }
                }
            }
        } else {
            // Debug: check if we can find desktop file at all
            QString wmClass = m_clients[0]->wmClass();
        }
    } else if (!m_waylandClients.isEmpty() || m_waylandClient) {
        WaylandClient* primary = m_waylandClient ? m_waylandClient
                                                 : (m_waylandClients.isEmpty() ? nullptr : m_waylandClients.first());
        if (primary) {
            displayText = primary->name();
            displayIcon = primary->icon();
            
            // Always try to load from desktop file if available (more reliable than wayland icon)
            QString desktopFile = findDesktopFile();
            if (!desktopFile.isEmpty()) {
                DesktopDataStore* dataStore = DesktopDataStore::instance();
                if (dataStore) {
                    DesktopEntryData entry = dataStore->getDesktopEntry(desktopFile);
                    if (!entry.isValid) {
                        entry = dataStore->parseDesktopFile(desktopFile);
                    }
                    if (entry.isValid && !entry.icon.isEmpty()) {
                        // Use loadIcon to load by icon name (like the menu does)
                        QIcon desktopIcon = UnifiedIconService::instance()->loadIcon(entry.icon, 32);
                        if (!desktopIcon.isNull() && !desktopIcon.availableSizes().isEmpty()) {
                            displayIcon = desktopIcon;
                        }
                    }
                }
            }
        }
    } else if (!m_icon.isNull()) {
        displayText = m_waylandText; // fallback text
        displayIcon = m_icon;
    } else {
        displayText = m_textItem ? m_textItem->text() : QString();
        if (m_iconItem && !m_iconItem->pixmap().isNull())
            displayIcon = QIcon(m_iconItem->pixmap());
    }

    // Ensure tooltip is always set with a non-empty text
    // If displayText is empty, don't set tooltip (Qt will not show empty tooltips)
    if (!displayText.isEmpty()) {
        setToolTip(displayText);
    } else {
        // Clear tooltip if text is empty to avoid showing stale tooltips
        setToolTip(QString());
    }

    // --- sizing rules ---
    const int sideMargin = 5;   // outer margin for narrow side panels
    const int iconPad    = 3;   // extra internal padding between button and icon
    const int gap        = 8;
    const int leftPad    = 12;
    const int rightPad   = 8;

    int iconSize = adjustHardcodedPixelSize(24);

    if (isVerticalPanel && !showText) {
        // Narrow side panel: icon = (thickness - 2*(sideMargin + iconPad))
        int thickness = panelWindow->panelWidth();
        if (cellW > 0) thickness = qMin(thickness, cellW);

        const int inset = sideMargin + iconPad;

        int wanted = thickness - 2 * inset;                    // e.g. 64 -> 64 - 16 = 48
        int maxByCellH = (cellH > 0) ? (cellH - 2 * inset) : wanted;

        iconSize = qMin(wanted, maxByCellH);
        if (iconSize < adjustHardcodedPixelSize(8))
            iconSize = adjustHardcodedPixelSize(8);
    } else if (isVerticalPanel && showText) {
        int maxByHeight = (cellH > 0) ? (cellH - 6 - 2*iconPad) : (panelWindow->panelHeight() - 6 - 2*iconPad);
        iconSize = qMin(adjustHardcodedPixelSize(32), maxByHeight);
        if (iconSize < adjustHardcodedPixelSize(8))
            iconSize = adjustHardcodedPixelSize(8);
    } else {
        int maxByHeight = (cellH > 0) ? (cellH - 8 - 2*iconPad) : (panelWindow->panelHeight() - 8 - 2*iconPad);
        iconSize = qMin(adjustHardcodedPixelSize(24), maxByHeight);
        if (iconSize < adjustHardcodedPixelSize(8))
            iconSize = adjustHardcodedPixelSize(8);
    }

    if (!displayIcon.isNull()) {
        QPixmap pix = displayIcon.pixmap(iconSize, iconSize);
        m_iconItem->setPixmap(pix);
        if (!pix.isNull()) {
            iconSize = pix.height(); // Use actual height for centering
        }
    }

    // --- positioning ---
    int iconX = 0;
    int iconY = 0;

    if (!isVerticalPanel) {
        // For horizontal panels, we center the content (icon + text) horizontally
        // if there's enough space, otherwise we left-align with padding.
        int textW = showText ? (gap + fm.horizontalAdvance(displayText)) : 0;
        int contentW = iconSize + textW;
        
        if (cellW > contentW + adjustHardcodedPixelSize(32)) {
            // Button is wide enough to center content
            iconX = (cellW - contentW) / 2;
        } else {
            // Left-align with standard padding
            iconX = adjustHardcodedPixelSize(8) + iconPad;
        }
        
        iconY = (cellH > 0) ? (cellH - iconSize) / 2 : 0;
    } else {
        if (!showText) {
            // Center icon horizontally in vertical narrow panel
            iconX = (cellW > 0) ? (cellW - m_iconItem->pixmap().width()) / 2 : (sideMargin + iconPad);
            iconY = (cellH > 0) ? (cellH - iconSize) / 2 : 0;
        } else {
            iconX = leftPad + iconPad;                 // inset even when text is shown
            iconY = (cellH > 0) ? (cellH - iconSize) / 2 : 0;
        }
    }

    m_iconItem->setPos(iconX, iconY);

    // --- text ---
    if (showText) {
        int availableW = 0;
        QString textToElide = displayText;

        if (!isVerticalPanel) {
            availableW = (cellW > 0 ? cellW : 200) - (iconX + iconSize + gap + rightPad);
            if (availableW < 0) availableW = 0;

            QString shortName = fm.elidedText(textToElide, Qt::ElideRight, availableW);
            m_textItem->setText(shortName);
            m_textItem->setVisible(true);

            int textX = iconX + iconSize + gap;
            int textY = panelWindow->textBaseLine();
            m_textItem->setPos(textX, textY);
        } else {
            availableW = (cellW > 0 ? cellW : panelWindow->panelWidth()) - (iconX + iconSize + gap + rightPad);
            if (availableW < 0) availableW = 0;

            QString shortName = fm.elidedText(textToElide, Qt::ElideRight, availableW);
            m_textItem->setText(shortName);
            m_textItem->setVisible(true);

            int textX = iconX + iconSize + gap;
            int textY = (cellH > 0) ? ((cellH - fm.height()) / 2 + fm.ascent())
                                    : panelWindow->textBaseLine();
            m_textItem->setPos(textX, textY);
        }
    } else {
        m_textItem->setText(QString());
        m_textItem->setVisible(false);
    }

    update();
}

void TaskBarItem::fontChanged()
{
    // Safety check: ensure dock item and applet are still valid before updating
    if (!m_textItem || !m_dockApplet || m_dockApplet->isDestroying() || !m_dockApplet->panelWindow()) {
        return;
    }
    
    m_textItem->setFont(m_dockApplet->panelWindow()->font());
    update();
}

void TaskBarItem::addClient(Client* client)
{
	m_clients.append(client);
	updateClientsIconGeometry();
	updateContent();
	// Force a repaint to show the running state (background, etc.)
	update();
}

void TaskBarItem::removeClient(Client* client)
{
	int index = m_clients.indexOf(client);
	if (index >= 0) {
		m_clients.remove(index);
	}
	if(m_clients.isEmpty())
	{
		// Don't delete pinned items - they should stay even when no windows are open
		if (!isPinned()) {
			// Mark for deletion - the TaskBarApplet will handle the actual deletion
			// Don't call unregisterTaskBarItem here as it will be called from destructor
			// Just mark that this item should be deleted
			m_shouldDelete = true;
		} else {
			// Pinned item with no clients - update to show unpinned state (no background)
			updateContent();
			update();
		}
	}
	else
	{
		updateContent();
		update(); // Force repaint to update dots indicator
		// Also update the scene to ensure the dots are redrawn
		if (scene()) {
			scene()->update(boundingRect());
		}
	}
}

void TaskBarItem::setWaylandClient(WaylandClient* waylandClient)
{
    if (!m_textItem || !m_iconItem) {
        return;
    }

    // Allow clearing the wayland client when a window closes.
    m_waylandClient = waylandClient;
    m_waylandText = waylandClient ? waylandClient->name() : QString();

    updateContent();
}

void TaskBarItem::addWaylandClient(WaylandClient* waylandClient)
{
    if (!waylandClient) return;
    if (!m_waylandClients.contains(waylandClient)) {
        m_waylandClients.append(waylandClient);
    }
    if (!m_waylandClient) {
        m_waylandClient = waylandClient;
    }
    updateContent();
    update();
    if (scene()) {
        scene()->update(boundingRect());
    }
}

void TaskBarItem::removeWaylandClient(WaylandClient* waylandClient)
{
    if (!waylandClient) return;
    // Remove all occurrences (defensive against accidental duplicates)
    for (;;) {
        int idx = m_waylandClients.indexOf(waylandClient);
        if (idx < 0) break;
        m_waylandClients.remove(idx);
    }
    if (m_waylandClient == waylandClient) {
        m_waylandClient = m_waylandClients.isEmpty() ? nullptr : m_waylandClients.first();
    }
    if (m_waylandClients.isEmpty() && m_clients.isEmpty()) {
        if (!isPinned()) {
            m_shouldDelete = true;
        }
    }
    updateContent();
    update();
    if (scene()) {
        scene()->update(boundingRect());
    }
}

void TaskBarItem::setText(const QString& text)
{
    // Safety check: ensure dock item and applet are still valid before updating
    if (!m_textItem || !m_dockApplet || m_dockApplet->isDestroying()) {
        return;
    }
    
    m_textItem->setText(text);
    // Update stored Wayland text if this is a Wayland client
    if (m_clients.isEmpty()) {
        m_waylandText = text;
    }
    // Use updateContent() for consistent styling with X11 applications
    updateContent();
}

void TaskBarItem::setIcon(const QIcon& icon)
{
    // Safety check: ensure dock item and applet are still valid before updating
    if (!m_iconItem || !m_dockApplet || m_dockApplet->isDestroying()) {
        return;
    }
    
    m_icon = icon;
    // Use updateContent() for consistent styling with X11 applications
    updateContent();
}

QString TaskBarItem::text() const
{
    if (m_textItem) {
        return m_textItem->text();
    }
    return QString();
}

void TaskBarItem::setTargetPosition(const QPoint& targetPosition)
{
	m_targetPosition = targetPosition;
	updateClientsIconGeometry();
}

void TaskBarItem::setTargetSize(const QSize& targetSize)
{
	m_targetSize = targetSize;
	updateClientsIconGeometry();
	updateContent();
}

void TaskBarItem::moveInstantly()
{
	m_position = m_targetPosition;
	m_size = m_targetSize;
	setPos(m_position.x(), m_position.y());
	update();
}

void TaskBarItem::startAnimation()
{
	// Can be null during teardown (or if construction failed).
	if (!m_animationTimer || !m_dockApplet || m_dockApplet->isDestroying())
		return;
	if(!m_animationTimer->isActive())
		m_animationTimer->start();
}

void TaskBarItem::animate()
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

void TaskBarItem::close()
{
	// Close X11 windows
	for(int i = 0; i < m_clients.size(); i++)
	{
		X11Support::closeWindow(m_clients[i]->handle());
	}
	
	// Close Wayland windows
	if (!m_waylandClients.isEmpty() || m_waylandClient) {
		WaylandSupport* waylandSupport = m_dockApplet->waylandSupport();
		if (waylandSupport) {
			// Close all wayland toplevels represented by this item
			for (WaylandClient* wc : m_waylandClients) {
				if (wc && wc->surface()) {
					waylandSupport->closeWindow(wc->surface());
				}
			}
			// If we have only primary and list is empty, close primary
			if (m_waylandClients.isEmpty() && m_waylandClient && m_waylandClient->surface()) {
				waylandSupport->closeWindow(m_waylandClient->surface());
			}
		}
	}
}

QRectF TaskBarItem::boundingRect() const
{
	return QRectF(0.0, 0.0, m_size.width() - 1, m_size.height() - 1);
}

void TaskBarItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(widget)
    Q_UNUSED(option)

    if (!m_dockApplet || m_dockApplet->isDestroying()) return;
    PanelWindow* panelWindow = m_dockApplet->panelWindow();
    if (!panelWindow) return;

    auto dp = [this](int px) { return adjustHardcodedPixelSize(px); };

    const int w = qMax(1, m_size.width());
    const int h = qMax(1, m_size.height());

    // Note: position, orientation, isVerticalPanel, showText and textThresholdPx 
    // are defined in updateContent() but not used in paint() - they're kept for potential future use
    Q_UNUSED(panelWindow->position());
    Q_UNUSED(panelWindow->orientation());

    // Outer margins: 3px each side (=> width - 6)
    const int outerMarginX = dp(3);
    const int outerMarginY = dp(3);

    const int buttonW = qMax(1, w - 2 * outerMarginX);

    // Height uses available space minus margins.
    int buttonH = qMax(1, h - 2 * outerMarginY);

    const int buttonX = outerMarginX;
    const int buttonY = (h - buttonH) / 2;

    QRectF rect(buttonX, buttonY, buttonW, buttonH);

    painter->setPen(Qt::NoPen);

    static const qreal roundRadius = 3.0; // small radius, dp not critical here
    QPointF center(rect.center().x(), rect.bottom() + dp(20));

    // Check if this is a pinned item with no running clients
    bool isPinnedWithoutClients = isPinned() && m_clients.isEmpty() && m_waylandClients.isEmpty() && !m_waylandClient;

    // Base hover background - skip for pinned items without clients (unless hovering)
    if (!isPinnedWithoutClients || m_highlightIntensity > 0.001) {
        QRadialGradient gradient(center, dp(200), center);
        QColor buttonColorStart = m_buttonColor;
        buttonColorStart.setAlpha(m_buttonColorTransparency +
                                  static_cast<int>(m_buttonColorTransparency * m_highlightIntensity));
        QColor buttonColorEnd = m_buttonColor;
        buttonColorEnd.setAlpha(0);

        gradient.setColorAt(0.0, buttonColorStart);
        gradient.setColorAt(1.0, buttonColorEnd);

        painter->setBrush(QBrush(gradient));
        painter->drawRoundedRect(rect, roundRadius, roundRadius);
    }

    // Focus highlight
    if (m_focusHighlightIntensity > 0.001) {
        QColor focusPenColor = m_focusColor;
        focusPenColor.setAlpha(static_cast<int>(m_focusColorTransparency * m_focusHighlightIntensity));
        QPen focusPen(focusPenColor);
        focusPen.setWidth(dp(2));

        painter->setPen(focusPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(rect.adjusted(1, 1, -1, -1), roundRadius, roundRadius);

        QRadialGradient gradient(center, dp(200), center);
        QColor focusColorStart = m_focusColor;
        focusColorStart.setAlpha(static_cast<int>(60 * m_focusHighlightIntensity));
        QColor focusColorEnd = m_focusColor;
        focusColorEnd.setAlpha(0);

        gradient.setColorAt(0.0, focusColorStart);
        gradient.setColorAt(1.0, focusColorEnd);

        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(gradient));
        painter->drawRoundedRect(rect, roundRadius, roundRadius);
    }

    // Urgency highlight
    if (m_urgencyHighlightIntensity > 0.001) {
        QRadialGradient gradient(center, dp(200), center);
        gradient.setColorAt(0.0, QColor(255, 100, 0,
                                        static_cast<int>(160 * m_urgencyHighlightIntensity)));
        gradient.setColorAt(1.0, QColor(255, 255, 255, 0));

        painter->setBrush(QBrush(gradient));
        painter->drawRoundedRect(rect, roundRadius, roundRadius);
    }
    
    // Draw dots indicator for grouped windows (when more than one window)
    int windowCount = m_clients.size() + m_waylandClients.size() + (m_waylandClient && m_waylandClients.isEmpty() ? 1 : 0);
    if (windowCount > 1) {
        const int dotSize = dp(3);
        const int dotSpacing = dp(4);
        const int dotsY = buttonY + buttonH - dp(6); // Position at bottom of button
        const int totalDotsWidth = (windowCount * dotSize) + ((windowCount - 1) * dotSpacing);
        const int dotsX = buttonX + (buttonW - totalDotsWidth) / 2; // Center the dots
        
        painter->setPen(Qt::NoPen);
        painter->setBrush(QBrush(QColor(255, 255, 255, 200))); // Semi-transparent white dots
        
        for (int i = 0; i < windowCount; i++) {
            int x = dotsX + (i * (dotSize + dotSpacing));
            painter->drawEllipse(x, dotsY, dotSize, dotSize);
        }
    }
}

void TaskBarItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event)
    startAnimation();
}

void TaskBarItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event)
    startAnimation();
}

void TaskBarItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
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

void TaskBarItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	if (event->button() == Qt::LeftButton) {
		m_dragging = false;
		m_dockApplet->draggingStopped();
		setZValue(0.0); // No more on top.
		startAnimation(); // Item can be out of it's regular, start animation to bring it back.
	}

	if (isUnderMouse()) {
		// Allow clicks on pinned items even if no windows are open
		if (m_clients.isEmpty() && !m_waylandClient && !isPinned()) return;

		if (event->button() == Qt::LeftButton) {
			static const qreal clickMouseMoveTolerance = 10.0;

			if ((event->scenePos() - m_mouseDownPosition).manhattanLength() < 
                clickMouseMoveTolerance) {
                
                // Handle Wayland clients
                if (!m_waylandClients.isEmpty() || m_waylandClient) {
                    TaskBarApplet* dockApplet = qobject_cast<TaskBarApplet*>(m_dockApplet);
                    if (dockApplet && dockApplet->waylandSupport()) {
                        // Cycle through Wayland windows if grouped
                        QVector<WaylandClient*> list = m_waylandClients;
                        if (list.isEmpty() && m_waylandClient) list.append(m_waylandClient);
                        if (!list.isEmpty()) {
                            int nextIndex = (m_lastClickedIndex + 1) % list.size();
                            WaylandClient* next = list[nextIndex];
                            if (next && next->surface()) {
                                dockApplet->waylandSupport()->activateWindow(next->surface());
                                m_lastClickedIndex = nextIndex;
                            }
                        }
                    }
                }
                // Handle X11 clients
                else if (!m_clients.isEmpty()) {
                    unsigned long activeWindow = m_dockApplet->activeWindow();
                    
                    // Find which client (if any) is currently active
                    int activeIndex = -1;
                    for (int i = 0; i < m_clients.size(); i++) {
                        if (m_clients[i]->handle() == activeWindow) {
                            activeIndex = i;
                            break;
                        }
                    }
                    
                    if (m_clients.size() == 1) {
                        // Single window - toggle minimize/restore
                        if (activeIndex >= 0) {
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
                        } else {
                            X11Support::activateWindow(m_clients[0]->handle());
                        }
                    } else {
                        // Multiple windows - cycle through them
                        // Start from last clicked index + 1, or 0 if none clicked yet
                        int nextIndex = (m_lastClickedIndex + 1) % m_clients.size();
                        X11Support::activateWindow(m_clients[nextIndex]->handle());
                        m_lastClickedIndex = nextIndex;
                    }
                }
                // Handle pinned items (no windows open) - launch the application
                else if (isPinned()) {
                    launchApplication();
                }
			}
		}

        if (event->button() == Qt::RightButton && !m_dragging) {
            HPopupMenu menu;

            menu.addTitle(tr("Application"));
            
            // Show close only if there are windows open
            if (!m_clients.isEmpty() || m_waylandClient) {
                menu.addAction(QIcon::fromTheme("window-close"), tr("Close"), this, SLOT(close()));
            }
            
            // Add "Pin to taskbar" or "Unpin from taskbar" if we can find a desktop file
            QString desktopFile = findDesktopFile();
            if (!desktopFile.isEmpty() && m_dockApplet) {
                QStringList pinnedItems = m_dockApplet->getPinnedItems();
                if (pinnedItems.contains(desktopFile)) {
                    menu.addAction(QIcon::fromTheme("emblem-unreadable"), tr("Unpin from taskbar"), this, SLOT(removeFromPinned()));
                } else {
                    menu.addAction(QIcon::fromTheme("emblem-favorite"), tr("Pin to taskbar"), this, SLOT(addToFavorites()));
                }
            }
            
            menu.addTitle(tr("Task Bar"));
            menu.addAction(QIcon::fromTheme("preferences-other"), tr("Configure Task Bar"), m_dockApplet, SLOT(showConfigurationDialog()));

            menu.addTitle(tr("Panel"));
            menu.addAction(QIcon::fromTheme("preferences-desktop"), tr("Configure Panel"), m_dockApplet->panelWindow(), SLOT(showConfigurationDialog()));

            menu.addAction(QIcon::fromTheme("list-add"), tr("Add Panel"), QApplication::instance(), SLOT(addPanel()));
            menu.addAction(QIcon::fromTheme("list-remove"), tr("Remove Panel"), m_dockApplet->panelWindow(), SLOT(removePanel()));

            menu.exec(event->screenPos());
        }
	}
}

void TaskBarItem::wheelEvent(QGraphicsSceneWheelEvent* event)
{
    int windowCount = m_clients.size() + m_waylandClients.size() + (m_waylandClient && m_waylandClients.isEmpty() ? 1 : 0);
    
    // If single window, activate it
    if (windowCount == 1) {
        if (!m_clients.isEmpty()) {
            X11Support::activateWindow(m_clients[0]->handle());
        } else if (!m_waylandClients.isEmpty() || m_waylandClient) {
            TaskBarApplet* dockApplet = qobject_cast<TaskBarApplet*>(m_dockApplet);
            if (dockApplet && dockApplet->waylandSupport()) {
                WaylandClient* wc = !m_waylandClients.isEmpty() ? m_waylandClients.first() : m_waylandClient;
                if (wc && wc->surface()) {
                    dockApplet->waylandSupport()->activateWindow(wc->surface());
                }
            }
        }
        event->accept();
        return;
    }
    
    // If no windows, ignore
    if (windowCount == 0) {
        event->ignore();
        return;
    }
    
    // Build combined list of all windows (X11 first, then Wayland)
    QVector<Client*> x11List = m_clients;
    QVector<WaylandClient*> wlList = m_waylandClients;
    if (wlList.isEmpty() && m_waylandClient) {
        wlList.append(m_waylandClient);
    }
    
    // Find the currently active window
    unsigned long activeWindow = m_dockApplet->activeWindow();
    int currentIndex = -1;
    
    // Check X11 clients first
    for (int i = 0; i < x11List.size(); i++) {
        if (x11List[i]->handle() == activeWindow) {
            currentIndex = i;
            break;
        }
    }
    
    // If not found in X11, check Wayland clients
    if (currentIndex < 0) {
        TaskBarApplet* dockApplet = qobject_cast<TaskBarApplet*>(m_dockApplet);
        if (dockApplet && dockApplet->waylandSupport()) {
            // Try to find active Wayland window by checking if any surface is focused
            // (Wayland doesn't have a simple "active window" handle like X11)
            // For now, we'll use the first window if none are found active
            currentIndex = x11List.size(); // Start from first Wayland window
        }
    }
    
    // If none of our windows are active, start from the first
    if (currentIndex < 0) {
        currentIndex = 0;
    }
    
    // Calculate next window index based on scroll direction
    // QGraphicsSceneWheelEvent uses delta() in both Qt5 and Qt6
    int delta = event->delta();
    
    int nextIndex = currentIndex;
    
    if (delta > 0) {
        // Scroll up - go to previous window
        nextIndex = (currentIndex - 1 + windowCount) % windowCount;
    } else if (delta < 0) {
        // Scroll down - go to next window
        nextIndex = (currentIndex + 1) % windowCount;
    } else {
        // No scroll, do nothing
        event->accept();
        return;
    }
    
    // Activate the next window
    if (nextIndex < x11List.size()) {
        // X11 window
        X11Support::activateWindow(x11List[nextIndex]->handle());
    } else {
        // Wayland window
        TaskBarApplet* dockApplet = qobject_cast<TaskBarApplet*>(m_dockApplet);
        if (dockApplet && dockApplet->waylandSupport()) {
            int wlIndex = nextIndex - x11List.size();
            if (wlIndex >= 0 && wlIndex < wlList.size()) {
                WaylandClient* wc = wlList[wlIndex];
                if (wc && wc->surface()) {
                    dockApplet->waylandSupport()->activateWindow(wc->surface());
                }
            }
        }
    }
    
    event->accept();
}

void TaskBarItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
	// Mouse events are sent only when mouse button is pressed.
	if(!m_dragging)
		return;

	// Get panel orientation
	PanelWindow::Orientation orientation = PanelWindow::Horizontal;
	if (m_dockApplet->panelWindow()) {
		orientation = m_dockApplet->panelWindow()->orientation();
	}

	QPointF delta = event->scenePos() - m_mouseDownPosition;
	
	if (orientation == PanelWindow::Horizontal) {
		// Horizontal layout: drag left/right
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
	} else {
		// Vertical layout: drag up/down
		m_position.setY(m_dragStartPosition.y() + static_cast<int>(delta.y()));
		if(m_position.y() < 0)
			m_position.setY(0);
		if(m_position.y() >= m_dockApplet->size().height() - m_targetSize.height())
			m_position.setY(m_dockApplet->size().height() - m_targetSize.height());
		setPos(m_position.x(), m_position.y());

		int criticalShift = m_targetSize.height()*55/100;

		if(m_position.y() < m_targetPosition.y() - criticalShift)
			m_dockApplet->moveItem(this, false);

		if(m_position.y() > m_targetPosition.y() + criticalShift)
			m_dockApplet->moveItem(this, true);
	}

	update();
}

void TaskBarItem::updateClientsIconGeometry()
{
    // Only meaningful on X11
#if QT_VERSION < 0x060000
    const bool isX11 = QX11Info::isPlatformX11();
#else
    const bool isX11 = qApp->platformName().toLower().contains("xcb");
#endif
    if (!isX11) return;

    // Defensive: never touch anything while we’re shutting down / item invalid
    if (m_shouldDelete) return;
    if (!m_dockApplet || m_dockApplet->isDestroying()) return;

    PanelWindow* pw = m_dockApplet->panelWindow();
    if (!pw) return;

    if (m_targetSize.width() <= 0 || m_targetSize.height() <= 0) return;

    // If this TaskBarApplet is not in a scene anymore, mapToScene can be unreliable
    // but it usually still works; keep it guarded.
    QPointF topLeft = m_dockApplet->mapToScene(m_targetPosition);

    QVector<unsigned long> values;
    values.resize(4);
    values[0] = static_cast<unsigned long>(qRound(topLeft.x() + pw->pos().x()));
    values[1] = static_cast<unsigned long>(qRound(topLeft.y() + pw->pos().y()));
    values[2] = static_cast<unsigned long>(m_targetSize.width());
    values[3] = static_cast<unsigned long>(m_targetSize.height());

    for (int i = 0; i < m_clients.size(); ++i) {
        Client* c = m_clients[i];
        if (!c) continue;
        if (!c->handle()) continue;
        X11Support::setWindowPropertyCardinalArray(c->handle(), "_NET_WM_ICON_GEOMETRY", values);
    }
}

bool TaskBarItem::isUrgent()
{
	for(int i = 0; i < m_clients.size(); i++)
	{
		if(m_clients[i]->isUrgent())
			return true;
	}
	return false;
}

bool TaskBarItem::isFocused() const
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

QString TaskBarItem::findDesktopFile()
{
    QString appId;
    QString wmClass;
    
    // Try to get app ID or WM_CLASS from the window
    if (m_waylandClient) {
        appId = m_waylandClient->appId();
    } else if (!m_clients.isEmpty()) {
        // Get WM_CLASS from the first X11 client (now stored in Client)
        wmClass = m_clients[0]->wmClass();
        // If wmClass is empty, try to get it directly as fallback
        if (wmClass.isEmpty()) {
            wmClass = X11Support::getWindowWMClass(m_clients[0]->handle());
        }
    }

    if (appId.isEmpty() && wmClass.isEmpty()) {
        return QString();
    }
    
    // Use DesktopDataStore like the start menu does
    DesktopDataStore* dataStore = DesktopDataStore::instance();
    if (!dataStore) {
        return QString();
    }
    
    // Get all desktop entries and search for matches
    QList<DesktopEntryData> allEntries = dataStore->getAllDesktopEntries();
    
    QString appIdLower = appId.toLower();
    QString wmClassLower = wmClass.toLower();
    
    // Try multiple matching strategies
    foreach (const DesktopEntryData& entry, allEntries) {
        if (entry.type != "Application" || !entry.shouldShow()) {
            continue;
        }
        
        // Get desktop file base name (without path and extension)
        QString fileName = QFileInfo(entry.desktopFile).completeBaseName().toLower();
        
        // Match by startupWMClass (most reliable)
        if (!wmClass.isEmpty() && entry.startupWMClass.toLower() == wmClassLower) {
            return entry.desktopFile;
        }
        
        // Match by desktop file name
        if (!appIdLower.isEmpty()) {
            if (fileName == appIdLower || 
                fileName.startsWith(appIdLower + "-") || 
                fileName.startsWith(appIdLower + "_")) {
                return entry.desktopFile;
            }
        }
        
        if (!wmClassLower.isEmpty()) {
            if (fileName == wmClassLower || 
                fileName.startsWith(wmClassLower + "-") || 
                fileName.startsWith(wmClassLower + "_")) {
                return entry.desktopFile;
            }
        }
        
        // Match by executable name (from Exec= field)
        if (!entry.exec.isEmpty()) {
            QString execLower = entry.exec.toLower();
            // Extract executable name (first word, before any arguments)
            QString execName = execLower.split(' ').first().split('/').last();
            
            if (!appIdLower.isEmpty() && execName == appIdLower) {
                return entry.desktopFile;
            }
            if (!wmClassLower.isEmpty() && execName == wmClassLower) {
                return entry.desktopFile;
            }
        }
        
        // Match by application name
        QString nameLower = entry.name.toLower();
        if (!appIdLower.isEmpty() && nameLower == appIdLower) {
            return entry.desktopFile;
        }
        if (!wmClassLower.isEmpty() && nameLower == wmClassLower) {
            return entry.desktopFile;
        }
    }
    
    return QString();
}

void TaskBarItem::setDesktopFile(const QString& desktopFile)
{
    m_desktopFile = desktopFile;
}

void TaskBarItem::launchApplication()
{
    if (m_desktopFile.isEmpty()) {
        return;
    }
    
    DesktopDataStore* dataStore = DesktopDataStore::instance();
    if (dataStore) {
        dataStore->launchApplication(m_desktopFile);
    }
}

void TaskBarItem::addToFavorites()
{
    QString desktopFile = findDesktopFile();
    if (desktopFile.isEmpty()) {
        return;
    }
    
    if (!m_dockApplet) {
        return;
    }
    
    // Get current taskbar pinned items (separate from start menu favorites)
    QStringList pinnedItems = m_dockApplet->getPinnedItems();
    
    // Add if not already pinned
    if (!pinnedItems.contains(desktopFile)) {
        pinnedItems << desktopFile;
        m_dockApplet->setPinnedItems(pinnedItems);
        
        // Also set the desktop file on this item so it becomes a pinned item
        setDesktopFile(desktopFile);
        
        // Update the item to show it's pinned (even if no windows are open)
        updateContent();
        
        // Save the updated list
        m_dockApplet->savePinnedItems();
    }
}

void TaskBarItem::removeFromPinned()
{
    if (m_desktopFile.isEmpty() || !m_dockApplet) {
        return;
    }
    
    // Get current taskbar pinned items
    QStringList pinnedItems = m_dockApplet->getPinnedItems();
    
    // Remove from pinned items
    if (pinnedItems.contains(m_desktopFile)) {
        pinnedItems.removeAll(m_desktopFile);
        m_dockApplet->setPinnedItems(pinnedItems);
        
        // Clear the desktop file
        m_desktopFile.clear();
        
        // Save the updated list
        m_dockApplet->savePinnedItems();
        
        // If no windows are open, mark for deletion
        if (m_clients.isEmpty() && !m_waylandClient) {
            m_shouldDelete = true;
            m_dockApplet->unregisterTaskBarItem(this);
            deleteLater();
        } else {
            updateContent();
        }
    }
}

void TaskBarItem::setButtonColor(const QColor& color, int transparency)
{
    m_buttonColor = color;
    m_buttonColorTransparency = transparency;
    update();
}

void TaskBarItem::setFocusColor(const QColor& color, int transparency)
{
    m_focusColor = color;
    m_focusColorTransparency = transparency;
    update();
}

bool TaskBarItem::hasClient(Client* client) const
{
	return m_clients.contains(client);
}

bool TaskBarItem::hasWaylandClient(WaylandClient* client) const
{
	return client && (m_waylandClient == client || m_waylandClients.contains(client));
}
