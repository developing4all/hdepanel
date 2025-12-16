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


#include "applicationsmenuapplet.h"

#if QT_VERSION >= 0x050000
#include <QMenu>
#include <QStyle>
#include <QPixmap>
#include <QGraphicsScene>
#else
#include <QtGui/QMenu>
#include <QtGui/QStyle>
#include <QtGui/QPixmap>
#include <QtGui/QGraphicsScene>
#endif
#include "../../lib/textgraphicsitem.h"
#include "../../lib/panelwindow.h"
#include "../../lib/desktopapplications.h"
#include "../../lib/desktopdatastore.h"
#include "../../lib/unifiediconservice.h"
#include "../../lib/dpisupport.h"

#include "../../lib/panelwindow.h"

#include <QApplication>
#include <QDebug>

#if QT_VERSION < 0x050000
int ApplicationsMenuStyle::pixelMetric(PixelMetric metric, const QStyleOption* option, const QWidget* widget) const
{
    if(metric == QStyle::PM_SmallIconSize)
        return adjustHardcodedPixelSize(32);
    else
        return QPlastiqueStyle::pixelMetric(metric, option, widget);
}
#endif

SubMenu::SubMenu(QMenu* parent, const QString& title, const QString& category, const QString& icon)
{
    m_menu = new QMenu(parent); // Will be deleted automatically.
    m_menu->setStyle(parent->style());
    m_menu->setFont(parent->font());
    m_menu->setTitle(title);
    m_menu->setIcon(UnifiedIconService::instance()->loadIcon(icon, 32));
    m_menu->menuAction()->setIconVisibleInMenu(true);
    m_category = category;
}

static const char* menuStyleSheet =
"QMenu { background-color: black; }\n"
"QMenu::item { height: %dpx; background-color: transparent; color: white; padding-left: %dpx; padding-right: %dpx; padding-top: %dpx; padding-bottom: %dpx; }\n"
"QMenu::item::selected { background-color: #606060; border-color: gray; }\n"
"QMenu::icon { left: %dpx; }\n";

ApplicationsMenuApplet::ApplicationsMenuApplet(PanelWindow* panelWindow)
    : Applet(panelWindow), m_menuOpened(false)
{
    setObjectName("ApplicationsMenu");
    
    // Create text item early so desiredSize() doesn't crash
    m_textItem = new TextGraphicsItem(this);
    m_textItem->setColor(Qt::white);
    m_textItem->setText(tr("Applications"));

    // Separate icon item (size will be adjusted in refreshIcons)
    m_iconItem = new TextGraphicsItem(this);
    m_iconItem->setColor(Qt::white);
    m_iconSize = 22;

    m_textItem->setAcceptedMouseButtons(Qt::NoButton);
    m_iconItem->setAcceptedMouseButtons(Qt::NoButton);
    #if QT_VERSION >= 0x050000
    m_textItem->setAcceptHoverEvents(false);
    m_iconItem->setAcceptHoverEvents(false);
    #else
    m_textItem->setAcceptsHoverEvents(false);
    m_iconItem->setAcceptsHoverEvents(false);
    #endif
}
void ApplicationsMenuApplet::setPanelWindow(PanelWindow *panelWindow)
{
    Applet::setPanelWindow(panelWindow);
    m_menu = new QMenu();
#if QT_VERSION >= 0x050000
    m_menu->setStyle(QStyleFactory::create("fusion"));
#else
    m_menu->setStyle(&m_style);
#endif
    m_menu->setFont(m_panelWindow->font());
    m_menu->setStyleSheet(QString::asprintf(menuStyleSheet,
        adjustHardcodedPixelSize(36),
        adjustHardcodedPixelSize(38),
        adjustHardcodedPixelSize(20),
        adjustHardcodedPixelSize(2),
        adjustHardcodedPixelSize(2),
        adjustHardcodedPixelSize(2)
    ));
    m_subMenus.append(SubMenu(m_menu, tr("Accessories"), "Utility", "applications-accessories"));
    m_subMenus.append(SubMenu(m_menu, tr("Development"), "Development", "applications-development"));
    m_subMenus.append(SubMenu(m_menu, tr("Education"), "Education", "applications-science"));
    m_subMenus.append(SubMenu(m_menu, tr("Office"), "Office", "applications-office"));
    m_subMenus.append(SubMenu(m_menu, tr("Graphics"), "Graphics", "applications-graphics"));
    m_subMenus.append(SubMenu(m_menu, tr("Multimedia"), "AudioVideo", "applications-multimedia"));
    m_subMenus.append(SubMenu(m_menu, tr("Games"), "Game", "applications-games"));
    m_subMenus.append(SubMenu(m_menu, tr("Network"), "Network", "applications-internet"));
    m_subMenus.append(SubMenu(m_menu, tr("System"), "System", "preferences-system"));
    m_subMenus.append(SubMenu(m_menu, tr("Settings"), "Settings", "preferences-desktop"));
    m_subMenus.append(SubMenu(m_menu, tr("Other"), "Other", "applications-other"));

    // Update text item font for new panel window
    m_textItem->setFont(m_panelWindow->font());
    m_textItem->setText(tr("Applications"));

    // Refresh icon now that we have a panel window
    refreshIcons();

    // Refresh icon when theme changes
    QObject::connect(qApp, SIGNAL(iconThemeChanged(const QString&)), this, SLOT(refreshIcons()));
}

void ApplicationsMenuApplet::fontChanged()
{
    m_textItem->setFont(m_panelWindow->font());
    m_menu->setFont(m_panelWindow->font());
    foreach(SubMenu sub, m_subMenus)
    {
        sub.menu()->setFont(m_panelWindow->font());
    }
}

void ApplicationsMenuApplet::refreshIcons()
{
#if QT_VERSION >= 0x050000
    if (!m_iconItem || !m_panelWindow)
        return;

    PanelWindow::Orientation orientation = m_panelWindow->orientation();
    PanelWindow::Position position = m_panelWindow->position();

    const bool isSide = (position == PanelWindow::Left || position == PanelWindow::Right);
    const bool horizontalPanel = (orientation == PanelWindow::Horizontal) && !isSide;

    const int sideMargin = 5;   // match StartApplet + taskbar feel
    const bool showText = !isSide || (m_panelWindow->panelWidth() >= 100);

    int thickness = horizontalPanel ? m_panelWindow->panelHeight()
                                    : m_panelWindow->panelWidth();
    if (thickness <= 0)
        thickness = 24;

    int size = 22;

    if (isSide && !showText) {
        // Narrow side panel: icon should be (width - 2*margin)
        size = thickness - 2 * sideMargin;  // e.g. 64 -> 54
        if (size < 8) size = 8;
    } else {
        // Normal case: keep a sane cap so it doesn't explode on wide panels
        const int padding = 4;
        size = thickness - 2 * padding;
        if (size < 8) size = 8;
        if (size > 32) size = 32;
    }

    m_iconSize = size;

    QImage img = UnifiedIconService::instance()->loadIconAsImage("start-here", m_iconSize);
    m_iconItem->setImage(img);
#endif
}

void ApplicationsMenuApplet::close()
{
    // Disconnect from DesktopDataStore signals to prevent callbacks after deletion
    DesktopDataStore* dataStore = DesktopDataStore::instance();
    if (dataStore) {
        disconnect(dataStore, &DesktopDataStore::desktopEntryAdded, this, &ApplicationsMenuApplet::onDataStoreApplicationAdded);
        disconnect(dataStore, &DesktopDataStore::desktopEntryUpdated, this, &ApplicationsMenuApplet::onDataStoreApplicationUpdated);
        disconnect(dataStore, &DesktopDataStore::desktopEntryRemoved, this, &ApplicationsMenuApplet::onDataStoreApplicationRemoved);
    }
}

ApplicationsMenuApplet::~ApplicationsMenuApplet()
{
    // Ensure close() was called (disconnect signals)
    close();
    
    foreach(QAction* action, m_actions)
    {
        delete action;
    }

    delete m_textItem;
    delete m_iconItem;
    delete m_menu;
}

bool ApplicationsMenuApplet::init()
{
    setInteractive(true);

    // Initialize data store with desktop applications
    DesktopDataStore* dataStore = DesktopDataStore::instance();
    
    if (dataStore) {
        // Connect to data store signals for updates
        connect(dataStore, &DesktopDataStore::desktopEntryAdded, this, &ApplicationsMenuApplet::onDataStoreApplicationAdded);
        connect(dataStore, &DesktopDataStore::desktopEntryUpdated, this, &ApplicationsMenuApplet::onDataStoreApplicationUpdated);
        connect(dataStore, &DesktopDataStore::desktopEntryRemoved, this, &ApplicationsMenuApplet::onDataStoreApplicationRemoved);
    }


    m_menu->addSeparator();
    m_menu->addAction(UnifiedIconService::instance()->loadIcon("application-exit", 32), tr("Quit"), qApp, SLOT(quit()));

    return true;
}

QSize ApplicationsMenuApplet::desiredSize()
{
    if (!m_panelWindow) {
        return QSize(100, 48);
    }

    PanelWindow::Orientation orientation = m_panelWindow->orientation();
    PanelWindow::Position position = m_panelWindow->position();

    const bool isSide = (position == PanelWindow::Left || position == PanelWindow::Right);
    const bool horizontalPanel = (orientation == PanelWindow::Horizontal) && !isSide;

    const int thickness = isSide ? m_panelWindow->panelWidth()
                                 : m_panelWindow->panelHeight();

    const bool showText = horizontalPanel || (!isSide ? true : (thickness >= 100));

    const int icon = (m_iconSize > 0 ? m_iconSize : 22);

    if (showText) {
        // Match StartApplet spacing
        QFontMetrics metrics(m_panelWindow->font());
        const QString label = tr("Applications");
        const int textW = metrics.horizontalAdvance(label);

        const int leftPadding  = 12;
        const int gap          = 12;
        const int rightPadding = 16;

        const int w = leftPadding + icon + gap + textW + rightPadding;
        const int h = m_panelWindow->panelHeight();
        return QSize(w, h);
    }

    // Narrow side-panel: square tile
    int t = m_panelWindow->panelWidth();
    if (t < 10) t = 10;
    return QSize(t, t);
}

void ApplicationsMenuApplet::clicked()
{
    m_menuOpened = true;
    animateHighlight();

    m_menu->move(localToScreen(QPoint(0, m_size.height())));
    m_menu->exec();

    m_menuOpened = false;
    animateHighlight();
}

void ApplicationsMenuApplet::layoutChanged()
{
    if (!m_panelWindow || !m_textItem || !m_iconItem)
        return;

    refreshIcons();

    PanelWindow::Orientation orientation = m_panelWindow->orientation();
    PanelWindow::Position position = m_panelWindow->position();

    const bool isSide = (position == PanelWindow::Left || position == PanelWindow::Right);
    const bool horizontalPanel = (orientation == PanelWindow::Horizontal) && !isSide;

    const int cellW = (m_size.width()  > 0) ? m_size.width()  : (isSide ? m_panelWindow->panelWidth()  : m_panelWindow->panelHeight());
    const int cellH = (m_size.height() > 0) ? m_size.height() : m_panelWindow->panelHeight();

    const bool showText = horizontalPanel || (!isSide ? true : (m_panelWindow->panelWidth() >= 100));

    const int sideMargin  = 5;
    const int leftPadding = 12;
    const int gap         = 12;

    const int iconSize = (m_iconSize > 0 ? m_iconSize : 22);

    if (showText) {
        // Icon left, text right (even on wide left/right panels)
        const int iconX = leftPadding;
        int iconY = (cellH - iconSize) / 2;
        if (iconY < 0) iconY = 0;
        m_iconItem->setPos(iconX, iconY);

        m_textItem->setText(tr("Applications"));
        m_textItem->setVisible(true);

        const int textX = iconX + iconSize + gap;

        // Use the same baseline logic as StartApplet for horizontal,
        // and visually centered text for side panels.
        int textY;
        if (!isSide) {
            textY = m_panelWindow->textBaseLine();
        } else {
            QFontMetrics fm(m_panelWindow->font());
            textY = (cellH - fm.height()) / 2 + fm.ascent();
        }

        m_textItem->setPos(textX, textY);
    } else {
        // Narrow side panel: icon-only and inset by 5px from left/right
        m_textItem->setText(QString());
        m_textItem->setVisible(false);

        const int iconX = sideMargin;
        int iconY = (cellH - iconSize) / 2;
        if (iconY < 0) iconY = 0;

        m_iconItem->setPos(iconX, iconY);
    }
}

bool ApplicationsMenuApplet::isHighlighted()
{
    return m_menuOpened || Applet::isHighlighted();
}

void ApplicationsMenuApplet::actionTriggered()
{
    DesktopDataStore::instance()->launchApplication(static_cast<QAction*>(sender())->data().toString());
}

void ApplicationsMenuApplet::applicationUpdated(const DesktopApplication& app)
{
    applicationRemoved(app.path());

    if(app.isNoDisplay())
        return;

    QAction* action = new QAction(m_menu);
    action->setIconVisibleInMenu(true);
    action->setData(app.path());
    action->setText(app.name());
    
    QIcon icon = UnifiedIconService::instance()->loadApplicationIcon(app, 32);
    
    action->setIcon(icon);

    connect(action, SIGNAL(triggered()), this, SLOT(actionTriggered()));

    // Add to relevant menu.
    int subMenuIndex = m_subMenus.size() - 1; // By default put it in "Other".
    for(int i = 0; i < m_subMenus.size() - 1; i++) // Without "Other".
    {
        if(app.categories().contains(m_subMenus[i].category()))
        {
            subMenuIndex = i;
            break;
        }
    }

    QMenu* menu = m_subMenus[subMenuIndex].menu();
    QList<QAction*> actions = menu->actions();
    QAction* before = NULL;
    for(int i = 0; i < actions.size(); i++)
    {
        if(actions[i]->text().compare(action->text(), Qt::CaseInsensitive) > 0)
        {
            before = actions[i];
            break;
        }
    }

    if(menu->actions().isEmpty())
    {
        QList<QAction*> actions = m_menu->actions();
        QAction* before = NULL;
        for(int i = 0; i < actions.size(); i++)
        {
            if(actions[i]->text().compare(menu->title(), Qt::CaseInsensitive) > 0)
            {
                before = actions[i];
                break;
            }
        }

        m_menu->insertMenu(before, menu);
    }

    menu->insertAction(before, action);


    m_actions[app.path()] = action;
}

void ApplicationsMenuApplet::applicationRemoved(const QString& path)
{
    if(m_actions.contains(path))
    {
        delete m_actions[path];
        m_actions.remove(path);
    }

    for(int i = 0; i < m_subMenus.size(); i++)
    {
        if(m_subMenus[i].menu()->actions().isEmpty())
            m_menu->removeAction(m_subMenus[i].menu()->menuAction());
    }
}

void ApplicationsMenuApplet::populateMenuFromDataStore()
{
    DesktopDataStore* dataStore = DesktopDataStore::instance();
    if (!dataStore) {
        qDebug() << "ApplicationsMenuApplet: Data store is null!";
        return;
    }
    
    // Get all desktop entries from data store
    QList<DesktopEntryData> entries = dataStore->getAllDesktopEntries();
    
    foreach (const DesktopEntryData& entryData, entries) {
        if (entryData.name.contains("cursor", Qt::CaseInsensitive) || 
            entryData.desktopFile.contains("cursor", Qt::CaseInsensitive)) {
        }
        addApplicationToMenu(entryData);
    }
}

void ApplicationsMenuApplet::addApplicationToMenu(const DesktopEntryData& entryData)
{
    if (!entryData.shouldShow() || !entryData.isValid) {
        return;
    }
    
    // Only show Application type entries
    if (entryData.type != "Application") {
        return;
    }
    
    // Remove existing action if it exists
    removeApplicationFromMenu(entryData.desktopFile);
    
    QAction* action = new QAction(m_menu);
    action->setIconVisibleInMenu(true);
    action->setData(entryData.desktopFile);
    action->setText(entryData.getDisplayName());
    
    // Use unified icon service for consistent icon loading
    QIcon icon = UnifiedIconService::instance()->loadIcon(entryData.icon, 32);
    action->setIcon(icon);
    
    connect(action, SIGNAL(triggered()), this, SLOT(actionTriggered()));
    
    // Add to relevant menu based on categories
    int subMenuIndex = m_subMenus.size() - 1; // By default put it in "Other"
    for (int i = 0; i < m_subMenus.size() - 1; i++) { // Without "Other"
        if (entryData.categories.contains(m_subMenus[i].category())) {
            subMenuIndex = i;
            break;
        }
    }
    
    QMenu* menu = m_subMenus[subMenuIndex].menu();
    QList<QAction*> actions = menu->actions();
    QAction* before = NULL;
    
    // Insert in alphabetical order
    for (int i = 0; i < actions.size(); i++) {
        if (entryData.getDisplayName() < actions[i]->text()) {
            before = actions[i];
            break;
        }
    }
    
    menu->insertAction(before, action);
    m_actions[entryData.desktopFile] = action;
    
    // Show the submenu if it has items
    if (menu->actions().size() == 1) {
        m_menu->addAction(menu->menuAction());
    }
}

void ApplicationsMenuApplet::removeApplicationFromMenu(const QString& desktopFile)
{
    if (m_actions.contains(desktopFile)) {
        QAction* action = m_actions[desktopFile];
        
        // Find which submenu contains this action
        for (int i = 0; i < m_subMenus.size(); i++) {
            QMenu* menu = m_subMenus[i].menu();
            if (menu->actions().contains(action)) {
                menu->removeAction(action);
                
                // Hide the submenu if it's empty
                if (menu->actions().isEmpty()) {
                    m_menu->removeAction(menu->menuAction());
                }
                break;
            }
        }
        
        delete action;
        m_actions.remove(desktopFile);
    }
}

void ApplicationsMenuApplet::onDataStoreApplicationAdded(const DesktopEntryData& entryData)
{
    addApplicationToMenu(entryData);
}

void ApplicationsMenuApplet::onDataStoreApplicationUpdated(const DesktopEntryData& entryData)
{
    addApplicationToMenu(entryData);
}

void ApplicationsMenuApplet::onDataStoreApplicationRemoved(const QString& desktopFile)
{
    removeApplicationFromMenu(desktopFile);
}

Applet* ApplicationsMenuAppletPlugin::createApplet(PanelWindow* panelWindow) {return new ApplicationsMenuApplet(panelWindow);}
