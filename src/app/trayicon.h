// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

class QWindow;
class KStatusNotifierItem;
class TelephonyClient;
class BluezDevices;

// The system tray entry, over the StatusNotifierItem protocol.
class TrayIcon : public QObject
{
    Q_OBJECT

public:
    TrayIcon(TelephonyClient *client, QWindow *window, QObject *parent = nullptr);

    // Optional. Only used to name the phone in the tooltip.
    void setBluezDevices(BluezDevices *devices);

    // Brings the window back and raises it.
    void showWindow();

private:
    void updateForCallState();
    // Name of the selected phone, or an empty string when none is connected.
    QString phoneName() const;

    KStatusNotifierItem *m_item;
    TelephonyClient *m_client;
    QWindow *m_window;
    BluezDevices *m_bluez = nullptr;
};
