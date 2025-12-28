#include "sni.h"
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDBusArgument>
#include <QBuffer>
#include <QTimer>
#include <QDebug>
#include <QStringList>
#include <QImage>
#include <QtEndian>
#include <QPixmap>
#include <QFile>
#include <QDBusMetaType>
#include "dbusmenu.h"

QDBusArgument &operator<<(QDBusArgument &argument, const SniPixmap &pixmap)
{
    argument.beginStructure();
    argument << pixmap.width << pixmap.height << pixmap.data;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SniPixmap &pixmap)
{
    argument.beginStructure();
    argument >> pixmap.width >> pixmap.height >> pixmap.data;
    argument.endStructure();
    return argument;
}

static const char* SNI_WATCHER = "org.kde.StatusNotifierWatcher";
static const char* SNI_ITEM_IFACE = "org.kde.StatusNotifierItem";

static QString getNameOwnerOrSelf(const QString& service, const QDBusConnection& bus)
{
    Q_UNUSED(bus)
    // Avoid synchronous GetNameOwner() calls (they can block the UI during panel resets).
    return service;
}

static QString dbusVariantToString(const QVariant& v)
{
    if (!v.isValid()) return QString();
    if (v.canConvert<QDBusVariant>()) {
        return qvariant_cast<QDBusVariant>(v).variant().toString();
    }
    return v.toString();
}

SniItemProxy::SniItemProxy(const QString &service, const QString &path, QObject *parent)
    : QObject(parent), m_service(service), m_path(path)
{
    // Unique key used by SniWatcher (must remain service+path)
    m_id = service + path;
    m_iface = new QDBusInterface(m_service, m_path, SNI_ITEM_IFACE, QDBusConnection::sessionBus(), this);
    m_ifaceFd = new QDBusInterface(m_service, m_path, "org.freedesktop.StatusNotifierItem", QDBusConnection::sessionBus(), this);

    // Read stable app id (SNI "Id" property) for de-duplication.
    // This is a property read, so it should be fast and is already consistent with other synchronous reads (icon(), hasMenu(), etc).
    QDBusInterface *iface = (m_iface && m_iface->isValid()) ? m_iface : m_ifaceFd;
    if (iface && iface->isValid()) {
        m_appId = dbusVariantToString(iface->property("Id"));
    }
    
    // Monitor property changes to update the icon/status dynamically
    QDBusConnection::sessionBus().connect(m_service, m_path, "org.freedesktop.DBus.Properties", "PropertiesChanged",
                                           this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
}

SniItemProxy::~SniItemProxy()
{
}

void SniItemProxy::onPropertiesChanged(const QString &iface, const QVariantMap &changedProps, const QStringList &invalidatedProps)
{
    Q_UNUSED(iface)
    Q_UNUSED(changedProps)
    Q_UNUSED(invalidatedProps)
    emit changed();
}

QIcon SniItemProxy::icon() const
{
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    if (!m_iface || !m_iface->isValid()) {
        if (!m_ifaceFd || !m_ifaceFd->isValid()) {
            if (trayDebug) {
                qDebug() << "SniItemProxy::icon - No valid interface for" << m_service;
            }
            return QIcon::fromTheme("image-missing");
        }
    }

    QDBusInterface *iface = (m_iface && m_iface->isValid()) ? m_iface : m_ifaceFd;
    QString ifaceName = (m_iface && m_iface->isValid()) ? SNI_ITEM_IFACE : "org.freedesktop.StatusNotifierItem";
    
    auto tryIcon = [&](const QString &propName) -> QIcon {
        QVariant v = iface->property(propName.toUtf8().constData());
        if (!v.isValid()) return QIcon();
        QString name;
        if (v.canConvert<QDBusVariant>()) {
            name = qvariant_cast<QDBusVariant>(v).variant().toString();
        } else {
            name = v.toString();
        }

        if (name.isEmpty()) return QIcon();

        // Check if it's an absolute path
        if (name.startsWith('/') && QFile::exists(name)) {
            return QIcon(name);
        }

        QString themePath = iface->property("IconThemePath").toString();
        if (!themePath.isEmpty()) {
            QStringList paths = QIcon::themeSearchPaths();
            if (!paths.contains(themePath)) {
                paths.prepend(themePath);
                QIcon::setThemeSearchPaths(paths);
            }
        }
        
        QIcon icon = QIcon::fromTheme(name);
        if (!icon.isNull() && !icon.availableSizes().isEmpty()) {
            return icon;
    }

    return QIcon();
    };

    // 1. Try IconName
    QIcon icon = tryIcon("IconName");
    if (!icon.isNull()) return icon;

    // 2. Try AttentionIconName
    icon = tryIcon("AttentionIconName");
    if (!icon.isNull()) return icon;

    // 3. Try IconPixmap using a low-level call to avoid the Qt5 "QDBusRawType" crash
    if (m_service.isEmpty() || m_service.startsWith('/')) {
        return QIcon::fromTheme("image-missing");
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, m_path, "org.freedesktop.DBus.Properties", "Get");
    msg << ifaceName << "IconPixmap";
    
    QDBusMessage replyMsg = QDBusConnection::sessionBus().call(msg);
    if (replyMsg.type() == QDBusMessage::ReplyMessage && !replyMsg.arguments().isEmpty()) {
        QVariant v = replyMsg.arguments().at(0);
        // The return type of Get is Variant, so we get a QVariant containing our actual data
        QVariant pixmapProp;
        if (v.canConvert<QDBusVariant>()) {
            pixmapProp = qvariant_cast<QDBusVariant>(v).variant();
        } else {
            pixmapProp = v;
        }

        if (pixmapProp.isValid()) {
            SniPixmapList pixmaps;
            if (pixmapProp.canConvert<SniPixmapList>()) {
                pixmaps = qvariant_cast<SniPixmapList>(pixmapProp);
            } else if (pixmapProp.userType() == qMetaTypeId<QDBusArgument>()) {
                const QDBusArgument &arg = pixmapProp.value<QDBusArgument>();
                if (arg.currentType() == QDBusArgument::ArrayType) {
                    arg.beginArray();
                    while (!arg.atEnd()) {
                        SniPixmap p;
                        arg >> p;
                        pixmaps.append(p);
                    }
                    arg.endArray();
                }
            }

            if (!pixmaps.isEmpty()) {
                QImage bestImg;
                for (const SniPixmap &sniPix : pixmaps) {
                    int w = sniPix.width;
                    int h = sniPix.height;
                    const QByteArray &data = sniPix.data;
                    
                    if (w > 0 && h > 0 && data.size() >= w * h * 4) {
                        QImage img(w, h, QImage::Format_ARGB32);
                        for (int y = 0; y < h; ++y) {
                            uint *dest = (uint*)img.scanLine(y);
                            const uint *src = (const uint*)(data.constData() + y * w * 4);
                            for (int x = 0; x < w; ++x) {
                                dest[x] = qFromBigEndian(src[x]);
                            }
                        }
                        
                        if (bestImg.isNull() || (w >= 24 && (bestImg.width() < 24 || w < bestImg.width()))) {
                            bestImg = img;
                        }
                    }
                }
                if (!bestImg.isNull()) {
                    return QIcon(QPixmap::fromImage(bestImg));
                }
            }
        }
    }

    if (trayDebug) {
        qDebug() << "SniItemProxy::icon - Failed to find icon for" << m_service << "path" << m_path;
    }
    return QIcon::fromTheme("image-missing");
}

void SniItemProxy::activate(int x, int y)
{
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    if (trayDebug) {
        qDebug() << "SniItemProxy::activate - service:" << m_service << "path:" << m_path << "x:" << x << "y:" << y;
    }
    if (m_iface && m_iface->isValid()) {
        m_iface->call(QDBus::NoBlock, "Activate", x, y);
    }
    if (m_ifaceFd && m_ifaceFd->isValid()) {
        m_ifaceFd->call(QDBus::NoBlock, "Activate", x, y);
    }
}

void SniItemProxy::secondaryActivate(int x, int y)
{
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    if (trayDebug) {
        qDebug() << "SniItemProxy::secondaryActivate - service:" << m_service << "path:" << m_path << "x:" << x << "y:" << y;
    }
    if (m_iface && m_iface->isValid()) {
        m_iface->call(QDBus::NoBlock, "SecondaryActivate", x, y);
    }
    if (m_ifaceFd && m_ifaceFd->isValid()) {
        m_ifaceFd->call(QDBus::NoBlock, "SecondaryActivate", x, y);
    }
}

void SniItemProxy::contextMenu(int x, int y)
{
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    if (trayDebug) {
        qDebug() << "SniItemProxy::contextMenu - service:" << m_service << "path:" << m_path << "x:" << x << "y:" << y;
    }
    if (m_iface && m_iface->isValid()) {
        m_iface->call(QDBus::NoBlock, "ContextMenu", x, y);
    }
    if (m_ifaceFd && m_ifaceFd->isValid()) {
        m_ifaceFd->call(QDBus::NoBlock, "ContextMenu", x, y);
    }
}

static QDBusObjectPath readObjectPathProperty(QDBusInterface *iface, const char *propName)
{
    if (!iface || !iface->isValid())
        return QDBusObjectPath();

    QVariant v = iface->property(propName);
    if (!v.isValid())
        return QDBusObjectPath();

    QVariant inner = v;
    if (v.canConvert<QDBusVariant>())
        inner = qvariant_cast<QDBusVariant>(v).variant();

    if (inner.canConvert<QDBusObjectPath>())
        return qvariant_cast<QDBusObjectPath>(inner);

    const QString s = inner.toString();
    if (!s.isEmpty() && s.startsWith(QLatin1Char('/')))
        return QDBusObjectPath(s);

    return QDBusObjectPath();
}

bool SniItemProxy::hasMenu() const
{
    QDBusObjectPath p = readObjectPathProperty(m_iface, "Menu");
    if (p.path().isEmpty() || p.path() == "/") {
        p = readObjectPathProperty(m_ifaceFd, "Menu");
    }
    return !p.path().isEmpty() && p.path() != "/" && p.path().startsWith(QLatin1Char('/'));
}

bool SniItemProxy::popupMenu(int x, int y) const
{
    QDBusObjectPath p = readObjectPathProperty(m_iface, "Menu");
    if (p.path().isEmpty() || p.path() == "/") {
        p = readObjectPathProperty(m_ifaceFd, "Menu");
    }
    if (p.path().isEmpty() || p.path() == "/" || !p.path().startsWith(QLatin1Char('/')))
        return false;

    DbusMenuClient client(m_service, p.path(), QDBusConnection::sessionBus());
    if (!client.isValid())
        return false;

    return client.popupAt(QPoint(x, y));
}

SniWatcher::SniWatcher(QObject *parent)
    : QObject(parent), m_bus(QDBusConnection::sessionBus())
{
    qDBusRegisterMetaType<SniPixmap>();
    qDBusRegisterMetaType<SniPixmapList>();
    
    registerWatcher();
    // Listen for items registering via DBus service owner changes under org.kde.StatusNotifierItem.*
    m_bus.connect(QString(), QString(), "org.freedesktop.DBus", "NameOwnerChanged",
                  this, SLOT(onServiceOwnerChanged(QString,QString,QString)));
}

SniWatcher::~SniWatcher()
{
    // Critical for panel reset/reorder: release DBus object/service so the next watcher can register.
    m_bus.unregisterObject("/StatusNotifierWatcher");
    m_bus.unregisterService(SNI_WATCHER);

    m_items.clear();
    m_registeredServices.clear();
}

void SniWatcher::registerWatcher()
{
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    // Register the object first so it's available as soon as the service is registered
    if (!m_bus.registerObject("/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher", this,
                         QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties)) {
        if (trayDebug) {
            qDebug() << "Failed to register SNI watcher object";
        }
    }

    // Announce as SNI watcher to encourage apps to register
    if (!m_bus.registerService(SNI_WATCHER)) {
        if (trayDebug) {
            qDebug() << "Failed to register SNI watcher service, another watcher may be active";
        }
    }
    
    // Query for existing items that may have registered before we started.
    // Do this in two phases: first read RegisteredStatusNotifierItems, then resolve/add them.
    QTimer::singleShot(150, this, &SniWatcher::queryRegisteredItems);
    QTimer::singleShot(300, this, &SniWatcher::queryExistingItems);
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
    
    QString service = it.value()->service(); 
    
    SniItemProxy* item = it.value();
    m_items.erase(it);
    emit itemRemoved(id);
    emit StatusNotifierItemUnregistered(service);
    item->deleteLater();
}

QStringList SniWatcher::registeredItems() const
{
    return m_registeredServices;
}

void SniWatcher::registerStatusNotifierItemInternal(const QString &service, const QString &senderService)
{
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    if (trayDebug) {
        qDebug() << "registerStatusNotifierItemInternal called with service:" << service << "senderService:" << senderService;
    }
    
    // Add to registered services list
    if (!m_registeredServices.contains(service)) {
        m_registeredServices.append(service);
        emit StatusNotifierItemRegistered(service);
    }
    
    QString serviceName = service;
    QString path = "/StatusNotifierItem";

    // Format can be:
    // - "service_name" (well-known name)
    // - "service_name/path" (well-known name with path)
    // - ":1.104@/StatusNotifierItem" (unique name with @ separator and path)
    // - "/org/some/path" (object path, service is the sender)
    
    if (service.startsWith('/')) {
        // If it starts with '/', it's an object path on the caller's bus
        if (!senderService.isEmpty()) {
            serviceName = senderService;
        } else {
            if (trayDebug) {
                qDebug() << "Service starts with / but no senderService provided, skipping";
            }
            return;
        }
        path = service;
    } else {
        // Check for @ separator (used by some implementations)
        int atPos = service.indexOf('@');
        if (atPos > 0) {
            serviceName = service.left(atPos);
            QString afterAt = service.mid(atPos + 1);
            if (!afterAt.isEmpty() && afterAt.startsWith('/')) {
                path = afterAt;
            }
        } else {
            // Check for / separator (service/path)
            int slashPos = service.indexOf('/');
            if (slashPos > 0) {
                serviceName = service.left(slashPos);
                path = service.mid(slashPos);
            }
        }
    }
    
    // Final validation: Ensure serviceName is NOT a path (doesn't start with /)
    if (serviceName.isEmpty() || serviceName.startsWith('/')) {
        if (trayDebug) {
            qDebug() << "Invalid service name:" << serviceName;
        }
        if (!senderService.isEmpty()) {
            serviceName = senderService;
        } else {
            if (trayDebug) {
                qDebug() << "No senderService available, skipping";
            }
            return;
        }
    }
    
    if (serviceName.isEmpty()) {
        if (trayDebug) {
            qDebug() << "Failed to determine service name, skipping";
        }
        return;
    }

    // Prefer unique sender name (avoids blocking GetNameOwner during resets)
    if (!serviceName.startsWith(QLatin1Char(':')) && !senderService.isEmpty()) {
        serviceName = senderService;
    }
    
    if (trayDebug) {
        qDebug() << "Adding SNI item - service:" << serviceName << "path:" << path;
    }
    addItem(serviceName, path);
}

void SniWatcher::RegisterStatusNotifierItem(const QString &service)
{
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    if (trayDebug) {
        qDebug() << "RegisterStatusNotifierItem called with service:" << service;
    }
    
    // Safely get sender service from D-Bus message context (if available)
    QString senderService;
    QDBusMessage msg = message();
    if (msg.type() != QDBusMessage::InvalidMessage) {
        senderService = msg.service();
    }
    
    registerStatusNotifierItemInternal(service, senderService);
}

void SniWatcher::RegisterStatusNotifierHost(const QString &service)
{
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    if (trayDebug) {
        qDebug() << "RegisterStatusNotifierHost called with service:" << service;
    }
    emit StatusNotifierHostRegistered();
}

void SniWatcher::queryRegisteredItems()
{
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    // Check if there's another StatusNotifierWatcher that has registered items
    // Some applications might have registered with a previous watcher
    QDBusInterface watcherInterface(SNI_WATCHER, "/StatusNotifierWatcher", 
                                     "org.kde.StatusNotifierWatcher", m_bus);
    if (watcherInterface.isValid()) {
        QVariant prop = watcherInterface.property("RegisteredStatusNotifierItems");
        if (prop.isValid()) {
            QStringList registered = prop.toStringList();
            if (trayDebug) {
                qDebug() << "Found" << registered.size() << "registered SNI items from watcher";
            }
            for (const QString &service : registered) {
                if (!m_registeredServices.contains(service)) {
                    if (trayDebug) {
                        qDebug() << "Processing registered SNI item:" << service;
                    }
                    // Call internal helper directly (no D-Bus message context available)
                    registerStatusNotifierItemInternal(service, QString());
                }
            }
        }
    }
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
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    // Only query services that have explicitly registered via RegisterStatusNotifierItem
    // This avoids checking all system services
    if (m_registeredServices.isEmpty()) {
        if (trayDebug) {
            qDebug() << "No registered SNI services to query";
        }
        return;
    }
    
    if (trayDebug) {
        qDebug() << "Querying" << m_registeredServices.size() << "registered SNI services";
    }
    
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
        
        // Avoid blocking owner-resolution; prefer service name as-is.
        serviceName = getNameOwnerOrSelf(serviceName, m_bus);
        
        // Check if it has tray icon properties
        if (hasTrayIconProperties(serviceName, path, m_bus)) {
            QString key = serviceName + path;
            if (!m_items.contains(key)) {
                if (trayDebug) {
                    qDebug() << "Found registered SNI item:" << serviceName << "at path" << path;
                }
                addItem(serviceName, path);
            }
        }
    }
}

void SniWatcher::onServiceOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(oldOwner)
    static const bool trayDebug = qEnvironmentVariableIsSet("HDE_TRAY_DEBUG");
    
    if (newOwner.isEmpty()) {
        // Service disappeared. Remove all items associated with this service name.
        QStringList toRemove;
        for (auto it = m_items.begin(); it != m_items.end(); ++it) {
            if (it.value()->service() == name) {
                toRemove.append(it.key());
            }
        }
        
        if (!toRemove.isEmpty()) {
            if (trayDebug) {
                qDebug() << "Service" << name << "disappeared, removing" << toRemove.size() << "SNI items";
            }
            for (const QString &id : toRemove) {
                removeItem(id);
            }
        }

        // Also remove from m_registeredServices if it matches the service name
        // or if it's a composite name like "service/path" or "service@path"
        for (int i = m_registeredServices.size() - 1; i >= 0; --i) {
            QString reg = m_registeredServices[i];
            if (reg == name || reg.startsWith(name + "/") || reg.startsWith(name + "@")) {
                m_registeredServices.removeAt(i);
            }
        }
        return;
    }

    // Probing logic for new services that might be tray icons
    if (name.startsWith("org.kde.StatusNotifierItem") || 
        name.contains("StatusNotifierItem")) {
        
        // Probe for the standard path
            QDBusInterface iface(name, "/StatusNotifierItem", SNI_ITEM_IFACE, QDBusConnection::sessionBus());
            if (iface.isValid()) {
            // Prefer the unique owner (newOwner) to avoid extra DBus lookups.
            addItem(!newOwner.isEmpty() ? newOwner : getNameOwnerOrSelf(name, m_bus), "/StatusNotifierItem");
        }
    }
}


