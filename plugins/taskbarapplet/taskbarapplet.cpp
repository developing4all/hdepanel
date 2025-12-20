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

#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtCore/QCoreApplication>
#include <QtGui/QPainter>
#include <QtGui/QFontMetrics>
#if QT_VERSION >= 0x050000
#include <QGraphicsScene>                                                    
#include <QGraphicsSceneMouseEvent>                                          
#include <QMenu>
#else
#include <QtGui/QGraphicsScene>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QMenu>
#endif
#include <QDebug>
#include <QScreen>
#include <QFileInfo>

#include "taskbarapplet.h"
#include "taskbaritem.h"
#include "client.h"
#include "waylandclient.h"
#include "textgraphicsitem.h"
#include "panelapplication.h"
#include "panelwindow.h"
#include "x11support.h"
#include "waylandsupport.h"
#include "animationutils.h"
#include "dpisupport.h"
#include <taskbarconfigurationdialog.h>

#include "../../lib/hpopupmenu.h"
#include "../../lib/desktopdatastore.h"
#include "../../lib/unifiediconservice.h"

#include <settings.h>
#include <QColor>
#include <QLocale>

// Include Xlib locally to access XSelectInput and event masks without leaking macros globally
#include <X11/Xlib.h>

TaskBarApplet::TaskBarApplet(PanelWindow* panelWindow)
	: Applet(panelWindow), m_dragging(false), m_initialized(false), m_destroying(false), m_updatingLayout(false), m_waylandSupport(nullptr),
	  m_buttonColor(255, 255, 255), m_buttonColorTransparency(80),
	  m_focusColor(0, 0, 0), m_focusColorTransparency(128)
{
    setObjectName("Dock");
    setExpandable(true);

    // Register for notifications about window property changes (X11 only).
#if QT_VERSION >= 0x050000
    if (qApp->platformName().toLower().contains("xcb") && X11Support::instance()) {
#endif
        connect(X11Support::instance(), SIGNAL(windowPropertyChanged(ulong,ulong)), this, SLOT(windowPropertyChanged(ulong,ulong)));
        connect(X11Support::instance(), SIGNAL(windowReconfigured(ulong, int, int, int, int)), this, SLOT(windowReconfigured(ulong, int, int, int, int)));
        connect(X11Support::instance(), SIGNAL(windowClosed(ulong)), this, SLOT(windowClosed(ulong)));
#if QT_VERSION >= 0x050000
    }
#endif

    // Initialize Wayland support (deferred). We intentionally avoid doing Wayland
    // roundtrips during plugin loading, because that can re-enter Qt internals and
    // has caused heap corruption/crashes on some setups.
    if (!qEnvironmentVariableIsSet("HDE_DISABLE_TASKBAR_WAYLAND")) {
        m_waylandSupport = new WaylandSupport(this);
        // Small delay gives QtWayland/layer-shell time to finish initial setup
        // (seat/keymap/etc.) before we create our own Wayland connection.
        QTimer::singleShot(200, this, [this]() {
            if (!m_waylandSupport) return;
            if (m_destroying) return;
            if (!m_waylandSupport->isAvailable()) return;
            if (!m_waylandSupport->initialize()) {
                delete m_waylandSupport;
                m_waylandSupport = nullptr;
                return;
            }
            connect(m_waylandSupport, &WaylandSupport::windowsUpdated, this, &TaskBarApplet::updateWaylandClientList, Qt::QueuedConnection);
            connect(m_waylandSupport, &WaylandSupport::windowClosed, this, &TaskBarApplet::onWaylandWindowClosed, Qt::QueuedConnection);
        });
    }
}

TaskBarApplet::~TaskBarApplet()
{
    close();
}

void TaskBarApplet::close()
{
    // Set destroying flag to prevent callbacks
    m_destroying = true;
    
    // Stop Wayland support updates
    if (m_waylandSupport) {
        delete m_waylandSupport;
        m_waylandSupport = nullptr;
    }
    
    // Remove ALL TaskBarItems from the scene first
    QGraphicsScene* panelScene = scene();
    for (TaskBarItem* item : m_dockItems) {
        if (item && panelScene && item->scene() == panelScene) {
            try {
                panelScene->removeItem(item);
            } catch (...) {
                // Ignore exceptions during cleanup
            }
        }
    }
    
    // TaskBarItems are owned by the applet; delete them here.
    QVector<TaskBarItem*> itemsToDelete = m_dockItems;
    m_dockItems.clear();
    for (TaskBarItem* item : itemsToDelete) {
        if (!item) continue;
        if (panelScene && item->scene() == panelScene) {
            panelScene->removeItem(item);
        }
        delete item;
    }
    
    QList<WaylandClient*> waylandClientsToDelete = m_waylandClients.values();
    m_waylandClients.clear();
    
    QMap<unsigned long, Client*> x11ClientsToDelete = m_clients;
    m_clients.clear();
    
    QList<Client*> loopClientsToDelete = m_in_loop;
    m_in_loop.clear();
    
    // Delete clients
    for (WaylandClient* client : waylandClientsToDelete) {
        if (client) {
            try {
                delete client;
            } catch (...) {
                // Ignore exceptions during cleanup
            }
        }
    }
    
    for (auto it = x11ClientsToDelete.begin(); it != x11ClientsToDelete.end(); ++it) {
        try {
            delete it.value();
        } catch (...) {
            // Ignore exceptions during cleanup
        }
    }
    
    for (Client* client : loopClientsToDelete) {
        try {
            delete client;
        } catch (...) {
            // Ignore exceptions during cleanup
        }
    }
}

void TaskBarApplet::onWaylandWindowClosed(const WaylandWindow& window)
{
    if (m_destroying) {
        return;
    }
    void* surface = window.surface;
    if (!surface) {
        return;
    }

    WaylandClient* client = m_waylandClients.value(surface, nullptr);
    if (!client) {
        return;
    }

    TaskBarItem* item = client->dockItem();
    client->clearDockItem();

    m_waylandClients.remove(surface);

    // If the item was already deleted/removed from our list, don't touch it.
    if (item && !m_dockItems.contains(item)) {
        item = nullptr;
    }

    if (item) {
        item->removeWaylandClient(client);
        if (item->shouldDelete()) {
            unregisterTaskBarItem(item);
            if (scene() && item->scene() == scene()) {
                scene()->removeItem(item);
            }
            delete item;
        }
    }
    delete client;

    updateLayout();
    if (scene()) {
        scene()->update(sceneBoundingRect());
    }
}

void TaskBarApplet::setPanelWindow(PanelWindow *panelWindow)
{
    Applet::setPanelWindow(panelWindow);
}

void TaskBarApplet::layoutChanged()
{
	updateLayout();
}

void TaskBarApplet::fontChanged()
{
    for(int i = 0; i < m_dockItems.size(); i++)
        m_dockItems[i]->fontChanged();
    updateLayout();
}

bool TaskBarApplet::init()
{
    readSettings();

    // IMPORTANT:
    // Allow the first population of clients. Otherwise a newly created TaskBarApplet
    // won't show already-open windows until _NET_CLIENT_LIST changes.
    m_initialized = true;

    // Load pinned items first (before updating client list)
    loadPinnedItems();

    // Populate immediately (existing windows)
    updateClientList();

#if QT_VERSION >= 0x050000
    if (qApp->platformName().toLower().contains("xcb")) {
        updateActiveWindow();
    }
#endif

    return true;
}

QSize TaskBarApplet::desiredSize()
{
	return QSize(-1, -1); // Take all available space.
}

void TaskBarApplet::updateLayout()
{
    // Prevent recursion: if we're already updating layout, don't recurse
    if (m_updatingLayout) {
        return;
    }
    m_updatingLayout = true;
    
    // Clean up dock items marked for deletion
    QVector<TaskBarItem*> itemsToDelete;
    for (int i = 0; i < m_dockItems.size(); i++) {
        if (m_dockItems[i]->shouldDelete()) {
            itemsToDelete.append(m_dockItems[i]);
        }
    }
    for (TaskBarItem* item : itemsToDelete) {
        // IMPORTANT: Clear back-references from clients before deleting the item
        // to avoid dangling pointers (Wayland windowClosed can arrive later).
        if (item) {
            for (Client* c : item->clients()) {
                if (c) c->clearDockItem();
            }
            for (WaylandClient* wc : item->waylandClients()) {
                if (wc) wc->clearDockItem();
            }
        }
        // Remove from list and scene, but don't call updateLayout() again (we're already in it)
        int index = m_dockItems.indexOf(item);
        if (index >= 0) {
            m_dockItems.remove(index);
        }
        if (scene() && item->scene() == scene()) {
            scene()->removeItem(item);
        }
        delete item;
    }

    // Get panel orientation / position
    PanelWindow::Orientation orientation = PanelWindow::Horizontal;
    PanelWindow::Position    position    = PanelWindow::Bottom;
    if (panelWindow()) {
        orientation = panelWindow()->orientation();
        position    = panelWindow()->position();
    }

    const bool isVerticalPanel =
        (orientation == PanelWindow::Vertical) ||
        (position == PanelWindow::Left || position == PanelWindow::Right);

    // Fallback sizes if not set yet
    int appW = (m_size.width()  > 0) ? m_size.width()  : adjustHardcodedPixelSize(48);
    int appH = (m_size.height() > 0) ? m_size.height() : adjustHardcodedPixelSize(600);

    // Requested margins/gaps
    const int edge = adjustHardcodedPixelSize(3); // 3px to edge
    const int gap  = adjustHardcodedPixelSize(5); // 5px between buttons

    if (!isVerticalPanel) {
        // -------------------------
        // HORIZONTAL (Top/Bottom)
        // Leave it as your current/previous behavior
        // -------------------------
        int freeSpace = appW;
        if (freeSpace <= 0) freeSpace = 800;

        int spaceForOneClient = (m_dockItems.size() > 0) ? freeSpace / m_dockItems.size() : 0;
        int currentPosition = 0;

        for (int i = 0; i < m_dockItems.size(); i++) {
            int spaceForThisClient = spaceForOneClient;
            static const int maxSpace = adjustHardcodedPixelSize(160);
            if (spaceForThisClient > maxSpace)
                spaceForThisClient = maxSpace;

            QPoint targetPos = QPoint(currentPosition, 0);
            QSize  targetSize = QSize(spaceForThisClient - 4,
                                      (appH > 0 ? appH : adjustHardcodedPixelSize(48)));

            m_dockItems[i]->setTargetPosition(targetPos);
            m_dockItems[i]->setTargetSize(targetSize);
            m_dockItems[i]->startAnimation();

            currentPosition += spaceForThisClient;
        }
    } else {
        // -------------------------
        // VERTICAL (Left/Right) — FIXED
        // -------------------------
        const int count = m_dockItems.size();
        if (count == 0) {
            update();
            if (scene()) scene()->update(sceneBoundingRect());
            m_updatingLayout = false;
            return;
        }

        const int btnW = qMax(1, appW - 2 * edge);  // width = panelWidth - 6
        const int availH = qMax(1, appH - 2 * edge - gap * (count - 1));

        // “Small panel” heuristic: if the panel is narrow, hide text and use square buttons.
        int panelW = appW;
        if (panelWindow())
            panelW = panelWindow()->panelWidth();

        const bool narrow = (panelW < adjustHardcodedPixelSize(100));

        int btnH = 0;
        if (narrow) {
            // Small side panel: square buttons (width - 6)
            btnH = btnW;
        } else {
            // Wide side panel: 24–30px height
            btnH = adjustHardcodedPixelSize(28);
            btnH = qBound(adjustHardcodedPixelSize(24), btnH, adjustHardcodedPixelSize(30));
        }

        // If we can’t fit, shrink (but keep a sane minimum)
        if (btnH * count > availH) {
            btnH = qMax(adjustHardcodedPixelSize(18), availH / count);
        }

        int y = edge;
        for (int i = 0; i < m_dockItems.size(); i++) {
            QPoint targetPos(edge, y);
            QSize  targetSize(btnW, btnH);

            m_dockItems[i]->setTargetPosition(targetPos);
            m_dockItems[i]->setTargetSize(targetSize);
            m_dockItems[i]->startAnimation();

            y += btnH + gap;
        }
    }

    update();

    // Force repaint
    if (scene()) {
        scene()->update(sceneBoundingRect());
    }
    
    m_updatingLayout = false;
}

void TaskBarApplet::draggingStarted()
{
	m_dragging = true;
}

void TaskBarApplet::draggingStopped()
{
	m_dragging = false;
	// Since we don't update it when dragging, we should do it now.
	updateClientList();
}

void TaskBarApplet::moveItem(TaskBarItem* dockItem, bool right)
{
	int index = m_dockItems.indexOf(dockItem);
	if(index == -1)
		return;

	if(right)
	{
		if(index < m_dockItems.size() - 1)
		{
			m_dockItems.swapItemsAt(index, index + 1);
		}
	}
	else
	{
		if(index > 0)
		{
			m_dockItems.swapItemsAt(index, index - 1);
		}
	}

	updateLayout();
}

void TaskBarApplet::registerTaskBarItem(TaskBarItem* dockItem)
{
	// Insert pinned items at the beginning, others at the end
	if (dockItem->isPinned()) {
		// Find the first non-pinned item to insert before it
		int insertPos = 0;
		for (int i = 0; i < m_dockItems.size(); i++) {
			if (!m_dockItems[i]->isPinned()) {
				insertPos = i;
				break;
			}
			insertPos = i + 1; // All items so far are pinned
		}
		m_dockItems.insert(insertPos, dockItem);
	} else {
		m_dockItems.append(dockItem);
	}
    updateLayout();
	dockItem->moveInstantly();
	
	// Force a complete repaint of the entire dock area
	if (scene()) {
		scene()->update(sceneBoundingRect());
	}
}

void TaskBarApplet::unregisterTaskBarItem(TaskBarItem* dockItem)
{
	int index = m_dockItems.indexOf(dockItem);
	if (index >= 0) {
		m_dockItems.remove(index);
		
		// Only update layout if not being destroyed AND not already updating layout
		// (prevents recursion: updateLayout() -> unregisterTaskBarItem() -> updateLayout())
		if (!m_destroying && !m_updatingLayout) {
			updateLayout();
			
			// Force a complete repaint of the entire dock area
			if (scene()) {
				scene()->update(sceneBoundingRect());
			}
		}
	}
}

TaskBarItem* TaskBarApplet::dockItemForClient(Client* client)
{
	if (!client) {
		return nullptr;
	}
	
	// Check if we already have a dock item for this client
	// But if grouping is disabled, we want separate items even if they have the same WM_CLASS
	// So we only return existing items if grouping is enabled OR if the item has only this one client
	for (TaskBarItem* item : m_dockItems) {
		if (item->hasClient(client)) {
			// If grouping is disabled and this item has multiple clients, don't return it
			// This allows ungrouping to work properly
			if (!m_group_windows && item->clients().size() > 1) {
				// Remove this client from the grouped item so we can create a separate one
				item->removeClient(client);
				client->clearDockItem();
				break; // Break out and create a new item below
			}
			return item;
		}
	}
	
	// Try to find a pinned item that matches this client
	// First, try direct matching by checking pinned items and their desktop files
	QString wmClass = client->wmClass();
	if (wmClass.isEmpty()) {
		// Fallback: try to get WM_CLASS directly
		wmClass = X11Support::getWindowWMClass(client->handle());
	}

	if (!wmClass.isEmpty()) {
		QString wmClassLower = wmClass.toLower();
		DesktopDataStore* dataStore = DesktopDataStore::instance();
		
		// First, try to match directly to pinned items by checking their desktop files
		// Create a copy to avoid issues if the list changes
		QVector<TaskBarItem*> itemsCopy = m_dockItems;
		for (TaskBarItem* item : itemsCopy) {
			// Skip if item was removed
			if (!m_dockItems.contains(item)) {
				continue;
			}
			
			if (!item->isPinned() || item->desktopFile().isEmpty()) {
				continue;
			}
			
			// Try to get the desktop entry for this pinned item
			QString desktopFile = item->desktopFile();
			QString normalizedPath = QFileInfo(desktopFile).canonicalFilePath();
			if (normalizedPath.isEmpty()) {
				normalizedPath = QFileInfo(desktopFile).absoluteFilePath();
			}
			
			DesktopEntryData entry;
			if (dataStore) {
				entry = dataStore->getDesktopEntry(normalizedPath);
				if (!entry.isValid) {
					entry = dataStore->getDesktopEntry(desktopFile);
				}
				if (!entry.isValid) {
					entry = dataStore->parseDesktopFile(normalizedPath.isEmpty() ? desktopFile : normalizedPath);
				}
			}
			
			// Match by startupWMClass (most reliable)
			if (entry.isValid && entry.startupWMClass.toLower() == wmClassLower) {
				// Check if this pinned item already has clients
				// If grouping is disabled and it already has a client, create a new button
				if (!m_group_windows && !item->clients().isEmpty()) {
					continue; // Skip this pinned item, will create new button below
				}
				
				item->addClient(client);
				item->update();
				return item;
			}
			
			// Match by desktop file name
			if (entry.isValid) {
				QString fileName = QFileInfo(entry.desktopFile).completeBaseName().toLower();
				if (fileName == wmClassLower || 
				    fileName.startsWith(wmClassLower + "-") || 
				    fileName.startsWith(wmClassLower + "_")) {
					// Check if this pinned item already has clients
					if (!m_group_windows && !item->clients().isEmpty()) {
						continue;
					}
					
					item->addClient(client);
					item->update();
					return item;
				}
			}
			
			// Also try matching by desktop file name directly (without loading entry)
			QString fileName = QFileInfo(desktopFile).completeBaseName().toLower();
			if (fileName == wmClassLower || 
			    fileName.startsWith(wmClassLower + "-") || 
			    fileName.startsWith(wmClassLower + "_")) {
				// Check if this pinned item already has clients
				if (!m_group_windows && !item->clients().isEmpty()) {
					continue;
				}
				
				item->addClient(client);
				item->update();
				return item;
			}
			
			// Match by executable name
			if (entry.isValid && !entry.exec.isEmpty()) {
				QString execLower = entry.exec.toLower();
				QString execName = execLower.split(' ').first().split('/').last();
				if (execName == wmClassLower) {
					// Check if this pinned item already has clients
					if (!m_group_windows && !item->clients().isEmpty()) {
						continue;
					}
					
					item->addClient(client);
					item->update();
					return item;
				}
			}
		}
		
		// If direct matching didn't work, try the full desktop entry search
		if (dataStore) {
			QList<DesktopEntryData> allEntries = dataStore->getAllDesktopEntries();
			QString matchedDesktopFile;
			
			foreach (const DesktopEntryData& entry, allEntries) {
				if (entry.type != "Application" || !entry.shouldShow()) {
					continue;
				}
				
				QString fileName = QFileInfo(entry.desktopFile).completeBaseName().toLower();
				
				if (entry.startupWMClass.toLower() == wmClassLower ||
				    fileName == wmClassLower || 
				    fileName.startsWith(wmClassLower + "-") || 
				    fileName.startsWith(wmClassLower + "_")) {
					matchedDesktopFile = entry.desktopFile;
					break;
				}
				
				if (!entry.exec.isEmpty()) {
					QString execLower = entry.exec.toLower();
					QString execName = execLower.split(' ').first().split('/').last();
					if (execName == wmClassLower) {
						matchedDesktopFile = entry.desktopFile;
						break;
					}
				}
				
				QString nameLower = entry.name.toLower();
				if (nameLower == wmClassLower) {
					matchedDesktopFile = entry.desktopFile;
					break;
				}
			}
			
			// If we found a matching desktop file, check if there's a pinned item for it
			if (!matchedDesktopFile.isEmpty()) {
				QString normalizedPath = QFileInfo(matchedDesktopFile).canonicalFilePath();
				if (normalizedPath.isEmpty()) {
					normalizedPath = QFileInfo(matchedDesktopFile).absoluteFilePath();
				}
				
				QVector<TaskBarItem*> itemsCopy2 = m_dockItems;
				for (TaskBarItem* item : itemsCopy2) {
					if (!m_dockItems.contains(item)) {
						continue;
					}
					
					if (!item->isPinned() || item->desktopFile().isEmpty()) {
						continue;
					}
					
					QString itemPath = QFileInfo(item->desktopFile()).canonicalFilePath();
					if (itemPath.isEmpty()) {
						itemPath = QFileInfo(item->desktopFile()).absoluteFilePath();
					}
					
					if (itemPath == normalizedPath || 
					    item->desktopFile() == matchedDesktopFile ||
					    item->desktopFile() == normalizedPath) {
						// Check if this pinned item already has clients
						// If grouping is disabled and it already has a client, create a new button
						if (!m_group_windows && !item->clients().isEmpty()) {
							break; // Skip this pinned item, will create new button below
						}
						
						item->addClient(client);
						item->update();
						return item;
					}
				}
			}
		}
	}
	
	// Before creating a new item, check if grouping is enabled and if there's an existing item for this app
	if (m_group_windows && !wmClass.isEmpty()) {
		QString wmClassLower = wmClass.toLower();
		
		// Look for existing non-pinned items with the same WM_CLASS
		for (TaskBarItem* item : m_dockItems) {
			if (item->isPinned() || item->clients().isEmpty()) {
				continue;
			}
			
			// Check if this item's first client has the same WM_CLASS
			Client* firstClient = item->clients().first();
			if (firstClient && firstClient->wmClass().toLower() == wmClassLower) {
				item->addClient(client);
				item->update();
				return item;
			}
		}
	}
	
	// Create a new dock item for this client
	TaskBarItem* dockItem = new TaskBarItem(this);
	dockItem->setButtonColor(m_buttonColor, m_buttonColorTransparency);
	dockItem->setFocusColor(m_focusColor, m_focusColorTransparency);
	dockItem->addClient(client);
	
	// Register the dock item immediately
	registerTaskBarItem(dockItem);
	
	// Update client's reference to this item
	client->setDockItem(dockItem);
	
	return dockItem;
}

TaskBarItem* TaskBarApplet::dockItemForWaylandClient(WaylandClient* client)
{
	if (!client) {
		return nullptr;
	}
	
	// Already assigned?
	for (TaskBarItem* item : m_dockItems) {
		if (item->hasWaylandClient(client)) {
			return item;
		}
	}

	const QString appIdLower = client->appId().toLower();

	// If grouping enabled, try to find existing item for the same appId (prefer pinned match)
	if (m_group_windows && !appIdLower.isEmpty()) {
		// 1) Pinned item match by desktop file basename
		for (TaskBarItem* item : m_dockItems) {
			if (!item || !item->isPinned()) continue;
			const QString pinnedBase = QFileInfo(item->desktopFile()).completeBaseName().toLower();
			if (pinnedBase == appIdLower ||
			    pinnedBase.startsWith(appIdLower + "_") ||
			    pinnedBase.startsWith(appIdLower + "-") ||
			    pinnedBase.contains(appIdLower)) {
				item->addWaylandClient(client);
				return item;
			}
		}

		// 2) Existing non-pinned item by appId
		for (TaskBarItem* item : m_dockItems) {
			if (!item || item->isPinned()) continue;
			const QVector<WaylandClient*>& wcs = item->waylandClients();
			if (!wcs.isEmpty() && wcs.first() && wcs.first()->appId().toLower() == appIdLower) {
				item->addWaylandClient(client);
				return item;
			}
		}
	}

	// No suitable existing item; create a new one
	TaskBarItem* dockItem = new TaskBarItem(this);
	dockItem->setButtonColor(m_buttonColor, m_buttonColorTransparency);
	dockItem->setFocusColor(m_focusColor, m_focusColorTransparency);
	dockItem->addWaylandClient(client);
	registerTaskBarItem(dockItem);
	return dockItem;
}

void TaskBarApplet::updateClientList()
{
    // Prevent multiple calls during initialization
    if (!m_initialized) {
        return;
    }

    // Update X11 clients if we're on X11 (regardless of whether WaylandSupport exists)
#if QT_VERSION >= 0x050000
    if (qApp->platformName().toLower().contains("xcb")) {
        updateX11ClientList();
    }
#else
    // For Qt4, always try X11
    updateX11ClientList();
#endif
    
    // Wayland updates are handled by signal/slot connection (if m_waylandSupport exists)

    // After creating clients, check if any should be matched to pinned items
    // This handles the case where clients were created before matching could happen
    matchClientsToPinnedItems();

    // Deduplicate dock items after updating (FIXED implementation below)
    if (m_group_windows) {
        deduplicateTaskBarItems();
    }

    // Update layout after deduplication to recalculate positions
    updateLayout();

    // Move items instantly to their new positions
    for (int i = 0; i < m_dockItems.size(); i++)
        m_dockItems[i]->moveInstantly();

    // Force a complete repaint of the entire dock area
    if (scene()) {
        scene()->update(sceneBoundingRect());
    }
}

void TaskBarApplet::updateWaylandClientList(const QList<WaylandWindow>& windows)
{
    // Safety check to prevent execution during destruction
    if (!m_waylandSupport || m_destroying) {
        return;
    }

    // Prevent re-entrancy from cascaded updates during item creation/layout.
    static bool processing = false;
    if (processing) return;
    processing = true;

    // Add new clients and update existing ones (keyed by surface/toplevel handle).
    for (const WaylandWindow& window : windows) {
        if (!window.surface) continue;

        WaylandClient* client = m_waylandClients.value(window.surface, nullptr);
        if (client) {
            // If this client lost its dock item during regroup/dedup, reattach now.
            TaskBarItem* item = client->dockItem();
            if (!item || !m_dockItems.contains(item)) {
                client->clearDockItem();
                TaskBarItem* newItem = dockItemForWaylandClient(client);
                if (newItem) {
                    client->setDockItem(newItem);
                }
            }
            client->updateFromWindow(window);
            continue;
        }

        // Create and attach a new client + item
        WaylandClient* waylandClient = new WaylandClient(this, window);
        m_waylandClients[window.surface] = waylandClient;

        TaskBarItem* item = dockItemForWaylandClient(waylandClient);
        if (item) {
            waylandClient->setDockItem(item);
        }
    }

    // Re-layout after updates
    matchClientsToPinnedItems();
    // Only deduplicate when grouping is enabled. When grouping is off, multiple items
    // per app are expected and dedup can incorrectly delete live items.
    if (m_group_windows) {
        deduplicateTaskBarItems();
    }
    updateLayout();
    for (int i = 0; i < m_dockItems.size(); i++) {
        m_dockItems[i]->moveInstantly();
    }
    if (scene()) {
        scene()->update(sceneBoundingRect());
    }

    processing = false;
}

void TaskBarApplet::updateX11ClientList()
{
#if QT_VERSION >= 0x050000
    if (qApp->platformName().toLower().contains("xcb") == false) return; 
#endif
	if (m_dragging) return;
	
	// Prevent multiple calls during initialization
	if (!m_initialized) {
		return;
	}
	
	QVector<unsigned long> windows = X11Support::getWindowPropertyWindowsArray(
        X11Support::rootWindow(), "_NET_CLIENT_LIST");
    
    // Fallback: if _NET_CLIENT_LIST is empty, try to get windows from window tree
    if (windows.isEmpty()) {
        windows = X11Support::getAllWindows();
    }

    unsigned long CurrentDesktop = X11Support::getWindowPropertyCardinal(X11Support::rootWindow(),"_NET_CURRENT_DESKTOP");
    unsigned long WindowDesktop;

    // Handle new clients.
	for (int i = 0; i < windows.size(); i++) {
        QString windowName = X11Support::getWindowName(windows[i]);
        QString windowClass = X11Support::getWindowPropertyUTF8String(windows[i], "WM_CLASS");

        // Skip system services and utilities
        QString windowNameLower = windowName.toLower();
        QString windowClassLower = windowClass.toLower();
        bool isSystemService = windowNameLower.contains("org.kde.xwaylandvideobridge") ||
                              windowNameLower.contains("org.kde.plasma") ||
                              windowNameLower.contains("org.gnome.shell") ||
                              windowNameLower.contains("com.canonical.unity") ||
                              windowNameLower.contains("com.ubuntu.") ||
                              windowNameLower.contains("org.freedesktop.") ||
                              windowNameLower.startsWith("gjs") ||
                              windowNameLower.startsWith("gnome-shell") ||
                              windowClassLower.contains("xwaylandvideobridge") ||
                              windowClassLower.contains("plasma") ||
                              windowClassLower.contains("gnome-shell") ||
                              windowClassLower.contains("hdepanel");
        
        if (isSystemService) {
            qDebug() << "TaskBarApplet::updateX11ClientList - Skipping system service window" << QString::number(windows[i], 16) << "name:" << windowName << "class:" << windowClass;
            continue;
        }

        // If Window isn't in current desktop , skip
        WindowDesktop = X11Support::getWindowPropertyCardinal(windows[i],"_NET_WM_DESKTOP");
        // Handle sticky windows (0xFFFFFFFFFFFFFFFF) and regular desktop windows
        if (WindowDesktop != CurrentDesktop && WindowDesktop != 0xFFFFFFFFFFFFFFFF) {
            continue;
        }


		if (!m_clients.contains(windows[i])) {
			// Skip our own windows.
			if (QWidget::find(windows[i]) != NULL) {
				continue;
			}
			
			// Skip special window types (panels, docks, etc.)
			QVector<unsigned long> windowTypes = X11Support::getWindowPropertyAtomsArray(windows[i], "_NET_WM_WINDOW_TYPE");
			bool isSpecialWindow = false;
			for (unsigned long windowType : windowTypes) {
				// Compare with known atom values for special window types
				unsigned long dockAtom = X11Support::atom("_NET_WM_WINDOW_TYPE_DOCK");
				unsigned long panelAtom = X11Support::atom("_NET_WM_WINDOW_TYPE_PANEL");
				unsigned long desktopAtom = X11Support::atom("_NET_WM_WINDOW_TYPE_DESKTOP");
				unsigned long notificationAtom = X11Support::atom("_NET_WM_WINDOW_TYPE_NOTIFICATION");
				
				if (windowType == dockAtom || 
				    windowType == panelAtom ||
				    windowType == desktopAtom ||
				    windowType == notificationAtom) {
					isSpecialWindow = true;
					break;
				}
			}
			if (isSpecialWindow) {
				continue;
			}

            // Check screen windows
            QRect windowGeometry = X11Support::getWindowWindowsGeometry(windows[i]);
            const QList<QScreen*> screens = QGuiApplication::screens();
            const int sidx = m_panelWindow->screen();
            const QScreen* screen = (sidx >= 0 && sidx < screens.size()) ? screens[sidx] : QGuiApplication::primaryScreen();
            const QRect screenGeometry = screen ? screen->geometry() : QRect(0,0,1920,1080);
            if(m_only_current_screen && (!screenGeometry.contains(windowGeometry.topLeft())))
            {
                continue;
            }
            // Check minimized windows
            if(m_only_minimized && !X11Support::getWindowMinimizedState(windows[i]))
            {
                continue;
            }

            m_clients[windows[i]] = new Client(this, windows[i]);
        }
        
	}

    m_in_loop.clear();
	// Handle removed clients.
	for (;;) {
		bool clientRemoved = false;
        foreach(Client* client, m_clients) {

            if(m_in_loop.contains(client))
            {
                continue;
            }

            unsigned long handle = client->handle();
            if (!windows.contains(handle))
            {
                delete m_clients[handle];
                m_clients.remove(handle);
                clientRemoved = true;
            }
        }

        if(!clientRemoved)
            break;
    }

    // Clean up any remaining clients in the loop
    foreach(Client* client, m_in_loop) {
        int handle = client->handle();
        delete m_clients[handle];
        m_clients.remove(handle);
    }
    
    // Update layout to clean up dock items marked for deletion
    updateLayout();
}


void TaskBarApplet::updateActiveWindow()
{
#if QT_VERSION >= 0x050000
    if (qApp->platformName().toLower().contains("xcb") == false) {
		// For Wayland, focus is tracked via WaylandWindow.focused field
		// which is updated in updateWaylandClientList()
		// Just trigger updates for all dock items
		for(int i = 0; i < m_dockItems.size(); i++)
		{
			m_dockItems[i]->startAnimation();
		}
		return;
	}
#endif
	unsigned long activeWindow = X11Support::getWindowPropertyWindow(X11Support::rootWindow(), "_NET_ACTIVE_WINDOW");
	
	if(activeWindow == 0)
		return;

	if(activeWindow == m_activeWindow) {
		// Still trigger animation in case focus highlight needs to update
		for(int i = 0; i < m_dockItems.size(); i++)
		{
			m_dockItems[i]->startAnimation();
		}
		return;
	}

	m_activeWindow = activeWindow;

	for(int i = 0; i < m_dockItems.size(); i++)
	{
		m_dockItems[i]->startAnimation();
	}
}

void TaskBarApplet::windowPropertyChanged(unsigned long window, unsigned long atom)
{
#if QT_VERSION >= 0x050000
    if (qApp->platformName().toLower().contains("xcb") == false) return; 
#endif
    
    // Check if this is a _NET_ACTIVE_WINDOW change on the root window
    unsigned long activeWindowAtom = X11Support::atom("_NET_ACTIVE_WINDOW");
    if (window == X11Support::rootWindow() && atom == activeWindowAtom) {
        updateActiveWindow();
        return;
    }
    
    // Check if this is a _NET_CLIENT_LIST change (new window created/removed)
    if (atom == X11Support::atom("_NET_CLIENT_LIST")) {
        updateClientList();
        return;
    }
    
    if (m_clients.contains(window))
		m_clients[window]->windowPropertyChanged(atom);
}

void TaskBarApplet::windowReconfigured(unsigned long window, int x, int y, int width, int height)
{
#if QT_VERSION >= 0x050000
    if (qApp->platformName().toLower().contains("xcb") == false) return; 
#endif
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(width)
    Q_UNUSED(height)

    if (m_clients.contains(window)) {
        // Handle window reconfiguration if needed
        return;
    }

    updateClientList();
}

void TaskBarApplet::windowClosed(unsigned long window)
{
#if QT_VERSION >= 0x050000
    //if (qApp->platformName().toLower().contains("xcb") == false) return; 
#endif
    if (m_clients.contains(window)) {
        // Remove the client
        delete m_clients[window];
        m_clients.remove(window);
        
        // Update layout
        updateLayout();
    }
}

bool TaskBarApplet::readSettings()
{
    bool oldGroupWindows = m_group_windows;
    
    m_only_current_screen = Settings::value(m_id, "only_current_screen", true).toBool();
    m_only_current_desktop = Settings::value(m_id, "only_current_desktop", true).toBool();
    m_only_minimized = Settings::value(m_id, "only_minimized", false).toBool();
    m_group_windows = Settings::value(m_id, "group_windows", false).toBool();
    
    // Load color settings
    m_buttonColor = Settings::value(m_id, "buttonColor", QColor(255, 255, 255)).value<QColor>();
    m_buttonColorTransparency = Settings::value(m_id, "buttonColorTransparency", 80).toInt();
    m_focusColor = Settings::value(m_id, "focusColor", QColor(0, 0, 0)).value<QColor>();
    m_focusColorTransparency = Settings::value(m_id, "focusColorTransparency", 128).toInt();
    
    // Update all existing dock items with new colors
    for (TaskBarItem* item : m_dockItems) {
        item->setButtonColor(m_buttonColor, m_buttonColorTransparency);
        item->setFocusColor(m_focusColor, m_focusColorTransparency);
    }
    
    // If grouping setting changed, regroup all windows
    bool groupingChanged = (oldGroupWindows != m_group_windows);
    if (groupingChanged) {
        regroupWindows();
    }
    
    return groupingChanged;
}

void TaskBarApplet::regroupWindows()
{
    // Prevent updateLayout() from being called during regrouping (which could cause recursion)
    if (m_updatingLayout) {
        return; // Already regrouping/updating
    }
    m_updatingLayout = true;
    
    // Collect all clients from existing items
    QList<Client*> allClients;
    QList<WaylandClient*> allWaylandClients;
    
    // Also track which clients are in pinned items (for ungrouping)
    QMap<Client*, TaskBarItem*> clientsInPinnedItems;
    
    for (TaskBarItem* item : m_dockItems) {
        // Collect X11 clients from all items (including pinned)
        QVector<Client*> itemClients = item->clients();
        for (Client* client : itemClients) {
            if (!allClients.contains(client)) {
                allClients.append(client);
            }
            
            // Track clients in pinned items
            if (item->isPinned()) {
                clientsInPinnedItems[client] = item;
            }
        }
    }
    
    // Collect wayland clients from the wayland clients map
    for (auto it = m_waylandClients.begin(); it != m_waylandClients.end(); ++it) {
        WaylandClient* wc = it.value();
        if (wc && !allWaylandClients.contains(wc)) {
            allWaylandClients.append(wc);
        }
    }

    auto findPinnedItemForWaylandAppId = [this](const QString& appIdLower) -> TaskBarItem* {
        if (appIdLower.isEmpty()) return nullptr;
        for (TaskBarItem* item : m_dockItems) {
            if (!item || !item->isPinned()) continue;
            const QString pinnedBase = QFileInfo(item->desktopFile()).completeBaseName().toLower();
            if (pinnedBase == appIdLower ||
                pinnedBase.startsWith(appIdLower + "_") ||
                pinnedBase.startsWith(appIdLower + "-") ||
                pinnedBase.contains(appIdLower)) {
                return item;
            }
        }
        return nullptr;
    };

    // ----- Wayland regrouping strategy -----
    //
    // Key invariant: after regroupWindows(), no WaylandClient may keep a dockItem pointer
    // to a TaskBarItem that was deleted or no longer contains it.
    //
    // To ensure this, we fully detach all Wayland clients from all items, clear their
    // backrefs, delete non-pinned items, then reattach based on m_group_windows.
    {
        // Detach all Wayland clients from all items (including pinned), and clear backrefs.
        for (TaskBarItem* item : m_dockItems) {
            if (!item) continue;
            const QVector<WaylandClient*> wcs = item->waylandClients(); // copy
            for (WaylandClient* wc : wcs) {
                if (!wc) continue;
                item->removeWaylandClient(wc);
                wc->clearDockItem();
            }
        }
        for (WaylandClient* wc : allWaylandClients) {
            if (wc) wc->clearDockItem();
        }
    }
    
    // When ungrouping, we need to remove clients from pinned items if they have multiple clients
    if (!m_group_windows) {
        for (auto it = clientsInPinnedItems.begin(); it != clientsInPinnedItems.end(); ++it) {
            Client* client = it.key();
            TaskBarItem* pinnedItem = it.value();
            
            // If the pinned item has multiple clients and grouping is disabled, remove this client
            if (pinnedItem && pinnedItem->clients().size() > 1) {
                pinnedItem->removeClient(client);
                client->clearDockItem();
            }
        }
    }
    
    // Clear all non-pinned items (they will be recreated)
    QVector<TaskBarItem*> itemsToRemove;
    for (TaskBarItem* item : m_dockItems) {
        if (item && !item->isPinned()) {
            itemsToRemove.append(item);
        }
    }

    for (TaskBarItem* item : itemsToRemove) {
        // Clear clients from item before deleting
        QVector<Client*> itemClients = item->clients();
        for (Client* client : itemClients) {
            item->removeClient(client);
            // Clear the client's reference to this item
            client->clearDockItem();
        }

        // IMPORTANT: detach any Wayland clients that currently point at this item.
        // Otherwise a regroup can delete the item while the WaylandClient still calls
        // m_dockItem->updateContent() on the next compositor update.
        for (auto it = m_waylandClients.begin(); it != m_waylandClients.end(); ++it) {
            WaylandClient* wc = it.value();
            if (!wc) continue;
            if (wc->dockItem() == item || item->hasWaylandClient(wc)) {
                // Remove from the item (keeps item internal list consistent)
                item->removeWaylandClient(wc);
                // Clear client backref; it will be reattached below
                wc->clearDockItem();
            }
        }
        
        unregisterTaskBarItem(item);
        m_dockItems.removeAll(item);
        delete item;
    }
    
    // Recreate items with new grouping setting
    // When ungrouping (m_group_windows == false), each client must get its own item
    // When grouping (m_group_windows == true), clients with same WM_CLASS will share an item
    if (!m_group_windows) {
        // Ungrouping: use dockItemForClient which will:
        // - Put the first matching client in a pinned item (if one exists)
        // - Create separate items for other clients
        // dockItemForClient already handles the logic: if a pinned item already has a client,
        // it skips it and creates a new item, so the first client goes to pinned, others get new items
        for (Client* client : allClients) {
            // Clear any stale reference
            client->clearDockItem();
            // dockItemForClient will match to pinned item if available and empty, or create new item
            TaskBarItem* item = dockItemForClient(client);
            if (item) {
                client->setDockItem(item);
            }
        }
    } else {
        // Grouping: use dockItemForClient which will group clients with same WM_CLASS
        for (Client* client : allClients) {
            // Clear any stale reference
            client->clearDockItem();
            dockItemForClient(client);
        }
    }
    
    // Reattach Wayland clients according to the new grouping setting.
    if (m_group_windows) {
        // Group: use standard grouping logic (pinned match + appId grouping).
        for (WaylandClient* wc : allWaylandClients) {
            if (!wc) continue;
            TaskBarItem* item = dockItemForWaylandClient(wc);
            if (item) wc->setDockItem(item);
        }
    } else {
        // Ungroup: each window gets its own item, but allow a pinned launcher to host
        // at most ONE window (mirrors the X11 pinned behavior above).
        QSet<TaskBarItem*> pinnedAlreadyHasWayland;
        for (WaylandClient* wc : allWaylandClients) {
            if (!wc) continue;
            const QString appIdLower = wc->appId().toLower();
            TaskBarItem* pinned = findPinnedItemForWaylandAppId(appIdLower);
            if (pinned && !pinnedAlreadyHasWayland.contains(pinned)) {
                pinned->addWaylandClient(wc);
                wc->setDockItem(pinned);
                pinnedAlreadyHasWayland.insert(pinned);
                continue;
            }
            // Create a dedicated item for this Wayland window
            TaskBarItem* dockItem = new TaskBarItem(this);
            dockItem->setButtonColor(m_buttonColor, m_buttonColorTransparency);
            dockItem->setFocusColor(m_focusColor, m_focusColorTransparency);
            dockItem->addWaylandClient(wc);
            registerTaskBarItem(dockItem);
            wc->setDockItem(dockItem);
        }
    }
    
    // When ungrouping, we need to ensure deduplication doesn't merge items back together
    // So we skip deduplication when grouping is disabled
    if (m_group_windows) {
        // Only deduplicate when grouping is enabled (to clean up any duplicates)
        deduplicateTaskBarItems();
    }
    
    // Update layout
    updateLayout();
    
    // Move items instantly to their new positions
    for (int i = 0; i < m_dockItems.size(); i++) {
        m_dockItems[i]->moveInstantly();
    }
    
    // Force a complete repaint
    if (scene()) {
        scene()->update(sceneBoundingRect());
    }
    
    m_updatingLayout = false;
}

void TaskBarApplet::deduplicateTaskBarItems()
{
    QVector<TaskBarItem*> itemsToRemove;

    // Dedup by *identity*, NOT by item->text() (text can be empty/elided)
    QSet<qulonglong> seenX11Handles;
    QSet<qulonglong> seenWaylandPtrs;

    for (TaskBarItem* item : m_dockItems) {
        if (!item) continue;
        
        // Don't remove pinned items even if they have no windows
        if (item->isPinned()) {
            continue;
        }

        bool matched = false;

        // --- X11: find which Client* this item belongs to, then dedup by handle ---
        for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
            Client* c = it.value();
            if (!c) continue;

            if (item->hasClient(c)) {
                matched = true;
                const qulonglong h = static_cast<qulonglong>(c->handle());
                if (seenX11Handles.contains(h)) {
                    itemsToRemove.append(item);
                } else {
                    seenX11Handles.insert(h);
                }
                break;
            }
        }

        if (matched) continue;

        // --- Wayland: find which WaylandClient* this item belongs to, dedup by pointer ---
        for (auto it = m_waylandClients.begin(); it != m_waylandClients.end(); ++it) {
            WaylandClient* wc = it.value();
            if (!wc) continue;

            if (item->hasWaylandClient(wc)) {
                matched = true;
                const qulonglong p = static_cast<qulonglong>(reinterpret_cast<uintptr_t>(wc));
                if (seenWaylandPtrs.contains(p)) {
                    itemsToRemove.append(item);
                } else {
                    seenWaylandPtrs.insert(p);
                }
                break;
            }
        }

        // If it matched neither X11 nor Wayland, we don't dedup it here.
    }

    // Remove duplicates safely
    for (TaskBarItem* item : itemsToRemove) {
        if (!item) continue;
        if (!m_dockItems.contains(item)) continue;

        // Remove from list first
        m_dockItems.removeAll(item);

        // IMPORTANT: detach any clients that currently point at this item,
        // otherwise the next Wayland/X11 update will call updateContent() on
        // a deleted TaskBarItem.
        for (WaylandClient* wc : item->waylandClients()) {
            if (!wc) continue;
            if (wc->dockItem() == item) {
                // Try to re-home to another item that contains this client.
                TaskBarItem* replacement = nullptr;
                for (TaskBarItem* other : m_dockItems) {
                    if (other && other->hasWaylandClient(wc)) {
                        replacement = other;
                        break;
                    }
                }
                wc->setDockItem(replacement);
            }
        }
        for (Client* c : item->clients()) {
            if (!c) continue;
            if (c->dockItem() == item) {
                TaskBarItem* replacement = nullptr;
                for (TaskBarItem* other : m_dockItems) {
                    if (other && other->hasClient(c)) {
                        replacement = other;
                        break;
                    }
                }
                c->setDockItem(replacement);
            }
        }

        // Remove from scene if present (avoid double-remove during shutdown)
        if (scene() && item->scene() == scene()) {
            scene()->removeItem(item);
        }

        delete item;
    }
}

TaskBarItem* TaskBarApplet::createTaskBarItem(const QString& name, const QIcon& icon, const QString& objectName)
{
	// Create the dock item
	TaskBarItem* dockItem = new TaskBarItem(this);
	dockItem->setButtonColor(m_buttonColor, m_buttonColorTransparency);
	dockItem->setFocusColor(m_focusColor, m_focusColorTransparency);
	
	// Set object name if provided
	if (!objectName.isEmpty()) {
		dockItem->setObjectName(objectName);
	}
	
	// Register the dock item first (this will trigger updateLayout which sets target size)
	registerTaskBarItem(dockItem);
	
	// Now set text and icon (this will call updateContent with proper target size)
	dockItem->setText(name);
	dockItem->setIcon(icon);
	
	return dockItem;
}

void TaskBarApplet::showConfigurationDialog()
{
    TaskBarConfigurationDialog dialog(m_id, panelWindow());
    if(dialog.exec())
    {
        // readSettings() calls regroupWindows() if grouping changed, which already handles everything
        bool groupingChanged = readSettings();
        // Only call updateClientList() if grouping didn't change (for other settings like colors)
        if (!groupingChanged) {
            updateClientList();
        }
        // Otherwise, regroupWindows() already updated everything
    }
}

void TaskBarApplet::loadPinnedItems()
{
    QStringList pinnedItems = Settings::value(m_id, "pinnedItems", QStringList()).toStringList();
    
    qDebug() << "TaskBarApplet::loadPinnedItems - Loading" << pinnedItems.size() << "pinned items for applet" << m_id;
    
    DesktopDataStore* dataStore = DesktopDataStore::instance();
    if (!dataStore) {
        qDebug() << "TaskBarApplet::loadPinnedItems - DesktopDataStore not available";
        return;
    }
    
    foreach (const QString& desktopFile, pinnedItems) {
        qDebug() << "TaskBarApplet::loadPinnedItems - Loading pinned item:" << desktopFile;
        
        // Normalize the path (resolve symlinks, make absolute)
        QString normalizedPath = QFileInfo(desktopFile).canonicalFilePath();
        if (normalizedPath.isEmpty()) {
            normalizedPath = QFileInfo(desktopFile).absoluteFilePath();
        }
        
        DesktopEntryData entry = dataStore->getDesktopEntry(normalizedPath);
        
        // If not found with normalized path, try the original path
        if (!entry.isValid) {
            entry = dataStore->getDesktopEntry(desktopFile);
        }
        
        // If still not found, try to parse it directly
        if (!entry.isValid) {
            qDebug() << "TaskBarApplet::loadPinnedItems - Entry not in cache, parsing directly:" << normalizedPath;
            entry = dataStore->parseDesktopFile(normalizedPath);
        }
        
        qDebug() << "TaskBarApplet::loadPinnedItems - Entry isValid:" << entry.isValid 
                 << "type:" << entry.type << "shouldShow:" << entry.shouldShow()
                 << "name:" << entry.name << "desktopFile:" << entry.desktopFile;
        if (entry.isValid && entry.type == "Application" && entry.shouldShow()) {
            // Use normalized path for consistency in matching
            // entry.desktopFile should already be the correct path from the data store
            QString pathToStore = normalizedPath.isEmpty() ? desktopFile : normalizedPath;
            // But prefer entry.desktopFile if it's valid (it might be the canonical path from the store)
            if (entry.desktopFile == normalizedPath || entry.desktopFile == desktopFile) {
                pathToStore = entry.desktopFile;
            } else if (!normalizedPath.isEmpty()) {
                pathToStore = normalizedPath;
            } else {
                pathToStore = desktopFile;
            }
            
            TaskBarItem* item = findOrCreatePinnedItem(pathToStore);
            if (item) {
                // Update icon and text from desktop entry
                QString displayName = entry.getDisplayName(QLocale::system().name());
                item->setText(displayName);
                
                // Load icon
                QIcon icon = UnifiedIconService::instance()->loadApplicationIcon(entry.icon, QString(), 32);
                if (icon.isNull()) {
                    icon = QIcon::fromTheme("application-x-executable");
                }
                item->setIcon(icon);
                
                // Ensure the item has content and is visible
                item->updateContent();
                qDebug() << "TaskBarApplet::loadPinnedItems - Created pinned item:" << displayName;
            }
        } else {
            qDebug() << "TaskBarApplet::loadPinnedItems - Invalid or non-application entry:" << desktopFile
                     << "isValid:" << entry.isValid << "type:" << entry.type << "shouldShow:" << entry.shouldShow();
        }
    }
    
    qDebug() << "TaskBarApplet::loadPinnedItems - Total dock items after loading:" << m_dockItems.size();
    
    // After loading pinned items, check if any existing clients match them
    // This handles the case where hdepanel starts and apps are already running
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        Client* client = it.value();
        if (!client) continue;
        
        // Check if this client already has a dock item
        bool hasDockItem = false;
        for (TaskBarItem* item : m_dockItems) {
            if (item->hasClient(client)) {
                hasDockItem = true;
                break;
            }
        }
        
        // If client doesn't have a dock item yet, try to match it to a pinned item
        if (!hasDockItem) {
            TaskBarItem* matchedItem = dockItemForClient(client);
            if (matchedItem && matchedItem->isPinned()) {
                qDebug() << "TaskBarApplet::loadPinnedItems - Matched existing client to pinned item";
            }
        }
    }
    
    // Update layout to position the newly loaded pinned items
    if (!m_dockItems.isEmpty()) {
        updateLayout();
        // Move items instantly to their positions
        for (int i = 0; i < m_dockItems.size(); i++) {
            m_dockItems[i]->moveInstantly();
        }
        // Force a repaint
        if (scene()) {
            scene()->update(sceneBoundingRect());
        }
    }
}

void TaskBarApplet::matchClientsToPinnedItems()
{
    // Temporarily disabled - the matching should happen in dockItemForClient
    // This function was causing segfaults when trying to move clients
    // TODO: Fix the matching in dockItemForClient to work correctly during initialization
    return;
    
    // Check all clients and see if they should be matched to pinned items
    // This is needed because when clients are created, they might create their own dock items
    // before the matching logic can find the pinned items
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        Client* client = it.value();
        if (!client) continue;
        
        // Check if this client already has a dock item
        TaskBarItem* currentItem = nullptr;
        for (TaskBarItem* item : m_dockItems) {
            if (item && item->hasClient(client)) {
                currentItem = item;
                break;
            }
        }
        
        // If client has a dock item that's not pinned, check if it should match a pinned item
        // Also verify currentItem is still valid (not deleted)
        if (currentItem && m_dockItems.contains(currentItem) && !currentItem->isPinned()) {
            bool clientMatched = false;
            QString wmClass = client->wmClass();
            if (wmClass.isEmpty()) {
                wmClass = X11Support::getWindowWMClass(client->handle());
            }
            
            if (!wmClass.isEmpty()) {
                DesktopDataStore* dataStore = DesktopDataStore::instance();
                if (dataStore) {
                    QString wmClassLower = wmClass.toLower();
                    QList<DesktopEntryData> allEntries = dataStore->getAllDesktopEntries();
                    QString matchedDesktopFile;
                    
                    foreach (const DesktopEntryData& entry, allEntries) {
                        if (entry.type != "Application" || !entry.shouldShow()) {
                            continue;
                        }
                        
                        QString fileName = QFileInfo(entry.desktopFile).completeBaseName().toLower();
                        
                        if (entry.startupWMClass.toLower() == wmClassLower ||
                            fileName == wmClassLower || 
                            fileName.startsWith(wmClassLower + "-") || 
                            fileName.startsWith(wmClassLower + "_")) {
                            matchedDesktopFile = entry.desktopFile;
                            break;
                        }
                        
                        if (!entry.exec.isEmpty()) {
                            QString execLower = entry.exec.toLower();
                            QString execName = execLower.split(' ').first().split('/').last();
                            if (execName == wmClassLower) {
                                matchedDesktopFile = entry.desktopFile;
                                break;
                            }
                        }
                        
                        QString nameLower = entry.name.toLower();
                        if (nameLower == wmClassLower) {
                            matchedDesktopFile = entry.desktopFile;
                            break;
                        }
                    }
                    
                    // Check if there's a pinned item for this desktop file
                    if (!matchedDesktopFile.isEmpty()) {
                        QString normalizedPath = QFileInfo(matchedDesktopFile).canonicalFilePath();
                        if (normalizedPath.isEmpty()) {
                            normalizedPath = QFileInfo(matchedDesktopFile).absoluteFilePath();
                        }
                        
                        // Create a copy of the dock items list to avoid issues while iterating
                        QVector<TaskBarItem*> dockItemsCopy = m_dockItems;
                        for (TaskBarItem* item : dockItemsCopy) {
                            // Skip if item was deleted
                            if (!m_dockItems.contains(item)) {
                                continue;
                            }
                            
                            if (!item->isPinned() || item->desktopFile().isEmpty()) {
                                continue;
                            }
                            
                            QString itemPath = QFileInfo(item->desktopFile()).canonicalFilePath();
                            if (itemPath.isEmpty()) {
                                itemPath = QFileInfo(item->desktopFile()).absoluteFilePath();
                            }
                            
                            if (itemPath == normalizedPath || 
                                item->desktopFile() == matchedDesktopFile ||
                                item->desktopFile() == normalizedPath) {
                                // Found matching pinned item - move client to it
                                qDebug() << "TaskBarApplet::matchClientsToPinnedItems - Moving client" << wmClass 
                                         << "from non-pinned item to pinned item" << item->desktopFile();
                                
                                // Add client to pinned item first (so it has a valid dock item)
                                item->addClient(client);
                                item->update();
                                
                                // Then remove from old item (this might mark it for deletion if empty)
                                // Note: currentItem might be marked for deletion if it becomes empty,
                                // but it won't be deleted until deduplicateTaskBarItems() runs
                                // Store a reference to the old item before removing
                                TaskBarItem* oldItem = currentItem;
                                if (oldItem && m_dockItems.contains(oldItem)) {
                                    oldItem->removeClient(client);
                                    // Don't access oldItem after this point as it might be deleted
                                }
                                
                                // If the old item is now empty and not pinned, it will be cleaned up
                                // by deduplicateTaskBarItems() which is called after this function
                                
                                // Mark that we matched this client
                                clientMatched = true;
                                break; // Break out of the inner loop (pinned items)
                            }
                        }
                    }
                }
                
                // If we matched the client, continue to next client
                if (clientMatched) {
                    continue;
                }
            }
        }
    }
}

void TaskBarApplet::savePinnedItems()
{
    QStringList pinnedItems;
    for (TaskBarItem* item : m_dockItems) {
        if (item->isPinned() && !item->desktopFile().isEmpty()) {
            pinnedItems << item->desktopFile();
        }
    }
    Settings::setValue(m_id, "pinnedItems", pinnedItems);
    qDebug() << "TaskBarApplet::savePinnedItems - Saved" << pinnedItems.size() << "pinned items for applet" << m_id;
}

QStringList TaskBarApplet::getPinnedItems() const
{
    return Settings::value(m_id, "pinnedItems", QStringList()).toStringList();
}

void TaskBarApplet::setPinnedItems(const QStringList& items)
{
    Settings::setValue(m_id, "pinnedItems", items);
}

TaskBarItem* TaskBarApplet::findOrCreatePinnedItem(const QString& desktopFile)
{
    // Normalize the path for comparison
    QString normalizedPath = QFileInfo(desktopFile).canonicalFilePath();
    if (normalizedPath.isEmpty()) {
        normalizedPath = QFileInfo(desktopFile).absoluteFilePath();
    }
    
    // Check if we already have a pinned item for this desktop file
    // Compare both original and normalized paths
    for (TaskBarItem* item : m_dockItems) {
        QString itemPath = item->desktopFile();
        if (itemPath.isEmpty()) {
            continue;
        }
        
        QString itemNormalized = QFileInfo(itemPath).canonicalFilePath();
        if (itemNormalized.isEmpty()) {
            itemNormalized = QFileInfo(itemPath).absoluteFilePath();
        }
        
        // Match if paths are the same (original or normalized)
        if (itemPath == desktopFile || 
            itemPath == normalizedPath ||
            itemNormalized == desktopFile ||
            itemNormalized == normalizedPath) {
            return item;
        }
    }
    
    // Create a new pinned item
    TaskBarItem* dockItem = new TaskBarItem(this);
    dockItem->setButtonColor(m_buttonColor, m_buttonColorTransparency);
    dockItem->setFocusColor(m_focusColor, m_focusColorTransparency);
    dockItem->setDesktopFile(desktopFile);
    
    // Register the dock item
    registerTaskBarItem(dockItem);
    
    return dockItem;
}


#include "moc_taskbarapplet.cpp"

