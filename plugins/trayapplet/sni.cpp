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

SniItemProxy::SniItemProxy(const QString &service, const QString &path, QObject *parent)
    : QObject(parent), m_service(service), m_path(path)
{
    m_id = service + path;
    m_iface = new QDBusInterface(m_service, m_path, SNI_ITEM_IFACE, QDBusConnection::sessionBus(), this);
    m_ifaceFd = new QDBusInterface(m_service, m_path, "org.freedesktop.StatusNotifierItem", QDBusConnection::sessionBus(), this);
    
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
    if (!m_iface || !m_iface->isValid()) {
        if (!m_ifaceFd || !m_ifaceFd->isValid()) {
            return QIcon::fromTheme("image-missing");
        }
    }

    QDBusInterface *iface = (m_iface && m_iface->isValid()) ? m_iface : m_ifaceFd;
    QString ifaceName = (m_iface && m_iface->isValid()) ? SNI_ITEM_IFACE : "org.freedesktop.StatusNotifierItem";
    
    auto tryIcon = [&](const QString &propName) -> QIcon {
        QVariant v = iface->property(propName.toUtf8().constData());
        if (!v.isValid()) return QIcon();
        QString name = v.toString();
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
    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, m_path, "org.freedesktop.DBus.Properties", "Get");
    msg << ifaceName << "IconPixmap";
    
    QDBusReply<QVariant> reply = QDBusConnection::sessionBus().call(msg);
    if (reply.isValid()) {
        QVariant pixmapProp = reply.value();
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
                    
                    if (w > 0 && h > 0 && data.size() == w * h * 4) {
                        QImage img(w, h, QImage::Format_ARGB32);
                        for (int y = 0; y < h; ++y) {
                            uint *dest = (uint*)img.scanLine(y);
                            const uint *src = (const uint*)(data.constData() + y * w * 4);
                            for (int x = 0; x < w; ++x) {
                                dest[x] = qFromBigEndian(src[x]);
                            }
                        }
                        
                        if (bestImg.isNull() || (w >= 24 && w < bestImg.width())) {
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

    return QIcon::fromTheme("image-missing");
}

void SniItemProxy::activate(int x, int y)
{
    qDebug() << "SniItemProxy::activate - service:" << m_service << "path:" << m_path << "x:" << x << "y:" << y;
    if (m_iface && m_iface->isValid()) {
        m_iface->call(QDBus::NoBlock, "Activate", x, y);
    }
    if (m_ifaceFd && m_ifaceFd->isValid()) {
        m_ifaceFd->call(QDBus::NoBlock, "Activate", x, y);
    }
}

void SniItemProxy::secondaryActivate(int x, int y)
{
    qDebug() << "SniItemProxy::secondaryActivate - service:" << m_service << "path:" << m_path << "x:" << x << "y:" << y;
    if (m_iface && m_iface->isValid()) {
        m_iface->call(QDBus::NoBlock, "SecondaryActivate", x, y);
    }
    if (m_ifaceFd && m_ifaceFd->isValid()) {
        m_ifaceFd->call(QDBus::NoBlock, "SecondaryActivate", x, y);
    }
}

void SniItemProxy::contextMenu(int x, int y)
{
    qDebug() << "SniItemProxy::contextMenu - service:" << m_service << "path:" << m_path << "x:" << x << "y:" << y;
    if (m_iface && m_iface->isValid()) {
        m_iface->call(QDBus::NoBlock, "ContextMenu", x, y);
    }
    if (m_ifaceFd && m_ifaceFd->isValid()) {
        m_ifaceFd->call(QDBus::NoBlock, "ContextMenu", x, y);
    }
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

void SniWatcher::registerWatcher()
{
    // Register the object first so it's available as soon as the service is registered
    if (!m_bus.registerObject("/StatusNotifierWatcher", "org.kde.StatusNotifierWatcher", this,
                         QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllProperties)) {
        qDebug() << "Failed to register SNI watcher object";
    }

    // Announce as SNI watcher to encourage apps to register
    if (!m_bus.registerService(SNI_WATCHER)) {
        qDebug() << "Failed to register SNI watcher service, another watcher may be active";
    }

    // Query for existing items that may have registered before we started
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
    
    // id is service + path. Extract service name if possible for the signal.
    // However, the signal usually wants the same string that was used for registration.
    // For now, we use the ID as a proxy or just the service part.
    QString service = it.value()->id(); 
    
    SniItemProxy* item = it.value();
    m_items.erase(it);
    emit itemRemoved(id);
    emit StatusNotifierItemUnregistered(id);
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
        emit StatusNotifierItemRegistered(service);
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

void SniWatcher::RegisterStatusNotifierHost(const QString &service)
{
    qDebug() << "RegisterStatusNotifierHost called with service:" << service;
    emit StatusNotifierHostRegistered();
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
            // or known services that use SNI (like kdeconnect)
            // and are not system services
            if ((service.contains("StatusNotifierItem") || service.startsWith("org.kde.kdeconnect")) && !isSystemService(service)) {
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
    
    // KDE Connect usually registers with a unique name and then calls RegisterStatusNotifierItem.
    // However, some apps just own a name. Let's be more liberal in probing.
    
    if (name.startsWith("org.kde.StatusNotifierItem") || 
        name.contains("StatusNotifierItem") ||
        name.startsWith("org.kde.kdeconnect")) {
        
        if (!newOwner.isEmpty()) {
            // Probe for the standard path
            QDBusInterface iface(name, "/StatusNotifierItem", SNI_ITEM_IFACE, QDBusConnection::sessionBus());
            if (iface.isValid()) {
                addItem(name, "/StatusNotifierItem");
            }
        } else {
            // Check all items for this service and remove them
            QStringList keys = m_items.keys();
            for (const QString &key : keys) {
                if (key.startsWith(name)) {
                    removeItem(key);
                }
            }
        }
    }
}


