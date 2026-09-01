// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "pendingendtracker.h"

class QDBusServiceWatcher;
class QDBusMessage;
class QDBusObjectPath;
class QTimer;
class AgModel;
class CallModel;
class AgDevice;
class Call;

class TelephonyClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AgModel *devices READ devices CONSTANT)
    Q_PROPERTY(CallModel *calls READ calls CONSTANT)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)
    // "active" once the phone opens a SCO link to this machine.
    Q_PROPERTY(QString transportState READ transportState NOTIFY transportStateChanged)
    // Address of the phone commands go to. The single source of truth for which
    // phone is selected: reading row 0 from the model instead would dial on one
    // phone and pull the phonebook from another.
    Q_PROPERTY(QString selectedAddress READ selectedAddress NOTIFY selectedAddressChanged)

public:
    explicit TelephonyClient(QObject *parent = nullptr);
    ~TelephonyClient() override;

    AgModel *devices() const { return m_devices; }
    CallModel *calls() const { return m_calls; }
    bool available() const { return m_available; }
    bool muted() const { return m_muted; }
    QString transportState() const;
    QString selectedAddress() const;

    Q_INVOKABLE void setSelectedDevice(int row);

    // A GSM 02.30 service code such as *710# or *#06#: starts with * or #, ends
    // with #, digits and *# in between. Dialled like a number, but it is not a call.
    Q_INVOKABLE static bool isServiceCode(const QString &number);

    // Opens the desktop's Bluetooth settings. bluedevil:// is not a KIO
    // protocol, so opening it as a URL only produced a "could not read file"
    // dialog; the settings module has to be launched as a process.
    Q_INVOKABLE static void openBluetoothSettings();

public Q_SLOTS:
    void dial(const QString &number);
    void answerCall(const QString &objectPath);
    void hangupCall(const QString &objectPath);

    // AG-level call control. HFP has no per-call hold.
    void hangupAll();
    void swapCalls();
    void holdAndAnswer();
    void releaseAndAnswer();
    void releaseAndSwap();
    void createMultiparty();
    void sendTones(const QString &tones);

    // No mute method on the AG: muting is microphone volume 0.
    void setMuted(bool muted);
    void setSpeakerVolume(int volume);

    // Move the call audio between this machine and the handset.
    void routeAudioHere();

    void refresh();

    // A number from outside the UI (a tel: URL). Fills the field, does not dial.
    void requestNumber(const QString &number);

Q_SIGNALS:
    void availableChanged(bool available);
    void mutedChanged(bool muted);
    void transportStateChanged();
    void selectedAddressChanged();
    void callEnded(const QString &number, bool incoming, qint64 durationSec);
    void commandFailed(const QString &reason);
    // A service code went out. HFP carries no reply channel, so the network's
    // answer can only appear on the handset.
    void serviceCodeSent(const QString &code);
    void incomingCall(const QString &objectPath, const QString &number);
    void numberRequested(const QString &number);

private Q_SLOTS:
    void onServiceAppeared();
    void onServiceVanished();
    void onInterfacesAdded(const QDBusMessage &message);
    void onInterfacesRemoved(const QDBusMessage &message);
    void onPollTimer();

private:
    void refreshDevices();
    void refreshCalls();
    // GetCalls is answered once per device; finishCallRefresh runs after the last one.
    void ingestCalls(const QVariantMap &calls);
    void finishCallRefresh();
    void setAvailable(bool available);
    AgDevice *selectedDevice() const;
    Call *findCall(const QString &path) const;

    QDBusServiceWatcher *m_watcher;
    AgModel *m_devices;
    CallModel *m_calls;
    QTimer *m_pollTimer;

    QList<AgDevice *> m_deviceList;
    QHash<QString, Call *> m_callByPath;
    QHash<QString, bool> m_wasIncomingByPath;
    QHash<QString, QString> m_prevStateByPath;
    QHash<QString, qint64> m_activeSinceByPath;

    QSet<QString> m_livePaths;
    int m_pendingCallReplies = 0;
    bool m_devicesRefreshInFlight = false;

    bool m_available = false;
    bool m_muted = false;
    // Restored on unmute; the AG range is 0-15.
    int m_volumeBeforeMute = 15;
    int m_selectedRow = 0;

    PendingEndTracker m_pendingEnd;
};