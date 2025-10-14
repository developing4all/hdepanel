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
 #pragma once

 #include <QWidget>
 #include <QVector>
 #include <QStringList>
 #include <QRect>
#include <QTimer>
#include <QGraphicsItem>

class QGraphicsScene;
 class QGraphicsView;
 class QMouseEvent;
 class QResizeEvent;
 class QShowEvent;
 class QScreen;
 class Applet;
 
 class PanelWindow : public QWidget
 {
	 Q_OBJECT
 public:
	 enum Orientation { Horizontal, Vertical };
	 enum Anchor { Min, Center, Max };
	 enum LayoutPolicy { Normal, AutoSize, FillSpace };
 
	 explicit PanelWindow(QString id);
	 ~PanelWindow() override;
 
	 // Settings / state
	 void setDockMode(bool dockMode);
	 void setScreen(int screen);
	 int screen() const { return m_screen; }

	 void setHorizontalAnchor(Anchor horizontalAnchor);
	 void setVerticalAnchor(Anchor verticalAnchor);
	 void setOrientation(Orientation orientation);
	 void setLayoutPolicy(LayoutPolicy layoutPolicy);
 
	 int  textBaseLine();
	 void resetApplets();
 
	 // Exposed helpers
	 QRect currentScreenGeometry() const;
	 QRect getAvailableScreenGeometry() const;
 
	 // Used by settings dialog
	 inline Anchor verticalAnchor() const { return m_verticalAnchor; }
	 inline Anchor horizontalAnchor() const { return m_horizontalAnchor; }
 	 // Context menu / settings UI
	 void showPanelContextMenu(const QPoint& point);
	 bool init();
	 void setFontName(const QString& fontName);

 protected:
	 void showEvent(QShowEvent* e) override;
	 void resizeEvent(QResizeEvent* event) override;
	 void mousePressEvent(QMouseEvent* event) override;
	 void mouseReleaseEvent(QMouseEvent* event) override;
 
 public slots:
	 void updateLayout();
	 void updatePosition();
	 void showConfigurationDialog();
	 void removePanel();

 private:
	 // Settings / init
	 void readSettings();
	 void setApplets();
	 void loadApplet(QString applet_id, class QDir &plugDir);
	 void removeApplets();
 
	 // Geometry / WM integration
	 void setupStrutProperties();
	 void applyX11Struts(const QRect& panelGeom);
	 void scheduleApplyStruts();                 // debounce wrapper
	 void scheduleApplyStrutsIfMoved();          // only when geometry changed
	 int  detectGnomeTopOffsetPx() const;        // uses X11Support::detectTopPanelHeight or fallback
 
	 // Utility
	 QRect getAnchorGeometry(const QRect& screen, const QRect& available) const;
  
	 // Wayland positioning
	 void forceWaylandPosition();
 
 private:
	 // Identity / settings
	 QString       m_id = {};
	 QStringList   m_appletnames;
 
	 // Layout & geometry
	 Orientation   m_orientation   = Horizontal;
	 Anchor        m_horizontalAnchor = Center;
	 Anchor        m_verticalAnchor   = Max;     // default bottom
	 LayoutPolicy  m_layoutPolicy  = Normal;
	 bool          m_dockMode      = false;
	 int           m_screen        = 0;
 
	 // Applets as QGraphicsItems
	 QVector<Applet*> m_applets;
	 QGraphicsScene*  m_scene = nullptr;
	 QGraphicsView*   m_view  = nullptr;
 
	 // Debounce strut application to avoid repeated XChangeProperty calls
	 QTimer        m_strutDebounce;
	 QRect         m_lastStrutGeom;              // last geometry we applied struts for
 
	 // Wayland helpers
	 QTimer*       m_waylandRepositionTimer = nullptr;
 
	 friend class PanelWindowGraphicsItem;
 
 public:
	 // Used by graphics item
	 class PanelWindowGraphicsItem : public QGraphicsItem
	 {
	 public:
		 explicit PanelWindowGraphicsItem(PanelWindow* panelWindow);
		 ~PanelWindowGraphicsItem() override;
		 QRectF boundingRect() const override;
		 void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
 
	 protected:
		 void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
		 void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
 
	 private:
		 PanelWindow* m_panelWindow;
	 };
 };
 