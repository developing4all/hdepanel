#ifndef STARTWINDOW_H
#define STARTWINDOW_H

#include <QtCore/QList>
#include <QtCore/QMap>
#if QT_VERSION >= 0x050000
#include <QAction>
#include <QStyleFactory>
#else
#include <QtGui/QAction>
#include <QtGui/QPlastiqueStyle>
#endif

#include <QWidget>

class QMenu;

namespace Ui {
class StartWindow;
}


class SubMenu
{
public:
    SubMenu()
    {
    }

    SubMenu(QMenu* parent, const QString& title, const QString& category, const QString& icon);

    QMenu* menu()
    {
        return m_menu;
    }

    const QString& category() const
    {
        return m_category;
    }

private:
    QMenu* m_menu;
    QString m_category;
};

class DesktopApplication;
class QListWidgetItem;

class StartWindow : public QWidget
{
    Q_OBJECT

public:
    explicit StartWindow(QWidget *parent = 0);
    ~StartWindow();

    bool init();
    void close(){}

protected:
    void layoutChanged();
    bool isHighlighted();
    void focusOutEvent(QFocusEvent *);

private slots:
    void actionTriggered();
    void applicationUpdated(const DesktopApplication& app);
    void applicationRemoved(const QString& path);
    void showContextMenuForWidget(const QPoint &pos);
    void addToFavorite();
    void removeFromFavorite();

    void on_menuList_itemActivated(QListWidgetItem *item);

    void on_itemsList_itemActivated(QListWidgetItem *item);

    void on_menuList_itemClicked(QListWidgetItem *item);

    void on_itemsList_itemClicked(QListWidgetItem *item);

    void on_searchEdit_textChanged(const QString &arg1);

public slots:
    void fontChanged(){}
    void setFocused();

private:
    void addMenuItems();
    void updateMenuList();
    void setProfileImage();
    void readFavorites();

    QMenu *getSubMenu(const QString &category);
    Ui::StartWindow *ui;

    QMenu* m_menu;
    QMenu *m_contextMenu;
    QMenu *m_favorites;

    QList<SubMenu> m_subMenus;
    QMap<QString, QAction*> m_actions;
    QPixmap m_profileImage;
};

#endif // STARTWINDOW_H
