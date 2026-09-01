// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pendingendtracker.h"

void PendingEndTracker::markDropped(const QString &path, const QString &number,
                                    bool incoming, qint64 durationSec, qint64 nowMs)
{
    if (number.isEmpty())
        return;
    // An already-tracked path keeps its original deadline.
    if (m_entries.contains(path))
        return;

    Entry entry;
    entry.number = number;
    entry.incoming = incoming;
    entry.durationSec = durationSec;
    entry.mtime = nowMs;
    m_entries.insert(path, entry);
}

void PendingEndTracker::cancel(const QString &path)
{
    m_entries.remove(path);
}

QList<PendingEndTracker::Ended> PendingEndTracker::flush(qint64 nowMs)
{
    QList<Ended> out;
    for (auto it = m_entries.begin(); it != m_entries.end();) {
        if (nowMs - it->mtime >= kGraceMs) {
            out.append(Ended{it->number, it->incoming, it->durationSec});
            it = m_entries.erase(it);
        } else {
            ++it;
        }
    }
    return out;
}
