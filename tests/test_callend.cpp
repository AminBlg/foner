// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QtTest>

#include "core/telephony/call.h"
#include "core/telephony/agmodel.h"
#include "core/telephony/bluezdevices.h"
#include "core/telephony/callmodel.h"
#include "core/telephony/telephonyclient.h"
#include "core/telephony/pendingendtracker.h"

namespace {
// The predicate the window's guards are written in terms of: main.qml asks
// anyCallIs("active") and anyCallIs("held") before offering swap and merge.
bool anyCallIs(const CallModel &model, const QString &state)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        if (model.data(model.index(row, 0), CallModel::StateRole).toString() == state)
            return true;
    }
    return false;
}
} // namespace

class TestCallEnd : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void doesNotFlushBeforeGraceWindow()
    {
        PendingEndTracker tracker;
        tracker.markDropped(QStringLiteral("/call1"), QStringLiteral("+213550000001"), false, 0, 1000);
        QCOMPARE(tracker.flush(1000).size(), 0);
        QCOMPARE(tracker.flush(2499).size(), 0);
    }

    void flushesAfterGraceWindow()
    {
        PendingEndTracker tracker;
        tracker.markDropped(QStringLiteral("/call1"), QStringLiteral("+213550000001"), true, 42, 1000);
        const QList<PendingEndTracker::Ended> ended = tracker.flush(4000);
        QCOMPARE(ended.size(), 1);
        QCOMPARE(ended.at(0).number, QStringLiteral("+213550000001"));
        QCOMPARE(ended.at(0).incoming, true);
        QCOMPARE(ended.at(0).durationSec, 42);
        // Flushing is destructive.
        QCOMPARE(tracker.flush(9000).size(), 0);
    }

    void resumedCallCancelsPendingEnd()
    {
        PendingEndTracker tracker;
        tracker.markDropped(QStringLiteral("/call1"), QStringLiteral("+213550000001"), false, 0, 1000);
        tracker.cancel(QStringLiteral("/call1"));
        QCOMPARE(tracker.flush(9000).size(), 0);
    }

    void reDroppingKeepsTheEarliestDeadline()
    {
        // A call flapping between polls keeps its original deadline.
        PendingEndTracker tracker;
        tracker.markDropped(QStringLiteral("/call1"), QStringLiteral("+213550000001"), false, 0, 1000);
        tracker.markDropped(QStringLiteral("/call1"), QStringLiteral("+213550000001"), false, 0, 3000);
        QCOMPARE(tracker.flush(4000).size(), 1);
    }

    void aRecycledPathCanEndTwice()
    {
        // Call paths recycle as callN, so the same path can end twice.
        PendingEndTracker tracker;
        tracker.markDropped(QStringLiteral("/call1"), QStringLiteral("+213550000001"), false, 0, 1000);
        QCOMPARE(tracker.flush(5000).size(), 1);
        tracker.markDropped(QStringLiteral("/call1"), QStringLiteral("+213550000009"), true, 7, 6000);
        const QList<PendingEndTracker::Ended> second = tracker.flush(10000);
        QCOMPARE(second.size(), 1);
        QCOMPARE(second.at(0).number, QStringLiteral("+213550000009"));
    }

    void aNumberlessCallIsNotWorthRecording()
    {
        PendingEndTracker tracker;
        tracker.markDropped(QStringLiteral("/call1"), QString(), false, 0, 1000);
        QCOMPARE(tracker.flush(9000).size(), 0);
    }

    void theCallerIsReadFromLineIdentification()
    {
        // IncomingLine is the called line and the network leaves it empty, so
        // reading it alone gave a blank notification and no recents row.
        Call call(QStringLiteral("/call1"));
        call.updateProperties({{QStringLiteral("State"), QStringLiteral("incoming")},
                               {QStringLiteral("LineIdentification"), QStringLiteral("+213550000001")},
                               {QStringLiteral("IncomingLine"), QString()}});
        QCOMPARE(call.remoteNumber(), QStringLiteral("+213550000001"));
    }

    void swapNeedsAnEstablishedPairNotJustTwoCalls()
    {
        // The window showed Swap whenever a held call sat beside any second
        // call, including one still ringing. There is nothing to swap with an
        // unanswered leg, so the gateway refuses it. Merge already tested for
        // an active partner; swap did not.
        CallModel model;
        Call held(QStringLiteral("/call1"));
        held.updateProperties({{QStringLiteral("State"), QStringLiteral("held")}});
        Call ringing(QStringLiteral("/call2"));
        ringing.updateProperties({{QStringLiteral("State"), QStringLiteral("incoming")}});
        model.setCalls({&held, &ringing});

        // Two calls, one of them held: the old guard was satisfied here.
        QCOMPARE(model.rowCount(), 2);
        QVERIFY(anyCallIs(model, QStringLiteral("held")));
        // But no active leg, which is what swapping actually requires.
        QVERIFY(!anyCallIs(model, QStringLiteral("active")));

        Call answered(QStringLiteral("/call2"));
        answered.updateProperties({{QStringLiteral("State"), QStringLiteral("active")}});
        model.setCalls({&held, &answered});
        QVERIFY(anyCallIs(model, QStringLiteral("active")));
    }

    void anUnknownPhoneFallsBackToItsAddress()
    {
        // BlueZ is best effort: a machine with no Bluetooth stack, or one where
        // the daemon has not answered yet, leaves the cache empty. The window
        // titles itself from nameFor(), so returning an empty string there
        // would title the window with nothing at all. It returns the address,
        // which is what the app showed before BlueZ was consulted.
        BluezDevices devices;
        const QString address = QStringLiteral("AA:BB:CC:DD:EE:FF");
        QCOMPARE(devices.nameFor(address), address);

        // No icon to offer, and no battery rather than a plausible-looking
        // zero: a phone reporting 0% and a phone reporting nothing are
        // different states, and the chip hides the reading for the second.
        QVERIFY(devices.iconFor(address).isEmpty());
        QCOMPARE(devices.batteryFor(address), -1);
    }

    void noPhoneMeansNoSelectedAddress()
    {
        // Every dial control is guarded on devicesModel.count, but the address
        // is what the command is actually sent to, and the phonebook pull and
        // the window title both read it. With no phone it has to be empty
        // rather than stale: a leftover address would aim a command at a phone
        // that is gone.
        TelephonyClient client;
        QCOMPARE(client.devices()->rowCount(), 0);
        QVERIFY(client.selectedAddress().isEmpty());
        QVERIFY(client.transportState().isEmpty());
    }

    void serviceCodesAreNotCalls()
    {
        QVERIFY(TelephonyClient::isServiceCode(QStringLiteral("*710#")));
        QVERIFY(TelephonyClient::isServiceCode(QStringLiteral("*#06#")));
        QVERIFY(TelephonyClient::isServiceCode(QStringLiteral("##002#")));
        QVERIFY(TelephonyClient::isServiceCode(QStringLiteral("*21*0550000001#")));
        QVERIFY(TelephonyClient::isServiceCode(QStringLiteral("  *710#  ")));

        // A number is not a service code, however many symbols it carries.
        QVERIFY(!TelephonyClient::isServiceCode(QStringLiteral("+213550000001")));
        QVERIFY(!TelephonyClient::isServiceCode(QStringLiteral("0550000001")));
        // Nor is an unterminated code, which the network would never act on.
        QVERIFY(!TelephonyClient::isServiceCode(QStringLiteral("*710")));
        QVERIFY(!TelephonyClient::isServiceCode(QStringLiteral("710#")));
        QVERIFY(!TelephonyClient::isServiceCode(QString()));
        QVERIFY(!TelephonyClient::isServiceCode(QStringLiteral("*#")));
    }

    void theCallerFallsBackToIncomingLine()
    {
        Call call(QStringLiteral("/call1"));
        call.updateProperties({{QStringLiteral("LineIdentification"), QString()},
                               {QStringLiteral("IncomingLine"), QStringLiteral("+213550000002")}});
        QCOMPARE(call.remoteNumber(), QStringLiteral("+213550000002"));
    }
};

QTEST_GUILESS_MAIN(TestCallEnd)
#include "test_callend.moc"
