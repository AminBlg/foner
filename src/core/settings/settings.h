// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

#include <KSharedConfig>

// User preferences, kept in ~/.config/fonerrc by KConfig.
class Settings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY closeToTrayChanged)
    Q_PROPERTY(bool startMinimised READ startMinimised WRITE setStartMinimised
                   NOTIFY startMinimisedChanged)
    Q_PROPERTY(bool notifyIncoming READ notifyIncoming WRITE setNotifyIncoming
                   NOTIFY notifyIncomingChanged)
    Q_PROPERTY(bool notifyMissed READ notifyMissed WRITE setNotifyMissed NOTIFY notifyMissedChanged)
    Q_PROPERTY(bool notifyEnded READ notifyEnded WRITE setNotifyEnded NOTIFY notifyEndedChanged)
    Q_PROPERTY(bool notifyErrors READ notifyErrors WRITE setNotifyErrors NOTIFY notifyErrorsChanged)
    Q_PROPERTY(bool notifyDevice READ notifyDevice WRITE setNotifyDevice NOTIFY notifyDeviceChanged)
    Q_PROPERTY(bool autoLoadContacts READ autoLoadContacts WRITE setAutoLoadContacts
                   NOTIFY autoLoadContactsChanged)
    // Contacts sorted by most recently called rather than by name.
    Q_PROPERTY(bool sortByLastCalled READ sortByLastCalled WRITE setSortByLastCalled
                   NOTIFY sortByLastCalledChanged)

public:
    explicit Settings(QObject *parent = nullptr);

    bool closeToTray() const { return m_closeToTray; }
    bool startMinimised() const { return m_startMinimised; }
    bool notifyIncoming() const { return m_notifyIncoming; }
    bool notifyMissed() const { return m_notifyMissed; }
    bool notifyEnded() const { return m_notifyEnded; }
    bool notifyErrors() const { return m_notifyErrors; }
    bool notifyDevice() const { return m_notifyDevice; }
    bool autoLoadContacts() const { return m_autoLoadContacts; }
    bool sortByLastCalled() const { return m_sortByLastCalled; }

    void setCloseToTray(bool on);
    void setStartMinimised(bool on);
    void setNotifyIncoming(bool on);
    void setNotifyMissed(bool on);
    void setNotifyEnded(bool on);
    void setNotifyErrors(bool on);
    void setNotifyDevice(bool on);
    void setAutoLoadContacts(bool on);
    void setSortByLastCalled(bool on);

Q_SIGNALS:
    void closeToTrayChanged();
    void startMinimisedChanged();
    void notifyIncomingChanged();
    void notifyMissedChanged();
    void notifyEndedChanged();
    void notifyErrorsChanged();
    void notifyDeviceChanged();
    void autoLoadContactsChanged();
    void sortByLastCalledChanged();

private:
    void write(const char *key, bool value);

    KSharedConfig::Ptr m_config;

    bool m_closeToTray = true;
    bool m_startMinimised = false;
    bool m_notifyIncoming = true;
    bool m_notifyMissed = true;
    bool m_notifyEnded = false;
    bool m_notifyErrors = true;
    bool m_notifyDevice = true;
    bool m_autoLoadContacts = false;
    bool m_sortByLastCalled = false;
};
