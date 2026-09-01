// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "core/contacts/contactitem.h"
#include "core/contacts/contactsmodel.h"
#include "core/recents/recententry.h"
#include "core/recents/recentsmodel.h"

class TestContacts : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // ContactsModel writes its caches under AppDataLocation. Point that at a
        // scratch directory so a test run never touches the real phonebook or
        // the real last-called store.
        QVERIFY(m_dataDir.isValid());
        qputenv("XDG_DATA_HOME", m_dataDir.path().toUtf8());
    }

    void parsesVcard21()
    {
        // Real phone output is vCard 2.1, not 3.0.
        const QByteArray data =
            "BEGIN:VCARD\r\n"
            "VERSION:2.1\r\n"
            "N:Example;Alice;;;\r\n"
            "FN:Alice Example\r\n"
            "TEL;CELL:0550000002\r\n"
            "END:VCARD\r\n";
        const QList<ContactItem> contacts = ContactsModel::parseVcf(data);
        QCOMPARE(contacts.size(), 1);
        QCOMPARE(contacts.at(0).name, QStringLiteral("Alice Example"));
        QCOMPARE(contacts.at(0).numbers.size(), 1);
        QCOMPARE(contacts.at(0).numbers.at(0), QStringLiteral("0550000002"));
    }

    void keepsEveryNumberOfAMultiNumberContact()
    {
        const QByteArray data =
            "BEGIN:VCARD\r\n"
            "VERSION:2.1\r\n"
            "FN:Alice Example\r\n"
            "TEL;CELL:0550000002\r\n"
            "TEL;HOME:+213550000001\r\n"
            "END:VCARD\r\n";
        const QList<ContactItem> contacts = ContactsModel::parseVcf(data);
        QCOMPARE(contacts.size(), 1);
        QCOMPARE(contacts.at(0).numbers.size(), 2);
    }

    void anAccentedNameIsFoundWithoutTheAccent()
    {
        // A phonebook of French and Arabic transliterations is unusable if it only
        // answers to the exact accent the contact was saved with.
        const QByteArray data =
            "BEGIN:VCARD\r\n"
            "VERSION:2.1\r\n"
            "FN;CHARSET=UTF-8:B\xc3\xa9" "chir Bouzid\r\n"
            "TEL;CELL:+213550000013\r\n"
            "END:VCARD\r\n";
        ContactsModel model;
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(data);
        f.close();
        model.loadFromVcfFile(f.fileName());
        QCOMPARE(model.rowCount(), 1);

        QCOMPARE(model.matchRank(0, QStringLiteral("Bechir")), 0);
        QCOMPARE(model.matchRank(0, QString::fromUtf8("B\xc3\xa9" "chir")), 0);
        QCOMPARE(model.matchRank(0, QStringLiteral("bechir")), 0);
        // The keypad path already folded; it must keep working.
        QCOMPARE(model.matchRank(0, QStringLiteral("232447")), 0);
        QCOMPARE(model.matchRank(0, QStringLiteral("Bouzid")), 1);
        QVERIFY(!model.matches(0, QStringLiteral("Zzz")));
    }

    void skipsCardsWithNoNumber()
    {
        // The phone's own card carries no TEL; it must not become a contact.
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nN:;My name;;;\r\nFN:My name\r\nEND:VCARD\r\n";
        QCOMPARE(ContactsModel::parseVcf(data).size(), 0);
    }

    void returnsNothingForGarbage()
    {
        QCOMPARE(ContactsModel::parseVcf(QByteArray("not a vcard at all")).size(), 0);
    }

    void nationalAndInternationalFormsShareAKey()
    {
        // A phonebook mixes national and international forms.
        QCOMPARE(ContactsModel::numberKey(QStringLiteral("0550000001")),
                 ContactsModel::numberKey(QStringLiteral("+213550000001")));
        QCOMPARE(ContactsModel::numberKey(QStringLiteral("0550 00 00 01")),
                 ContactsModel::numberKey(QStringLiteral("+213-550-000-001")));
    }

    void differentNumbersDoNotShareAKey()
    {
        QVERIFY(ContactsModel::numberKey(QStringLiteral("0550000001"))
                != ContactsModel::numberKey(QStringLiteral("0550000009")));
    }

    void resolvesANameAcrossNumberFormats()
    {
        ContactsModel model;
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Alice Example\r\n"
            "TEL;CELL:0550000001\r\nEND:VCARD\r\n";
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(data);
        f.close();
        model.loadFromVcfFile(f.fileName());
        QCOMPARE(model.rowCount(), 1);

        QCOMPARE(model.nameForNumber(QStringLiteral("+213550000001")),
                 QStringLiteral("Alice Example"));
        // An unknown number comes back unchanged.
        QCOMPARE(model.nameForNumber(QStringLiteral("+213550000009")),
                 QStringLiteral("+213550000009"));
    }

    void searchMatchesNamesAndNumbers()
    {
        ContactsModel model;
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Alice Example\r\n"
            "TEL;CELL:0550000001\r\nEND:VCARD\r\n"
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Bob Other\r\n"
            "TEL;CELL:0550000042\r\nEND:VCARD\r\n";
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(data);
        f.close();
        model.loadFromVcfFile(f.fileName());
        QCOMPARE(model.rowCount(), 2);

        // Rows are in file order: Alice then Bob.
        QVERIFY(model.matches(0, QStringLiteral("alice")));   // case-insensitive name
        QVERIFY(!model.matches(1, QStringLiteral("alice")));
        QVERIFY(model.matches(1, QStringLiteral("0042")));    // digits hit the number
        QVERIFY(!model.matches(0, QStringLiteral("0042")));
        QVERIFY(model.matches(0, QString()));                 // empty query keeps everything
        QVERIFY(model.matches(1, QString()));
    }

    void foldsNamesToKeypadDigits()
    {
        // The canonical example: M-I-K-E is 6-4-5-3.
        QCOMPARE(ContactsModel::t9Key(QStringLiteral("Mike")), QStringLiteral("6453"));
        QCOMPARE(ContactsModel::t9Key(QStringLiteral("abc def")), QStringLiteral("222 333"));
        QCOMPARE(ContactsModel::t9Key(QStringLiteral("PQRS WXYZ")), QStringLiteral("7777 9999"));
    }

    void foldsAccentsToTheBaseLetter()
    {
        // Accents fold to their base letter.
        QCOMPARE(ContactsModel::t9Key(QStringLiteral("Béchir")),
                 ContactsModel::t9Key(QStringLiteral("Bechir")));
        QCOMPARE(ContactsModel::t9Key(QStringLiteral("Zoë")),
                 ContactsModel::t9Key(QStringLiteral("Zoe")));
    }

    void ranksNameStartAboveWordStartAboveMidWord()
    {
        ContactsModel model;
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Ali Example\r\n"
            "TEL;CELL:0550000011\r\nEND:VCARD\r\n"
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Sami Ali\r\n"
            "TEL;CELL:0550000012\r\nEND:VCARD\r\n"
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Khalil Other\r\n"
            "TEL;CELL:0550000013\r\nEND:VCARD\r\n";
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(data);
        f.close();
        model.loadFromVcfFile(f.fileName());
        QCOMPARE(model.rowCount(), 3);

        // "ALI" is 2-5-4 on the keypad.
        const QString query = QStringLiteral("254");
        QCOMPARE(model.matchRank(0, query), 0);   // "Ali Example"  -- starts the name
        QCOMPARE(model.matchRank(1, query), 1);   // "Sami Ali"     -- starts a word
        QCOMPARE(model.matchRank(2, query), 2);   // "Khalil Other" -- mid-word
    }

    void ranksANumberOnlyMatchLast()
    {
        ContactsModel model;
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Ali Example\r\n"
            "TEL;CELL:0550000011\r\nEND:VCARD\r\n"
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Bob Other\r\n"
            "TEL;CELL:0550000254\r\nEND:VCARD\r\n";
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(data);
        f.close();
        model.loadFromVcfFile(f.fileName());

        const QString query = QStringLiteral("254");
        QCOMPARE(model.matchRank(0, query), 0);   // name match wins
        QCOMPARE(model.matchRank(1, query), 3);   // only the digits match
        QVERIFY(model.matches(1, query));
    }

    void reportsNoMatchRatherThanGuessing()
    {
        ContactsModel model;
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Ali Example\r\n"
            "TEL;CELL:0550000011\r\nEND:VCARD\r\n";
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(data);
        f.close();
        model.loadFromVcfFile(f.fileName());

        QCOMPARE(model.matchRank(0, QStringLiteral("999888")), ContactsModel::kNoMatch);
        QVERIFY(!model.matches(0, QStringLiteral("999888")));
        // An out-of-range row must not be reported as a match either.
        QCOMPARE(model.matchRank(7, QStringLiteral("254")), ContactsModel::kNoMatch);
    }

    void matchOffsetLocatesTheMatchedPrefix()
    {
        ContactsModel model;
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Amine Badri\r\n"
            "TEL;CELL:+213550000031\r\nEND:VCARD\r\n";
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(data);
        f.close();
        model.loadFromVcfFile(f.fileName());
        QCOMPARE(model.rowCount(), 1);

        // Name start.
        QCOMPARE(model.matchOffset(0, QStringLiteral("Ami")), 0);
        QCOMPARE(model.matchLength(0, QStringLiteral("Ami")), 3);

        // Word start: "Badri" begins at index 6 of "Amine Badri".
        QCOMPARE(model.matchOffset(0, QStringLiteral("Bad")), 6);

        // T9: 26463 spells AMINE on the keypad.
        QCOMPARE(model.matchOffset(0, QStringLiteral("26463")), 0);
        QCOMPARE(model.matchLength(0, QStringLiteral("26463")), 5);

        // No match, and no highlight for a number-only hit.
        QCOMPARE(model.matchOffset(0, QStringLiteral("Zzz")), -1);
        QCOMPARE(model.matchOffset(0, QStringLiteral("0000031")), -1);
        QCOMPARE(model.matchOffset(0, QString()), -1);
        QCOMPARE(model.matchOffset(99, QStringLiteral("Ami")), -1);
    }

    void matchOffsetIsIndexedOnTheUnfoldedName()
    {
        // The offset is used to slice the name shown on screen, which keeps its
        // accents. An index taken from the folded form must still land right.
        ContactsModel model;
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\n"
            "FN;CHARSET=UTF-8:B\xc3\xa9" "chir Bouzid\r\n"
            "TEL;CELL:+213550000013\r\nEND:VCARD\r\n";
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(data);
        f.close();
        model.loadFromVcfFile(f.fileName());

        QCOMPARE(model.matchOffset(0, QStringLiteral("Bechir")), 0);
        // "Bouzid" starts at index 7 of "Béchir Bouzid" in both forms.
        QCOMPARE(model.matchOffset(0, QStringLiteral("Bouzid")), 7);
    }

    void lastCalledPicksTheNewestAcrossAContactsNumbers()
    {
        ContactsModel model;
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Karim Larbi\r\n"
            "TEL;CELL:+213550000021\r\nTEL;HOME:0550000022\r\nEND:VCARD\r\n";
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(data);
        f.close();
        model.loadFromVcfFile(f.fileName());
        QCOMPARE(model.rowCount(), 1);

        RecentsModel recents;
        model.setRecents(&recents);
        const QDateTime older = QDateTime::fromString(QStringLiteral("20260101T090000"),
                                                      QStringLiteral("yyyyMMddTHHmmss"));
        const QDateTime newer = QDateTime::fromString(QStringLiteral("20260102T170000"),
                                                      QStringLiteral("yyyyMMddTHHmmss"));
        recents.append(QStringLiteral("+213550000021"), QStringLiteral("outgoing"), older);
        // The second number of the same contact, and the newer of the two.
        recents.append(QStringLiteral("0550000022"), QStringLiteral("incoming"), newer);

        QCOMPARE(model.lastCalled(0), newer);
    }

    void lastCalledMatchesAcrossNumberFormats()
    {
        // Recents hold the national form, the phonebook the international one.
        ContactsModel model;
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Ali Amrani\r\n"
            "TEL;CELL:+213550000012\r\nEND:VCARD\r\n";
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(data);
        f.close();
        model.loadFromVcfFile(f.fileName());

        RecentsModel recents;
        model.setRecents(&recents);
        const QDateTime when = QDateTime::fromString(QStringLiteral("20260103T120000"),
                                                     QStringLiteral("yyyyMMddTHHmmss"));
        recents.append(QStringLiteral("0550000012"), QStringLiteral("outgoing"), when);

        QCOMPARE(model.lastCalled(0), when);
    }

    void aContactNeverCalledHasNoTimestamp()
    {
        ContactsModel model;
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Nadia Ouali\r\n"
            "TEL;CELL:+213550000026\r\nEND:VCARD\r\n";
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(data);
        f.close();
        model.loadFromVcfFile(f.fileName());

        RecentsModel recents;
        model.setRecents(&recents);
        QVERIFY(!model.lastCalled(0).isValid());
        // Out of range must not crash or invent a time either.
        QVERIFY(!model.lastCalled(99).isValid());
    }

    void aNotedCallBeatsAnOlderRecentsEntry()
    {
        ContactsModel model;
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Omar Rahmani\r\n"
            "TEL;CELL:+213550000027\r\nEND:VCARD\r\n";
        QTemporaryFile f;
        QVERIFY(f.open());
        f.write(data);
        f.close();
        model.loadFromVcfFile(f.fileName());

        RecentsModel recents;
        model.setRecents(&recents);
        const QDateTime older = QDateTime::fromString(QStringLiteral("20260101T080000"),
                                                      QStringLiteral("yyyyMMddTHHmmss"));
        const QDateTime newer = QDateTime::fromString(QStringLiteral("20260105T080000"),
                                                      QStringLiteral("yyyyMMddTHHmmss"));
        recents.append(QStringLiteral("+213550000027"), QStringLiteral("outgoing"), older);
        model.noteCall(QStringLiteral("+213550000027"), newer);

        QCOMPARE(model.lastCalled(0), newer);
        // An older note must never move the stamp backwards.
        model.noteCall(QStringLiteral("+213550000027"), older);
        QCOMPARE(model.lastCalled(0), newer);
    }

    void parsesCallHistoryAsThePhoneWritesIt()
    {
        // Shaped as the handset writes it. Synthetic data.
        const QByteArray data =
            "BEGIN:VCARD\r\n"
            "VERSION:2.1\r\n"
            "FN:Sami Example\r\n"
            "N:Sami Example\r\n"
            "TEL;CELL:0550000003\r\n"
            "X-IRMC-CALL-DATETIME;RECEIVED:20260810T133238\r\n"
            "END:VCARD\r\n"
            "BEGIN:VCARD\r\n"
            "VERSION:2.1\r\n"
            "FN:\r\n"
            "N:\r\n"
            "TEL;VOICE:0550000004\r\n"
            "X-IRMC-CALL-DATETIME;RECEIVED:20260810T095636\r\n"
            "END:VCARD\r\n";

        const QList<RecentEntry> entries =
            RecentsModel::parseCallHistory(data, QStringLiteral("incoming"));
        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries.at(0).name, QStringLiteral("Sami Example"));
        QCOMPARE(entries.at(0).number, QStringLiteral("0550000003"));
        QCOMPARE(entries.at(0).direction, QStringLiteral("incoming"));
        QCOMPARE(entries.at(0).timestamp,
                 QDateTime(QDate(2026, 8, 10), QTime(13, 32, 38)));
        // A nameless card still yields a callable number.
        QCOMPARE(entries.at(1).number, QStringLiteral("0550000004"));
        QVERIFY(entries.at(1).name.isEmpty());
    }

    void readsDirectionFromTheCardNotTheBook()
    {
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nTEL;CELL:0550000003\r\n"
            "X-IRMC-CALL-DATETIME;DIALED:20260810T133238\r\nEND:VCARD\r\n";
        const QList<RecentEntry> entries =
            RecentsModel::parseCallHistory(data, QStringLiteral("missed"));
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.at(0).direction, QStringLiteral("outgoing"));
    }

    void fallsBackToTheBookWhenTheCardIsSilent()
    {
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nTEL;CELL:0550000003\r\nEND:VCARD\r\n";
        const QList<RecentEntry> entries =
            RecentsModel::parseCallHistory(data, QStringLiteral("missed"));
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.at(0).direction, QStringLiteral("missed"));
    }

    void skipsHistoryCardsWithNoNumber()
    {
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:2.1\r\nFN:Nobody\r\n"
            "X-IRMC-CALL-DATETIME;MISSED:20260810T133238\r\nEND:VCARD\r\n";
        QCOMPARE(RecentsModel::parseCallHistory(data, QStringLiteral("missed")).size(), 0);
    }

    void doesNotRecordTheSameCallTwice()
    {
        // ich and mch can both report a missed call.
        RecentsModel model;
        const QDateTime when(QDate(2026, 8, 10), QTime(13, 32, 38));
        model.append(QStringLiteral("0550000003"), QStringLiteral("incoming"), when, 0);
        model.append(QStringLiteral("+213550000003"), QStringLiteral("missed"), when, 0);
        QCOMPARE(model.rowCount(), 1);
    }

    void keepsTheNewestCallFirst()
    {
        RecentsModel model;
        model.append(QStringLiteral("0550000003"), QStringLiteral("incoming"),
                     QDateTime(QDate(2026, 8, 10), QTime(9, 0, 0)), 0);
        model.append(QStringLiteral("0550000004"), QStringLiteral("outgoing"),
                     QDateTime(QDate(2026, 8, 10), QTime(13, 0, 0)), 0);
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0, 0), RecentsModel::NumberRole).toString(),
                 QStringLiteral("0550000004"));
    }

    void exportedContactsReimportIdentically()
    {
        // The round trip is the whole contract of the export: a file this app
        // writes has to be a file this app can read back.
        const QByteArray data =
            "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Ali Amrani\r\n"
            "TEL;TYPE=CELL:+213550000012\r\nEND:VCARD\r\n"
            "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:B\xc3\xa9""chir Bouzid\r\n"
            "TEL;TYPE=CELL:+213550000013\r\nTEL;TYPE=CELL:0550000014\r\n"
            "END:VCARD\r\n";
        QTemporaryFile in;
        QVERIFY(in.open());
        in.write(data);
        in.close();

        ContactsModel model;
        model.loadFromVcfFile(in.fileName());
        QCOMPARE(model.rowCount(), 2);

        const QString path = m_dataDir.filePath(QStringLiteral("out.vcf"));
        QVERIFY2(model.exportVcfFile(path), qPrintable(model.lastError()));

        ContactsModel reloaded;
        reloaded.loadFromVcfFile(path);
        QCOMPARE(reloaded.rowCount(), 2);

        // Accents have to survive the encoding round trip, not just the count.
        QStringList names;
        for (int row = 0; row < reloaded.rowCount(); ++row)
            names << reloaded.data(reloaded.index(row, 0), ContactsModel::NameRole).toString();
        names.sort();
        QCOMPARE(names, QStringList({QStringLiteral("Ali Amrani"),
                                     QString::fromUtf8("B\xc3\xa9""chir Bouzid")}));

        // A contact with two numbers must not come back with one.
        for (int row = 0; row < reloaded.rowCount(); ++row) {
            const QString name =
                reloaded.data(reloaded.index(row, 0), ContactsModel::NameRole).toString();
            if (name != QString::fromUtf8("B\xc3\xa9""chir Bouzid"))
                continue;
            const QVariant numbers =
                reloaded.data(reloaded.index(row, 0), ContactsModel::NumbersRole);
            QCOMPARE(numbers.toStringList().size(), 2);
        }
    }

    void exportingAnEmptyPhonebookFailsLoudly()
    {
        // The constructor restores the cached phonebook, and an earlier test in
        // this class writes that cache, so "empty" has to be made explicitly.
        ContactsModel model;
        model.clear();
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(!model.exportVcfFile(m_dataDir.filePath(QStringLiteral("empty.vcf"))));
        QVERIFY(!model.lastError().isEmpty());
    }

    void exportingToAnUnwritablePathFails()
    {
        const QByteArray data = "BEGIN:VCARD\r\nVERSION:3.0\r\nFN:Ali Amrani\r\n"
                                "TEL;TYPE=CELL:+213550000012\r\nEND:VCARD\r\n";
        QTemporaryFile in;
        QVERIFY(in.open());
        in.write(data);
        in.close();

        ContactsModel model;
        model.loadFromVcfFile(in.fileName());
        QCOMPARE(model.rowCount(), 1);
        QVERIFY(!model.exportVcfFile(QStringLiteral("/proc/foner-cannot-write.vcf")));
        QVERIFY(!model.lastError().isEmpty());
    }

    void oneFailureReportsExactlyOneError()
    {
        // setLastError emits lastErrorChanged and error together. The window
        // once connected both and reported at the call site as well, so a
        // single failed export raised three banners. Whatever the window
        // listens to, one failure has to mean one error.
        ContactsModel model;
        model.clear();
        QSignalSpy errors(&model, &ContactsModel::error);
        QVERIFY(!model.exportVcfFile(m_dataDir.filePath(QStringLiteral("nope.vcf"))));
        QCOMPARE(errors.count(), 1);
    }

private:
    QTemporaryDir m_dataDir;
};

QTEST_GUILESS_MAIN(TestContacts)
#include "test_contacts.moc"
