// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QHash>
#include <QList>
#include <QString>

// Defers "this call ended" past a grace window. A path that reappears inside the
// window cancels its pending end.
class PendingEndTracker
{
public:
    struct Ended {
        QString number;
        bool incoming = false;
        qint64 durationSec = 0;
    };

    static constexpr qint64 kGraceMs = 3000;

    void markDropped(const QString &path, const QString &number, bool incoming,
                     qint64 durationSec, qint64 nowMs);
    void cancel(const QString &path);
    QList<Ended> flush(qint64 nowMs);

private:
    struct Entry {
        QString number;
        bool incoming = false;
        qint64 durationSec = 0;
        qint64 mtime = 0;
    };
    QHash<QString, Entry> m_entries;
};
