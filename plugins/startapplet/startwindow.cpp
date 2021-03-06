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
    m_menu->setIcon(QIcon::fromTheme(icon));
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

    connect(ui->exitButton, SIGNAL(clicked()), qApp, SLOT(quit()));

    addMenuItems();

    setProfileImage();
}

void StartWindow::readFavorites()
{
    m_favorites->clear();

    QSettings setting(this);
    QStringList favorites = setting.value("favorites", QStringList()).toStringList();

    foreach (QString appfile, favorites) {
        if(m_actions[appfile] != 0)
        {
            //qDebug() << m_actions[appfile]->text();
            m_favorites->addAction(m_actions[appfile]);
        }
    }
}

void StartWindow::showContextMenuForWidget(const QPoint &pos)
{
    m_contextMenu->clear();

    if(ui->itemsList->count() == 0)
    {
        return;
    }

    if((ui->menuList->currentItem() != 0) && (ui->menuList->currentItem()->text() == tr("Favorites")))
    {
        m_contextMenu->addAction( QIcon::fromTheme("emblem-favorite"), tr("Remove from favorite"), this, SLOT(removeFromFavorite()));
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
        m_profileImage.load(setting.value("Icon").toString());
        qDebug() << "Icon: " << setting.value("Icon").toString();
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

    QListWidgetItem *favoriteItem = new QListWidgetItem(QIcon::fromTheme("emblem-favorite"), "Favorites", ui->menuList );
    favoriteItem->setSizeHint(QSize(favoriteItem->sizeHint().width(), 30));

    QListWidgetItem *recentItem = new QListWidgetItem(QIcon::fromTheme("document-open-recent"), "Recently Used", ui->menuList );
    recentItem->setSizeHint(QSize(recentItem->sizeHint().width(), 30));

    QListWidgetItem *allItem = new QListWidgetItem(QIcon::fromTheme("start-here"), "All", ui->menuList );
    allItem->setSizeHint(QSize(allItem->sizeHint().width(), 30));

    foreach(SubMenu submenu, m_subMenus)
    {
        //qDebug() << submenu.category();
        QListWidgetItem *newItem = new QListWidgetItem(submenu.menu()->icon(), submenu.menu()->title(), ui->menuList );
        newItem->setToolTip( submenu.menu()->toolTip() );
        newItem->setData(Qt::UserRole, submenu.category());
        newItem->setSizeHint(QSize(newItem->sizeHint().width(), 28));
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
    //m_menu->setFont(m_panelWindow->font());
    m_menu->setStyleSheet(QString().sprintf(menuStyleSheet,
        adjustHardcodedPixelSize(36),
        adjustHardcodedPixelSize(38),
        adjustHardcodedPixelSize(20),
        adjustHardcodedPixelSize(2),
        adjustHardcodedPixelSize(2),
        adjustHardcodedPixelSize(2)
    ));

    m_subMenus.append(SubMenu(m_menu, "Accessories", "Utility", "applications-accessories"));
    m_subMenus.append(SubMenu(m_menu, "Development", "Development", "applications-development"));
    m_subMenus.append(SubMenu(m_menu, "Education", "Education", "applications-science"));
    m_subMenus.append(SubMenu(m_menu, "Office", "Office", "applications-office"));
    m_subMenus.append(SubMenu(m_menu, "Graphics", "Graphics", "applications-graphics"));
    m_subMenus.append(SubMenu(m_menu, "Multimedia", "AudioVideo", "applications-multimedia"));
    m_subMenus.append(SubMenu(m_menu, "Games", "Game", "applications-games"));
    m_subMenus.append(SubMenu(m_menu, "Network", "Network", "applications-internet"));
    m_subMenus.append(SubMenu(m_menu, "System", "System", "preferences-system"));
    m_subMenus.append(SubMenu(m_menu, "Settings", "Settings", "preferences-desktop"));
    m_subMenus.append(SubMenu(m_menu, "Other", "Other", "applications-other"));
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

    // Context menu
    m_contextMenu = new QMenu(tr("Context menu"), ui->itemsList);
    ui->itemsList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->itemsList, SIGNAL(customContextMenuRequested(const QPoint &)),
             SLOT(showContextMenuForWidget(const QPoint &)));


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
    else if(item->text() == "All")
    {
        foreach (QAction *action, m_actions) {
            QListWidgetItem *newItem = new QListWidgetItem(action->icon(), action->text(), ui->itemsList );
            newItem->setToolTip( action->toolTip() );
            newItem->setData(Qt::UserRole, action->data());
        }
    }else if(item->text() == "Favorites")
    {
        readFavorites();

        foreach (QAction *action, m_favorites->actions()) {
            QListWidgetItem *newItem = new QListWidgetItem(action->icon(), action->text(), ui->itemsList );
            newItem->setToolTip( action->toolTip() );
            newItem->setData(Qt::UserRole, action->data());
        }

    }
    else if(item->text() == "Recently Used")
    {
        // https://specifications.freedesktop.org/recent-file-spec/recent-file-spec-0.2.html
        // filename defaults to ~/.recently-used
    }
}

void StartWindow::on_itemsList_itemActivated(QListWidgetItem *item)
{
    qDebug() << "item activated: " << item->text();

    if(!item->data(Qt::UserRole).isNull())
    {
        DesktopApplications::instance()->launch(item->data(Qt::UserRole).toString());
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
