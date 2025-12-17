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

#include "customtooltip.h"
#include <QPainter>

CustomTooltip::CustomTooltip() : QWidget(nullptr)
{
    // Create as a top-level window (no parent) with tooltip flags
    // Qt5/Qt6 compatible window flags
    Qt::WindowFlags flags = Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus;
    setWindowFlags(flags);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setAttribute(Qt::WA_NoMouseReplay, true);
    
    // Set styling for readable tooltip
    setStyleSheet(
        "QWidget {"
        "    background-color: #ffffff;"
        "    color: #000000;"
        "    border: 1px solid #808080;"
        "    padding: 4px 8px;"
        "    border-radius: 3px;"
        "}"
    );
    
    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setStyleSheet("background-color: transparent; color: #000000;");
    m_label->setWordWrap(false);
    m_label->setTextFormat(Qt::PlainText);
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(0);
    layout->addWidget(m_label);
    
    hide();
}

void CustomTooltip::setText(const QString& text)
{
    if (m_label) {
        m_label->setText(text);
        m_label->adjustSize();
        adjustSize();
        updateGeometry();
    }
}

void CustomTooltip::showAtPosition(const QPoint& globalPos)
{
    if (isVisible() && pos() == globalPos) {
        return; // Already shown at this position
    }
    move(globalPos);
    show();
    raise();
    // Force update to ensure visibility
    update();
    repaint();
}

QSize CustomTooltip::sizeHint() const
{
    if (m_label) {
        QSize labelSize = m_label->sizeHint();
        return labelSize + QSize(8, 8); // Add padding
    }
    return QSize(100, 30);
}

void CustomTooltip::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
}

