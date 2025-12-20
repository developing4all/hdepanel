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

#ifndef SNITRAYITEM_H
#define SNITRAYITEM_H

#include <QtCore/QSize>
#include <QtGui/QIcon>
#include <QGraphicsItem>

class TrayApplet;
class SniItemProxy;

class SniTrayItem: public QObject, public QGraphicsItem
{
	Q_OBJECT
	Q_INTERFACES(QGraphicsItem)
public:
	SniTrayItem(TrayApplet* trayApplet, SniItemProxy* sniItem);
	~SniTrayItem();

	void setPosition(const QPoint& position);
	void setSize(const QSize& size);

	QRectF boundingRect() const override;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
	void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

	SniItemProxy* sniItem() const
	{
		return m_sniItem;
	}

private slots:
	void updateIcon();

private:
	QSize m_size;
	TrayApplet* m_trayApplet;
	SniItemProxy* m_sniItem;
	QIcon m_cachedIcon;
};

#endif // SNITRAYITEM_H

