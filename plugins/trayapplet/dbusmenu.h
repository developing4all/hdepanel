#ifndef DBUSMENU_H
#define DBUSMENU_H

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QMenu>
#include <QPoint>
#include <QVariantMap>

struct DbusMenuLayout {
    int id = 0;
    QVariantMap properties;
    QList<DbusMenuLayout> children;
};

Q_DECLARE_METATYPE(DbusMenuLayout)

const QDBusArgument &operator>>(const QDBusArgument &argument, DbusMenuLayout &layout);

class DbusMenuClient {
public:
    DbusMenuClient(const QString &service, const QString &path, const QDBusConnection &bus);

    bool isValid() const;
    bool popupAt(const QPoint &globalPos) const;

private:
    bool fetchLayout(DbusMenuLayout &outLayout) const;
    void populateMenu(QMenu *menu, const DbusMenuLayout &layout) const;
    void sendClickedEvent(int id) const;

    QString m_service;
    QString m_path;
    QDBusConnection m_bus;
};

#endif // DBUSMENU_H



