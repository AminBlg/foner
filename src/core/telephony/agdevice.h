// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include <functional>

class AgDevice : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString address READ address NOTIFY addressChanged)
    Q_PROPERTY(QString objectPath READ objectPath CONSTANT)
    Q_PROPERTY(QString transportState READ transportState NOTIFY transportStateChanged)

public:
    explicit AgDevice(const QString &objectPath, const QString &address, QObject *parent = nullptr);
    ~AgDevice() override;

    QString address() const { return m_address; }
    QString objectPath() const { return m_objectPath; }
    QString transportState() const { return m_transportState; }
    // 0-15; -1 until the first read lands.
    int microphoneVolume() const { return m_micVolume; }

public Q_SLOTS:
    void dial(const QString &number);
    void hangupAll();
    void holdAndAnswer();
    void releaseAndAnswer();
    void releaseAndSwap();
    void swapCalls();
    void createMultiparty();
    void sendTones(const QString &tones);
    void setSpeakerVolume(uchar volume);

    // Where the call audio plays. Activate() asks the phone to open a SCO link
    // to this machine; RejectSCO=true refuses one, which leaves the audio on
    // the handset. Both verified by introspection on AudioGatewayTransport1.
    void routeAudioHere();
    void setMicrophoneVolume(uchar volume);

Q_SIGNALS:
    void addressChanged();
    void transportStateChanged(const QString &state);
    void commandFailed(const QString &reason);

private Q_SLOTS:
    void onPropertiesChanged(const QString &interfaceName, const QVariantMap &changed, const QStringList &invalidated);

private:
    void callAg(const QString &method, const QVariantList &args = {});
    void setDbusProperty(const char *interfaceName, const QString &name, const QVariant &value);
    void getDbusProperty(const char *interfaceName, const QString &name,
                         std::function<void(const QVariant &)> onValue);

    QString m_objectPath;
    QString m_address;
    QString m_transportState;
    int m_micVolume = -1;
};
