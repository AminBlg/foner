// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "agdevice.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>

#include "core/logging.h"

namespace {
constexpr auto kService = "org.pipewire.Telephony";
constexpr auto kAgIface = "org.pipewire.Telephony.AudioGateway1";
constexpr auto kTransportIface = "org.pipewire.Telephony.AudioGatewayTransport1";
constexpr auto kPropsIface = "org.freedesktop.DBus.Properties";
}

AgDevice::AgDevice(const QString &objectPath, const QString &address, QObject *parent)
    : QObject(parent)
    , m_objectPath(objectPath)
    , m_address(address)
{
    QDBusConnection::sessionBus().connect(
        QLatin1String(kService),
        m_objectPath,
        QLatin1String(kPropsIface),
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));

    getDbusProperty(kTransportIface, QStringLiteral("State"), [this](const QVariant &v) {
        const QString s = v.toString();
        if (s != m_transportState) {
            m_transportState = s;
            Q_EMIT transportStateChanged(m_transportState);
        }
    });
    // Tracked from PropertiesChanged after this first read.
    getDbusProperty(kAgIface, QStringLiteral("MicrophoneVolume"), [this](const QVariant &v) {
        m_micVolume = v.toInt();
    });
}

AgDevice::~AgDevice()
{
    QDBusConnection::sessionBus().disconnect(
        QLatin1String(kService), m_objectPath,
        QLatin1String(kPropsIface),
        QStringLiteral("PropertiesChanged"), this,
        SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
}

void AgDevice::callAg(const QString &method, const QVariantList &args)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QLatin1String(kService), m_objectPath, QLatin1String(kAgIface), method);
    msg.setArguments(args);

    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, method](QDBusPendingCallWatcher *w) {
                w->deleteLater();
                const QDBusMessage reply = w->reply();
                if (reply.type() == QDBusMessage::ErrorMessage) {
                    qCWarning(FONER_TELEPHONY) << "AG" << method << "refused:"
                                               << reply.errorMessage();
                    Q_EMIT commandFailed(reply.errorMessage().isEmpty()
                                             ? method
                                             : reply.errorMessage());
                } else {
                    qCInfo(FONER_TELEPHONY) << "AG" << method << "accepted";
                }
            });
}

void AgDevice::setDbusProperty(const char *interfaceName, const QString &name, const QVariant &value)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QLatin1String(kService), m_objectPath, QLatin1String(kPropsIface), QStringLiteral("Set"));
    msg << QString::fromLatin1(interfaceName) << name << QVariant::fromValue(QDBusVariant(value));

    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, name](QDBusPendingCallWatcher *w) {
                w->deleteLater();
                const QDBusMessage reply = w->reply();
                if (reply.type() == QDBusMessage::ErrorMessage) {
                    qCWarning(FONER_TELEPHONY) << "AG property" << name << "refused:"
                                               << reply.errorMessage();
                    Q_EMIT commandFailed(reply.errorMessage().isEmpty() ? name
                                                                        : reply.errorMessage());
                }
            });
}

void AgDevice::getDbusProperty(const char *interfaceName, const QString &name,
                               std::function<void(const QVariant &)> onValue)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QLatin1String(kService), m_objectPath, QLatin1String(kPropsIface), QStringLiteral("Get"));
    msg << QString::fromLatin1(interfaceName) << name;

    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [onValue = std::move(onValue), name](QDBusPendingCallWatcher *w) {
                w->deleteLater();
                const QDBusMessage reply = w->reply();
                if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
                    qCDebug(FONER_TELEPHONY) << "AG property" << name << "unreadable:"
                                             << reply.errorMessage();
                    return;
                }
                onValue(reply.arguments().at(0).value<QDBusVariant>().variant());
            });
}

void AgDevice::dial(const QString &number) { callAg(QStringLiteral("Dial"), {number}); }
void AgDevice::hangupAll() { callAg(QStringLiteral("HangupAll")); }
void AgDevice::holdAndAnswer() { callAg(QStringLiteral("HoldAndAnswer")); }
void AgDevice::releaseAndAnswer() { callAg(QStringLiteral("ReleaseAndAnswer")); }
void AgDevice::releaseAndSwap() { callAg(QStringLiteral("ReleaseAndSwap")); }
void AgDevice::swapCalls() { callAg(QStringLiteral("SwapCalls")); }
void AgDevice::createMultiparty() { callAg(QStringLiteral("CreateMultiparty")); }
void AgDevice::sendTones(const QString &tones) { callAg(QStringLiteral("SendTones"), {tones}); }

void AgDevice::setSpeakerVolume(uchar volume)
{
    setDbusProperty(kAgIface, QStringLiteral("SpeakerVolume"), QVariant::fromValue(volume));
}

void AgDevice::routeAudioHere()
{
    // Clear any standing refusal first, or Activate() opens a link the next
    // RejectSCO immediately tears down again.
    setDbusProperty(kTransportIface, QStringLiteral("RejectSCO"), false);

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QLatin1String(kService), m_objectPath, QLatin1String(kTransportIface),
        QStringLiteral("Activate"));
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *w) {
                w->deleteLater();
                const QDBusMessage reply = w->reply();
                if (reply.type() == QDBusMessage::ErrorMessage) {
                    qCWarning(FONER_TELEPHONY) << "transport Activate refused:"
                                               << reply.errorMessage();
                    Q_EMIT commandFailed(reply.errorMessage());
                }
            });
}


void AgDevice::setMicrophoneVolume(uchar volume)
{
    m_micVolume = volume;
    setDbusProperty(kAgIface, QStringLiteral("MicrophoneVolume"), QVariant::fromValue(volume));
}

void AgDevice::onPropertiesChanged(const QString &interfaceName, const QVariantMap &changed,
                                   const QStringList &invalidated)
{
    if (interfaceName == QLatin1String(kAgIface)) {
        if (changed.contains(QLatin1String("Address"))) {
            const QString a = changed.value(QLatin1String("Address")).toString();
            if (a != m_address) {
                m_address = a;
                Q_EMIT addressChanged();
            }
        }
        if (changed.contains(QLatin1String("MicrophoneVolume")))
            m_micVolume = changed.value(QLatin1String("MicrophoneVolume")).toInt();
    } else if (interfaceName == QLatin1String(kTransportIface)) {
        if (changed.contains(QLatin1String("State"))) {
            const QString s = changed.value(QLatin1String("State")).toString();
            if (s != m_transportState) {
                m_transportState = s;
                Q_EMIT transportStateChanged(s);
            }
        }
    }
    (void)invalidated;
}
