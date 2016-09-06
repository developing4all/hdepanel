/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * This Files has been imported to hde from qtpanel
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

#include <QtGui/QResizeEvent>
#if QT_VERSION >= 0x050000
#include <QApplication>                                                   
#include <QDesktopWidget>                                                    
#include <QGraphicsScene>                                                    
#include <QGraphicsSceneMouseEvent>                                          
#include <QGraphicsView>                                                     
//#include <QMenu>
#else
#include <QtGui/QApplication>
#include <QtGui/QDesktopWidget>
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QGraphicsView>
//#include <QtGui/QMenu>
#endif
#include <QPluginLoader>

#include <QStandardPaths>

#include <QDir>

#include "hpopupmenu.h"
#include "applet.h"

#include "settings.h"

#include "panelwindow.h"
#include "dpisupport.h"
#include "panelapplication.h"
#include "x11support.h"

#include "panelsettings.h"


PanelWindowGraphicsItem::PanelWindowGraphicsItem(PanelWindow* panelWindow)
	: m_panelWindow(panelWindow)
{
	setZValue(-10.0); // Background.
    setAcceptedMouseButtons(Qt::RightButton);
}

PanelWindowGraphicsItem::~PanelWindowGraphicsItem()
{
}

QRectF PanelWindowGraphicsItem::boundingRect() const
{
	return QRectF(0.0, 0.0, m_panelWindow->width(), m_panelWindow->height());
}

void PanelWindowGraphicsItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

	painter->setPen(Qt::NoPen);
	painter->setBrush(QColor(0, 0, 0, 128));
	painter->drawRect(boundingRect());

	static const int borderThickness = 3;
	if(m_panelWindow->verticalAnchor() == PanelWindow::Min)
	{
		QLinearGradient gradient(0.0, m_panelWindow->height() - borderThickness, 0.0, m_panelWindow->height());
		gradient.setSpread(QGradient::RepeatSpread);
		gradient.setColorAt(0.0, QColor(255, 255, 255, 0));
		gradient.setColorAt(1.0, QColor(255, 255, 255, 128));
		painter->setBrush(QBrush(gradient));
		painter->drawRect(0.0, m_panelWindow->height() - borderThickness, m_panelWindow->width(), borderThickness);
	}
	else
	{
		QLinearGradient gradient(0.0, 0.0, 0.0, borderThickness);
		gradient.setSpread(QGradient::RepeatSpread);
		gradient.setColorAt(0.0, QColor(255, 255, 255, 128));
		gradient.setColorAt(1.0, QColor(255, 255, 255, 0));
		painter->setBrush(QBrush(gradient));
		painter->drawRect(0.0, 0.0, m_panelWindow->width(), borderThickness);
	}
}

void PanelWindowGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    Q_UNUSED(event)
}

void PanelWindowGraphicsItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
	if(isUnderMouse())
	{
		m_panelWindow->showPanelContextMenu(QPoint(static_cast<int>(event->pos().x()), static_cast<int>(event->pos().y())));
	}
}

PanelWindow::PanelWindow(QString id)
{
    m_id = id;
    m_dockMode = false;
    m_screen = 0;
    m_horizontalAnchor = Center;
    m_verticalAnchor = Min;
    m_orientation = Horizontal;
    m_layoutPolicy = Normal;

    setStyleSheet("background-color: transparent");
	setAttribute(Qt::WA_TranslucentBackground);

	m_scene = new QGraphicsScene();
	m_scene->setBackgroundBrush(QBrush(Qt::NoBrush));

	m_panelItem = new PanelWindowGraphicsItem(this);
	m_scene->addItem(m_panelItem);

	m_view = new QGraphicsView(m_scene, this);
	m_view->setStyleSheet("border-style: none;");
	m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	m_view->setRenderHint(QPainter::Antialiasing);
	m_view->move(0, 0);

    readSettings();

    resize(adjustHardcodedPixelSize(512), adjustHardcodedPixelSize(48));
}

void PanelWindow::readSettings()
{
    setFontName(Settings::value(m_id, "fontName", "default").toString());
    setScreen(Settings::value(m_id, "screen", 0).toInt());

    PanelWindow::Anchor m_verticalAnchor;
    QString verticalPosition = Settings::value(m_id, "verticalPosition", "Bottom").toString();
    if(verticalPosition == "Top")
        m_verticalAnchor = PanelWindow::Min;
    else if(verticalPosition == "Bottom")
        m_verticalAnchor = PanelWindow::Max;

    setVerticalAnchor(m_verticalAnchor);

    QStringList applets = Settings::value(m_id, "applets", QStringList() ).toStringList();

    setApplets(applets);
}

void PanelWindow::resetApplets()
{
    removeApplets();

    QStringList applets = Settings::value(m_id, "applets", QStringList() ).toStringList();

    setApplets(applets);
    init();
    //repaint();
    updateLayout();
    updatePosition();

}
void PanelWindow::setApplets(QStringList applets)
{
    qDebug() << applets;

    QDir plugDir = QDir(qApp->applicationDirPath() + "/plugins");

    // If does not exists check the standard plugin directory
    if((!plugDir.exists()) && QDir("/usr/lib/hde/panel/plugins").exists())
    {
        plugDir.cd("/usr/lib/hde/panel/plugins");
    }

    //qDebug() << "Plugins directoy: " << plugDir.absolutePath();

    foreach(QString applet_name, applets)
    {
        loadApplet(applet_name, plugDir);
    }
}

void PanelWindow::loadApplet(QString applet_name, QDir &plugDir)
{
    QString applet_path = plugDir.absolutePath() + "/lib" + applet_name.toLower() + ".so";
    //qDebug() << "applet_path: " << applet_path;
    if(QLibrary::isLibrary(applet_path))
    {
        QPluginLoader loader(applet_path, this);
        AppletPlugin *appletplugin = qobject_cast<AppletPlugin *>(loader.instance());
        if (appletplugin)
        {

            Applet *applet = appletplugin->createApplet(this);
            if(applet)
            {

                m_applets.append(applet);
                //qDebug() << applet->objectName();

            }
            else
            {
                qDebug() << "applet '" << applet_name << "' not loaded";
            }

        }
        else
        {
            //qDebug() << "";
            //qDebug() << "BAD";
            qDebug() << loader.errorString();
            //qDebug() << "BAD";
            //qDebug() << "";
        }
        //loader.unload();
    }

}

void PanelWindow::removeApplets()
{
    qDebug() << "removing apllets";
    while(!m_applets.isEmpty())
    {
        delete m_applets.takeLast();
    }
}

PanelWindow::~PanelWindow()
{
    removeApplets();
	delete m_view;
	delete m_panelItem;
	delete m_scene;
}

bool PanelWindow::init()
{
	for(int i = 0; i < m_applets.size();)
	{
		if(!m_applets[i]->init())
			m_applets.remove(i);
		else
			i++;
	}
    return true;
}

void PanelWindow::setDockMode(bool dockMode)
{
    m_dockMode = dockMode;

	// FIXME: no DOCK effect for Qt5?
    setAttribute(Qt::WA_X11NetWmWindowTypeDock, m_dockMode);
#if QT_VERSION >= 0x050000
    setWindowFlags(Qt::BypassWindowManagerHint | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
#endif

	if (!m_dockMode)
	{
		// No need to reserve space anymore.
        X11Support::removeWindowProperty(winId(), "_NET_WM_STRUT");
        X11Support::removeWindowProperty(winId(), "_NET_WM_STRUT_PARTIAL");
	}

	// When in dock mode, panel should appear on all desktops.
	unsigned long desktop = m_dockMode ? 0xFFFFFFFF : 0;
	X11Support::setWindowPropertyCardinal(winId(), "_NET_WM_DESKTOP", desktop);

	updateLayout();
	updatePosition();
}

void PanelWindow::setScreen(int screen)
{
	m_screen = screen;
	updateLayout();
	updatePosition();
}

void PanelWindow::setHorizontalAnchor(Anchor horizontalAnchor)
{
	m_horizontalAnchor = horizontalAnchor;
	updatePosition();
}

void PanelWindow::setVerticalAnchor(Anchor verticalAnchor)
{
	m_verticalAnchor = verticalAnchor;
	updatePosition();
}

void PanelWindow::setOrientation(Orientation orientation)
{
	m_orientation = orientation;
}

void PanelWindow::setLayoutPolicy(LayoutPolicy layoutPolicy)
{
	m_layoutPolicy = layoutPolicy;
	updateLayout();
}

void PanelWindow::updatePosition()
{
    QRect screenGeometry = QApplication::desktop()->screenGeometry(m_screen);

    int left = 0;
    int right = 0;
    int top = 0;
    int buttom = 0;
    int leftStartY = 0;
    int leftEndY = 0;
    int rightStartY = 0;
    int rightEndY = 0;
    int topStartX = 0;
    int topEndX = 0;
    int bottomStartX = 0;
    int bottomEndX= 0;

    //top = screenGeometry.top() + height()-1;
    //topEndX = screenGeometry.x() + width()-1;

    //qDebug() << "screen: " << m_screen << " geometry: " << screenGeometry;

    X11Support::setStrut(winId(), // winid
                         left, right, top, buttom, // int left, int right, int top,  int bottom,
                         leftStartY, leftEndY,    // int leftStartY,   int leftEndY,
                         rightStartY, rightEndY,    // rightStartY,  int rightEndY,
                         topStartX, topEndX, // topStartX,    int topEndX,
                         bottomStartX, bottomEndX // bottomStartX, int bottomEndX
                     );

    if(!m_dockMode)
        return;


	int x;

    switch(m_horizontalAnchor)
	{
	case Min:
		x = screenGeometry.left();
        break;
	case Center:
		x = (screenGeometry.left() + screenGeometry.right() + 1 - width())/2;
        break;
	case Max:
		x = screenGeometry.right() - width() + 1;
        break;
	default:
		Q_ASSERT(false);
		break;
	}

	int y;

	switch(m_verticalAnchor)
	{
	case Min:
		y = screenGeometry.top();
		break;
	case Center:
		y = (screenGeometry.top() + screenGeometry.bottom() + 1 - height())/2;
		break;
	case Max:
		y = screenGeometry.bottom() - height() + 1;
		break;
	default:
		Q_ASSERT(false);
		break;
	}

	move(x, y);
    //setScreen(m_screen);
    qDebug() << "screen: " << screen() << " m_screen: " << m_screen;

    // @ToDo: More tests on multiple screens, it works with tow horizontal aligned screens has the same geometries.
	// Update reserved space.
    if(m_dockMode)
	{
        QVector<unsigned long> values; // Values for setting _NET_WM_STRUT_PARTIAL property.
        values.fill(0, 12);
        switch(m_horizontalAnchor)
		{
		case Min:
            values[0] = x + width();    // left
            values[4] = y;              // leftStartY
            values[5] = y + height();   // leftEndY
			break;
		case Max:
            values[1] = QApplication::desktop()->width() - x;   // right
            values[6] = y;                                      // rightStartY
            values[7] = y + height();                           // rightEndY
			break;
		default:
			break;
		}
		switch(m_verticalAnchor)
		{
		case Min:
            values[2] = y + height();       // Top
            values[8] = x;         // topStartX
            values[9] = x ;        // topEndX
			break;
		case Max:
            values[3] = QApplication::desktop()->height() - y; // buttom
            values[10] = x;                                    // bottomStartX
            values[11] = x ;                          // bottomEndX
			break;
		default:
			break;
		}
        /*
    if(m_dockMode)
    {
        QVector<unsigned long> values; // Values for setting _NET_WM_STRUT_PARTIAL property.
        values.fill(0, 12);
        switch(m_horizontalAnchor)
        {
        case Min:
            values[0] = x + width();
            values[4] = y;
            values[5] = y + height();
            break;
        case Max:
            values[1] = QApplication::desktop()->width() - x;
            values[6] = y;
            values[7] = y + height();
            break;
        default:
            break;
        }
        switch(m_verticalAnchor)
        {
        case Min:
            values[2] = y + height();
            values[8] = x;
            values[9] = x + width();
            break;
        case Max:
            values[3] = QApplication::desktop()->height() - y;
            values[10] = x;
            values[11] = x + width();
            break;
        default:
            break;
        }         */
		X11Support::setWindowPropertyCardinalArray(winId(), "_NET_WM_STRUT_PARTIAL", values);
        values.resize(4);
        X11Support::setWindowPropertyCardinalArray(winId(), "_NET_WM_STRUT", values);
	}

	// Update "blur behind" hint.
    QVector<unsigned long> values;
    values.resize(4);
    values[0] = 0;
    values[1] = 0;
    values[2] = width();
    values[3] = height();
    X11Support::setWindowPropertyCardinalArray(winId(), "_KDE_NET_WM_BLUR_BEHIND_REGION", values);
}

int PanelWindow::textBaseLine()
{
	QFontMetrics metrics(font());
	return (height() - metrics.height())/2 + metrics.ascent();
}

void PanelWindow::resizeEvent(QResizeEvent* event)
{
	m_view->resize(event->size());
	m_view->setSceneRect(0, 0, event->size().width(), event->size().height());
	updateLayout();
	updatePosition();
}

void PanelWindow::updateLayout()
{
	// TODO: Vertical orientation support.

	static const int spacing = adjustHardcodedPixelSize(4);

	if(m_layoutPolicy != Normal && !m_dockMode)
	{
		int desiredSize = 0;
		if(m_layoutPolicy == AutoSize)
		{
			for(int i = 0; i < m_applets.size(); i++)
			{
				if(m_applets[i]->desiredSize().width() >= 0)
					desiredSize += m_applets[i]->desiredSize().width();
				else
					desiredSize += 64; // Spacer applets don't really make sense on auto-size panel.
			}
			desiredSize += spacing*(m_applets.size() - 1);
			if(desiredSize < 0)
				desiredSize = 0;
		}
		if(m_layoutPolicy == FillSpace)
		{
			QRect screenGeometry = QApplication::desktop()->screenGeometry(m_screen);
			desiredSize = screenGeometry.width();
		}

		if(desiredSize != width())
			resize(desiredSize, height());
	}

	// Get total amount of space available for "spacer" applets (that take all available free space).
	int freeSpace = width() - spacing*(m_applets.size() - 1);
	int numSpacers = 0;
	for(int i = 0; i < m_applets.size(); i++)
	{
		if(m_applets[i]->desiredSize().width() >= 0)
			freeSpace -= m_applets[i]->desiredSize().width();
		else
			numSpacers++;
	}
	int spaceForOneSpacer = numSpacers > 0 ? (freeSpace/numSpacers) : 0;

	// Calculate rectangles for each applet.
	int spacePos = 0;
	for(int i = 0; i < m_applets.size(); i++)
	{
		QPoint appletPosition(spacePos, 0);
		QSize appletSize = m_applets[i]->desiredSize();

		if(appletSize.width() < 0)
		{
			if(numSpacers > 1)
			{
				appletSize.setWidth(spaceForOneSpacer);
				freeSpace -= spaceForOneSpacer;
				numSpacers--;
			}
			else
			{
				appletSize.setWidth(freeSpace);
				freeSpace = 0;
				numSpacers--;
			}
		}

		appletSize.setHeight(height());

		m_applets[i]->setPosition(appletPosition);
		m_applets[i]->setSize(appletSize);

		spacePos += appletSize.width() + spacing;
	}
}

void PanelWindow::showPanelContextMenu(const QPoint& point)
{
    //QMenu menu;
    HPopupMenu menu;
    /*
     * -= Panel =-
     * Configure panel
     * Add Panel
     * Remove Panel
     */
    menu.addTitle("Panel");
    menu.addAction(QIcon::fromTheme("preferences-desktop"), "Configure Panel", this, SLOT(showConfigurationDialog()));

    menu.addAction(QIcon::fromTheme("list-add"), "Add Panel", QApplication::instance(), SLOT(addPanel()));
    menu.addAction(QIcon::fromTheme("list-remove"), "Remove Panel", this, SLOT(removePanel()));

    menu.exec(pos() + point);
}


void PanelWindow::showConfigurationDialog()
{
    PanelSettings *panelSettings = new PanelSettings(m_id);
    panelSettings->setPanelWindow(this);
    if(panelSettings->exec())
    {

    }
    delete panelSettings;
}


void PanelWindow::removePanel()
{

    ((PanelApplication *)QApplication::instance())->removePanel(m_id);
}

void PanelWindow::setFontName(const QString& fontName)
{
    QFont m_panelFont;
    QString m_fontName = fontName;
    if(m_fontName != "default")
    {
        // Parse font name. Example: "Droid Sans 11".
        int lastSpacePos = m_fontName.lastIndexOf(' ');
        // Should have at least one space, otherwise string is malformed, keep default in that case.
        if(lastSpacePos != -1)
        {
            int fontSize = m_fontName.mid(lastSpacePos).toInt();
            QString fontFamily = m_fontName;
            fontFamily.truncate(lastSpacePos);
            m_panelFont = QFont(fontFamily, fontSize);
        }
    }
    else
    {
        m_panelFont = QApplication::font();
    }

    this->setFont(m_panelFont);

    for(int i = 0; i < m_applets.size();)
    {
        (m_applets.at(i))->fontChanged();
        i++;
    }
}
