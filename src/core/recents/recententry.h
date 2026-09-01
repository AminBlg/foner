// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QString>
#include <QMetaType>

struct RecentEntry {
    QString number;
    QString name;
    QString direction; // "missed" | "incoming" | "outgoing"
    QDateTime timestamp;
    qint64 durationSec = 0;
};

Q_DECLARE_METATYPE(RecentEntry)
Q_DECLARE_METATYPE(QList<RecentEntry>)