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

 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "tooltipeventfilter.h"
#include "panelwindow.h"
#include <QMouseEvent>
#include <QEvent>
#include <QToolTip>
#include <QGraphicsScene>

TooltipEventFilter::TooltipEventFilter(QGraphicsView* view, PanelWindow* panelWindow, QObject* parent)
    : QObject(parent)
    , m_view(view)
    , m_panelWindow(panelWindow)
    , m_hoverTimer(new QTimer(this))
    , m_currentItem(nullptr)
{
    m_hoverTimer->setSingleShot(true);
    m_hoverTimer->setInterval(500); // Show tooltip after 500ms hover
    connect(m_hoverTimer, &QTimer::timeout, [this]() {
        if (m_currentItem && m_panelWindow && m_view && !m_currentItem->toolTip().isEmpty()) {
            // Get the item's bounding rect center in scene coordinates
            QRectF itemRect = m_currentItem->boundingRect();
            QPointF itemCenter = m_currentItem->mapToScene(itemRect.center());
            // Convert to widget coordinates
            QPoint widgetPos = m_view->mapFromScene(itemCenter);
            m_panelWindow->showCustomTooltip(m_currentItem->toolTip(), widgetPos);
        }
    });
}

bool TooltipEventFilter::eventFilter(QObject* obj, QEvent* event)
{
    Q_UNUSED(obj)
    if (!m_view) return QObject::eventFilter(obj, event);
    
    if (event->type() == QEvent::MouseMove) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        QPointF scenePos = m_view->mapToScene(mouseEvent->pos());
        QList<QGraphicsItem*> items = m_view->scene()->items(scenePos, Qt::IntersectsItemShape, Qt::DescendingOrder);
        
        QGraphicsItem* tooltipItem = nullptr;
        for (QGraphicsItem* item : items) {
            if (item && !item->toolTip().isEmpty() && item->isVisible() && item->isEnabled()) {
                // Skip the panel background item (PanelWindowGraphicsItem)
                // Check if it's not the root graphics item
                if (item->parentItem() != nullptr || item->type() != QGraphicsItem::UserType + 1) {
                    tooltipItem = item;
                    break;
                }
            }
        }
        
        if (tooltipItem && tooltipItem != m_currentItem) {
            // New item under cursor, start hover timer
            m_currentItem = tooltipItem;
            m_currentScenePos = scenePos;
            m_hoverTimer->start();
        } else if (!tooltipItem) {
            // No item with tooltip under cursor
            m_hoverTimer->stop();
            m_panelWindow->hideCustomTooltip();
            m_currentItem = nullptr;
        } else if (tooltipItem == m_currentItem) {
            // Same item, restart timer if it was stopped
            if (!m_hoverTimer->isActive()) {
                m_hoverTimer->start();
            }
        }
    } else if (event->type() == QEvent::Leave) {
        // Hide tooltip when mouse leaves the view
        m_hoverTimer->stop();
        m_panelWindow->hideCustomTooltip();
        m_currentItem = nullptr;
    } else if (event->type() == QEvent::ToolTip) {
        // Consume tooltip events to prevent default tooltip from showing
        QToolTip::hideText();
        event->accept();
        return true;
    }
    
    return QObject::eventFilter(obj, event);
}

