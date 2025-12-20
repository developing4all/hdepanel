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
#include "sni.h"
#include <QTimer>
#include <QDBusMetaType>

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
    // Guard X11 calls during shutdown or on non-X11 platforms
#if QT_VERSION >= 0x050000
    const bool isX11 = qApp && qApp->platformName().toLower().contains("xcb");
#else
    const bool isX11 = true;
#endif
    if (isX11 && m_window) {
        X11Support::reparentWindow(m_window, X11Support::rootWindow());
    }

    // Avoid double-removal during TrayApplet destruction
    if (m_trayApplet && !m_trayApplet->isDestroying()) {
        m_trayApplet->unregisterTrayItem(this);
    }
}

void TrayItem::setPosition(const QPoint& position)
{
	setPos(position.x(), position.y());
	
	// Only update X11 window position if we have a valid size
	if (m_size.width() > 0 && m_size.height() > 0) {
		X11Support::moveWindow(m_window,
			static_cast<int>(m_trayApplet->pos().x()) + position.x() + m_size.width()/2 - m_trayApplet->iconSize()/2,
			static_cast<int>(m_trayApplet->pos().y()) + position.y() + m_size.height()/2 - m_trayApplet->iconSize()/2
		);
	}
}

void TrayItem::setSize(const QSize& size)
{
	if (m_size == size)
		return;
		
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

	// Icon itself.
    QPixmap pix = X11Support::getWindowPixmap(m_window);
    if (!pix.isNull()) {
        // Background - only if icon exists.
	painter->setPen(Qt::NoPen);
	QPointF center(m_size.width()/2.0, m_size.height()/2.0);
	QRadialGradient gradient(center, m_size.width()/2.0, center);
	gradient.setColorAt(0.0, QColor(255, 255, 255, 80));
	gradient.setColorAt(1.0, QColor(255, 255, 255, 0));
	painter->setBrush(QBrush(gradient));
	painter->drawRect(boundingRect());

        // Center the icon using its actual size
        int iconX = m_size.width()/2 - pix.width()/2;
        int iconY = m_size.height()/2 - pix.height()/2;
        painter->drawPixmap(iconX, iconY, pix);
    }
}

void TrayItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    event->ignore();
}

SniTrayItem::SniTrayItem(TrayApplet* trayApplet, SniItemProxy* sniItem)
	: m_trayApplet(trayApplet), m_sniItem(sniItem)
{
	setParentItem(m_trayApplet);
	m_trayApplet->registerSniTrayItem(this);
	m_size = QSize(m_trayApplet->iconSize(), m_trayApplet->iconSize());

	if (m_sniItem) {
		connect(m_sniItem, &SniItemProxy::changed, this, &SniTrayItem::updateIcon);
		updateIcon();
	}
}

SniTrayItem::~SniTrayItem()
{
	// Avoid double-removal during TrayApplet destruction
	if (m_trayApplet && !m_trayApplet->isDestroying()) {
		m_trayApplet->unregisterSniTrayItem(this);
	}
}

void SniTrayItem::updateIcon()
{
	if (m_sniItem) {
		m_cachedIcon = m_sniItem->icon();
	}
	update();
}

void SniTrayItem::setPosition(const QPoint& position)
{
	setPos(position.x(), position.y());
}

void SniTrayItem::setSize(const QSize& size)
{
	m_size = size;
	update();
}

QRectF SniTrayItem::boundingRect() const
{
	return QRectF(0.0, 0.0, m_size.width() - 1, m_size.height() - 1);
}

void SniTrayItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Q_UNUSED(option)
	Q_UNUSED(widget)

	// Icon itself.
	if (m_sniItem) {
		if (!m_cachedIcon.isNull()) {
			QPixmap pix = m_cachedIcon.pixmap(m_trayApplet->iconSize(), m_trayApplet->iconSize());
			if (!pix.isNull()) {
                // Background - only if icon exists.
                painter->setPen(Qt::NoPen);
                QPointF center(m_size.width()/2.0, m_size.height()/2.0);
                QRadialGradient gradient(center, m_size.width()/2.0, center);
                gradient.setColorAt(0.0, QColor(255, 255, 255, 80));
                gradient.setColorAt(1.0, QColor(255, 255, 255, 0));
                painter->setBrush(QBrush(gradient));
                painter->drawRect(boundingRect());

				// Center the icon using its actual size
				int iconX = m_size.width()/2 - pix.width()/2;
				int iconY = m_size.height()/2 - pix.height()/2;
				painter->drawPixmap(iconX, iconY, pix);
			}
		}
	}
}

void SniTrayItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (!m_sniItem) return;
    QPoint globalPos = event->screenPos();
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    if (trayDebug) {
        qDebug() << "SniTrayItem::mousePressEvent - button:" << event->button() << "pos:" << globalPos;
    }
    if (event->button() == Qt::LeftButton) {
        // Many Ayatana/indicator items only expose a DBusMenu; show it on left-click if available.
        if (!(m_sniItem->hasMenu() && m_sniItem->popupMenu(globalPos.x(), globalPos.y()))) {
            m_sniItem->activate(globalPos.x(), globalPos.y());
        }
    } else if (event->button() == Qt::RightButton) {
        // Prefer DBusMenu if exported; otherwise fall back to the SNI ContextMenu call.
        if (!(m_sniItem->hasMenu() && m_sniItem->popupMenu(globalPos.x(), globalPos.y()))) {
            m_sniItem->contextMenu(globalPos.x(), globalPos.y());
        }
    } else if (event->button() == Qt::MiddleButton) {
        m_sniItem->secondaryActivate(globalPos.x(), globalPos.y());
	}
    event->accept();
}

TrayApplet::TrayApplet(PanelWindow* panelWindow)
	: Applet(panelWindow), m_initialized(false), m_iconSize(adjustHardcodedPixelSize(24)), m_spacing(adjustHardcodedPixelSize(4))
{
    qDBusRegisterMetaType<SniPixmap>();
    qDBusRegisterMetaType<SniPixmapList>();

    setObjectName("Tray");
    // Cache atoms used in hot-path event processing (avoid repeated XInternAtom).
    m_trayOpcodeAtom = X11Support::atom("_NET_SYSTEM_TRAY_OPCODE");
    m_managerAtom = X11Support::atom("MANAGER");
    m_systemTrayAtom = X11Support::atom("_NET_SYSTEM_TRAY_S" + QString::number(0));

    // Initialize SNI watcher on both X11 and Wayland
    // Many modern applications (like Unity Hub) use SNI even on X11
    m_sniWatcher = new SniWatcher(this);
    connect(m_sniWatcher, &SniWatcher::itemAdded, this, [this](SniItemProxy* item){
        static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
		if (item) {
            if (trayDebug) {
                qDebug() << "SNI item added:" << item->id();
            }
			new SniTrayItem(this, item);
		}
		updateLayout();
		update();
	});
    connect(m_sniWatcher, &SniWatcher::itemRemoved, this, [this](const QString &id){
        static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
        if (trayDebug) {
            qDebug() << "SNI item removed:" << id;
        }
		// Find and remove the corresponding SNI tray item
		for(int i = 0; i < m_sniTrayItems.size(); i++) {
			if(m_sniTrayItems[i]->sniItem() && m_sniTrayItems[i]->sniItem()->id() == id) {
				delete m_sniTrayItems[i];
				break;
			}
		}
		updateLayout();
		update();
	});
	// Add any existing SNI items that were already registered
	const QMap<QString, SniItemProxy*>& existingItems = m_sniWatcher->items();
	for(auto it = existingItems.constBegin(); it != existingItems.constEnd(); ++it) {
		if (it.value()) {
            static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
            if (trayDebug) {
                qDebug() << "Adding existing SNI item:" << it.value()->id();
            }
			new SniTrayItem(this, it.value());
		}
	}
	
	// Register as StatusNotifierHost. This is required by some applications (like KDE apps)
	// before they will show their icons.
    // Use a small delay to ensure SniWatcher is fully registered
    QTimer::singleShot(100, this, [this]() {
        QDBusMessage msg = QDBusMessage::createMethodCall("org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher", 
                                                          "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierHost");
        msg << "hdepanel";
        QDBusConnection::sessionBus().call(msg);
    });
}

TrayApplet::~TrayApplet()
{
    m_destroying = true;
    close();
}

void TrayApplet::setPanelWindow(PanelWindow *panelWindow)
{
    Applet::setPanelWindow(panelWindow);
}

void TrayApplet::close()
{
    // IMPORTANT: PanelWindow::removeApplets() calls applet->close() before deleting the applet.
    // If we don't mark ourselves as destroying here, TrayItem/SniTrayItem destructors will call
    // back into TrayApplet (unregister*) which triggers PanelWindow::updateLayout() during teardown,
    // causing re-entrancy and potential freezes.
    m_destroying = true;

    // CRITICAL: Disconnect all signals from SniWatcher BEFORE deleting items.
    // Otherwise, SniWatcher signals (itemAdded/itemRemoved) can fire during cleanup
    // and call updateLayout() even though we're destroying, causing freezes.
    if (m_sniWatcher) {
        disconnect(m_sniWatcher, nullptr, this, nullptr);
    }

#if QT_VERSION >= 0x050000
    if(m_initialized && qApp->platformName().toLower().contains("xcb"))
#else
	if(m_initialized)
#endif
		X11Support::freeSystemTray();

    // Delete SNI tray items FIRST (before deleting SniWatcher)
    // This prevents SniTrayItem destructors from accessing deleted SniWatcher
    while(!m_sniTrayItems.isEmpty()) {
        SniTrayItem* item = m_sniTrayItems.takeLast();
        if (item) {
            // Clear the reference to prevent callbacks during deletion
            item->setParentItem(nullptr);
            delete item;
        }
    }
    
    // Delete tray items safely
    while(!m_trayItems.isEmpty()) {
        TrayItem* item = m_trayItems.takeLast();
        if (item) {
            item->setParentItem(nullptr);
            delete item;
        }
    }
    
    // Delete SniWatcher last (it's a child, but explicit deletion ensures cleanup order)
    // Since we disconnected signals above, no callbacks will fire during deletion
    if (m_sniWatcher) {
        delete m_sniWatcher;
        m_sniWatcher = nullptr;
    }
}

bool TrayApplet::init()
{
    // System tray is X11-specific; skip on Wayland
#if QT_VERSION >= 0x050000
    if (qApp->platformName().toLower().contains("xcb") == false) {
        // Wayland path: rely on SNI via DBus; report initialized to keep layout
        m_initialized = true;
        return true;
    }
#endif
    m_initialized = X11Support::makeSystemTray(m_panelWindow->winId());

	if(!m_initialized)
	{
        qDebug() << "Another tray is active - this might be from a previous instance";
        // This can happen if a previous instance didn't clean up properly
        // Try to continue anyway - the messages should still work
        qDebug() << "Continuing with tray initialization anyway";
        // Don't return false - try to work with existing setup
	}

	// Register for client messages on the tray window
	// Note: ClientMessage events are always delivered, but we register for other events too
	X11Support::registerForWindowPropertyChanges(m_panelWindow->winId());
	X11Support::registerForWindowStructureNotify(m_panelWindow->winId());

    // Cache ids we compare against in the client-message hot path
    m_trayWindowId = m_panelWindow->winId();
	
	// Also register for events on root window to catch MANAGER messages
	X11Support::registerForWindowStructureNotify(X11Support::rootWindow());

	connect(X11Support::instance(), SIGNAL(windowClosed(ulong)), this, SLOT(windowClosed(ulong)));
	connect(X11Support::instance(), SIGNAL(windowReconfigured(ulong,int,int,int,int)), this, SLOT(windowReconfigured(ulong,int,int,int,int)));
	connect(X11Support::instance(), SIGNAL(windowDamaged(ulong)), this, SLOT(windowDamaged(ulong)));
    connect(X11Support::instance(), SIGNAL(clientMessageReceived(ulong,ulong,void*)), this, SLOT(clientMessageReceived(ulong,ulong,void*)));

	qDebug() << "Tray applet initialized on window" << m_panelWindow->winId();
	
	// Give applications time to respond to the MANAGER message and re-send dock requests
	// Some applications might need a moment to process the tray availability notification
	QTimer::singleShot(500, this, [this]() {
		qDebug() << "Checking for tray icons that may have registered during startup";
		// The MANAGER message should have triggered applications to send TRAY_REQUEST_DOCK
		// If they haven't by now, they might not support re-registration
	});
	
	return true;
}

QSize TrayApplet::desiredSize()
{
    if (!m_panelWindow)
        return QSize(0, -1);

    const PanelWindow::Position pos = m_panelWindow->position();
    const bool horizontal = (pos == PanelWindow::Top || pos == PanelWindow::Bottom);

    // Panel "thickness" drives tray tile size.
    int thickness = horizontal ? m_panelWindow->panelHeight()
                               : m_panelWindow->panelWidth();
    if (thickness <= 0)
        thickness = adjustHardcodedPixelSize(24);

    // Margin inside each tray tile (matches your other applets)
    const int tileMargin = adjustHardcodedPixelSize(2);

    // icon size is thickness minus margins, capped at maxIcon.
    int icon = thickness - 2 * tileMargin;

    const int minIcon = adjustHardcodedPixelSize(12);
    const int maxIcon = adjustHardcodedPixelSize(24);
    m_iconSize = qBound(minIcon, icon, maxIcon);

    // Tile side is based on icon size for compactness.
    const int tileSide = m_iconSize + 2 * tileMargin;

    m_spacing = adjustHardcodedPixelSize(2);

    const int totalItems = m_trayItems.size() + m_sniTrayItems.size();
    if (totalItems <= 0) {
        // Don't waste space when empty
        return horizontal ? QSize(0, -1) : QSize(-1, 0);
    }

    // Length along the flow direction
    int desiredLen = tileSide * totalItems + m_spacing * (totalItems - 1);
    if (desiredLen < 0) desiredLen = 0;

    return horizontal ? QSize(desiredLen, -1)    // width grows on top/bottom
                      : QSize(-1, desiredLen);   // height grows on left/right
}

void TrayApplet::registerTrayItem(TrayItem* trayItem)
{
	m_trayItems.append(trayItem);
	if (!m_destroying && m_panelWindow) {
		m_panelWindow->updateLayout();
	}
}

void TrayApplet::unregisterTrayItem(TrayItem* trayItem)
{
    int idx = m_trayItems.indexOf(trayItem);
    if (idx >= 0 && idx < m_trayItems.size()) {
        m_trayItems.removeAt(idx);
    }
	// Don't call updateLayout() during destruction - it's already blocked by PanelWindow guards
	if (!m_destroying && m_panelWindow) {
		m_panelWindow->updateLayout();
	}
}

void TrayApplet::registerSniTrayItem(SniTrayItem* trayItem)
{
	m_sniTrayItems.append(trayItem);
	if (!m_destroying && m_panelWindow) {
		m_panelWindow->updateLayout();
	}
}

void TrayApplet::unregisterSniTrayItem(SniTrayItem* trayItem)
{
    int idx = m_sniTrayItems.indexOf(trayItem);
    if (idx >= 0 && idx < m_sniTrayItems.size()) {
        m_sniTrayItems.removeAt(idx);
    }
	// Don't call updateLayout() during destruction - it's already blocked by PanelWindow guards
	if (!m_destroying && m_panelWindow) {
		m_panelWindow->updateLayout();
	}
}

void TrayApplet::layoutChanged()
{
	updateLayout();
}

void TrayApplet::clientMessageReceived(unsigned long window, unsigned long atom, void* data)
{
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    const unsigned long trayOpcodeAtom = m_trayOpcodeAtom ? m_trayOpcodeAtom : X11Support::atom("_NET_SYSTEM_TRAY_OPCODE");
    const unsigned long managerAtom = m_managerAtom ? m_managerAtom : X11Support::atom("MANAGER");
    const unsigned long trayWindow = m_trayWindowId ? m_trayWindowId : (m_panelWindow ? m_panelWindow->winId() : 0);

    // Fast path: ignore irrelevant client messages completely.
    // Some desktops send a *lot* of ClientMessage traffic; we only care about tray-related atoms.
    if (atom != trayOpcodeAtom && atom != managerAtom) {
        return;
    }
    
    u_int32_t *l = reinterpret_cast<u_int32_t *>(data);
    
    // Check if this is a tray opcode message (sent directly to tray window)
    if(atom == trayOpcodeAtom && window == trayWindow)
    {
        if (trayDebug) {
            qDebug() << "Received _NET_SYSTEM_TRAY_OPCODE message on tray window";
            qDebug() << "  Data[0]:" << l[0] << "Opcode:" << l[1] << "Window:" << l[2];
        }
        
        if(l[1] == 0) // TRAY_REQUEST_DOCK
        {
            if (trayDebug) {
                qDebug() << "TRAY_REQUEST_DOCK for window" << l[2];
            }
            for(int i = 0; i < m_trayItems.size(); i++)
			{
                if(m_trayItems[i]->window() == l[2]) {
                    if (trayDebug) {
                        qDebug() << "Window already added, skipping";
                    }
                    return; // Already added.
                }
			}
            if (trayDebug) {
                qDebug() << "Creating new TrayItem for window" << l[2];
            }
            new TrayItem(this, l[2]);
        }
        return;
    }
    
    // Check for MANAGER messages on root window (for applications discovering the tray)
    if(atom == managerAtom && window == X11Support::rootWindow())
    {
        if (trayDebug) {
            qDebug() << "Received MANAGER message on root window";
        }
        const unsigned long systemTrayAtom = m_systemTrayAtom ? m_systemTrayAtom : X11Support::atom("_NET_SYSTEM_TRAY_S" + QString::number(0));
        if (trayDebug) {
            qDebug() << "  Manager data[0]:" << l[0] << "data[1]:" << l[1] << "systemTrayAtom:" << systemTrayAtom << "data[2]:" << l[2];
        }
        if(l[1] == systemTrayAtom) {
            if (trayDebug) {
                qDebug() << "Manager message for system tray - tray window:" << l[2];
            }
        }
        return;
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
    if (!m_panelWindow)
        return;

    const PanelWindow::Position pos = m_panelWindow->position();
    const bool horizontal = (pos == PanelWindow::Top || pos == PanelWindow::Bottom);

    int thickness = horizontal ? m_panelWindow->panelHeight()
                               : m_panelWindow->panelWidth();
    if (thickness <= 0)
        thickness = adjustHardcodedPixelSize(24);

    const int tileMargin = adjustHardcodedPixelSize(2);

    int icon = thickness - 2 * tileMargin;
    const int minIcon = adjustHardcodedPixelSize(12);
    const int maxIcon = adjustHardcodedPixelSize(24);
    m_iconSize = qBound(minIcon, icon, maxIcon);

    const int tileSide = m_iconSize + 2 * tileMargin;

    if (m_spacing <= 0)
        m_spacing = adjustHardcodedPixelSize(2);

    // Fallback applet size if not assigned yet
    int appW = m_size.width();
    int appH = m_size.height();
    if (appW <= 0) appW = horizontal ? tileSide : thickness;
    if (appH <= 0) appH = horizontal ? thickness : tileSide;

    int current = 0;

#if QT_VERSION >= 0x050000
    const bool isX11 = qApp && qApp->platformName().toLower().contains("xcb");
#else
    const bool isX11 = true;
#endif

    if (horizontal) {
        // Left -> Right
        for (int i = 0; i < m_trayItems.size(); ++i) {
            TrayItem* it = m_trayItems[i];
            it->setSize(QSize(tileSide, appH));
            if (isX11) X11Support::resizeWindow(it->window(), m_iconSize, m_iconSize);
            it->setPosition(QPoint(current, 0));
            current += tileSide + m_spacing;
        }
        for (int i = 0; i < m_sniTrayItems.size(); ++i) {
            SniTrayItem* it = m_sniTrayItems[i];
            it->setSize(QSize(tileSide, appH));
            it->setPosition(QPoint(current, 0));
            current += tileSide + m_spacing;
        }
    } else {
        // Top -> Bottom
        for (int i = 0; i < m_trayItems.size(); ++i) {
            TrayItem* it = m_trayItems[i];
            it->setSize(QSize(appW, tileSide));
            if (isX11) X11Support::resizeWindow(it->window(), m_iconSize, m_iconSize);
            it->setPosition(QPoint(0, current));
            current += tileSide + m_spacing;
        }
        for (int i = 0; i < m_sniTrayItems.size(); ++i) {
            SniTrayItem* it = m_sniTrayItems[i];
            it->setSize(QSize(appW, tileSide));
            it->setPosition(QPoint(0, current));
            current += tileSide + m_spacing;
        }
    }

    update();
}
