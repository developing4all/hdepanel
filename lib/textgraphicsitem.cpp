/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL3+
 *
 * Copyright: 2015-2025 Haydar Alkaduhimi
 * Copyright (C) 2014 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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
 * END_COMMON_COPYRIGHT_HEADER 
*/

#include "textgraphicsitem.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

TextGraphicsItem::TextGraphicsItem(QGraphicsItem* parent)
    : QGraphicsItem(parent)
{
}

TextGraphicsItem::~TextGraphicsItem() = default;

void TextGraphicsItem::setColor(const QColor& color)
{
    if (m_color == color) return;
    m_color = color;
    update();
}

void TextGraphicsItem::setFont(const QFont& font)
{
    if (m_font == font) return;
    prepareGeometryChange();
    m_font = font;
    update();
}

void TextGraphicsItem::setText(const QString& text)
{
    if (m_text == text) return;
    prepareGeometryChange();
    m_text = text;
    update();
}

void TextGraphicsItem::setImage(const QImage& image)
{
    if (m_image.cacheKey() == image.cacheKey()) return;
    prepareGeometryChange();
    m_image = image;
    update();
}

QRectF TextGraphicsItem::multiLineBoundingRect(const QString& text) const
{
    QFontMetrics fm(m_font);
    const int lineH = fm.height();

    QString normalized = text;
    normalized.replace("\r\n", "\n");
    const QStringList lines = normalized.split('\n', Qt::KeepEmptyParts);

    QRect total;
    bool first = true;

    for (int i = 0; i < lines.size(); ++i) {
        QRect r = fm.boundingRect(lines[i]);   // IMPORTANT: baseline-based rect (y can be negative)
        r.translate(0, i * lineH);             // next line baseline
        if (first) { total = r; first = false; }
        else       { total = total.united(r); }
    }

    if (first) {
        // empty text
        total = fm.boundingRect(QString());
    }

    return total;
}

QRectF TextGraphicsItem::boundingRect() const
{
    if (!m_image.isNull()) {
        return QRectF(0.0, 0.0, m_image.width(), m_image.height());
    }

    // Single-line: keep the old behavior exactly
    if (!m_text.contains('\n') && !m_text.contains("\r\n")) {
        QFontMetrics fm(m_font);
        return fm.boundingRect(m_text);
    }

    return multiLineBoundingRect(m_text);
}

void TextGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    if (!m_image.isNull()) {
        painter->drawImage(0, 0, m_image);
        return;
    }

    painter->setFont(m_font);

    // Single-line: old behavior exactly (baseline at y=0)
    if (!m_text.contains('\n') && !m_text.contains("\r\n")) {
        painter->setPen(QPen(Qt::black));
        painter->drawText(1, 1, m_text);
        painter->setPen(QPen(m_color));
        painter->drawText(0, 0, m_text);
        return;
    }

    // Multi-line: draw each line manually, still baseline-based
    QFontMetrics fm(m_font);
    const int lineH = fm.height();

    QString normalized = m_text;
    normalized.replace("\r\n", "\n");
    const QStringList lines = normalized.split('\n', Qt::KeepEmptyParts);

    for (int i = 0; i < lines.size(); ++i) {
        const int y = i * lineH; // baseline
        painter->setPen(QPen(Qt::black));
        painter->drawText(1, y + 1, lines[i]);
        painter->setPen(QPen(m_color));
        painter->drawText(0, y, lines[i]);
    }
}
