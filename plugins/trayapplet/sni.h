/* Minimal Status Notifier (SNI) watcher and item interfaces for Wayland */

#ifndef SNI_H
#define SNI_H

#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QIcon>
#include <QMap>

class SniItemProxy : public QObject {
    Q_OBJECT
public:
    explicit SniItemProxy(const QString &service, const QString &path, QObject *parent = nullptr);
    QString id() const { return m_id; }
    QIcon icon() const;
    void activate(int x, int y);
    void contextMenu(int x, int y);

signals:
    void changed();

private slots:
    void onPropertiesChanged(const QString &iface, const QVariantMap &changedProps, const QStringList &invalidatedProps);

private:
    QString m_service;
    QString m_path;
    QString m_id;
};

class SniWatcher : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.StatusNotifierWatcher")
    Q_PROPERTY(QStringList RegisteredStatusNotifierItems READ registeredItems)
    Q_PROPERTY(bool IsStatusNotifierHostRegistered READ isHostRegistered)
    Q_PROPERTY(int ProtocolVersion READ protocolVersion)
public:
    explicit SniWatcher(QObject *parent = nullptr);
    const QMap<QString, SniItemProxy*> &items() const { return m_items; }
    QStringList registeredItems() const;
    bool isHostRegistered() const { return true; }
    int protocolVersion() const { return 0; }

signals:
    void itemAdded(SniItemProxy* item);
    void itemRemoved(const QString &id);
    void StatusNotifierItemRegistered(const QString &service);
    void StatusNotifierItemUnregistered(const QString &service);
    void StatusNotifierHostRegistered();

public slots:
    // DBus method called by applications to register their tray icon
    void RegisterStatusNotifierItem(const QString &service);
    void RegisterStatusNotifierHost(const QString &service);

private slots:
    void onServiceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    void registerWatcher();
    void addItem(const QString &service, const QString &path);
    void removeItem(const QString &id);
    void queryExistingItems();
    void queryRegisteredItems();

    QDBusConnection m_bus;
    QMap<QString, SniItemProxy*> m_items; // key: unique id
    QStringList m_registeredServices; // List of registered service paths
};

#endif // SNI_H


