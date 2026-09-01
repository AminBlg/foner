// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusPendingCall>
#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>

// Pulls a phonebook over Bluetooth PBAP. The session is held open until the
// transfer object reports Status "complete"; obexd drops a session whose owner exits.
class PbapClient : public QObject
{
    Q_OBJECT

public:
    explicit PbapClient(QObject *parent = nullptr);

    // location is "int" (phone memory); phonebook is "pb", "ich", "och" or "mch".
    void pull(const QString &deviceAddress, const QString &location, const QString &phonebook);

Q_SIGNALS:
    void pullFinished(const QString &vcfPath);
    void errorOccurred(const QString &message);

private Q_SLOTS:
    void onTransferPropertiesChanged(const QString &interfaceName, const QVariantMap &changed,
                                     const QStringList &invalidated);

private:
    QDBusPendingCall callSession(const QString &method, const QVariantList &args);
    void selectPhonebook(const QString &location, const QString &phonebook);
    void startTransfer();
    void fail(const QString &message);
    void cleanup();

    QString m_sessionPath;
    QString m_transferPath;
    QString m_targetFile;
    QTemporaryDir m_tmp;
};
