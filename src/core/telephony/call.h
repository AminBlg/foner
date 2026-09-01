// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusVariant>
#include <QObject>
#include <QString>

// A single call object on the AG. Read from QML through CallModel's roles.
class Call : public QObject
{
    Q_OBJECT

public:
    explicit Call(const QString &objectPath, QObject *parent = nullptr);
    ~Call() override;

    QString objectPath() const { return m_objectPath; }
    QString state() const { return m_state; }
    QString lineIdentification() const { return m_line; }
    QString incomingLine() const { return m_incomingLine; }
    QString name() const { return m_name; }
    bool multiparty() const { return m_multiparty; }

    // The other party's number. IncomingLine is the called line, which the network
    // usually leaves empty, so LineIdentification carries the caller either way.
    QString remoteNumber() const { return m_line.isEmpty() ? m_incomingLine : m_line; }

    // Milliseconds since the epoch when the call first went active, or 0 while
    // it is still ringing or dialling. The window needs it to run a timer; the
    // call log needs it to record a duration.
    qint64 activeSince() const { return m_activeSince; }
    void setActiveSince(qint64 msecs);

    void updateProperties(const QVariantMap &props);

public Q_SLOTS:
    void answer();
    void hangup();

Q_SIGNALS:
    void stateChanged(const QString &state);
    void activeSinceChanged();
    void commandFailed(const QString &reason);

private Q_SLOTS:
    void onPropertyChanged(const QString &property, const QDBusVariant &value);
    void onPropertiesChanged(const QString &interfaceName, const QVariantMap &changed, const QStringList &invalidated);

private:
    void setState(const QString &s);

    qint64 m_activeSince = 0;
    void callMethod(const QString &method);

    QString m_objectPath;
    QString m_state;
    QString m_line;
    QString m_incomingLine;
    QString m_name;
    bool m_multiparty = false;
};
