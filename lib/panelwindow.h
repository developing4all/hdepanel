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

#ifndef PANELWINDOW_H
#define PANELWINDOW_H

#include <QtCore/QVector>
#if QT_VERSION >= 0x050000
#include <QWidget>
#include <QGraphicsItem>
#else
#include <QtGui/QWidget>
#include <QtGui/QGraphicsItem>
#endif

class QFont;
class QGraphicsScene;
class QGraphicsView;
class Applet;
class PanelWindow;
class QDir;

class PanelWindowGraphicsItem: public QGraphicsItem
{
public:
	PanelWindowGraphicsItem(PanelWindow* panelWindow);
	~PanelWindowGraphicsItem();

	QRectF boundingRect() const;
	void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);

protected:
	void mousePressEvent(QGraphicsSceneMouseEvent* event);
	void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);

private:
	PanelWindow* m_panelWindow;
};

class PanelWindow: public QWidget
{
	Q_OBJECT
public:
    PanelWindow(QString id);
	~PanelWindow();

	enum Anchor
	{
		Min,
		Center,
		Max,
	};

	enum Orientation
	{
		Horizontal,
		Vertical,
	};

	enum LayoutPolicy
	{
		Normal,
		AutoSize,
		FillSpace,
	};

	bool init();

	bool dockMode() const
	{
		return m_dockMode;
	}

	void setDockMode(bool dockMode);

	int screen() const
	{
		return m_screen;
	}

	void setScreen(int screen);

	Anchor horizontalAnchor() const
	{
		return m_horizontalAnchor;
	}

	void setHorizontalAnchor(Anchor horizontalAnchor);

	Anchor verticalAnchor() const
	{
		return m_verticalAnchor;
	}

	void setVerticalAnchor(Anchor verticalAnchor);

	Orientation orientation() const
	{
		return m_orientation;
	}

	void setOrientation(Orientation orientation);

	LayoutPolicy layoutPolicy() const
	{
		return m_layoutPolicy;
	}

	void setLayoutPolicy(LayoutPolicy layoutPolicy);

	void updatePosition();

    //const QFont& font() const;

	int textBaseLine();

	PanelWindowGraphicsItem* panelItem()
	{
		return m_panelItem;
	}

	void resizeEvent(QResizeEvent* event);

	void updateLayout();

	void showPanelContextMenu(const QPoint& point);
    void setFontName(const QString& fontName);

    void readSettings();

public slots:
    void showConfigurationDialog();
    void removePanel();
    void resetApplets();
    void setApplets(QStringList applets);

private:
    void removeApplets();
    void loadApplet(QString applet_name, QDir &plugDir);

	bool m_dockMode;
	int m_screen;
    QString m_id;
	Anchor m_horizontalAnchor;
	Anchor m_verticalAnchor;
	Orientation m_orientation;
	LayoutPolicy m_layoutPolicy;

	QGraphicsScene* m_scene;
	QGraphicsView* m_view;
	PanelWindowGraphicsItem* m_panelItem;
    QVector<Applet*> m_applets;
 };

#endif
