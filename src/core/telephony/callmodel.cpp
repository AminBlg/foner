// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "callmodel.h"
#include "call.h"

CallModel::CallModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CallModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_calls.size();
}

QVariant CallModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_calls.size())
        return {};
    const Call *c = m_calls.at(index.row());
    switch (role) {
    case ObjectPathRole: return c->objectPath();
    case StateRole: return c->state();
    case LineIdentificationRole: return c->lineIdentification();
    case IncomingLineRole: return c->incomingLine();
    case NameRole: return c->name();
    case MultipartyRole: return c->multiparty();
    case ActiveSinceRole: return c->activeSince();
    }
    return {};
}

QHash<int, QByteArray> CallModel::roleNames() const
{
    return {
        {ObjectPathRole, "objectPath"},
        {StateRole, "state"},
        {LineIdentificationRole, "line"},
        {IncomingLineRole, "incomingLine"},
        {NameRole, "name"},
        {MultipartyRole, "multiparty"},
        {ActiveSinceRole, "activeSince"},
    };
}

void CallModel::setCalls(const QList<Call *> &calls)
{
    // Reset only when the row set differs; this runs on every poll tick.
    if (calls == m_calls) {
        if (!m_calls.isEmpty())
            Q_EMIT dataChanged(index(0, 0), index(m_calls.size() - 1, 0));
        refreshIncomingIndex();
        return;
    }

    beginResetModel();
    m_calls = calls;
    endResetModel();
    Q_EMIT countChanged();

    for (Call *c : m_calls) {
        connect(c, &Call::stateChanged, this, &CallModel::refreshIncomingIndex,
                Qt::UniqueConnection);
    }
    refreshIncomingIndex();
}

void CallModel::clear()
{
    beginResetModel();
    m_calls.clear();
    endResetModel();
    Q_EMIT countChanged();
    refreshIncomingIndex();
}

int CallModel::incomingCallIndex() const
{
    return m_incomingIndex;
}

void CallModel::refreshIncomingIndex()
{
    int found = -1;
    for (int i = 0; i < m_calls.size(); ++i) {
        const QString state = m_calls.at(i)->state();
        if (state == QLatin1String("incoming") || state == QLatin1String("waiting")) {
            found = i;
            break;
        }
    }

    if (!m_calls.isEmpty())
        Q_EMIT dataChanged(index(0, 0), index(m_calls.size() - 1, 0));

    if (found == m_incomingIndex)
        return;
    m_incomingIndex = found;
    Q_EMIT incomingCallIndexChanged();
}

QVariantMap CallModel::get(int row) const
{
    QVariantMap map;
    const QModelIndex idx = index(row, 0);
    if (!idx.isValid())
        return map;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.constBegin(); it != roles.constEnd(); ++it)
        map.insert(QString::fromUtf8(it.value()), data(idx, it.key()));
    return map;
}