// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "telephonyclient.h"

#include "agdevice.h"
#include "agmodel.h"
#include "call.h"
#include "callmodel.h"

#include "core/logging.h"

#include <QDBusArgument>
#include <QProcess>
#include <QStandardPaths>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <QTimer>
#include <utility>

namespace {
constexpr auto kService = "org.pipewire.Telephony";
constexpr auto kObjMgrPath = "/org/pipewire/Telephony";
constexpr auto kObjMgrIface = "org.freedesktop.DBus.ObjectManager";
constexpr auto kAgIface = "org.pipewire.Telephony.AudioGateway1";
constexpr auto kOfonoCallMgrIface = "org.ofono.VoiceCallManager";
constexpr auto kOfonoCallIface = "org.ofono.VoiceCall";
}

namespace {

struct AgInfo {
    QString path;
    QString address;
};

QList<AgInfo> parseManagedObjects(const QVariant &result)
{
    QList<AgInfo> out;
    const QDBusArgument arg = result.value<QDBusArgument>();
    if (arg.currentSignature() != QLatin1String("a{oa{sa{sv}}}"))
        return out;

    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QDBusObjectPath op;
        arg >> op;
        arg.beginMap();
        while (!arg.atEnd()) {
            arg.beginMapEntry();
            QString ifaceName;
            arg >> ifaceName;
            if (ifaceName == QLatin1String(kAgIface)) {
                QVariantMap props;
                arg.beginMap();
                while (!arg.atEnd()) {
                    arg.beginMapEntry();
                    QString key;
                    QVariant value;
                    arg >> key;
                    arg >> value;
                    props.insert(key, value);
                    arg.endMapEntry();
                }
                arg.endMap();
                AgInfo info;
                info.path = op.path();
                info.address = props.value(QStringLiteral("Address")).toString();
                out.append(info);
            } else {
                // Skip the value a{sv}.
                arg.beginMap();
                while (!arg.atEnd()) {
                    arg.beginMapEntry();
                    QString key;
                    QVariant value;
                    arg >> key;
                    arg >> value;
                    arg.endMapEntry();
                }
                arg.endMap();
            }
            arg.endMapEntry();
        }
        arg.endMap();
        arg.endMapEntry();
    }
    arg.endMap();
    return out;
}

QVariantMap parseCallProps(const QVariant &result)
{
    QVariantMap props;
    const QDBusArgument arg = result.value<QDBusArgument>();
    if (arg.currentSignature() != QLatin1String("a{oa{sv}}"))
        return props;

    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QDBusObjectPath op;
        arg >> op;
        QVariantMap entry;
        arg.beginMap();
        while (!arg.atEnd()) {
            arg.beginMapEntry();
            QString key;
            QVariant value;
            arg >> key;
            arg >> value;
            entry.insert(key, value);
            arg.endMapEntry();
        }
        arg.endMap();
        props.insert(op.path(), QVariant::fromValue(entry));
        arg.endMapEntry();
    }
    arg.endMap();
    return props;
}

} // namespace

TelephonyClient::TelephonyClient(QObject *parent)
    : QObject(parent)
    , m_watcher(new QDBusServiceWatcher(QLatin1String(kService),
                                        QDBusConnection::sessionBus(),
                                        QDBusServiceWatcher::WatchForRegistration
                                            | QDBusServiceWatcher::WatchForUnregistration,
                                        this))
    , m_devices(new AgModel(this))
    , m_calls(new CallModel(this))
    , m_pollTimer(new QTimer(this))
{
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, &TelephonyClient::onServiceAppeared);
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, &TelephonyClient::onServiceVanished);

    // Safety net: object-manager signals are missed on call teardown.
    m_pollTimer->setInterval(1500);
    connect(m_pollTimer, &QTimer::timeout, this, &TelephonyClient::onPollTimer);
    m_pollTimer->start();

    const bool registered = QDBusConnection::sessionBus().interface()->isServiceRegistered(QLatin1String(kService));
    if (registered)
        onServiceAppeared();
}

TelephonyClient::~TelephonyClient()
{
    qDeleteAll(m_callByPath);
    qDeleteAll(m_deviceList);
}

void TelephonyClient::setAvailable(bool available)
{
    if (m_available == available)
        return;
    m_available = available;
    Q_EMIT availableChanged(m_available);
}

void TelephonyClient::onServiceAppeared()
{
    setAvailable(true);
    QDBusConnection::sessionBus().connect(
        QLatin1String(kService), QLatin1String(kObjMgrPath), QLatin1String(kObjMgrIface),
        QStringLiteral("InterfacesAdded"), this, SLOT(onInterfacesAdded(QDBusMessage)));
    QDBusConnection::sessionBus().connect(
        QLatin1String(kService), QLatin1String(kObjMgrPath), QLatin1String(kObjMgrIface),
        QStringLiteral("InterfacesRemoved"), this, SLOT(onInterfacesRemoved(QDBusMessage)));
    refresh();
}

void TelephonyClient::onServiceVanished()
{
    setAvailable(false);
    QDBusConnection::sessionBus().disconnect(
        QLatin1String(kService), QLatin1String(kObjMgrPath), QLatin1String(kObjMgrIface),
        QStringLiteral("InterfacesAdded"), this, SLOT(onInterfacesAdded(QDBusMessage)));
    QDBusConnection::sessionBus().disconnect(
        QLatin1String(kService), QLatin1String(kObjMgrPath), QLatin1String(kObjMgrIface),
        QStringLiteral("InterfacesRemoved"), this, SLOT(onInterfacesRemoved(QDBusMessage)));
    m_pendingCallReplies = 0;
    m_devicesRefreshInFlight = false;
    m_livePaths.clear();

    m_devices->clear();
    qDeleteAll(m_deviceList);
    m_deviceList.clear();

    m_calls->clear();
    qDeleteAll(m_callByPath);
    m_callByPath.clear();
    m_wasIncomingByPath.clear();
    m_prevStateByPath.clear();
    m_activeSinceByPath.clear();
}

void TelephonyClient::onInterfacesAdded(const QDBusMessage &message)
{
    Q_UNUSED(message)
    refreshDevices();
    refreshCalls();
}

void TelephonyClient::onInterfacesRemoved(const QDBusMessage &message)
{
    Q_UNUSED(message)
    refreshDevices();
    refreshCalls();
}

void TelephonyClient::onPollTimer()
{
    refreshCalls();
}

void TelephonyClient::refresh()
{
    refreshDevices();
    refreshCalls();
}

void TelephonyClient::refreshDevices()
{
    if (m_devicesRefreshInFlight)
        return;
    m_devicesRefreshInFlight = true;

    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(QLatin1String(kService), QLatin1String(kObjMgrPath),
                                           QLatin1String(kObjMgrIface),
                                           QStringLiteral("GetManagedObjects"))),
        this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *w) {
                w->deleteLater();
                m_devicesRefreshInFlight = false;

                const QDBusMessage reply = w->reply();
                if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
                    qCWarning(FONER_TELEPHONY) << "GetManagedObjects failed:" << reply.errorMessage();
                    return;
                }

                // Reconcile by object path; rebuilding would reset the selection.
                QHash<QString, AgDevice *> previous;
                for (AgDevice *device : std::as_const(m_deviceList))
                    previous.insert(device->objectPath(), device);

                QList<AgDevice *> next;
                const QList<AgInfo> infos = parseManagedObjects(reply.arguments().at(0));
                for (const AgInfo &info : infos) {
                    AgDevice *device = previous.take(info.path);
                    if (!device) {
                        device = new AgDevice(info.path, info.address, this);
                        connect(device, &AgDevice::transportStateChanged, this,
                                &TelephonyClient::transportStateChanged);
                        connect(device, &AgDevice::commandFailed, this,
                                &TelephonyClient::commandFailed);
                    }
                    next.append(device);
                }

                m_deviceList = next;
                // A phone that goes away must not leave the selection pointing
                // past the end of the list.
                if (m_selectedRow >= m_deviceList.size())
                    m_selectedRow = 0;

                // Model gets the new list before anything is destroyed.
                m_devices->setDevices(m_deviceList);
                qDeleteAll(previous);

                // The chip names the selected phone, so it has to hear about a
                // list rebuild and not only about an explicit choice.
                Q_EMIT selectedAddressChanged();
                Q_EMIT transportStateChanged();
            });
}

void TelephonyClient::refreshCalls()
{
    // One refresh at a time; the poll retries.
    if (m_pendingCallReplies > 0)
        return;

    m_livePaths.clear();

    if (m_deviceList.isEmpty()) {
        finishCallRefresh();
        return;
    }

    m_pendingCallReplies = m_deviceList.size();
    for (AgDevice *device : std::as_const(m_deviceList)) {
        auto *watcher = new QDBusPendingCallWatcher(
            QDBusConnection::sessionBus().asyncCall(
                QDBusMessage::createMethodCall(QLatin1String(kService), device->objectPath(),
                                               QLatin1String(kOfonoCallMgrIface),
                                               QStringLiteral("GetCalls"))),
            this);

        connect(watcher, &QDBusPendingCallWatcher::finished, this,
                [this](QDBusPendingCallWatcher *w) {
                    w->deleteLater();

                    const QDBusMessage reply = w->reply();
                    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
                        ingestCalls(parseCallProps(reply.arguments().at(0)));
                    else
                        qCWarning(FONER_TELEPHONY) << "GetCalls failed:" << reply.errorMessage();

                    if (m_pendingCallReplies > 0 && --m_pendingCallReplies == 0)
                        finishCallRefresh();
                });
    }
}

void TelephonyClient::ingestCalls(const QVariantMap &calls)
{
    for (auto it = calls.constBegin(); it != calls.constEnd(); ++it) {
        m_livePaths.insert(it.key());

        Call *call = m_callByPath.value(it.key());
        if (!call) {
            call = new Call(it.key(), this);
            connect(call, &Call::commandFailed, this, &TelephonyClient::commandFailed);
            m_callByPath.insert(it.key(), call);
            // A path reappearing inside the grace window is a resumed call.
            m_pendingEnd.cancel(it.key());
        }
        call->updateProperties(it.value().toMap());

        // Sticky: an answered call reports "active", which would otherwise turn an
        // incoming call into an outgoing one in the recents.
        if (call->state() == QLatin1String("incoming") || call->state() == QLatin1String("waiting"))
            m_wasIncomingByPath.insert(it.key(), true);
        else if (!m_wasIncomingByPath.contains(it.key()))
            m_wasIncomingByPath.insert(it.key(), false);

        const QString prev = m_prevStateByPath.value(it.key());
        if (call->state() != prev) {
            qCDebug(FONER_TELEPHONY) << "call" << it.key() << ":" << prev << "->"
                                     << call->state() << "number" << call->remoteNumber();
        }
        if (call->state() == QLatin1String("incoming") && prev != QLatin1String("incoming"))
            Q_EMIT incomingCall(it.key(), call->remoteNumber());

        // Stamped on connect, so the recorded duration is talk time.
        if (call->state() == QLatin1String("active")
            && !m_activeSinceByPath.contains(it.key())) {
            m_activeSinceByPath.insert(it.key(), QDateTime::currentMSecsSinceEpoch());
        }
        // Mirrored onto the call so the window can run a timer from the same
        // instant the log will later measure.
        call->setActiveSince(m_activeSinceByPath.value(it.key(), 0));
        m_prevStateByPath.insert(it.key(), call->state());
    }
}

void TelephonyClient::finishCallRefresh()
{
    // Deferred past a grace window: GetCalls reports 0 during transitions.
    QStringList dropped = m_callByPath.keys();
    dropped.removeIf([this](const QString &p) { return m_livePaths.contains(p); });

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QList<Call *> retired;
    retired.reserve(dropped.size());

    for (const QString &p : dropped) {
        Call *call = m_callByPath.take(p);
        if (call) {
            const bool incoming = m_wasIncomingByPath.value(p, false);
            const QString number = call->remoteNumber();
            const qint64 activeSince = m_activeSinceByPath.value(p, 0);
            // Never reached "active" means never answered.
            const qint64 durationSec = activeSince > 0 ? (now - activeSince) / 1000 : 0;
            qCDebug(FONER_TELEPHONY) << "call dropped" << p << "number" << number
                                     << "incoming" << incoming << "duration" << durationSec;
            m_pendingEnd.markDropped(p, number, incoming, durationSec, now);
            retired.append(call);
        }
        m_wasIncomingByPath.remove(p);
        m_activeSinceByPath.remove(p);
        // Call paths recycle as callN, so no stale state may survive.
        m_prevStateByPath.remove(p);
    }

    // Model gets the new list before anything is destroyed.
    m_calls->setCalls(m_callByPath.values());
    qDeleteAll(retired);

    const QList<PendingEndTracker::Ended> ended = m_pendingEnd.flush(now);
    for (const PendingEndTracker::Ended &e : ended)
        Q_EMIT callEnded(e.number, e.incoming, e.durationSec);
}

bool TelephonyClient::isServiceCode(const QString &number)
{
    const QString trimmed = number.trimmed();
    if (trimmed.size() < 3 || !trimmed.endsWith(QLatin1Char('#')))
        return false;
    if (!trimmed.startsWith(QLatin1Char('*')) && !trimmed.startsWith(QLatin1Char('#')))
        return false;
    for (const QChar &ch : trimmed) {
        if (!ch.isDigit() && ch != QLatin1Char('*') && ch != QLatin1Char('#'))
            return false;
    }
    return true;
}

void TelephonyClient::openBluetoothSettings()
{
    // There is no portal or URL scheme for "open Bluetooth settings": the
    // freedesktop portals cover files, printing and screenshots, but not
    // settings panels. So this is a best-effort walk through what the major
    // desktops actually ship, cheapest and most portable first.

    // 1. A desktop entry, if one is installed. Launching by entry keeps the
    //    command in the distribution's hands rather than ours, so a desktop we
    //    have never heard of still works as long as it registers an entry.
    static const QStringList entries = {
        QStringLiteral("kcm_bluetooth.desktop"),            // Plasma
        QStringLiteral("gnome-bluetooth-panel.desktop"),    // GNOME
        QStringLiteral("blueman-manager.desktop"),          // Blueman, any WM
        QStringLiteral("cinnamon-bluetooth-panel.desktop"), // Cinnamon
        QStringLiteral("xfce4-bluetooth-settings.desktop"), // Xfce
    };
    for (const QString &entry : entries) {
        const QString path = QStandardPaths::locate(
            QStandardPaths::ApplicationsLocation, entry);
        if (path.isEmpty())
            continue;
        // gio and gtk-launch both run a desktop entry properly, honouring its
        // Exec, TryExec and D-Bus activation. Neither is a dependency, so the
        // launcher itself has to be on PATH before it is worth trying.
        for (const QString &launcher :
             {QStringLiteral("gio"), QStringLiteral("gtk-launch")}) {
            if (QStandardPaths::findExecutable(launcher).isEmpty())
                continue;
            const QStringList args = launcher == QLatin1String("gio")
                ? QStringList{QStringLiteral("launch"), path}
                : QStringList{entry};
            // startDetached reports that the process started, not that it kept
            // running, so this is "handed off", never "succeeded".
            if (QProcess::startDetached(launcher, args)) {
                qCDebug(FONER_BLUEZ) << "handed Bluetooth settings to" << launcher
                                     << "for" << entry;
                return;
            }
        }
    }

    // 2. Failing that, the settings binaries themselves.
    static const QList<QStringList> commands = {
        {QStringLiteral("systemsettings"), QStringLiteral("kcm_bluetooth")},
        {QStringLiteral("kcmshell6"), QStringLiteral("kcm_bluetooth")},
        {QStringLiteral("gnome-control-center"), QStringLiteral("bluetooth")},
        {QStringLiteral("blueman-manager")},
    };
    for (const QStringList &command : commands) {
        // Skip what is not installed, so a missing first candidate cannot
        // shadow a working later one.
        if (QStandardPaths::findExecutable(command.first()).isEmpty())
            continue;
        if (QProcess::startDetached(command.first(), command.mid(1))) {
            qCDebug(FONER_BLUEZ) << "handed Bluetooth settings to" << command.first();
            return;
        }
    }

    // Static, so there is no instance to report through; the log is the only
    // channel. The button simply does nothing on a desktop with no Bluetooth
    // settings installed, which is the same as before.
    qCWarning(FONER_BLUEZ) << "found no Bluetooth settings application to open";
}

void TelephonyClient::setSelectedDevice(int row)
{
    if (m_selectedRow == row)
        return;
    m_selectedRow = row;
    Q_EMIT transportStateChanged();
    Q_EMIT selectedAddressChanged();
}

QString TelephonyClient::selectedAddress() const
{
    if (AgDevice *d = selectedDevice())
        return d->address();
    return QString();
}

QString TelephonyClient::transportState() const
{
    if (AgDevice *d = selectedDevice())
        return d->transportState();
    return QString();
}

AgDevice *TelephonyClient::selectedDevice() const
{
    if (m_selectedRow >= 0 && m_selectedRow < m_deviceList.size())
        return m_deviceList.at(m_selectedRow);
    return m_deviceList.value(0);
}

void TelephonyClient::dial(const QString &number)
{
    AgDevice *d = selectedDevice();
    if (!d)
        return;
    d->dial(number);
    if (isServiceCode(number))
        Q_EMIT serviceCodeSent(number.trimmed());
}

void TelephonyClient::answerCall(const QString &objectPath)
{
    if (Call *c = findCall(objectPath))
        c->answer();
}

void TelephonyClient::hangupCall(const QString &objectPath)
{
    if (Call *c = findCall(objectPath))
        c->hangup();
}

void TelephonyClient::requestNumber(const QString &number)
{
    if (!number.isEmpty())
        Q_EMIT numberRequested(number);
}

void TelephonyClient::hangupAll()
{
    if (AgDevice *d = selectedDevice())
        d->hangupAll();
}

void TelephonyClient::swapCalls()
{
    if (AgDevice *d = selectedDevice())
        d->swapCalls();
}

void TelephonyClient::holdAndAnswer()
{
    if (AgDevice *d = selectedDevice())
        d->holdAndAnswer();
}

void TelephonyClient::releaseAndAnswer()
{
    if (AgDevice *d = selectedDevice())
        d->releaseAndAnswer();
}

void TelephonyClient::releaseAndSwap()
{
    if (AgDevice *d = selectedDevice())
        d->releaseAndSwap();
}

void TelephonyClient::createMultiparty()
{
    if (AgDevice *d = selectedDevice())
        d->createMultiparty();
}

void TelephonyClient::sendTones(const QString &tones)
{
    if (AgDevice *d = selectedDevice())
        d->sendTones(tones);
}

void TelephonyClient::setMuted(bool muted)
{
    AgDevice *d = selectedDevice();
    if (!d || muted == m_muted)
        return;

    if (muted) {
        // Cached by AgDevice. A -1 must not be remembered, or unmute restores silence.
        const int current = d->microphoneVolume();
        if (current > 0)
            m_volumeBeforeMute = current;
        d->setMicrophoneVolume(0);
    } else {
        d->setMicrophoneVolume(static_cast<uchar>(m_volumeBeforeMute));
    }

    m_muted = muted;
    Q_EMIT mutedChanged(m_muted);
}

void TelephonyClient::setSpeakerVolume(int volume)
{
    if (AgDevice *d = selectedDevice())
        d->setSpeakerVolume(static_cast<uchar>(qBound(0, volume, 15)));
}

void TelephonyClient::routeAudioHere()
{
    if (AgDevice *d = selectedDevice())
        d->routeAudioHere();
}


Call *TelephonyClient::findCall(const QString &path) const
{
    return m_callByPath.value(path);
}