// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "agmodel.h"
#include "agdevice.h"

AgModel::AgModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AgModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_devices.size();
}

QVariant AgModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_devices.size())
        return {};
    const AgDevice *d = m_devices.at(index.row());
    switch (role) {
    case AddressRole: return d->address();
    case ObjectPathRole: return d->objectPath();
    case TransportStateRole: return d->transportState();
    }
    return {};
}

QHash<int, QByteArray> AgModel::roleNames() const
{
    return {
        {AddressRole, "address"},
        {ObjectPathRole, "objectPath"},
        {TransportStateRole, "transportState"},
    };
}

void AgModel::setDevices(const QList<AgDevice *> &devices)
{
    beginResetModel();
    m_devices = devices;
    endResetModel();
    Q_EMIT countChanged();
}

void AgModel::clear()
{
    beginResetModel();
    m_devices.clear();
    endResetModel();
    Q_EMIT countChanged();
}
