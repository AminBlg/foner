// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QUrl>
#include <QWindow>

#include "core/telephony/telephonyclient.h"
#include "core/telephony/agmodel.h"
#include "core/telephony/bluezdevices.h"
#include "core/telephony/callmodel.h"
#include "core/contacts/contactsmodel.h"
#include "core/recents/recentsmodel.h"
#include "core/notifications/eventnotifier.h"
#include "core/notifications/incomingcallnotifier.h"
#include "core/settings/settings.h"
#include "app/trayicon.h"

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedString>
#include <KLocalizedQmlContext>

namespace {

// Pulls the number out of a tel: URL, dropping the RFC 3966 separators and
// parameters. Anything that is not a tel: URL is handed back untouched.
QString numberFromArgument(const QString &argument)
{
    QString candidate = argument;
    const QUrl url(argument);
    if (url.scheme().compare(QLatin1String("tel"), Qt::CaseInsensitive) == 0)
        candidate = url.path(QUrl::FullyDecoded);

    candidate = candidate.section(QLatin1Char(';'), 0, 0);

    QString out;
    out.reserve(candidate.size());
    for (const QChar &ch : candidate) {
        if (ch.isDigit() || ch == QLatin1Char('+') || ch == QLatin1Char('*')
            || ch == QLatin1Char('#')) {
            out.append(ch);
        }
    }
    return out;
}

} // namespace

namespace {

// Pulls the phonebook and then the call history when a phone appears.
//
// The two models own a PbapClient each, so neither one's "a transfer is
// already running" guard can see the other, and obexd would be handed two
// overlapping OBEX sessions to the same phone. The history therefore waits for
// the phonebook rather than starting beside it.
QString connectedPhoneAddress(const TelephonyClient &client)
{
    if (client.devices()->rowCount() == 0)
        return QString();
    const QModelIndex first = client.devices()->index(0, 0);
    return client.devices()->data(first, AgModel::AddressRole).toString();
}

void connectContactsPull(TelephonyClient &client, ContactsModel &contacts, Settings &settings)
{
    QObject::connect(client.devices(), &AgModel::countChanged, &contacts,
                     [&client, &contacts, &settings]() {
                         if (!settings.autoLoadContacts() || contacts.loading())
                             return;
                         if (contacts.rowCount() > 0)
                             return;
                         const QString address = connectedPhoneAddress(client);
                         if (!address.isEmpty())
                             contacts.loadFromPhone(address);
                     });
}

void connectHistoryPull(TelephonyClient &client, ContactsModel &contacts,
                        RecentsModel &recents, Settings &settings)
{
    const auto pull = [&client, &contacts, &recents, &settings]() {
        if (!settings.autoLoadContacts() || contacts.loading())
            return;
        if (recents.loading() || recents.rowCount() > 0)
            return;
        const QString address = connectedPhoneAddress(client);
        if (!address.isEmpty())
            recents.loadFromPhone(address);
    };

    // Two triggers, because either alone leaves a case uncovered. A phone
    // connecting with the phonebook already restored from cache never starts a
    // contacts pull, so loadingChanged would never fire; a phone connecting
    // with no cache does start one, and countChanged arrives while it is still
    // running. Both paths end at the same guard.
    QObject::connect(client.devices(), &AgModel::countChanged, &recents, pull);
    QObject::connect(&contacts, &ContactsModel::loadingChanged, &recents, pull);
}

} // namespace


int main(int argc, char *argv[])
{
    // QApplication, not QGuiApplication: the tray context menu is a QMenu.
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    KLocalizedString::setApplicationDomain("foner");
    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));

    KAboutData about(QStringLiteral("foner"), i18n("Foner"), QStringLiteral("0.1.0"),
                     i18n("Make and receive phone calls through a paired Bluetooth phone"),
                     KAboutLicense::Unknown);
    about.setDesktopFileName(QStringLiteral("org.unscale.foner"));
    // KDBusService names itself from this. It must be set on the KAboutData, which
    // overwrites QCoreApplication's own domain in setApplicationData below.
    about.setOrganizationDomain(QByteArray("unscale.org"));
    KAboutData::setApplicationData(about);
    // The app's own icon, not a generic theme handset. Falls back when foner is
    // run without being installed.
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("org.unscale.foner"),
                                       QIcon::fromTheme(QStringLiteral("call-start"))));

    QCommandLineParser parser;
    parser.addPositionalArgument(QStringLiteral("number"),
                                 i18n("A phone number or tel: URL to dial"));
    about.setupCommandLine(&parser);
    parser.process(app);
    about.processCommandLine(&parser);

    // A second launch hands its arguments to the running instance.
    KDBusService service(KDBusService::Unique);

    Settings settings;
    TelephonyClient client;
    // Names, icons and battery for paired phones. The only system-bus user.
    BluezDevices bluez;
    ContactsModel contactsModel;
    RecentsModel recentsModel;

    // Display names for recents and for the incoming-call notification.
    auto resolver = [&contactsModel](const QString &number) {
        return contactsModel.nameForNumber(number);
    };
    recentsModel.setContactNameResolver(resolver);
    // Sorting contacts by last called reads recents live and a persisted stamp.
    contactsModel.setRecents(&recentsModel);
    QObject::connect(&client, &TelephonyClient::callEnded, &recentsModel,
                     [&recentsModel, &contactsModel](const QString &number, bool wasIncoming,
                                     qint64 durationSec) {
                         // A service code is not a call and does not belong here.
                         if (TelephonyClient::isServiceCode(number))
                             return;
                         // Rang and never picked up is missed, not zero-second incoming.
                         const QString direction = wasIncoming
                             ? (durationSec > 0 ? QStringLiteral("incoming")
                                                : QStringLiteral("missed"))
                             : QStringLiteral("outgoing");
                         const QDateTime now = QDateTime::currentDateTime();
                         recentsModel.append(number, direction, now, durationSec);
                         // Survives a restart, which recents alone do not.
                         contactsModel.noteCall(number, now);
                     });

    IncomingCallNotifier notifier(&client);
    notifier.setSettings(&settings);
    notifier.setNameResolver(resolver);

    EventNotifier events(&client, &contactsModel, &recentsModel, &settings);
    events.setNameResolver(resolver);

    connectContactsPull(client, contactsModel, settings);
    connectHistoryPull(client, contactsModel, recentsModel, settings);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedQmlContext(&engine));
    engine.rootContext()->setContextProperty(QStringLiteral("telephonyClient"), &client);
    engine.rootContext()->setContextProperty(QStringLiteral("devicesModel"), client.devices());
    engine.rootContext()->setContextProperty(QStringLiteral("callsModel"), client.calls());
    engine.rootContext()->setContextProperty(QStringLiteral("contactsModel"), &contactsModel);
    engine.rootContext()->setContextProperty(QStringLiteral("recentsModel"), &recentsModel);
    engine.rootContext()->setContextProperty(QStringLiteral("settings"), &settings);
    engine.rootContext()->setContextProperty(QStringLiteral("bluez"), &bluez);
    // KAboutData is a Q_GADGET, so QML reads its properties directly. This is
    // what Kirigami.AboutPage renders; org.kde.coreaddons does not export the
    // type itself, so it has to come from here.
    engine.rootContext()->setContextProperty(
        QStringLiteral("aboutData"), QVariant::fromValue(KAboutData::applicationData()));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    QWindow *window = nullptr;
    if (!engine.rootObjects().isEmpty())
        window = qobject_cast<QWindow *>(engine.rootObjects().first());

    // Outlives the window: closing to the tray must not end the process.
    TrayIcon tray(&client, window);
    tray.setBluezDevices(&bluez);

    if (window && settings.startMinimised())
        window->hide();

    const auto raiseWindow = [&tray]() { tray.showWindow(); };

    // A later `foner tel:...` arrives here.
    QObject::connect(&service, &KDBusService::activateRequested, &app,
                     [&client, raiseWindow](const QStringList &arguments, const QString &) {
                         raiseWindow();
                         if (arguments.size() > 1)
                             client.requestNumber(numberFromArgument(arguments.at(1)));
                     });

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty())
        client.requestNumber(numberFromArgument(positional.first()));

    return app.exec();
}
