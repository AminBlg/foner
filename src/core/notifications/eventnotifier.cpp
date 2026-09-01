// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "eventnotifier.h"

#include "core/contacts/contactsmodel.h"
#include "core/recents/recentsmodel.h"
#include "core/settings/settings.h"
#include "core/telephony/agmodel.h"
#include "core/telephony/telephonyclient.h"

#include <KLocalizedString>
#include <KNotification>

EventNotifier::EventNotifier(TelephonyClient *client, ContactsModel *contacts,
                             RecentsModel *recents, Settings *settings, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_settings(settings)
{
    connect(m_client, &TelephonyClient::callEnded, this,
            [this](const QString &number, bool incoming, qint64 durationSec) {
                // Rang and never answered: a missed call.
                if (incoming && durationSec <= 0) {
                    if (m_settings->notifyMissed()) {
                        notify(QStringLiteral("missed-call"), i18n("Missed call"),
                               displayName(number), QStringLiteral("call-missed"));
                    }
                    return;
                }

                if (!m_settings->notifyEnded())
                    return;

                const QString length = durationSec >= 60
                    ? i18ncp("call length in minutes", "%1 minute", "%1 minutes", durationSec / 60)
                    : i18ncp("call length in seconds", "%1 second", "%1 seconds", durationSec);
                notify(QStringLiteral("call-ended"), i18n("Call ended"),
                       i18nc("who and how long", "%1 · %2", displayName(number), length),
                       incoming ? QStringLiteral("call-incoming")
                                : QStringLiteral("call-outgoing"));
            });

    connect(m_client, &TelephonyClient::commandFailed, this, [this](const QString &reason) {
        if (m_settings->notifyErrors()) {
            notify(QStringLiteral("call-failed"), i18n("The phone refused the command"), reason,
                   QStringLiteral("dialog-error"));
        }
    });

    connect(contacts, &ContactsModel::error, this, [this](const QString &message) {
        if (m_settings->notifyErrors()) {
            notify(QStringLiteral("call-failed"), i18n("Could not load contacts"), message,
                   QStringLiteral("dialog-error"));
        }
    });

    connect(contacts, &ContactsModel::loadFinished, this, [this](int count) {
        if (m_settings->notifyDevice()) {
            notify(QStringLiteral("contacts-loaded"), i18n("Contacts loaded"),
                   i18np("%1 contact read from the phone", "%1 contacts read from the phone", count),
                   QStringLiteral("im-user"));
        }
    });

    // Fires on import, not on each row the live log appends.
    connect(recents, &RecentsModel::loadingChanged, this, [this, recents]() {
        if (recents->loading() || !m_settings->notifyDevice())
            return;
        notify(QStringLiteral("contacts-loaded"), i18n("Call history imported"),
               i18np("%1 call read from the phone", "%1 calls read from the phone",
                     recents->rowCount()),
               QStringLiteral("view-history"));
    });

    // The device list is rebuilt by polling, so watch the row count.
    connect(m_client->devices(), &AgModel::countChanged, this, [this]() {
        const int count = m_client->devices()->rowCount();
        const int previous = m_lastDeviceCount;
        m_lastDeviceCount = count;

        if (previous < 0 || previous == count || !m_settings->notifyDevice())
            return;

        if (count > previous) {
            notify(QStringLiteral("phone-connected"), i18n("Phone connected"),
                   i18n("Foner can place and answer calls."),
                   QStringLiteral("smartphone"));
        } else if (count == 0) {
            notify(QStringLiteral("phone-disconnected"), i18n("Phone disconnected"),
                   i18n("Calls cannot be placed until it reconnects."),
                   QStringLiteral("network-disconnect"));
        }
    });
}

QString EventNotifier::displayName(const QString &number) const
{
    if (number.isEmpty())
        return i18nc("caller with no number", "Unknown number");
    return m_nameResolver ? m_nameResolver(number) : number;
}

void EventNotifier::notify(const QString &eventId, const QString &title, const QString &text,
                           const QString &iconName)
{
    auto *n = new KNotification(eventId, KNotification::CloseOnTimeout, this);
    n->setTitle(title);
    n->setText(text);
    n->setIconName(iconName);
    n->sendEvent();
}
