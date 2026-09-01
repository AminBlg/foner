// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QList>

class AgDevice;

class AgModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        AddressRole = Qt::UserRole + 1,
        ObjectPathRole,
        TransportStateRole,
    };
    Q_ENUM(Role)

    explicit AgModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setDevices(const QList<AgDevice *> &devices);
    void clear();


Q_SIGNALS:
    void countChanged();

private:
    QList<AgDevice *> m_devices;
};