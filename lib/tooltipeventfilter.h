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

#ifndef TOOLTIPEVENTFILTER_H
#define TOOLTIPEVENTFILTER_H

#include <QObject>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QTimer>

class PanelWindow;

class TooltipEventFilter : public QObject
{
    Q_OBJECT

public:
    explicit TooltipEventFilter(QGraphicsView* view, PanelWindow* panelWindow, QObject* parent = nullptr);
    
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QGraphicsView* m_view;
    PanelWindow* m_panelWindow;
    QTimer* m_hoverTimer;
    QGraphicsItem* m_currentItem;
    QPointF m_currentScenePos;
};

#endif // TOOLTIPEVENTFILTER_H


