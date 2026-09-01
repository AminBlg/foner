// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "call.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>

namespace {
constexpr auto kService = "org.pipewire.Telephony";
// Call1.Hangup is rejected; see docs/M0-selfio-notes.md.
constexpr auto kOfonoCallIface = "org.ofono.VoiceCall";
}

Call::Call(const QString &objectPath, QObject *parent)
    : QObject(parent)
    , m_objectPath(objectPath)
{
    QDBusConnection::sessionBus().connect(
        QLatin1String(kService), m_objectPath, QLatin1String(kOfonoCallIface),
        QStringLiteral("PropertyChanged"), this,
        SLOT(onPropertyChanged(QString, QDBusVariant)));
    QDBusConnection::sessionBus().connect(
        QLatin1String(kService), m_objectPath,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"), this,
        SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
}

Call::~Call()
{
    // Same signatures as the connect, or the match rule leaks.
    QDBusConnection::sessionBus().disconnect(
        QLatin1String(kService), m_objectPath, QLatin1String(kOfonoCallIface),
        QStringLiteral("PropertyChanged"), this,
        SLOT(onPropertyChanged(QString, QDBusVariant)));
    QDBusConnection::sessionBus().disconnect(
        QLatin1String(kService), m_objectPath,
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"), this,
        SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
}

void Call::callMethod(const QString &method)
{
    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(
            QDBusMessage::createMethodCall(QLatin1String(kService), m_objectPath,
                                           QLatin1String(kOfonoCallIface), method)),
        this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, method](QDBusPendingCallWatcher *w) {
                w->deleteLater();
                const QDBusMessage reply = w->reply();
                if (reply.type() == QDBusMessage::ErrorMessage)
                    Q_EMIT commandFailed(reply.errorMessage().isEmpty() ? method
                                                                       : reply.errorMessage());
            });
}

void Call::answer()
{
    callMethod(QStringLiteral("Answer"));
}

void Call::hangup()
{
    callMethod(QStringLiteral("Hangup"));
}

void Call::updateProperties(const QVariantMap &props)
{
    if (props.contains(QStringLiteral("State")))
        setState(props.value(QStringLiteral("State")).toString());
    if (props.contains(QStringLiteral("LineIdentification")))
        m_line = props.value(QStringLiteral("LineIdentification")).toString();
    if (props.contains(QStringLiteral("IncomingLine")))
        m_incomingLine = props.value(QStringLiteral("IncomingLine")).toString();
    if (props.contains(QStringLiteral("Name")))
        m_name = props.value(QStringLiteral("Name")).toString();
    if (props.contains(QStringLiteral("Multiparty")))
        m_multiparty = props.value(QStringLiteral("Multiparty")).toBool();
}

void Call::setState(const QString &s)
{
    if (s == m_state)
        return;
    m_state = s;
    Q_EMIT stateChanged(m_state);
}

void Call::onPropertyChanged(const QString &property, const QDBusVariant &value)
{
    QVariantMap m;
    m.insert(property, value.variant());
    updateProperties(m);
}

void Call::onPropertiesChanged(const QString &interfaceName, const QVariantMap &changed,
                               const QStringList &invalidated)
{
    Q_UNUSED(interfaceName)
    Q_UNUSED(invalidated)
    updateProperties(changed);
}

void Call::setActiveSince(qint64 msecs)
{
    if (m_activeSince == msecs)
        return;
    m_activeSince = msecs;
    Q_EMIT activeSinceChanged();
}
