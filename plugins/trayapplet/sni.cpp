#include "sni.h"
#include <QDBusInterface>
#include <QDBusVariant>
#include <QDBusArgument>
#include <QBuffer>

static const char* SNI_WATCHER = "org.kde.StatusNotifierWatcher";
static const char* SNI_ITEM_IFACE = "org.kde.StatusNotifierItem";

SniItemProxy::SniItemProxy(const QString &service, const QString &path, QObject *parent)
    : QObject(parent), m_service(service), m_path(path)
{
    m_id = service + path;
}

QIcon SniItemProxy::icon() const
{
    // Try IconPixmap property first (array of (w,h,bytes))
    QDBusInterface iface(m_service, m_path, SNI_ITEM_IFACE, QDBusConnection::sessionBus());
    QVariant v = iface.property("IconPixmap");
    if (v.isValid()) {
        // Minimal: ignore, fall back to IconName due to complex parsing
    }
    QString name = iface.property("IconName").toString();
    if (!name.isEmpty()) return QIcon::fromTheme(name);
    return QIcon();
}

SniWatcher::SniWatcher(QObject *parent)
    : QObject(parent), m_bus(QDBusConnection::sessionBus())
{
    registerWatcher();
    // Listen for items registering via DBus service owner changes under org.kde.StatusNotifierItem.*
    m_bus.connect(QString(), QString(), "org.freedesktop.DBus", "NameOwnerChanged",
                  this, SLOT(onServiceOwnerChanged(QString,QString,QString)));
}

void SniWatcher::registerWatcher()
{
    // Announce as SNI watcher to encourage apps to register
    m_bus.registerService(SNI_WATCHER);
    m_bus.registerObject("/StatusNotifierWatcher", this,
                         QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableProperties);
}

void SniWatcher::addItem(const QString &service, const QString &path)
{
    QString key = service + path;
    if (m_items.contains(key)) return;
    SniItemProxy* item = new SniItemProxy(service, path, this);
    m_items.insert(key, item);
    emit itemAdded(item);
}

void SniWatcher::removeItem(const QString &id)
{
    auto it = m_items.find(id);
    if (it == m_items.end()) return;
    SniItemProxy* item = it.value();
    m_items.erase(it);
    emit itemRemoved(id);
    item->deleteLater();
}

void SniWatcher::onServiceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    // Items usually register under names like org.kde.StatusNotifierItem-... or app-specific unique names
    if (name.startsWith("org.kde.StatusNotifierItem")) {
        if (!newOwner.isEmpty()) {
            // Probe the item for ObjectPath property to determine menu/icon path
            QDBusInterface iface(name, "/StatusNotifierItem", SNI_ITEM_IFACE, QDBusConnection::sessionBus());
            if (iface.isValid()) {
                addItem(name, "/StatusNotifierItem");
            }
        } else {
            removeItem(name + "/StatusNotifierItem");
        }
    }
}


