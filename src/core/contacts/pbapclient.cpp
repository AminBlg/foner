// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pbapclient.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace {
constexpr auto kService = "org.bluez.obex";
constexpr auto kRootPath = "/org/bluez/obex";
constexpr auto kClientIface = "org.bluez.obex.Client1";
constexpr auto kPhonebookIface = "org.bluez.obex.PhonebookAccess1";
constexpr auto kPropsIface = "org.freedesktop.DBus.Properties";
}

PbapClient::PbapClient(QObject *parent)
    : QObject(parent)
{
}

QDBusPendingCall PbapClient::callSession(const QString &method, const QVariantList &args)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QLatin1String(kService), m_sessionPath, QLatin1String(kPhonebookIface), method);
    msg.setArguments(args);
    return QDBusConnection::sessionBus().asyncCall(msg);
}

void PbapClient::pull(const QString &deviceAddress, const QString &location,
                      const QString &phonebook)
{
    if (!m_sessionPath.isEmpty()) {
        Q_EMIT errorOccurred(tr("A phonebook transfer is already running."));
        return;
    }
    if (!m_tmp.isValid()) {
        Q_EMIT errorOccurred(tr("Could not create a temporary directory."));
        return;
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QLatin1String(kService), QLatin1String(kRootPath), QLatin1String(kClientIface),
        QStringLiteral("CreateSession"));
    QVariantMap args;
    args.insert(QStringLiteral("Target"), QStringLiteral("pbap"));
    msg << deviceAddress << args;

    auto *watcher = new QDBusPendingCallWatcher(
        QDBusConnection::sessionBus().asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, location, phonebook](QDBusPendingCallWatcher *w) {
                w->deleteLater();
                const QDBusPendingReply<QDBusObjectPath> reply = *w;
                if (reply.isError()) {
                    fail(reply.error().message());
                    return;
                }
                m_sessionPath = reply.value().path();
                selectPhonebook(location, phonebook);
            });
}

void PbapClient::selectPhonebook(const QString &location, const QString &phonebook)
{
    auto *watcher = new QDBusPendingCallWatcher(
        callSession(QStringLiteral("Select"), {location, phonebook}), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *w) {
                w->deleteLater();
                const QDBusPendingReply<> reply = *w;
                if (reply.isError()) {
                    fail(reply.error().message());
                    return;
                }
                startTransfer();
            });
}

void PbapClient::startTransfer()
{
    m_targetFile = m_tmp.filePath(QStringLiteral("phonebook.vcf"));

    auto *watcher = new QDBusPendingCallWatcher(
        callSession(QStringLiteral("PullAll"), {m_targetFile, QVariant::fromValue(QVariantMap())}),
        this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *w) {
                w->deleteLater();
                const QDBusMessage reply = w->reply();
                if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
                    fail(reply.errorMessage());
                    return;
                }
                m_transferPath = reply.arguments().at(0).value<QDBusObjectPath>().path();

                // PullAll only starts the transfer. Completion arrives later.
                QDBusConnection::sessionBus().connect(
                    QLatin1String(kService), m_transferPath, QLatin1String(kPropsIface),
                    QStringLiteral("PropertiesChanged"), this,
                    SLOT(onTransferPropertiesChanged(QString, QVariantMap, QStringList)));
            });
}

void PbapClient::onTransferPropertiesChanged(const QString &interfaceName,
                                             const QVariantMap &changed,
                                             const QStringList &invalidated)
{
    Q_UNUSED(interfaceName)
    Q_UNUSED(invalidated)

    const QString status = changed.value(QStringLiteral("Status")).toString();
    if (status == QLatin1String("complete")) {
        const QString path = m_targetFile;
        cleanup();
        Q_EMIT pullFinished(path);
    } else if (status == QLatin1String("error")) {
        fail(tr("The phone rejected the phonebook transfer."));
    }
}

void PbapClient::fail(const QString &message)
{
    cleanup();
    Q_EMIT errorOccurred(message);
}

void PbapClient::cleanup()
{
    if (!m_transferPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(
            QLatin1String(kService), m_transferPath, QLatin1String(kPropsIface),
            QStringLiteral("PropertiesChanged"), this,
            SLOT(onTransferPropertiesChanged(QString, QVariantMap, QStringList)));
        m_transferPath.clear();
    }
    if (!m_sessionPath.isEmpty()) {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            QLatin1String(kService), QLatin1String(kRootPath), QLatin1String(kClientIface),
            QStringLiteral("RemoveSession"));
        msg << QVariant::fromValue(QDBusObjectPath(m_sessionPath));
        QDBusConnection::sessionBus().asyncCall(msg);
        m_sessionPath.clear();
    }
}
