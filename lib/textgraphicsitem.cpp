/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * Copyright: 2015-2016 Haydar Alkaduhimi
 * Copyright (C) 2014 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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

#include "textgraphicsitem.h"

#include <QtGui/QFontMetrics>
#include <QtGui/QPainter>

TextGraphicsItem::TextGraphicsItem(QGraphicsItem* parent)
	: QGraphicsItem(parent)
{
}

TextGraphicsItem::~TextGraphicsItem()
{
}

void TextGraphicsItem::setColor(const QColor& color)
{
	m_color = color;
	update(boundingRect());
}

void TextGraphicsItem::setFont(const QFont& font)
{
	m_font = font;
	update(boundingRect());
}

void TextGraphicsItem::setText(const QString& text)
{
	m_text = text;
	update(boundingRect());
}

void TextGraphicsItem::setImage(const QImage& image) 
{
    m_image = image;
    update(boundingRect());
}

QRectF TextGraphicsItem::boundingRect() const
{
	if (!m_image.isNull()) {
        return m_image.rect();
    }
    QFontMetrics metrics(m_font);
	return metrics.boundingRect(m_text);
}

void TextGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	if (m_image.isNull()) {
        painter->setFont(m_font);
	    painter->setPen(QPen(Qt::black));
	    painter->drawText(1, 1, m_text);
	    painter->setPen(QPen(m_color));
	    painter->drawText(0, 0, m_text);
    } else {
        painter->drawImage(0, 0, m_image);
    }
}
