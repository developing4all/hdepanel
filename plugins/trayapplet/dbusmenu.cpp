#include "dbusmenu.h"

#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDateTime>
#include <QDebug>

const QDBusArgument &operator>>(const QDBusArgument &argument, DbusMenuLayout &layout)
{
    argument.beginStructure();
    argument >> layout.id;
    argument >> layout.properties;

    layout.children.clear();
    // The DBusMenu layout uses `av` for children; each entry is a variant that contains
    // another `(ia{sv}av)` layout. Parse defensively.
    argument.beginArray();
    while (!argument.atEnd()) {
        QVariant childVar;
        argument >> childVar;

        QVariant inner = childVar;
        if (childVar.canConvert<QDBusVariant>())
            inner = qvariant_cast<QDBusVariant>(childVar).variant();

        if (inner.userType() == qMetaTypeId<QDBusArgument>()) {
            const QDBusArgument &childArg = inner.value<QDBusArgument>();
            DbusMenuLayout child;
            childArg >> child;
            layout.children.append(child);
        }
    }
    argument.endArray();

    argument.endStructure();
    return argument;
}

DbusMenuClient::DbusMenuClient(const QString &service, const QString &path, const QDBusConnection &bus)
    : m_service(service), m_path(path), m_bus(bus)
{
}

bool DbusMenuClient::isValid() const
{
    if (m_service.isEmpty() || m_path.isEmpty())
        return false;
    if (!m_path.startsWith(QLatin1Char('/')))
        return false;
    return m_bus.isConnected();
}

bool DbusMenuClient::fetchLayout(DbusMenuLayout &outLayout) const
{
    if (!isValid())
        return false;

    // Best-effort: let the indicator update before we render.
    {
        QDBusMessage about = QDBusMessage::createMethodCall(m_service, m_path, "com.canonical.dbusmenu", "AboutToShow");
        about << 0;
        m_bus.call(about, QDBus::NoBlock);
    }

    // GetLayout(parentId, recursionDepth, propertyNames) -> (revision, layout)
    const QStringList props = {
        "type",
        "label",
        "visible",
        "enabled",
        "toggle-type",
        "toggle-state",
        "children-display"
    };

    QDBusMessage getLayout = QDBusMessage::createMethodCall(m_service, m_path, "com.canonical.dbusmenu", "GetLayout");
    getLayout << 0 << 10 << props;
    QDBusMessage reply = m_bus.call(getLayout);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().size() < 2) {
        qDebug() << "DbusMenuClient::fetchLayout - GetLayout failed for" << m_service << m_path
                 << "type:" << reply.type() << "args:" << reply.arguments();
        return false;
    }

    QVariant layoutVar = reply.arguments().at(1);
    if (layoutVar.userType() != qMetaTypeId<QDBusArgument>()) {
        qDebug() << "DbusMenuClient::fetchLayout - unexpected layout type for" << m_service << m_path
                 << "typeId:" << layoutVar.userType();
        return false;
    }

    const QDBusArgument &arg = layoutVar.value<QDBusArgument>();
    DbusMenuLayout root;
    arg >> root;
    outLayout = root;
    return true;
}

static bool propBool(const QVariantMap &m, const char *key, bool def)
{
    auto it = m.find(QString::fromLatin1(key));
    if (it == m.end())
        return def;
    return it.value().toBool();
}

static QString propString(const QVariantMap &m, const char *key)
{
    auto it = m.find(QString::fromLatin1(key));
    if (it == m.end())
        return QString();
    return it.value().toString();
}

static int propInt(const QVariantMap &m, const char *key, int def)
{
    auto it = m.find(QString::fromLatin1(key));
    if (it == m.end())
        return def;
    return it.value().toInt();
}

void DbusMenuClient::sendClickedEvent(int id) const
{
    if (!isValid())
        return;

    // Event(id, eventId, data, timestamp)
    // Use a best-effort timestamp (ms truncated to u32).
    const quint32 ts = static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() & 0xffffffff);
    QDBusMessage ev = QDBusMessage::createMethodCall(m_service, m_path, "com.canonical.dbusmenu", "Event");
    // The data variant (3rd arg) must be a valid QDBusVariant. Use an empty string.
    ev << id << QString("clicked") << QVariant::fromValue(QDBusVariant(QString())) << ts;
    m_bus.call(ev, QDBus::NoBlock);
}

void DbusMenuClient::populateMenu(QMenu *menu, const DbusMenuLayout &layout) const
{
    // The layout we get is a tree; most implementations use a root node (id 0)
    // with actual menu entries as its children.
    for (const DbusMenuLayout &child : layout.children) {
        const QVariantMap &p = child.properties;

        const bool visible = propBool(p, "visible", true);
        if (!visible)
            continue;

        const QString type = propString(p, "type");
        if (type == "separator") {
            menu->addSeparator();
            continue;
        }

        QString label = propString(p, "label");
        // DBusMenu often uses underscores for mnemonics; keep them.
        if (label.isEmpty())
            label = QStringLiteral(" ");

        const bool enabled = propBool(p, "enabled", true);

        if (!child.children.isEmpty()) {
            QMenu *sub = menu->addMenu(label);
            sub->setEnabled(enabled);
            populateMenu(sub, child);
            continue;
        }

        QAction *act = menu->addAction(label);
        act->setEnabled(enabled);
        act->setData(child.id);

        const QString toggleType = propString(p, "toggle-type");
        if (!toggleType.isEmpty()) {
            act->setCheckable(true);
            const int toggleState = propInt(p, "toggle-state", 0);
            act->setChecked(toggleState == 1);
        }

        QObject::connect(act, &QAction::triggered, menu, [this, act]() {
            bool ok = false;
            const int id = act->data().toInt(&ok);
            if (ok)
                sendClickedEvent(id);
        });
    }
}

bool DbusMenuClient::popupAt(const QPoint &globalPos) const
{
    DbusMenuLayout layout;
    if (!fetchLayout(layout))
        return false;

    QMenu menu;
    populateMenu(&menu, layout);
    if (menu.actions().isEmpty())
        return false;

    menu.exec(globalPos);
    return true;
}


