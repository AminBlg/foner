// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "incomingcallnotifier.h"

#include "../logging.h"
#include "../settings/settings.h"
#include "../telephony/callmodel.h"
#include "../telephony/telephonyclient.h"

#include <KLocalizedString>
#include <KNotification>

IncomingCallNotifier::IncomingCallNotifier(TelephonyClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    connect(m_client, &TelephonyClient::incomingCall, this, &IncomingCallNotifier::onIncomingCall);
    // A call answered in the window, on the handset, or by the caller giving up
    // leaves the popup on screen otherwise, with buttons still aimed at it.
    connect(m_client->calls(), &CallModel::incomingCallIndexChanged, this,
            &IncomingCallNotifier::onRingingChanged);
}

void IncomingCallNotifier::onRingingChanged()
{
    if (m_client->calls()->incomingCallIndex() >= 0)
        return;
    qCDebug(FONER_NOTIFY) << "ringing stopped, dropping path" << m_objectPath
                          << "notification alive:" << !m_notification.isNull();
    m_objectPath.clear();
    if (m_notification) {
        m_notification->close();
        m_notification = nullptr;
    }
}

void IncomingCallNotifier::onIncomingCall(const QString &objectPath, const QString &number)
{
    m_objectPath = objectPath;
    m_number = number;

    qCDebug(FONER_NOTIFY) << "incoming call" << objectPath << "number" << number;

    // Silences the desktop notification only; the in-app sheet still appears.
    if (m_settings && !m_settings->notifyIncoming()) {
        qCDebug(FONER_NOTIFY) << "notification suppressed by settings";
        return;
    }

    if (m_number.isEmpty())
        m_number = i18nc("caller with no number", "Unknown number");
    m_name = m_nameResolver ? m_nameResolver(m_number) : m_number;

    if (m_notification)
        m_notification->close();

    KNotification *n = new KNotification(QStringLiteral("incoming-call"), KNotification::CloseOnTimeout, this);
    n->setTitle(i18n("Incoming call"));
    n->setText(m_name == m_number ? m_number
                                  : i18nc("caller name and number", "%1 (%2)", m_name, m_number));
    n->setIconName(QStringLiteral("call-incoming"));

    KNotificationAction *answerAction = n->addAction(i18n("Answer"));
    KNotificationAction *declineAction = n->addAction(i18n("Decline"));
    connect(answerAction, &KNotificationAction::activated, this, &IncomingCallNotifier::onAnswer);
    connect(declineAction, &KNotificationAction::activated, this, &IncomingCallNotifier::onDecline);

    connect(n, &KNotification::closed, this, []() {
        qCDebug(FONER_NOTIFY) << "notification closed by the server or the user";
    });

    m_notification = n;
    n->sendEvent();
    qCDebug(FONER_NOTIFY) << "notification posted for" << m_objectPath;
}

void IncomingCallNotifier::onAnswer()
{
    // The path is cleared the moment the call stops ringing, so a stale popup
    // that survived on screen cannot answer a call that is already up.
    qCDebug(FONER_NOTIFY) << "answer action, path" << m_objectPath;
    if (m_objectPath.isEmpty()) {
        qCWarning(FONER_NOTIFY) << "answer pressed on a notification whose call is gone";
        return;
    }
    if (m_client)
        m_client->answerCall(m_objectPath);
}

void IncomingCallNotifier::onDecline()
{
    qCDebug(FONER_NOTIFY) << "decline action, path" << m_objectPath;
    if (m_objectPath.isEmpty()) {
        qCWarning(FONER_NOTIFY) << "decline pressed on a notification whose call is gone";
        return;
    }
    if (m_client)
        m_client->hangupCall(m_objectPath);
}