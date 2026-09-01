// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "logging.h"

// QtInfoMsg as the default threshold: debug output stays quiet, warnings and
// above always reach the journal.
Q_LOGGING_CATEGORY(FONER_TELEPHONY, "foner.telephony", QtInfoMsg)
Q_LOGGING_CATEGORY(FONER_CONTACTS, "foner.contacts", QtInfoMsg)
Q_LOGGING_CATEGORY(FONER_BLUEZ, "foner.bluez", QtInfoMsg)
Q_LOGGING_CATEGORY(FONER_NOTIFY, "foner.notifications", QtInfoMsg)
Q_LOGGING_CATEGORY(FONER_TRAY, "foner.tray", QtInfoMsg)
