// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <functional>

class KNotification;
class TelephonyClient;
class Settings;

// Incoming-call notification, with Answer and Decline actions.
class IncomingCallNotifier : public QObject
{
    Q_OBJECT

public:
    explicit IncomingCallNotifier(TelephonyClient *client, QObject *parent = nullptr);

    using NameResolver = std::function<QString(const QString &)>;
    void setSettings(Settings *settings) { m_settings = settings; }
    void setNameResolver(NameResolver resolver) { m_nameResolver = std::move(resolver); }

private Q_SLOTS:
    void onIncomingCall(const QString &objectPath, const QString &number);
    // Closes the popup once the call stops ringing, however it was answered.
    void onRingingChanged();
    void onAnswer();
    void onDecline();

private:
    TelephonyClient *m_client;
    Settings *m_settings = nullptr;
    QString m_objectPath;
    QString m_number;
    QString m_name;
    // KNotification deletes itself when the event closes, so a raw pointer here
    // dangles after a timeout or a user dismissal.
    QPointer<KNotification> m_notification;
    NameResolver m_nameResolver;
};