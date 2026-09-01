// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QList>

class Call;

class CallModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // Row of the first ringing call, or -1.
    Q_PROPERTY(int incomingCallIndex READ incomingCallIndex NOTIFY incomingCallIndexChanged)

public:
    enum Role {
        ObjectPathRole = Qt::UserRole + 1,
        StateRole,
        LineIdentificationRole,
        IncomingLineRole,
        NameRole,
        MultipartyRole,
        ActiveSinceRole,
    };
    Q_ENUM(Role)

    explicit CallModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setCalls(const QList<Call *> &calls);
    void clear();

    Q_INVOKABLE QVariantMap get(int row) const;

    int incomingCallIndex() const;

Q_SIGNALS:
    void countChanged();
    void incomingCallIndexChanged();

private:
    void refreshIncomingIndex();

    QList<Call *> m_calls;
    int m_incomingIndex = -1;
};