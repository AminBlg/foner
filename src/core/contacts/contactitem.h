// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>
#include <QMetaType>

struct ContactItem {
    QString name;
    QStringList numbers;
    QByteArray photo;

    // The name folded to keypad digits. See ContactsModel::t9Key.
    QString t9;
    // The name with its accents removed. See ContactsModel::foldAccents.
    QString folded;

    QString primaryNumber() const { return numbers.isEmpty() ? QString() : numbers.first(); }
};

Q_DECLARE_METATYPE(ContactItem)
Q_DECLARE_METATYPE(QList<ContactItem>)