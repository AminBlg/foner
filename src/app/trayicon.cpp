// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#include "trayicon.h"

#include "core/logging.h"
#include "core/telephony/agmodel.h"
#include "core/telephony/bluezdevices.h"
#include "core/telephony/callmodel.h"
#include "core/telephony/telephonyclient.h"

#include <QAction>
#include <QMenu>
#include <QWindow>

#include <KLocalizedString>
#include <KStatusNotifierItem>

TrayIcon::TrayIcon(TelephonyClient *client, QWindow *window, QObject *parent)
    : QObject(parent)
    , m_item(new KStatusNotifierItem(QStringLiteral("org.unscale.foner"), this))
    , m_client(client)
    , m_window(window)
{
    m_item->setCategory(KStatusNotifierItem::Communications);
    m_item->setTitle(i18n("Foner"));
    m_item->setStatus(KStatusNotifierItem::Passive);
    m_item->setIconByName(QStringLiteral("org.unscale.foner"));
    m_item->setAssociatedWindow(m_window);

    auto *menu = new QMenu();

    auto *hangUp = menu->addAction(QIcon::fromTheme(QStringLiteral("call-stop")),
                                   i18n("Hang up"));
    hangUp->setVisible(false);
    connect(hangUp, &QAction::triggered, m_client, &TelephonyClient::hangupAll);

    menu->addSeparator();
    m_item->setContextMenu(menu);

    // The glyph carries connection state, so the device list matters as much as
    // the call list.
    connect(m_client->devices(), &AgModel::countChanged, this, &TrayIcon::updateForCallState);
    connect(m_client->calls(), &CallModel::countChanged, this, &TrayIcon::updateForCallState);
    connect(m_client->calls(), &CallModel::incomingCallIndexChanged, this,
            &TrayIcon::updateForCallState);

    connect(m_client->calls(), &CallModel::countChanged, hangUp, [this, hangUp]() {
        hangUp->setVisible(m_client->calls()->rowCount() > 0);
    });

    updateForCallState();
}

QString TrayIcon::phoneName() const
{
    if (m_client->devices()->rowCount() == 0)
        return QString();
    const QModelIndex first = m_client->devices()->index(0, 0);
    const QString address = m_client->devices()->data(first, AgModel::AddressRole).toString();
    return m_bluez ? m_bluez->nameFor(address) : address;
}

void TrayIcon::updateForCallState()
{
    const int calls = m_client->calls()->rowCount();
    const bool ringing = m_client->calls()->incomingCallIndex() >= 0;
    const QString phone = phoneName();

    // Never Passive while the application runs. Plasma hides Passive items behind
    // the tray's expander by default, and a dialler you cannot see is a dialler
    // you forget is listening. The glyph carries the state instead.
    QString icon;
    QString text;
    auto status = KStatusNotifierItem::Active;

    if (ringing) {
        status = KStatusNotifierItem::NeedsAttention;
        icon = QStringLiteral("call-incoming");
        text = i18n("Incoming call");
    } else if (calls > 0) {
        icon = QStringLiteral("call-start");
        text = i18np("%1 call in progress", "%1 calls in progress", calls);
    } else if (phone.isEmpty()) {
        icon = QStringLiteral("network-disconnect");
        text = i18n("No phone connected");
    } else {
        icon = QStringLiteral("org.unscale.foner");
        text = i18n("No calls");
    }

    const QString title = phone.isEmpty() ? i18n("Foner")
                                          : i18nc("application and phone", "Foner · %1", phone);

    qCDebug(FONER_TRAY) << "status" << status << "icon" << icon << "phone" << phone;

    m_item->setStatus(status);
    m_item->setIconByName(icon);
    // Same icon as the item itself; these used to disagree.
    m_item->setToolTip(icon, title, text);
}

void TrayIcon::setBluezDevices(BluezDevices *devices)
{
    m_bluez = devices;
    if (!m_bluez)
        return;
    connect(m_bluez, &BluezDevices::deviceChanged, this, &TrayIcon::updateForCallState);
    updateForCallState();
}

void TrayIcon::showWindow()
{
    if (!m_window)
        return;
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
}
