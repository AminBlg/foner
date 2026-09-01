// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QTimer>

#include "core/telephony/telephonyclient.h"
#include "core/telephony/agmodel.h"
#include "core/telephony/callmodel.h"
#include "core/contacts/contactsmodel.h"

namespace {

// Straight to stdout. qInfo() is filtered off by default, so this tool used to
// print nothing at all.
QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

void runContactParseTest(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        out() << "contacts: cannot open " << path << "\n";
        return;
    }
    const QList<ContactItem> contacts = ContactsModel::parseVcf(f.readAll());
    f.close();
    out() << "contacts: parsed " << contacts.size() << " from " << path << "\n";
    for (const ContactItem &c : contacts)
        out() << "  " << c.name << " -> " << c.numbers.join(QStringLiteral(", ")) << "\n";
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // ponytail: argv[1] overrides the built-in sample. Add real arg parsing
    // when there is a second option.
    const QStringList args = QCoreApplication::arguments();
    runContactParseTest(args.size() > 1 ? args.at(1) : QLatin1String(QC_PHONEBOOK_VCF));

    TelephonyClient client;

    QTimer::singleShot(500, &app, [&client]() {
        out() << "available: " << (client.available() ? "yes" : "no") << "\n";
        out() << "devices: " << client.devices()->rowCount() << "\n";
        for (int i = 0; i < client.devices()->rowCount(); ++i) {
            const QModelIndex idx = client.devices()->index(i, 0);
            out() << "  device " << i
                  << " address=" << client.devices()->data(idx, AgModel::AddressRole).toString()
                  << " path=" << client.devices()->data(idx, AgModel::ObjectPathRole).toString()
                  << " transport=" << client.devices()->data(idx, AgModel::TransportStateRole).toString()
                  << "\n";
        }
        out() << "calls: " << client.calls()->rowCount() << "\n";
        for (int i = 0; i < client.calls()->rowCount(); ++i) {
            const QModelIndex idx = client.calls()->index(i, 0);
            out() << "  call " << i
                  << " state=" << client.calls()->data(idx, CallModel::StateRole).toString()
                  << " line=" << client.calls()->data(idx, CallModel::LineIdentificationRole).toString()
                  << " incomingLine=" << client.calls()->data(idx, CallModel::IncomingLineRole).toString()
                  << "\n";
        }
        out().flush();
        QCoreApplication::exit(0);
    });

    return app.exec();
}
