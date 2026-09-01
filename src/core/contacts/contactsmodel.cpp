// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "contactsmodel.h"

#include "pbapclient.h"
#include "core/logging.h"
#include "core/recents/recentsmodel.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <KLocalizedString>
#include <KContacts/Addressee>
#include <KContacts/PhoneNumber>
#include <KContacts/VCardConverter>

ContactsModel::ContactsModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_pbap(new PbapClient(this))
{
    connect(m_pbap, &PbapClient::pullFinished, this, [this](const QString &vcfPath) {
        setLoading(false);
        QFile f(vcfPath);
        if (!f.open(QIODevice::ReadOnly)) {
            setLastError(i18n("Could not read the phonebook downloaded from the phone."));
            return;
        }
        ingestVcf(f.readAll());
    });
    connect(m_pbap, &PbapClient::errorOccurred, this, [this](const QString &msg) {
        setLoading(false);
        setLastError(msg);
    });

    loadLastCalled();

    QFile cached(cachePath());
    if (cached.open(QIODevice::ReadOnly))
        setContacts(parseVcf(cached.readAll()));
}

QString ContactsModel::lastCalledPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/lastcalled");
}

void ContactsModel::setRecents(RecentsModel *recents)
{
    m_recents = recents;
    if (!m_contacts.isEmpty())
        Q_EMIT dataChanged(index(0, 0), index(m_contacts.size() - 1, 0), {LastCalledRole});
}

QDateTime ContactsModel::lastCalled(int row) const
{
    if (row < 0 || row >= m_contacts.size())
        return {};

    QDateTime newest;
    for (const QString &n : m_contacts.at(row).numbers) {
        const QDateTime stored = m_lastCalledByKey.value(numberKey(n));
        if (stored.isValid() && (!newest.isValid() || stored > newest))
            newest = stored;

        if (!m_recents)
            continue;
        const QDateTime live = m_recents->lastCalled(n);
        if (live.isValid() && (!newest.isValid() || live > newest))
            newest = live;
    }
    return newest;
}

void ContactsModel::noteCall(const QString &number, const QDateTime &when)
{
    const QString key = numberKey(number);
    if (key.isEmpty() || !when.isValid())
        return;
    const QDateTime existing = m_lastCalledByKey.value(key);
    if (existing.isValid() && existing >= when)
        return;

    m_lastCalledByKey.insert(key, when);
    saveLastCalled();
    qCDebug(FONER_CONTACTS) << "last called" << key << when;

    if (!m_contacts.isEmpty())
        Q_EMIT dataChanged(index(0, 0), index(m_contacts.size() - 1, 0), {LastCalledRole});
}

void ContactsModel::loadLastCalled()
{
    QFile f(lastCalledPath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    QDataStream in(&f);
    in.setVersion(QDataStream::Qt_6_0);
    in >> m_lastCalledByKey;
    if (in.status() != QDataStream::Ok) {
        qCWarning(FONER_CONTACTS) << "last-called store is unreadable, starting empty";
        m_lastCalledByKey.clear();
    }
}

void ContactsModel::saveLastCalled() const
{
    const QString path = lastCalledPath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(FONER_CONTACTS) << "cannot write the last-called store:" << f.errorString();
        return;
    }
    QDataStream out(&f);
    out.setVersion(QDataStream::Qt_6_0);
    out << m_lastCalledByKey;
}

QString ContactsModel::cachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/phonebook.vcf");
}

void ContactsModel::ingestVcf(const QByteArray &data)
{
    const QList<ContactItem> contacts = parseVcf(data);
    setContacts(contacts);
    if (contacts.isEmpty())
        return;

    const QString path = cachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    // A failed cache write is not fatal - the contacts are already loaded - but
    // it is why they will be gone after a restart, so say so rather than let
    // the user find out later with no explanation.
    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || out.write(data) != data.size()) {
        setLastError(i18n("Loaded the contacts, but could not save them for next time: %1",
                          out.errorString()));
    }
}

int ContactsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_contacts.size();
}

QVariant ContactsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_contacts.size())
        return {};
    const ContactItem &c = m_contacts.at(index.row());
    switch (role) {
    case NameRole: return c.name;
    case NumbersRole: return QVariant::fromValue(c.numbers);
    case PrimaryNumberRole: return c.primaryNumber();
    case PhotoRole: return c.photo;
    case RankRole: return matchRank(index.row(), m_query);
    case LastCalledRole: return lastCalled(index.row());
    case HighlightedNameRole: return highlightedName(index.row(), m_query);
    }
    return {};
}

QHash<int, QByteArray> ContactsModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {NumbersRole, "numbers"},
        {PrimaryNumberRole, "phone"},
        {PhotoRole, "photo"},
        {RankRole, "rank"},
        {LastCalledRole, "lastCalled"},
        {HighlightedNameRole, "highlightedName"},
    };
}

void ContactsModel::setQuery(const QString &query)
{
    if (m_query == query)
        return;
    m_query = query;
    Q_EMIT queryChanged();
    if (!m_contacts.isEmpty())
        Q_EMIT dataChanged(index(0, 0), index(m_contacts.size() - 1, 0),
                           {RankRole, HighlightedNameRole});
}

void ContactsModel::setLoading(bool on)
{
    if (m_loading == on)
        return;
    m_loading = on;
    Q_EMIT loadingChanged();
}

void ContactsModel::clear()
{
    beginResetModel();
    m_contacts.clear();
    endResetModel();
    Q_EMIT countChanged();
    // Or the next start would bring them all back.
    QFile::remove(cachePath());
}

void ContactsModel::loadFromPhone(const QString &deviceAddress)
{
    setLoading(true);
    m_pbap->pull(deviceAddress, QStringLiteral("int"), QStringLiteral("pb"));
}

void ContactsModel::loadFromVcfFile(const QString &filePath)
{
    // QML file dialogs hand back file:// URLs; plain paths also work.
    const QUrl url(filePath);
    QFile f(url.isLocalFile() ? url.toLocalFile() : filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        setLastError(i18n("Could not open %1.", filePath));
        return;
    }
    ingestVcf(f.readAll());
}

bool ContactsModel::exportVcfFile(const QString &filePath)
{
    const QUrl url(filePath);
    const QString path = url.isLocalFile() ? url.toLocalFile() : filePath;

    if (m_contacts.isEmpty()) {
        setLastError(i18n("There are no contacts to export."));
        return false;
    }

    KContacts::Addressee::List addressees;
    addressees.reserve(m_contacts.size());
    for (const ContactItem &c : m_contacts) {
        KContacts::Addressee a;
        a.setFormattedName(c.name);
        for (const QString &number : c.numbers)
            // No type: the model stores numbers as plain strings, so the
            // original Home/Work/Cell distinction is already gone by here.
            // Claiming Cell would invent information the import never kept.
            a.insertPhoneNumber(KContacts::PhoneNumber(number));
        addressees.append(a);
    }

    // QSaveFile so a failure part way through cannot truncate a file the user
    // already had.
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        setLastError(i18n("Could not write to %1.", path));
        return false;
    }
    const QByteArray data =
        KContacts::VCardConverter().exportVCards(addressees, KContacts::VCardConverter::v3_0);
    if (out.write(data) != data.size() || !out.commit()) {
        setLastError(i18n("Could not write to %1.", path));
        return false;
    }

    qCInfo(FONER_CONTACTS) << "exported" << addressees.size() << "contacts to" << path;
    return true;
}

void ContactsModel::setContacts(const QList<ContactItem> &contacts)
{
    beginResetModel();
    m_contacts = contacts;
    endResetModel();
    Q_EMIT countChanged();
    Q_EMIT loadFinished(m_contacts.size());
}

void ContactsModel::setLastError(const QString &message)
{
    m_lastError = message;
    Q_EMIT lastErrorChanged();
    Q_EMIT error(message);
}

QString ContactsModel::numberKey(const QString &number)
{
    QString digits;
    digits.reserve(number.size());
    for (const QChar &ch : number) {
        if (ch.isDigit())
            digits.append(ch);
    }
    // ponytail: 9 fits Algerian subscriber numbers; widen if this ships elsewhere.
    constexpr int kSignificantDigits = 9;
    return digits.right(kSignificantDigits);
}

QString ContactsModel::nameForNumber(const QString &number) const
{
    const QString key = numberKey(number);
    if (key.isEmpty())
        return number;
    for (const ContactItem &c : m_contacts) {
        for (const QString &n : c.numbers) {
            if (numberKey(n) == key)
                return c.name;
        }
    }
    return number;
}

QString ContactsModel::foldAccents(const QString &text)
{
    // Decomposed first, so the combining marks of an accented letter can be dropped.
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(decomposed.size());
    for (const QChar &ch : decomposed) {
        if (ch.category() != QChar::Mark_NonSpacing)
            out.append(ch);
    }
    return out;
}

QString ContactsModel::t9Key(const QString &name)
{
    const QString folded = foldAccents(name).toUpper();

    QString out;
    out.reserve(folded.size());
    for (const QChar &ch : folded) {
        if (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z')) {
            // ITU keypad: ABC=2 DEF=3 GHI=4 JKL=5 MNO=6 PQRS=7 TUV=8 WXYZ=9.
            static const char *kLetterToDigit = "22233344455566677778889999";
            out.append(QLatin1Char(kLetterToDigit[ch.unicode() - 'A']));
        } else if (ch.isDigit()) {
            out.append(ch);
        } else {
            // A separator, so word boundaries stay visible to the ranking.
            out.append(QLatin1Char(' '));
        }
    }
    return out;
}

namespace {

QString digitsOnly(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar &ch : text) {
        if (ch.isDigit())
            out.append(ch);
    }
    return out;
}

} // namespace

int ContactsModel::matchRank(int row, const QString &query) const
{
    if (row < 0 || row >= m_contacts.size())
        return kNoMatch;
    if (query.isEmpty())
        return 0;

    const ContactItem &c = m_contacts.at(row);

    // Both sides folded, or a phonebook of French and Arabic transliterations only
    // answers to the exact accent the contact was saved with.
    const int nameAt = c.folded.indexOf(foldAccents(query), 0, Qt::CaseInsensitive);
    if (nameAt == 0)
        return 0;
    if (nameAt > 0)
        return c.folded.at(nameAt - 1).isSpace() ? 1 : 2;

    const QString queryDigits = digitsOnly(query);
    if (queryDigits.isEmpty())
        return kNoMatch;

    // Digits match both T9 against the name and the number itself.
    const int t9At = c.t9.indexOf(queryDigits);
    if (t9At == 0)
        return 0;
    if (t9At > 0)
        return c.t9.at(t9At - 1) == QLatin1Char(' ') ? 1 : 2;

    for (const QString &n : c.numbers) {
        if (numberKey(n).contains(queryDigits) || digitsOnly(n).contains(queryDigits))
            return 3;
    }
    return kNoMatch;
}

int ContactsModel::matchOffset(int row, const QString &query) const
{
    if (row < 0 || row >= m_contacts.size() || query.isEmpty())
        return -1;

    // folded and t9 are built character for character from the name, so an index
    // into either lands on the same character of the name itself.
    const ContactItem &c = m_contacts.at(row);
    const int nameAt = c.folded.indexOf(foldAccents(query), 0, Qt::CaseInsensitive);
    if (nameAt >= 0)
        return nameAt;

    const QString queryDigits = digitsOnly(query);
    if (queryDigits.isEmpty())
        return -1;
    const int t9At = c.t9.indexOf(queryDigits);
    if (t9At >= 0)
        return t9At;

    // A number-only match highlights nothing; the name did not match at all.
    return -1;
}

int ContactsModel::matchLength(int row, const QString &query) const
{
    if (matchOffset(row, query) < 0)
        return 0;
    const ContactItem &c = m_contacts.at(row);
    const QString folded = foldAccents(query);
    if (c.folded.indexOf(folded, 0, Qt::CaseInsensitive) >= 0)
        return folded.length();
    return digitsOnly(query).length();
}

QString ContactsModel::highlightedName(int row, const QString &query) const
{
    if (row < 0 || row >= m_contacts.size())
        return {};
    const QString name = m_contacts.at(row).name;

    const int at = matchOffset(row, query);
    const int len = matchLength(row, query);
    if (at < 0 || len <= 0 || at + len > name.length())
        return name.toHtmlEscaped();

    return name.left(at).toHtmlEscaped()
        + QStringLiteral("<b>") + name.mid(at, len).toHtmlEscaped() + QStringLiteral("</b>")
        + name.mid(at + len).toHtmlEscaped();
}

bool ContactsModel::matches(int row, const QString &query) const
{
    return matchRank(row, query) != kNoMatch;
}

QList<ContactItem> ContactsModel::parseVcf(const QByteArray &data)
{
    QList<ContactItem> out;
    const KContacts::Addressee::List addressees = KContacts::VCardConverter().parseVCards(data);
    out.reserve(addressees.size());
    for (const KContacts::Addressee &a : addressees) {
        ContactItem item;
        item.name = a.formattedName();
        if (item.name.isEmpty())
            item.name = a.realName();
        const KContacts::PhoneNumber::List numbers = a.phoneNumbers();
        for (const KContacts::PhoneNumber &n : numbers)
            item.numbers.append(n.number());
        if (item.numbers.isEmpty())
            continue;
        // Folded once here rather than on every keystroke.
        item.t9 = t9Key(item.name);
        item.folded = foldAccents(item.name);
        out.append(item);
    }
    return out;
}