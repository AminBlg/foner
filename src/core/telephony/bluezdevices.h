// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QDBusMessage;

// Friendly names, icons and battery levels for paired Bluetooth devices.
//
// This is the only part of Foner on the system bus: org.pipewire.Telephony
// exposes an address and nothing else, and the name a user recognises lives on
// org.bluez.Device1. Everything here is best effort. A missing or unreachable
// BlueZ leaves the address showing, which is what the app did before.
class BluezDevices : public QObject
{
    Q_OBJECT

public:
    explicit BluezDevices(QObject *parent = nullptr);

    struct Device {
        QString alias;
        QString icon;
        // 0-100, or -1 when the device exposes no org.bluez.Battery1. BlueZ adds
        // and drops that interface as the phone reports, so -1 is a normal
        // steady state and not a failure.
        int battery = -1;
    };

    // Invokable: QML reads these directly to label the phone chip.
    // The user-facing name for an address, or the address itself if unknown.
    Q_INVOKABLE QString nameFor(const QString &address) const;
    // Freedesktop icon name such as "phone", or an empty string.
    Q_INVOKABLE QString iconFor(const QString &address) const;
    Q_INVOKABLE int batteryFor(const QString &address) const;

Q_SIGNALS:
    // Any of the three changed for this address.
    void deviceChanged(const QString &address);

private Q_SLOTS:
    void onInterfacesAdded(const QDBusMessage &message);
    void onInterfacesRemoved(const QDBusMessage &message);
    // Takes the whole message, because the object path is the only way to know
    // which device changed and a plain slot signature does not carry it.
    void onPropertiesChanged(const QDBusMessage &message);

private:
    void refresh();
    void ingest(const QString &objectPath, const QVariantMap &deviceProps,
                const QVariantMap &batteryProps);

    // Addresses are the key because that is all the audio gateway gives us.
    QHash<QString, Device> m_byAddress;
    // Object path to address, so a PropertiesChanged on a path can be routed.
    QHash<QString, QString> m_addressByPath;
};
