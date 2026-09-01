// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <functional>

class Settings;
class TelephonyClient;
class ContactsModel;
class RecentsModel;

// Desktop notifications for ended, missed and failed calls and for the phone
// connecting. Ringing lives in IncomingCallNotifier. Each category has a setting.
class EventNotifier : public QObject
{
    Q_OBJECT

public:
    using NameResolver = std::function<QString(const QString &)>;

    EventNotifier(TelephonyClient *client, ContactsModel *contacts, RecentsModel *recents,
                  Settings *settings, QObject *parent = nullptr);

    void setNameResolver(NameResolver resolver) { m_nameResolver = std::move(resolver); }

private:
    void notify(const QString &eventId, const QString &title, const QString &text,
                const QString &iconName);
    QString displayName(const QString &number) const;

    TelephonyClient *m_client;
    Settings *m_settings;
    NameResolver m_nameResolver;

    // The device list is polled, so changes are spotted by comparing counts.
    int m_lastDeviceCount = -1;
};
