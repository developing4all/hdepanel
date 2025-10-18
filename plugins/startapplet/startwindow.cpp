#include "startwindow.h"
#include "ui_startwindow.h"

#include "../lib/dpisupport.h"
#include "../lib/desktopapplications.h"

#include <QMenu>
#include <QStyleFactory>
#include <QMap>
#include <QAction>

#include <QFile>
#include <QSettings>

#include <QFocusEvent>
#include <QDebug>
#include <QSettings>

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
    
    // Try to load the icon, with fallback to a generic folder icon if not found
    QIcon menuIcon = QIcon::fromTheme(icon);
    if (menuIcon.isNull()) {
        menuIcon = QIcon::fromTheme("folder");
    }
    if (menuIcon.isNull()) {
        menuIcon = QIcon::fromTheme("inode-directory");
    }
    
    m_menu->setIcon(menuIcon);
    m_menu->menuAction()->setIconVisibleInMenu(true);
    m_category = category;
}

static const char* menuStyleSheet =
"QMenu { background-color: black; }\n"
"QMenu::item { height: %dpx; background-color: transparent; color: white; padding-left: %dpx; padding-right: %dpx; padding-top: %dpx; padding-bottom: %dpx; }\n"
"QMenu::item::selected { background-color: #606060; border-color: gray; }\n"
"QMenu::icon { left: %dpx; }\n";





StartWindow::StartWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::StartWindow)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Widget | Qt::Popup);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    m_favorites  = new QMenu();

    // Remove backgroud color from lists
    ui->menuList->viewport()->setAutoFillBackground( false );
    ui->itemsList->viewport()->setAutoFillBackground( false );

    ui->exitButton->setIcon(QIcon::fromTheme("application-exit"));
    ui->settingsButton->setIcon(QIcon::fromTheme("preferences-system"));

    ui->profilePicture->setPixmap(QIcon::fromTheme("system-users").pixmap(64,64));

    ui->exitButton->setFocusProxy(this);
    ui->itemsList->setFocusProxy(this);
    ui->menuList->setFocusProxy(this);
    //ui->menuList->setSpacing(3);
    ui->itemsList->setSpacing(3);

    ui->profilePicture->setFocusProxy(this);
    ui->searchEdit->setFocusProxy(this);

    // Set fixed icon size for all menu group items
    ui->menuList->setIconSize(QSize(24, 24));

    connect(ui->exitButton, SIGNAL(clicked()), qApp, SLOT(quit()));

    addMenuItems();

    setProfileImage();

    // Update icons when theme changes
    QObject::connect(qApp, SIGNAL(iconThemeChanged(const QString&)), this, SLOT(refreshIcons()));
}

void StartWindow::readFavorites()
{
    m_favorites->clear();

    QSettings setting(this);
    QStringList favorites = setting.value("favorites", QStringList()).toStringList();
    
    // Add some sensible defaults on first run if favorites is empty
    if(favorites.isEmpty())
    {
        QStringList defaultFavorites;
        // Common applications that might be installed
        defaultFavorites << "/usr/share/applications/org.gnome.Nautilus.desktop"
                        << "/usr/share/applications/nautilus.desktop"
                        << "/usr/share/applications/thunar.desktop"
                        << "/usr/share/applications/org.gnome.Terminal.desktop"
                        << "/usr/share/applications/gnome-terminal.desktop"
                        << "/usr/share/applications/konsole.desktop"
                        << "/usr/share/applications/firefox.desktop"
                        << "/usr/share/applications/firefox-esr.desktop"
                        << "/usr/share/applications/chromium.desktop"
                        << "/usr/share/applications/org.gnome.gedit.desktop"
                        << "/usr/share/applications/gedit.desktop";
        
        // Add to favorites if the application exists
        foreach (QString appfile, defaultFavorites) {
            if(m_actions.contains(appfile) && m_actions[appfile] != 0)
            {
                favorites << appfile;
            }
        }
        
        // Save the defaults
        if(!favorites.isEmpty())
        {
            setting.setValue("favorites", favorites);
        }
    }

    foreach (QString appfile, favorites) {
        if(m_actions[appfile] != 0)
        {
            //qDebug() << m_actions[appfile]->text();
            m_favorites->addAction(m_actions[appfile]);
        }
    }
}

void StartWindow::refreshIcons()
{
    ui->exitButton->setIcon(QIcon::fromTheme("application-exit"));
    ui->settingsButton->setIcon(QIcon::fromTheme("preferences-system"));
    ui->profilePicture->setPixmap(QIcon::fromTheme("system-users").pixmap(64,64));
    updateMenuList();
}

void StartWindow::showContextMenuForWidget(const QPoint &pos)
{
    
    if(!m_contextMenu)
    {
        return;
    }
    
    m_contextMenu->clear();

    if(ui->itemsList->count() == 0)
    {
        return;
    }

    if((ui->menuList->currentItem() != 0) && (ui->menuList->currentItem()->text() == tr("Favorites")))
    {
        m_contextMenu->addAction( QIcon::fromTheme("emblem-favorite"), tr("Remove from favorite"), this, SLOT(removeFromFavorite()));
        m_contextMenu->addSeparator();
        m_contextMenu->addAction( QIcon::fromTheme("go-up"), tr("Move Up"), this, SLOT(moveFavoriteUp()));
        m_contextMenu->addAction( QIcon::fromTheme("go-down"), tr("Move Down"), this, SLOT(moveFavoriteDown()));
        m_contextMenu->addSeparator();
        m_contextMenu->addAction( QIcon::fromTheme("view-sort-ascending"), tr("Sort Alphabetically"), this, SLOT(sortFavoritesAlphabetically()));
    }
    else
    {
        m_contextMenu->addAction( QIcon::fromTheme("emblem-favorite"), tr("Add to favorite"), this, SLOT(addToFavorite()));
    }

    m_contextMenu->exec( ui->itemsList->mapToGlobal(pos));
}

void StartWindow::setProfileImage()
{
    QString name = qgetenv("USER");
    if (name.isEmpty())
        name = qgetenv("USERNAME");

    // /var/lib/AccountsService/icons/[user name]
    QFile imageFile("/var/lib/AccountsService/icons/" + name);
    if(imageFile.exists())
    {
        m_profileImage.load(imageFile.fileName());
    }

    // If not succeed try to load gnome3 profile image
    if(m_profileImage.isNull())
    {
        // /var/lib/AccountsService/user
        // [User]
        // Icon=
        QSettings setting("/var/lib/AccountsService/users/" + name, QSettings::IniFormat);
        setting.beginGroup("User");
        QString iconPath = setting.value("Icon").toString();
        if (!iconPath.isEmpty()) {
            m_profileImage.load(iconPath);
        }
        // qDebug() << "Icon: " << iconPath;
    }

    // If not succeed try to load KDE profile image
    if(name.isEmpty() || m_profileImage.isNull())
    {
        // $ENV{ HOME }/.face.png
        m_profileImage.load(qgetenv("USER") + "/.face.png");
    }

    if(!m_profileImage.isNull())
    {
        ui->profilePicture->setPixmap(m_profileImage);
    }
}

void StartWindow::addToFavorite()
{
    QListWidgetItem *item = ui->itemsList->currentItem();
    QSettings setting(this);
    QStringList favorites = setting.value("favorites", QStringList()).toStringList();
    //qDebug() << "favorites: " << favorites;
    QString itemsfile = item->data(Qt::UserRole).toString();
    if(!favorites.contains(itemsfile))
    {
        favorites << itemsfile;
        setting.setValue("favorites", favorites);
        readFavorites();
    }

    //qDebug() << itemsfile;
}

void StartWindow::removeFromFavorite()
{
    QListWidgetItem *item = ui->itemsList->currentItem();
    QSettings setting(this);
    QStringList favorites = setting.value("favorites", QStringList()).toStringList();
    //qDebug() << "favorites: " << favorites;
    QString itemsfile = item->data(Qt::UserRole).toString();
    if(favorites.contains(itemsfile))
    {
        favorites.removeAll(itemsfile);
        setting.setValue("favorites", favorites);
        readFavorites();
        //qDebug() << "Removed: " << itemsfile;
        on_menuList_itemActivated(ui->menuList->currentItem());
    }
}

void StartWindow::moveFavoriteUp()
{
    QListWidgetItem *item = ui->itemsList->currentItem();
    if(!item) return;
    
    QString itemsfile = item->data(Qt::UserRole).toString();
    QSettings setting(this);
    QStringList favorites = setting.value("favorites", QStringList()).toStringList();
    
    int index = favorites.indexOf(itemsfile);
    if(index > 0)  // Can only move up if not already at the top
    {
        favorites.move(index, index - 1);
        setting.setValue("favorites", favorites);
        on_menuList_itemActivated(ui->menuList->currentItem());
        
        // Select the item at its new position
        ui->itemsList->setCurrentRow(index - 1);
    }
}

void StartWindow::moveFavoriteDown()
{
    QListWidgetItem *item = ui->itemsList->currentItem();
    if(!item) return;
    
    QString itemsfile = item->data(Qt::UserRole).toString();
    QSettings setting(this);
    QStringList favorites = setting.value("favorites", QStringList()).toStringList();
    
    int index = favorites.indexOf(itemsfile);
    if(index >= 0 && index < favorites.size() - 1)  // Can only move down if not already at the bottom
    {
        favorites.move(index, index + 1);
        setting.setValue("favorites", favorites);
        on_menuList_itemActivated(ui->menuList->currentItem());
        
        // Select the item at its new position
        ui->itemsList->setCurrentRow(index + 1);
    }
}

void StartWindow::sortFavoritesAlphabetically()
{
    QSettings setting(this);
    QStringList favorites = setting.value("favorites", QStringList()).toStringList();
    
    if(favorites.isEmpty()) return;
    
    // Create a map of app names to their file paths
    QMap<QString, QString> nameToPath;
    foreach (QString appfile, favorites) {
        if(m_actions.contains(appfile) && m_actions[appfile] != 0)
        {
            nameToPath[m_actions[appfile]->text()] = appfile;
        }
    }
    
    // Get sorted list of names
    QStringList sortedNames = nameToPath.keys();
    sortedNames.sort(Qt::CaseInsensitive);
    
    // Build new sorted favorites list
    QStringList sortedFavorites;
    foreach (QString name, sortedNames) {
        sortedFavorites << nameToPath[name];
    }
    
    setting.setValue("favorites", sortedFavorites);
    on_menuList_itemActivated(ui->menuList->currentItem());
}

StartWindow::~StartWindow()
{
    foreach(QAction* action, m_actions)
    {
        delete action;
    }

    delete m_menu;

    delete ui;
}

void StartWindow::updateMenuList()
{
    //qDeleteAll(ui->menuList->items());
    ui->menuList->clear();

    const int itemWidth = ui->menuList->maximumWidth() - 5;
    const int itemHeight = 30;

    QListWidgetItem *allItem = new QListWidgetItem(QIcon::fromTheme("start-here"), tr("All"), ui->menuList );
    allItem->setSizeHint(QSize(itemWidth, itemHeight));

    QListWidgetItem *favoriteItem = new QListWidgetItem(QIcon::fromTheme("emblem-favorite"), tr("Favorites"), ui->menuList );
    favoriteItem->setSizeHint(QSize(itemWidth, itemHeight));

    QListWidgetItem *recentItem = new QListWidgetItem(QIcon::fromTheme("document-open-recent"), tr("Recently Used"), ui->menuList );
    recentItem->setSizeHint(QSize(itemWidth, itemHeight));
    
    // Add separator after Recently Used
    QListWidgetItem *separator = new QListWidgetItem(ui->menuList);
    separator->setFlags(Qt::NoItemFlags); // Make it non-selectable
    separator->setSizeHint(QSize(separator->sizeHint().width(), 5));
    separator->setBackground(QBrush(QColor(255, 255, 255, 0)));
    
    foreach(SubMenu submenu, m_subMenus)
    {
        //qDebug() << submenu.category();
        QListWidgetItem *newItem = new QListWidgetItem(submenu.menu()->icon(), submenu.menu()->title(), ui->menuList );
        newItem->setToolTip( submenu.menu()->toolTip() );
        newItem->setData(Qt::UserRole, submenu.category());
        newItem->setSizeHint(QSize(itemWidth, itemHeight));
    }
}

void StartWindow::addMenuItems()
{
    m_menu = new QMenu();
#if QT_VERSION >= 0x050000
    m_menu->setStyle(QStyleFactory::create("fusion"));
#else
    m_menu->setStyle(&m_style);
#endif

    m_subMenus.append(SubMenu(m_menu, tr("Accessories"), "Utility", "applications-accessories"));
    m_subMenus.append(SubMenu(m_menu, tr("Development"), "Development", "applications-development"));
    m_subMenus.append(SubMenu(m_menu, tr("Education"), "Education", "applications-science-symbolic"));
    m_subMenus.append(SubMenu(m_menu, tr("Office"), "Office", "applications-office"));
    m_subMenus.append(SubMenu(m_menu, tr("Graphics"), "Graphics", "applications-graphics"));
    m_subMenus.append(SubMenu(m_menu, tr("Multimedia"), "AudioVideo", "applications-multimedia"));
    m_subMenus.append(SubMenu(m_menu, tr("Games"), "Game", "applications-games"));
    m_subMenus.append(SubMenu(m_menu, tr("Network"), "Network", "applications-internet"));
    m_subMenus.append(SubMenu(m_menu, tr("System"), "System", "preferences-system"));
    m_subMenus.append(SubMenu(m_menu, tr("Settings"), "Settings", "preferences-desktop"));
    m_subMenus.append(SubMenu(m_menu, tr("Other"), "Other", "application-x-executable"));
}


bool StartWindow::init()
{
    connect(DesktopApplications::instance(), SIGNAL(applicationUpdated(DesktopApplication)), this, SLOT(applicationUpdated(DesktopApplication)));
    connect(DesktopApplications::instance(), SIGNAL(applicationRemoved(QString)), this, SLOT(applicationRemoved(QString)));

    QList<DesktopApplication> apps = DesktopApplications::instance()->applications();
    foreach(const DesktopApplication& app, apps)
    {
        applicationUpdated(app);
    }
    updateMenuList();

    // Context menu setup
    qDebug() << "Setting up context menu for itemsList";
    m_contextMenu = new QMenu(tr("Context menu"), ui->itemsList);
    ui->itemsList->setContextMenuPolicy(Qt::CustomContextMenu);
    
    bool connected = connect(ui->itemsList, SIGNAL(customContextMenuRequested(const QPoint &)),
                            this, SLOT(showContextMenuForWidget(const QPoint &)));
    qDebug() << "Context menu connection result:" << connected;
    
    if(!connected) {
        qDebug() << "WARNING: Failed to connect context menu signal!";
    }

    return true;
}


void StartWindow::applicationUpdated(const DesktopApplication& app)
{
    applicationRemoved(app.path());

    if(app.isNoDisplay())
        return;

    QAction* action = new QAction(m_menu);
    action->setIconVisibleInMenu(true);
    action->setData(app.path());
    action->setText(app.name());
    action->setIcon(QIcon(QPixmap::fromImage(app.iconImage())));

    if(action->icon().isNull())
    {
        action->setIcon(QIcon::fromTheme("unknown"));
    }
    if(action->icon().isNull())
    {
        QPixmap emptyPix = QPixmap(QSize(22,22));
        emptyPix.fill(Qt::transparent);
        action->setIcon(QIcon(emptyPix));
    }

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

void StartWindow::applicationRemoved(const QString& path)
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

void StartWindow::actionTriggered()
{
    DesktopApplications::instance()->launch(static_cast<QAction*>(sender())->data().toString());
}

void StartWindow::on_menuList_itemActivated(QListWidgetItem *item)
{
    ui->itemsList->clear();
    if(!item->data(Qt::UserRole).isNull())
    {
        QMenu *menu = getSubMenu(item->data(Qt::UserRole).toString());
        if(menu->isEmpty())
            return;

        foreach(QAction *action, menu->actions())
        {
            QListWidgetItem *newItem = new QListWidgetItem(action->icon(), action->text(), ui->itemsList );
            newItem->setToolTip( action->toolTip() );
            newItem->setData(Qt::UserRole, action->data());
        }
    }
    else if(item->text() == tr("All"))
    {
        foreach (QAction *action, m_actions) {
            QListWidgetItem *newItem = new QListWidgetItem(action->icon(), action->text(), ui->itemsList );
            newItem->setToolTip( action->toolTip() );
            newItem->setData(Qt::UserRole, action->data());
        }
    }    else if(item->text() == tr("Favorites"))
    {
        readFavorites();

        if(m_favorites->actions().isEmpty())
        {
            // Show helpful message when favorites is empty
            QListWidgetItem *helpItem = new QListWidgetItem(QIcon::fromTheme("help-about"), 
                                                            tr("Right-click any app to add to favorites"), 
                                                            ui->itemsList);
            helpItem->setFlags(helpItem->flags() & ~Qt::ItemIsSelectable);
            QFont italicFont = helpItem->font();
            italicFont.setItalic(true);
            helpItem->setFont(italicFont);
        }
        else
        {
            foreach (QAction *action, m_favorites->actions()) {
                QListWidgetItem *newItem = new QListWidgetItem(action->icon(), action->text(), ui->itemsList );
                newItem->setToolTip( action->toolTip() );
                newItem->setData(Qt::UserRole, action->data());
            }
        }

    }
    else if(item->text() == tr("Recently Used"))
    {
        // Load recently used applications from settings
        QSettings settings(this);
        QStringList recentApps = settings.value("recentlyUsed", QStringList()).toStringList();
        
        // Display up to 10 most recent apps
        int count = 0;
        foreach (QString appfile, recentApps) {
            if(count >= 10) break;
            
            if(m_actions.contains(appfile) && m_actions[appfile] != 0)
            {
                QListWidgetItem *newItem = new QListWidgetItem(m_actions[appfile]->icon(), m_actions[appfile]->text(), ui->itemsList);
                newItem->setToolTip(m_actions[appfile]->toolTip());
                newItem->setData(Qt::UserRole, m_actions[appfile]->data());
                count++;
            }
        }
    }
}

void StartWindow::on_itemsList_itemActivated(QListWidgetItem *item)
{
    qDebug() << "item activated: " << item->text();

    if(!item->data(Qt::UserRole).isNull())
    {
        QString appfile = item->data(Qt::UserRole).toString();
        
        // Track this as recently used
        QSettings settings(this);
        QStringList recentApps = settings.value("recentlyUsed", QStringList()).toStringList();
        
        // Remove if already in list (to move to top)
        recentApps.removeAll(appfile);
        
        // Add to front of list
        recentApps.prepend(appfile);
        
        // Keep only last 20 apps
        while(recentApps.size() > 20) {
            recentApps.removeLast();
        }
        
        // Save updated list
        settings.setValue("recentlyUsed", recentApps);
        
        // Launch the application
        DesktopApplications::instance()->launch(appfile);
        hide();
    }
}

void StartWindow::on_menuList_itemClicked(QListWidgetItem *item)
{
    return on_menuList_itemActivated(item);
}

void StartWindow::on_itemsList_itemClicked(QListWidgetItem *item)
{
    return on_itemsList_itemActivated(item);
}

QMenu *StartWindow::getSubMenu(const QString &category)
{
    // Add to relevant menu.
    int subMenuIndex = m_subMenus.size() - 1; // By default put it in "Other".
    for(int i = 0; i < m_subMenus.size() - 1; i++) // Without "Other".
    {
        if(category == m_subMenus[i].category())
        {
            subMenuIndex = i;
            break;
        }
    }

    return m_subMenus[subMenuIndex].menu();
}


void StartWindow::on_searchEdit_textChanged(const QString &arg1)
{
    ui->menuList->setCurrentRow(2);
    ui->itemsList->clear();
    foreach (QAction *action, m_actions) {
        if(action->text().contains(arg1, Qt::CaseInsensitive) || action->data().toString().contains(arg1, Qt::CaseInsensitive))
        {
            QListWidgetItem *searchItem = new QListWidgetItem(action->icon(), action->text(), ui->itemsList );
            searchItem->setToolTip( action->toolTip() );
            searchItem->setData(Qt::UserRole, action->data());
        }
    }
}

void StartWindow::setFocused()
{
    //ui->searchEdit->setFocus();
    ui->searchEdit->grabKeyboard();
    setFocus();

    ui->searchEdit->clear();
    ui->menuList->setCurrentRow(0);
    on_menuList_itemActivated(ui->menuList->currentItem());
}


void StartWindow::focusOutEvent(QFocusEvent *)
{
    if(m_contextMenu->isVisible())
        return;

    hide();
}
