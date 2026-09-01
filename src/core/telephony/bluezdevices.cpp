// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluezdevices.h"

#include "core/logging.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>

namespace {
constexpr auto kService = "org.bluez";
constexpr auto kDeviceIface = "org.bluez.Device1";
constexpr auto kBatteryIface = "org.bluez.Battery1";
constexpr auto kPropsIface = "org.freedesktop.DBus.Properties";
}

BluezDevices::BluezDevices(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::systemBus().connect(
        QLatin1String(kService), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesAdded"), this, SLOT(onInterfacesAdded(QDBusMessage)));
    QDBusConnection::systemBus().connect(
        QLatin1String(kService), QStringLiteral("/"),
        QStringLiteral("org.freedesktop.DBus.ObjectManager"),
        QStringLiteral("InterfacesRemoved"), this, SLOT(onInterfacesRemoved(QDBusMessage)));

    // Path-wide, because a device's alias or battery can change at any time and
    // there is no per-device object to hang a narrower rule on.
    QDBusConnection::systemBus().connect(
        QLatin1String(kService), QString(), QLatin1String(kPropsIface),
        QStringLiteral("PropertiesChanged"), this,
        SLOT(onPropertiesChanged(QDBusMessage)));

    refresh();
}

void BluezDevices::refresh()
{
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::systemBus().asyncCall(
            QDBusMessage::createMethodCall(QLatin1String(kService), QStringLiteral("/"),
                                           QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                                           QStringLiteral("GetManagedObjects"))),
        this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *w) {
                w->deleteLater();
                const QDBusMessage reply = w->reply();
                if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
                    // Not an error worth a banner: a machine with no Bluetooth
                    // stack is a normal machine, and the address still shows.
                    // Logged because "the phone has no name" is otherwise
                    // impossible to explain.
                    qCInfo(FONER_BLUEZ) << "BlueZ did not answer, so phones keep their"
                                        << "addresses instead of their names:"
                                        << reply.errorMessage();
                    return;
                }

                const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
                if (arg.currentSignature() != QLatin1String("a{oa{sa{sv}}}"))
                    return;

                arg.beginMap();
                while (!arg.atEnd()) {
                    arg.beginMapEntry();
                    QDBusObjectPath op;
                    arg >> op;

                    QVariantMap device;
                    QVariantMap battery;
                    arg.beginMap();
                    while (!arg.atEnd()) {
                        arg.beginMapEntry();
                        QString ifaceName;
                        arg >> ifaceName;
                        QVariantMap props;
                        arg.beginMap();
                        while (!arg.atEnd()) {
                            arg.beginMapEntry();
                            QString key;
                            QVariant value;
                            arg >> key >> value;
                            props.insert(key, value);
                            arg.endMapEntry();
                        }
                        arg.endMap();
                        if (ifaceName == QLatin1String(kDeviceIface))
                            device = props;
                        else if (ifaceName == QLatin1String(kBatteryIface))
                            battery = props;
                        arg.endMapEntry();
                    }
                    arg.endMap();
                    arg.endMapEntry();

                    ingest(op.path(), device, battery);
                }
                arg.endMap();
            });
}

void BluezDevices::ingest(const QString &objectPath, const QVariantMap &deviceProps,
                          const QVariantMap &batteryProps)
{
    QString address = deviceProps.value(QStringLiteral("Address")).toString();
    if (address.isEmpty())
        address = m_addressByPath.value(objectPath);
    if (address.isEmpty())
        return;

    m_addressByPath.insert(objectPath, address);
    Device entry = m_byAddress.value(address);
    const Device before = entry;

    if (deviceProps.contains(QStringLiteral("Alias")))
        entry.alias = deviceProps.value(QStringLiteral("Alias")).toString();
    if (deviceProps.contains(QStringLiteral("Icon")))
        entry.icon = deviceProps.value(QStringLiteral("Icon")).toString();
    if (batteryProps.contains(QStringLiteral("Percentage")))
        entry.battery = batteryProps.value(QStringLiteral("Percentage")).toInt();

    if (entry.alias == before.alias && entry.icon == before.icon
        && entry.battery == before.battery && m_byAddress.contains(address)) {
        return;
    }

    m_byAddress.insert(address, entry);
    Q_EMIT deviceChanged(address);
}

QString BluezDevices::nameFor(const QString &address) const
{
    const QString alias = m_byAddress.value(address).alias;
    return alias.isEmpty() ? address : alias;
}

QString BluezDevices::iconFor(const QString &address) const
{
    return m_byAddress.value(address).icon;
}

int BluezDevices::batteryFor(const QString &address) const
{
    if (!m_byAddress.contains(address))
        return -1;
    return m_byAddress.value(address).battery;
}

void BluezDevices::onInterfacesAdded(const QDBusMessage &message)
{
    Q_UNUSED(message)
    // A newly paired or reconnected device. Cheaper to re-read the lot than to
    // unpack the nested argument a second way.
    refresh();
}

void BluezDevices::onInterfacesRemoved(const QDBusMessage &message)
{
    // Battery1 comes and goes while the phone stays paired, so a removal has to
    // clear the level or the display keeps showing a stale percentage forever.
    if (message.arguments().size() < 2)
        return;
    const QString path = message.arguments().at(0).value<QDBusObjectPath>().path();
    if (!m_addressByPath.contains(path))
        return;
    if (!message.arguments().at(1).toStringList().contains(QLatin1String(kBatteryIface)))
        return;

    const QString address = m_addressByPath.value(path);
    Device entry = m_byAddress.value(address);
    if (entry.battery < 0)
        return;
    entry.battery = -1;
    m_byAddress.insert(address, entry);
    Q_EMIT deviceChanged(address);
}

void BluezDevices::onPropertiesChanged(const QDBusMessage &message)
{
    const QString path = message.path();
    if (path.isEmpty() || !m_addressByPath.contains(path))
        return;
    if (message.arguments().size() < 2)
        return;

    const QString interfaceName = message.arguments().at(0).toString();
    const QDBusArgument arg = message.arguments().at(1).value<QDBusArgument>();
    QVariantMap changed;
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QString key;
        QVariant value;
        arg >> key >> value;
        changed.insert(key, value);
        arg.endMapEntry();
    }
    arg.endMap();

    if (interfaceName == QLatin1String(kDeviceIface))
        ingest(path, changed, {});
    else if (interfaceName == QLatin1String(kBatteryIface))
        ingest(path, {}, changed);
}
