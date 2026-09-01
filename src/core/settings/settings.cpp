// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings.h"

#include <KConfigGroup>

namespace {
constexpr auto kGroup = "General";
}

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_config(KSharedConfig::openConfig())
{
    const KConfigGroup group = m_config->group(QLatin1String(kGroup));

    m_closeToTray = group.readEntry("CloseToTray", m_closeToTray);
    m_startMinimised = group.readEntry("StartMinimised", m_startMinimised);
    m_notifyIncoming = group.readEntry("NotifyIncoming", m_notifyIncoming);
    m_notifyMissed = group.readEntry("NotifyMissed", m_notifyMissed);
    m_notifyEnded = group.readEntry("NotifyEnded", m_notifyEnded);
    m_notifyErrors = group.readEntry("NotifyErrors", m_notifyErrors);
    m_notifyDevice = group.readEntry("NotifyDevice", m_notifyDevice);
    m_autoLoadContacts = group.readEntry("AutoLoadContacts", m_autoLoadContacts);
    m_sortByLastCalled = group.readEntry("SortByLastCalled", m_sortByLastCalled);
}

void Settings::write(const char *key, bool value)
{
    KConfigGroup group = m_config->group(QLatin1String(kGroup));
    group.writeEntry(key, value);
    // Written immediately, not on quit.
    group.sync();
}

#define FONER_DEFINE_SETTER(Setter, Member, Key, Signal)                                           \
    void Settings::Setter(bool on)                                                                 \
    {                                                                                              \
        if (Member == on)                                                                          \
            return;                                                                                \
        Member = on;                                                                               \
        write(Key, on);                                                                            \
        Q_EMIT Signal();                                                                           \
    }

FONER_DEFINE_SETTER(setCloseToTray, m_closeToTray, "CloseToTray", closeToTrayChanged)
FONER_DEFINE_SETTER(setStartMinimised, m_startMinimised, "StartMinimised", startMinimisedChanged)
FONER_DEFINE_SETTER(setNotifyIncoming, m_notifyIncoming, "NotifyIncoming", notifyIncomingChanged)
FONER_DEFINE_SETTER(setNotifyMissed, m_notifyMissed, "NotifyMissed", notifyMissedChanged)
FONER_DEFINE_SETTER(setNotifyEnded, m_notifyEnded, "NotifyEnded", notifyEndedChanged)
FONER_DEFINE_SETTER(setNotifyErrors, m_notifyErrors, "NotifyErrors", notifyErrorsChanged)
FONER_DEFINE_SETTER(setNotifyDevice, m_notifyDevice, "NotifyDevice", notifyDeviceChanged)
FONER_DEFINE_SETTER(setAutoLoadContacts, m_autoLoadContacts, "AutoLoadContacts",
                    autoLoadContactsChanged)
FONER_DEFINE_SETTER(setSortByLastCalled, m_sortByLastCalled, "SortByLastCalled",
                    sortByLastCalledChanged)

#undef FONER_DEFINE_SETTER
