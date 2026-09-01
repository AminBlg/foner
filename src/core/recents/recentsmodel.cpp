// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "recentsmodel.h"

#include "core/contacts/contactsmodel.h"
#include "core/contacts/pbapclient.h"

#include <QFile>
#include <QLocale>
#include <KLocalizedString>

#include <algorithm>

namespace {

// PBAP splits history into three phonebooks. A card with no direction line
// inherits the book's.
struct HistoryBook {
    const char *name;
    const char *direction;
};
constexpr HistoryBook kHistoryBooks[] = {
    {"ich", "incoming"},
    {"och", "outgoing"},
    {"mch", "missed"},
};

QString directionForParameter(const QString &parameter, const QString &fallback)
{
    if (parameter.compare(QLatin1String("RECEIVED"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("incoming");
    if (parameter.compare(QLatin1String("DIALED"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("outgoing");
    if (parameter.compare(QLatin1String("MISSED"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("missed");
    return fallback;
}

} // namespace

RecentsModel::RecentsModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_pbap(new PbapClient(this))
{
    connect(m_pbap, &PbapClient::pullFinished, this, [this](const QString &vcfPath) {
        QFile f(vcfPath);
        if (f.open(QIODevice::ReadOnly))
            mergeEntries(parseCallHistory(f.readAll(), m_currentBook));
        else
            setLastError(i18n("Could not read the call history file: %1", f.errorString()));
        pullNext();
    });
    connect(m_pbap, &PbapClient::errorOccurred, this, [this](const QString &message) {
        // One unsupported book must not abort the other two.
        setLastError(message);
        pullNext();
    });
}

int RecentsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant RecentsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};
    const RecentEntry &e = m_entries.at(index.row());
    switch (role) {
    case NumberRole: return e.number;
    case NameRole: return e.name;
    case DirectionRole: return e.direction;
    case TimestampRole: return e.timestamp;
    case DurationRole: return e.durationSec;
    }
    return {};
}

QHash<int, QByteArray> RecentsModel::roleNames() const
{
    return {
        {NumberRole, "number"},
        {NameRole, "name"},
        {DirectionRole, "direction"},
        {TimestampRole, "timestamp"},
        {DurationRole, "duration"},
    };
}

void RecentsModel::append(const QString &number, const QString &direction,
                          const QDateTime &timestamp, qint64 durationSec)
{
    RecentEntry entry;
    entry.number = number;
    entry.direction = direction;
    entry.timestamp = timestamp;
    entry.durationSec = durationSec;
    if (m_nameResolver)
        entry.name = m_nameResolver(number);
    mergeEntries({entry});
}

void RecentsModel::add(const QString &number, const QString &direction)
{
    append(number, direction);
}

void RecentsModel::loadFromPhone(const QString &deviceAddress)
{
    if (m_loading)
        return;
    m_address = deviceAddress;
    m_remainingBooks.clear();
    for (const HistoryBook &book : kHistoryBooks)
        m_remainingBooks.append(QLatin1String(book.name));
    setLoading(true);
    pullNext();
}

void RecentsModel::pullNext()
{
    if (m_remainingBooks.isEmpty()) {
        setLoading(false);
        return;
    }
    const QString book = m_remainingBooks.takeFirst();
    for (const HistoryBook &known : kHistoryBooks) {
        if (book == QLatin1String(known.name))
            m_currentBook = QLatin1String(known.direction);
    }
    m_pbap->pull(m_address, QStringLiteral("int"), book);
}

QList<RecentEntry> RecentsModel::parseCallHistory(const QByteArray &data,
                                                  const QString &fallbackDirection)
{
    QList<RecentEntry> out;
    RecentEntry current;
    bool inCard = false;

    const QList<QByteArray> lines = data.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QString line = QString::fromUtf8(rawLine).trimmed();

        if (line.compare(QLatin1String("BEGIN:VCARD"), Qt::CaseInsensitive) == 0) {
            current = RecentEntry();
            current.direction = fallbackDirection;
            inCard = true;
            continue;
        }
        if (!inCard)
            continue;
        if (line.compare(QLatin1String("END:VCARD"), Qt::CaseInsensitive) == 0) {
            inCard = false;
            // A row with no number cannot be called back.
            if (!current.number.isEmpty())
                out.append(current);
            continue;
        }

        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon < 0)
            continue;
        const QString property = line.left(colon);
        const QString value = line.mid(colon + 1);
        const QString name = property.section(QLatin1Char(';'), 0, 0).toUpper();

        if (name == QLatin1String("TEL")) {
            if (current.number.isEmpty())
                current.number = value;
        } else if (name == QLatin1String("FN")) {
            if (current.name.isEmpty())
                current.name = value;
        } else if (name == QLatin1String("N")) {
            if (current.name.isEmpty())
                current.name = value.split(QLatin1Char(';')).join(QLatin1Char(' ')).trimmed();
        } else if (name == QLatin1String("X-IRMC-CALL-DATETIME")) {
            current.direction =
                directionForParameter(property.section(QLatin1Char(';'), 1, 1), fallbackDirection);
            // The phone reports local time as yyyyMMddTHHmmss, with no zone.
            current.timestamp = QDateTime::fromString(value, QStringLiteral("yyyyMMddTHHmmss"));
        }
    }
    return out;
}

QDateTime RecentsModel::lastCalled(const QString &number) const
{
    const QString key = ContactsModel::numberKey(number);
    if (key.isEmpty())
        return {};

    QDateTime newest;
    for (const RecentEntry &e : m_entries) {
        if (ContactsModel::numberKey(e.number) != key)
            continue;
        if (!newest.isValid() || e.timestamp > newest)
            newest = e.timestamp;
    }
    return newest;
}

void RecentsModel::mergeEntries(const QList<RecentEntry> &entries)
{
    QList<RecentEntry> fresh;
    for (const RecentEntry &candidate : entries) {
        const QString key = ContactsModel::numberKey(candidate.number);
        // The same call can appear in two books, so match on number and time.
        const bool duplicate = std::any_of(
            m_entries.cbegin(), m_entries.cend(), [&](const RecentEntry &existing) {
                return ContactsModel::numberKey(existing.number) == key
                    && existing.timestamp == candidate.timestamp;
            });
        if (duplicate)
            continue;

        RecentEntry entry = candidate;
        if (entry.name.isEmpty() && m_nameResolver)
            entry.name = m_nameResolver(entry.number);
        fresh.append(entry);
    }

    if (fresh.isEmpty())
        return;

    beginResetModel();
    m_entries.append(fresh);
    sortNewestFirst();
    endResetModel();
    Q_EMIT countChanged();
}

void RecentsModel::sortNewestFirst()
{
    std::sort(m_entries.begin(), m_entries.end(),
              [](const RecentEntry &a, const RecentEntry &b) {
                  return a.timestamp > b.timestamp;
              });
}

void RecentsModel::setLoading(bool on)
{
    if (m_loading == on)
        return;
    m_loading = on;
    Q_EMIT loadingChanged();
}

void RecentsModel::setLastError(const QString &message)
{
    m_lastError = message;
    Q_EMIT lastErrorChanged();
}

void RecentsModel::clear()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();
    Q_EMIT countChanged();
}
