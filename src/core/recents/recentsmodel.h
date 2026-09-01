// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

#include "recententry.h"

class PbapClient;

class RecentsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    enum Role {
        NumberRole = Qt::UserRole + 1,
        NameRole,
        DirectionRole,
        TimestampRole,
        DurationRole,
    };
    Q_ENUM(Role)

    using NameResolver = std::function<QString(const QString &)>;

    explicit RecentsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loading() const { return m_loading; }
    QString lastError() const { return m_lastError; }

    void setContactNameResolver(NameResolver resolver) { m_nameResolver = std::move(resolver); }

    void append(const QString &number, const QString &direction,
                const QDateTime &timestamp = QDateTime::currentDateTime(), qint64 durationSec = 0);

    // Newest call involving this number, or an invalid QDateTime. Matched on
    // ContactsModel::numberKey, so national and international forms agree.
    QDateTime lastCalled(const QString &number) const;

    // Direction and time come from the X-IRMC-CALL-DATETIME line.
    static QList<RecentEntry> parseCallHistory(const QByteArray &data,
                                               const QString &fallbackDirection);

public Q_SLOTS:
    void add(const QString &number, const QString &direction);
    // Imports the ich, och and mch phonebooks over PBAP.
    void loadFromPhone(const QString &deviceAddress);
    void clear();

Q_SIGNALS:
    void countChanged();
    void loadingChanged();
    void lastErrorChanged();

private:
    void setLoading(bool on);
    void setLastError(const QString &message);
    void pullNext();
    void mergeEntries(const QList<RecentEntry> &entries);
    void sortNewestFirst();

    QList<RecentEntry> m_entries;
    NameResolver m_nameResolver;

    PbapClient *m_pbap = nullptr;
    QString m_address;
    QString m_currentBook;
    QStringList m_remainingBooks;
    bool m_loading = false;
    QString m_lastError;
};
