/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * This Files has been imported to hde from qtpanel
 *
 * Copyright: 2015-2016 Haydar Alkaduhimi
 * Copyright: 2014 Leslie Zhai <xiang.zhai@i-soft.com.cn>
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

#include <QtGui/QPainter>
#if QT_VERSION >= 0x050000
#include <QGraphicsSceneMouseEvent>
#else
#include <QtGui/QGraphicsSceneMouseEvent>
#endif

#include "trayapplet.h"
#include "panelapplication.h"
#include "panelwindow.h"
#include "x11support.h"
#include "dpisupport.h"

TrayItem::TrayItem(TrayApplet* trayApplet, unsigned long window)
	: m_trayApplet(trayApplet), m_window(window)
{
	setParentItem(m_trayApplet);

	// This is needed for non-composited tray icons, otherwise we'll get a BadMatch on reparent attempt.
	// Doesn't affect composited icons.
	X11Support::setWindowBackgroundBlack(m_window);

	X11Support::registerForTrayIconUpdates(m_window);
	X11Support::reparentWindow(m_window, m_trayApplet->panelWindow()->winId());
	X11Support::resizeWindow(m_window, m_trayApplet->iconSize(), m_trayApplet->iconSize());
	X11Support::redirectWindow(m_window);
	X11Support::mapWindow(m_window);

	m_trayApplet->registerTrayItem(this);
}

TrayItem::~TrayItem()
{
	X11Support::reparentWindow(m_window, X11Support::rootWindow());

	m_trayApplet->unregisterTrayItem(this);
}

void TrayItem::setPosition(const QPoint& position)
{
	setPos(position.x(), position.y());
	X11Support::moveWindow(m_window,
		static_cast<int>(m_trayApplet->pos().x()) + position.x() + m_size.width()/2 - m_trayApplet->iconSize()/2,
		static_cast<int>(m_trayApplet->pos().y()) + position.y() + m_size.height()/2 - m_trayApplet->iconSize()/2
	);
}

void TrayItem::setSize(const QSize& size)
{
	m_size = size;
	update();
}

QRectF TrayItem::boundingRect() const
{
	return QRectF(0.0, 0.0, m_size.width() - 1, m_size.height() - 1);
}

void TrayItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    // Background.
	painter->setPen(Qt::NoPen);
	QPointF center(m_size.width()/2.0, m_size.height()/2.0);
	QRadialGradient gradient(center, m_size.width()/2.0, center);
	gradient.setColorAt(0.0, QColor(255, 255, 255, 80));
	gradient.setColorAt(1.0, QColor(255, 255, 255, 0));
	painter->setBrush(QBrush(gradient));
	painter->drawRect(boundingRect());

	// Icon itself.
    QPixmap pix = X11Support::getWindowPixmap(m_window);
    painter->drawPixmap(m_size.width()/2 - m_trayApplet->iconSize()/2, m_size.height()/2 - m_trayApplet->iconSize()/2, pix);
}

TrayApplet::TrayApplet(PanelWindow* panelWindow)
	: Applet(panelWindow), m_initialized(false), m_iconSize(adjustHardcodedPixelSize(24)), m_spacing(adjustHardcodedPixelSize(4))
{
    setObjectName("Tray");
}

TrayApplet::~TrayApplet()
{
    qDebug() << "Deleting tray";
    close();
}

void TrayApplet::setPanelWindow(PanelWindow *panelWindow)
{
    Applet::setPanelWindow(panelWindow);
}

void TrayApplet::close()
{
	if(m_initialized)
		X11Support::freeSystemTray();

	while(!m_trayItems.isEmpty())
	{
		delete m_trayItems[m_trayItems.size() - 1];
	}
}

bool TrayApplet::init()
{
	m_initialized = X11Support::makeSystemTray(m_panelWindow->winId());

	if(!m_initialized)
	{
        qDebug() << "Another tray is active.";
		// Another tray is active.
		return false;
	}

	connect(X11Support::instance(), SIGNAL(windowClosed(ulong)), this, SLOT(windowClosed(ulong)));
	connect(X11Support::instance(), SIGNAL(windowReconfigured(ulong,int,int,int,int)), this, SLOT(windowReconfigured(ulong,int,int,int,int)));
	connect(X11Support::instance(), SIGNAL(windowDamaged(ulong)), this, SLOT(windowDamaged(ulong)));
    connect(X11Support::instance(), SIGNAL(clientMessageReceived(ulong,ulong,void*)), this, SLOT(clientMessageReceived(ulong,ulong,void*)));

	return true;
}

QSize TrayApplet::desiredSize()
{
	int desiredWidth = (m_iconSize + m_spacing)*m_trayItems.size() - m_spacing;
	if(desiredWidth < 0)
		desiredWidth = 0;
	return QSize(desiredWidth, -1);
}

void TrayApplet::registerTrayItem(TrayItem* trayItem)
{
	m_trayItems.append(trayItem);
	m_panelWindow->updateLayout();
}

void TrayApplet::unregisterTrayItem(TrayItem* trayItem)
{
	m_trayItems.remove(m_trayItems.indexOf(trayItem));
	m_panelWindow->updateLayout();
}

void TrayApplet::layoutChanged()
{
	updateLayout();
}

void TrayApplet::clientMessageReceived(unsigned long window, unsigned long atom, void* data)
{
    Q_UNUSED(window)

    if(atom == X11Support::atom("_NET_SYSTEM_TRAY_OPCODE"))
    {
        //qDebug()<< "tray";
        u_int32_t *l = reinterpret_cast<u_int32_t *>(data);
        //qDebug()<< l[1];
        //qDebug()<< l[2];
        //qDebug()<< window;
        if(l[1] == 0) // TRAY_REQUEST_DOCK
        {
            //qDebug()<< "tray request dock";
            for(int i = 0; i < m_trayItems.size(); i++)
			{
                if(m_trayItems[i]->window() == l[2])
                    return; // Already added.
			}
            new TrayItem(this, l[2]);
        }
    }
}

void TrayApplet::windowClosed(unsigned long window)
{
	for(int i = 0; i < m_trayItems.size(); i++)
	{
		if(m_trayItems[i]->window() == window)
		{
			delete m_trayItems[i];
			break;
		}
	}
}

void TrayApplet::windowReconfigured(unsigned long window, int x, int y, int width, int height)
{
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(width)
    Q_UNUSED(height)

	for(int i = 0; i < m_trayItems.size(); i++)
	{
		if(m_trayItems[i]->window() == window)
		{
			X11Support::resizeWindow(window, m_iconSize, m_iconSize);
			break;
		}
	}
}

void TrayApplet::windowDamaged(unsigned long window)
{
	for(int i = 0; i < m_trayItems.size(); i++)
	{
		if(m_trayItems[i]->window() == window)
		{
			m_trayItems[i]->update();
			break;
		}
	}
}

void TrayApplet::updateLayout()
{
	int currentPosition = 0;
	for(int i = 0; i < m_trayItems.size(); i++)
	{
		m_trayItems[i]->setSize(QSize(m_iconSize, m_size.height()));
		m_trayItems[i]->setPosition(QPoint(currentPosition, 0));
		currentPosition += m_iconSize + m_spacing;
	}
}
