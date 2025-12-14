#include "sni.h"
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDBusArgument>
#include <QBuffer>
#include <QTimer>
#include <QDebug>
#include <QStringList>

static const char* SNI_WATCHER = "org.kde.StatusNotifierWatcher";
static const char* SNI_ITEM_IFACE = "org.kde.StatusNotifierItem";

SniItemProxy::SniItemProxy(const QString &service, const QString &path, QObject *parent)
    : QObject(parent), m_service(service), m_path(path)
{
    m_id = service + path;
}

QIcon SniItemProxy::icon() const
{
    QDBusInterface iface(m_service, m_path, SNI_ITEM_IFACE, QDBusConnection::sessionBus());
    
    // Try IconName first (simpler, doesn't require complex type registration)
    QString name = iface.property("IconName").toString();
    if (!name.isEmpty()) {
        return QIcon::fromTheme(name);
    }
    
    // IconPixmap is a complex type (array of (iiay)) that requires type registration.
    // Since we're not parsing it anyway, we skip it to avoid the warning.
    // If needed in the future, we'd need to register the type with:
    // qDBusRegisterMetaType<IconPixmapArray>();
    
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
    if (!m_bus.registerService(SNI_WATCHER)) {
        qDebug() << "Failed to register SNI watcher service, another watcher may be active";
    }
    if (!m_bus.registerObject("/StatusNotifierWatcher", this,
                         QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableProperties)) {
        qDebug() << "Failed to register SNI watcher object";
    }
    
    // Query for existing items that may have registered before we started
    // Only query once after a short delay to let applications register
    QTimer::singleShot(500, this, &SniWatcher::queryExistingItems);
    QTimer::singleShot(1000, this, &SniWatcher::queryRegisteredItems);
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

QStringList SniWatcher::registeredItems() const
{
    return m_registeredServices;
}

void SniWatcher::RegisterStatusNotifierItem(const QString &service)
{
    qDebug() << "RegisterStatusNotifierItem called with service:" << service;
    
    // Add to registered services list
    if (!m_registeredServices.contains(service)) {
        m_registeredServices.append(service);
    }
    
    // Extract service name and path from the service string
    // Format can be:
    // - "service_name" (well-known name)
    // - "service_name/path" (well-known name with path)
    // - ":1.104@/StatusNotifierItem" (unique name with @ separator and path)
    // - ":1.104" (unique name without path)
    QString serviceName = service;
    QString path = "/StatusNotifierItem";
    
    // Check for @ separator (used by some implementations to separate unique name from path)
    int atPos = service.indexOf('@');
    if (atPos > 0) {
        // Format: ":1.104@/StatusNotifierItem"
        serviceName = service.left(atPos);
        QString afterAt = service.mid(atPos + 1);
        if (!afterAt.isEmpty() && afterAt.startsWith('/')) {
            path = afterAt;
        }
    } else {
        // No @ separator, check for / separator
        int slashPos = service.indexOf('/');
        if (slashPos > 0) {
            // Format: "service_name/path"
            serviceName = service.left(slashPos);
            path = service.mid(slashPos);
        } else {
            // No path specified, use default
            serviceName = service;
            path = "/StatusNotifierItem";
        }
    }
    
    // Validate service name (must be non-empty and valid D-Bus name format)
    if (serviceName.isEmpty()) {
        qDebug() << "Invalid service name (empty), skipping";
        return;
    }
    
    qDebug() << "Adding SNI item - service:" << serviceName << "path:" << path;
    addItem(serviceName, path);
}

void SniWatcher::queryRegisteredItems()
{
    // Check if there's another StatusNotifierWatcher that has registered items
    // Some applications might have registered with a previous watcher
    QDBusInterface watcherInterface(SNI_WATCHER, "/StatusNotifierWatcher", 
                                     "org.kde.StatusNotifierWatcher", m_bus);
    if (watcherInterface.isValid()) {
        QVariant prop = watcherInterface.property("RegisteredStatusNotifierItems");
        if (prop.isValid()) {
            QStringList registered = prop.toStringList();
            qDebug() << "Found" << registered.size() << "registered SNI items from watcher";
            for (const QString &service : registered) {
                if (!m_registeredServices.contains(service)) {
                    qDebug() << "Processing registered SNI item:" << service;
                    RegisterStatusNotifierItem(service);
                }
            }
        }
    }
}

static bool isSystemService(const QString &service)
{
    // Filter out system services that are not tray icons
    return service.startsWith("org.freedesktop.") ||
           service.startsWith("org.gnome.") ||
           service.startsWith("org.kde.StatusNotifierWatcher") ||
           service.startsWith("org.a11y.") ||
           service.startsWith("org.gtk.") ||
           service.startsWith("org.pipewire.") ||
           service.startsWith("org.pulseaudio.") ||
           service.startsWith("org.freedesktop.impl.portal.") ||
           service.startsWith("org.freedesktop.portal.") ||
           service.startsWith("org.freedesktop.ReserveDevice") ||
           service.startsWith("org.freedesktop.secrets") ||
           service.startsWith("org.freedesktop.systemd") ||
           service.startsWith(":1."); // DBus unique names (usually system services)
}

static bool hasTrayIconProperties(const QString &service, const QString &path, const QDBusConnection &bus)
{
    // Check if the service actually has tray icon properties
    QDBusInterface iface(service, path, SNI_ITEM_IFACE, bus);
    if (!iface.isValid()) {
        return false;
    }
    
    // Check for IconName - real tray icons will have this
    QString iconName = iface.property("IconName").toString();
    
    // Also check for Id property - real tray icons should have this
    QString id = iface.property("Id").toString();
    
    // Skip IconPixmap to avoid type registration warning - IconName is sufficient
    // If it has an icon name or an ID, it's likely a real tray icon
    return !iconName.isEmpty() || !id.isEmpty();
}

void SniWatcher::queryExistingItems()
{
    // Only query services that have explicitly registered via RegisterStatusNotifierItem
    // This avoids checking all system services
    if (m_registeredServices.isEmpty()) {
        qDebug() << "No registered SNI services to query";
        return;
    }
    
    qDebug() << "Querying" << m_registeredServices.size() << "registered SNI services";
    
    for (const QString &servicePath : m_registeredServices) {
        // Extract service name and path (same logic as RegisterStatusNotifierItem)
        QString serviceName = servicePath;
        QString path = "/StatusNotifierItem";
        
        // Check for @ separator first
        int atPos = servicePath.indexOf('@');
        if (atPos > 0) {
            serviceName = servicePath.left(atPos);
            QString afterAt = servicePath.mid(atPos + 1);
            if (!afterAt.isEmpty() && afterAt.startsWith('/')) {
                path = afterAt;
            }
        } else {
            // Check for / separator
            int slashPos = servicePath.indexOf('/');
            if (slashPos > 0) {
                serviceName = servicePath.left(slashPos);
                path = servicePath.mid(slashPos);
            }
        }
        
        // Validate service name
        if (serviceName.isEmpty()) {
            continue;
        }
        
        // Skip system services
        if (isSystemService(serviceName)) {
            continue;
        }
        
        // Check if it has tray icon properties
        if (hasTrayIconProperties(serviceName, path, m_bus)) {
            QString key = serviceName + path;
            if (!m_items.contains(key)) {
                qDebug() << "Found registered SNI item:" << serviceName << "at path" << path;
                addItem(serviceName, path);
            }
        }
    }
    
    // Also check for services with "StatusNotifierItem" in their name (but filter system services)
    QDBusInterface dbusInterface("org.freedesktop.DBus", "/org/freedesktop/DBus",
                                  "org.freedesktop.DBus", m_bus);
    QDBusReply<QStringList> reply = dbusInterface.call("ListNames");
    if (reply.isValid()) {
        QStringList services = reply.value();
        for (const QString &service : services) {
            // Only check services that explicitly have StatusNotifierItem in the name
            // and are not system services
            if (service.contains("StatusNotifierItem") && !isSystemService(service)) {
                QDBusInterface iface(service, "/StatusNotifierItem", SNI_ITEM_IFACE, m_bus);
                if (iface.isValid() && hasTrayIconProperties(service, "/StatusNotifierItem", m_bus)) {
                    QString key = service + "/StatusNotifierItem";
                    if (!m_items.contains(key)) {
                        qDebug() << "Found SNI item by name:" << service;
                        addItem(service, "/StatusNotifierItem");
                    }
                }
            }
        }
    }
}

void SniWatcher::onServiceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    // Items usually register under names like org.kde.StatusNotifierItem-... or app-specific unique names
    if (name.startsWith("org.kde.StatusNotifierItem") || name.contains("StatusNotifierItem")) {
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


