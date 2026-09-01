// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>

#include "contactitem.h"

class PbapClient;
class RecentsModel;

class ContactsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    // The live search text. RankRole is computed against it.
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        NumbersRole,
        PrimaryNumberRole,
        PhotoRole,
        RankRole,
        LastCalledRole,
        // The name with the matched run wrapped in bold, against the live query.
        HighlightedNameRole,
    };
    Q_ENUM(Role)

    explicit ContactsModel(QObject *parent = nullptr);

    QString lastError() const { return m_lastError; }
    QString query() const { return m_query; }
    void setQuery(const QString &query);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loading() const { return m_loading; }

    // Reads the phone's PBAP "pb" phonebook.
    Q_INVOKABLE void loadFromPhone(const QString &deviceAddress);
    Q_INVOKABLE void loadFromVcfFile(const QString &filePath);
    // Writes every contact as vCard 3.0. Returns false and sets lastError on
    // failure, so the caller can surface it in the existing error banner.
    Q_INVOKABLE bool exportVcfFile(const QString &filePath);
    // The contact name for a number, or the number itself if unknown.
    Q_INVOKABLE QString nameForNumber(const QString &number) const;
    Q_INVOKABLE bool matches(int row, const QString &query) const;
    // Where the match starts in the contact's name, or -1. Lets the keypad draw
    // the matched prefix in bold rather than guessing at it in QML.
    Q_INVOKABLE int matchOffset(int row, const QString &query) const;
    // How many characters of the name the query accounts for.
    Q_INVOKABLE int matchLength(int row, const QString &query) const;
    // The name with the matched run wrapped in <b>, for Text.StyledText. Done
    // here rather than in QML because a proxy row cannot be mapped back to a
    // source row from QML, and only this class knows how the fold was done.
    QString highlightedName(int row, const QString &query) const;
    // Lower sorts first: name start, word start, mid-word, number only, kNoMatch.
    Q_INVOKABLE int matchRank(int row, const QString &query) const;

    static constexpr int kNoMatch = 99;

    static QList<ContactItem> parseVcf(const QByteArray &data);
    // Where the last successful pull is kept, so a restart still knows who calls.
    static QString cachePath();
    // Where the last-called stamps are kept, beside the phonebook.
    static QString lastCalledPath();

    // Recents answer "who did I call", but only for calls this session saw or
    // imported. The persisted stamp covers restarts. Newest of the two wins.
    void setRecents(RecentsModel *recents);
    // Newest call for the contact in this row, or an invalid QDateTime.
    Q_INVOKABLE QDateTime lastCalled(int row) const;
    // Records a call against every matching contact and writes it to disk.
    void noteCall(const QString &number, const QDateTime &when);
    // Trailing significant digits, so national and international forms compare equal.
    static QString numberKey(const QString &number);
    // A name folded to keypad digits: "Mike" -> "6453".
    static QString t9Key(const QString &name);
    // The same text with its accents removed: "Béchir" -> "Bechir".
    static QString foldAccents(const QString &text);

Q_SIGNALS:
    void countChanged();
    void loadingChanged();
    void lastErrorChanged();
    void queryChanged();
    void loadFinished(int contacts);
    void error(const QString &message);

public Q_SLOTS:
    void clear();

private:
    void setLoading(bool on);
    void setContacts(const QList<ContactItem> &contacts);
    // Parses, shows, and keeps the vCards unless they yielded nothing.
    void ingestVcf(const QByteArray &data);
    void setLastError(const QString &message);

    void loadLastCalled();
    void saveLastCalled() const;

    QList<ContactItem> m_contacts;
    RecentsModel *m_recents = nullptr;
    // Keyed by ContactsModel::numberKey, so it survives format differences.
    QHash<QString, QDateTime> m_lastCalledByKey;
    PbapClient *m_pbap;
    bool m_loading = false;
    QString m_lastError;
    QString m_query;
};