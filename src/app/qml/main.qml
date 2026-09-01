// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels

Kirigami.ApplicationWindow {
    id: root

    width: 420
    height: 640
    // Explicit: an ApplicationWindow defaults to hidden, and main.cpp only ever
    // calls hide(). On a plain X server without a launcher to activate us, the
    // window never mapped at all.
    visible: !settings.startMinimised
    minimumWidth: Kirigami.Units.gridUnit * 18
    minimumHeight: Kirigami.Units.gridUnit * 26

    // BlueZ answers GetManagedObjects asynchronously, and nameFor() is a plain
    // invokable with no NOTIFY, so a binding on it alone evaluates once against
    // an empty cache and keeps showing the address forever. Bumping this counter
    // from deviceChanged gives the binding the dependency it is missing.
    readonly property var applicationAboutData: aboutData
    property int bluezRevision: 0
    Connections {
        target: bluez
        function onDeviceChanged(address) { root.bluezRevision++ }
    }

    // The phone's name lives here rather than in a row of its own. Plasma shows
    // the title in the taskbar and the window list, so it is already on screen.
    // Only the phone name: the shell appends applicationDisplayName itself, and
    // spelling "Foner" here as well printed it twice.
    title: {
        root.bluezRevision
        const address = telephonyClient.selectedAddress
        if (address.length === 0)
            return i18n("Foner")
        return bluez.nameFor(address)
    }

    onClosing: (close) => {
        if (settings.closeToTray) {
            close.accepted = false
            root.hide()
        }
    }

    // Declared once and referenced from both the menubar and the drawer, so the
    // two surfaces cannot drift apart as entries are added.
    readonly property Kirigami.Action importAction: Kirigami.Action {
        text: i18n("Import vCard…")
        icon.name: "document-open"
        shortcut: StandardKey.Open
        onTriggered: vcfDialog.open()
    }
    readonly property Kirigami.Action exportAction: Kirigami.Action {
        text: i18n("Export vCard…")
        icon.name: "document-save-as"
        shortcut: StandardKey.SaveAs
        enabled: contactsModel.count > 0
        onTriggered: exportDialog.open()
    }
    readonly property Kirigami.Action settingsAction: Kirigami.Action {
        text: i18n("Settings")
        icon.name: "configure"
        onTriggered: settingsSheet.open()
    }
    readonly property Kirigami.Action hideAction: Kirigami.Action {
        text: i18n("Hide to tray")
        icon.name: "window-restore"
        onTriggered: root.hide()
    }
    readonly property Kirigami.Action appQuitAction: Kirigami.Action {
        text: i18n("Quit")
        icon.name: "application-exit"
        shortcut: StandardKey.Quit
        onTriggered: Qt.quit()
    }
    readonly property Kirigami.Action bluetoothAction: Kirigami.Action {
        text: i18n("Bluetooth settings…")
        icon.name: "preferences-system-bluetooth"
        onTriggered: telephonyClient.openBluetoothSettings()
    }
    // An object literal cannot appear inside an actions array, so the drawer's
    // separator has to be a declared action like the rest.
    readonly property Kirigami.Action separatorAction: Kirigami.Action {
        separator: true
    }
    readonly property Kirigami.Action aboutAction: Kirigami.Action {
        text: i18n("About Foner")
        icon.name: "help-about"
        onTriggered: root.pageStack.layers.push(aboutPage)
    }

    // Exported to the desktop's global menu where one exists, and drawn in the
    // window where it does not.
    menuBar: Controls.MenuBar {
        Controls.Menu {
            title: i18nc("@title:menu", "&File")
            Controls.MenuItem { action: root.importAction }
            Controls.MenuItem { action: root.exportAction }
            Controls.MenuSeparator {}
            Controls.MenuItem { action: root.settingsAction }
            Controls.MenuItem { action: root.hideAction }
            Controls.MenuSeparator {}
            Controls.MenuItem { action: root.appQuitAction }
        }

        Controls.Menu {
            id: deviceMenu
            title: i18nc("@title:menu", "&Device")

            // One entry per connected phone, checked to show the selected one.
            Instantiator {
                model: devicesModel
                delegate: Controls.MenuItem {
                    required property int index
                    required property string address
                    text: bluez.nameFor(address)
                    // Not checkable: clicking a checkable button writes checked
                    // imperatively, which destroys the binding below it, and the
                    // menu then keeps a stale mark on every phone ever picked.
                    icon.name: address === telephonyClient.selectedAddress
                        ? "dialog-ok" : ""
                    // Not while a call is up: every AG command goes to the
                    // selected device, so switching mid-call would aim the
                    // hang-up at the wrong phone.
                    enabled: !root.inCall
                    onTriggered: telephonyClient.setSelectedDevice(index)
                    // nameFor() has no NOTIFY, so without this the label would
                    // keep whatever the cache held before BlueZ replied.
                    property int revision: root.bluezRevision
                    onRevisionChanged: text = bluez.nameFor(address)
                }
                onObjectAdded: (index, object) => deviceMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => deviceMenu.removeItem(object)
            }

            Controls.MenuSeparator {}
            Controls.MenuItem { action: root.bluetoothAction }
        }

        Controls.Menu {
            title: i18nc("@title:menu", "&Help")
            Controls.MenuItem { action: root.aboutAction }
        }
    }

    Component {
        id: aboutPage
        // The context property of the same name; qualifying it avoids the
        // property binding to itself.
        Kirigami.AboutPage { aboutData: root.applicationAboutData }
    }

    globalDrawer: Kirigami.GlobalDrawer {
        isMenu: true
        actions: [
            root.settingsAction,
            root.importAction,
            root.exportAction,
            root.bluetoothAction,
            root.hideAction,
            root.separatorAction,
            root.aboutAction,
            root.appQuitAction
        ]
    }

    // The field doubles as the contact search, so letters are not dialable.
    function isDialable(text) {
        return /^[0-9*#+][0-9*#+,;]*$/.test(text.trim())
    }

    function stateLabel(state) {
        switch (state) {
        case "incoming": return i18nc("call state", "Incoming")
        case "waiting": return i18nc("call state", "Waiting")
        case "dialing": return i18nc("call state", "Dialling")
        case "alerting": return i18nc("call state", "Ringing")
        case "active": return i18nc("call state", "Active")
        case "held": return i18nc("call state", "On hold")
        case "disconnected": return i18nc("call state", "Ended")
        default: return state
        }
    }

    function formatDuration(seconds) {
        const m = Math.floor(seconds / 60)
        const s = Math.floor(seconds % 60)
        return m > 0 ? i18nc("call length", "%1m %2s", m, s) : i18nc("call length", "%1s", s)
    }

    // One clock drives every call timer: a Timer per delegate would drift apart
    // and wake the process once per call rather than once per second.
    property double callTick: Date.now()
    Timer {
        interval: 1000
        running: root.inCall
        repeat: true
        onTriggered: root.callTick = Date.now()
    }

    readonly property int incomingIndex: callsModel.incomingCallIndex
    readonly property bool hasOtherCall: callsModel.count > 1
    readonly property bool inCall: callsModel.count > 0

    // inCall only counts calls. These ask what state they are in, which is a
    // different question: a call that is ringing, dialling or held is counted
    // by inCall but is not a leg the gateway will act on. Tones need a call
    // that is actually up, and merge and swap need an established second one.
    function anyCallIs(state) {
        for (let i = 0; i < callsModel.count; ++i) {
            if (callsModel.get(i).state === state)
                return true
        }
        return false
    }
    readonly property bool hasHeldCall: anyCallIs("held")
    readonly property bool hasActiveCall: anyCallIs("active")

    // Shared by the suggestion strip and the contacts tab, sorted best match first.
    KItemModels.KSortFilterProxyModel {
        id: matchedContacts
        sourceModel: contactsModel
        // While a search is running, best match wins whatever the sort choice is:
        // a hit ranked third is not useful sitting at the bottom alphabetically.
        // The choice applies to the unfiltered list.
        readonly property bool searching: numberField.text.length > 0
        sortRoleName: searching ? "rank"
                                : (settings.sortByLastCalled ? "lastCalled" : "name")
        // Most recent first; everything else reads naturally ascending.
        sortOrder: (!searching && settings.sortByLastCalled) ? Qt.DescendingOrder
                                                             : Qt.AscendingOrder
        // Re-created on every query change, which is what invalidates the filter.
        filterRowCallback: (row, parent) => contactsModel.matches(row, numberField.text)
    }

    pageStack.initialPage: Kirigami.Page {
        // No header: the titlebar already says Foner and the lists need the height.
        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None
        padding: 0

        footer: Kirigami.NavigationTabBar {
            id: tabs
            activeFocusOnTab: true
            Keys.onLeftPressed: paneStack.currentIndex = Math.max(0, paneStack.currentIndex - 1)
            Keys.onRightPressed: paneStack.currentIndex = Math.min(paneStack.count - 1,
                                                                   paneStack.currentIndex + 1)
            actions: [
                Kirigami.Action {
                    text: i18n("Keypad")
                    icon.name: "input-dialpad"
                    checked: paneStack.currentIndex === 0
                    onTriggered: paneStack.currentIndex = 0
                },
                Kirigami.Action {
                    text: i18n("Contacts")
                    icon.name: "im-user"
                    checked: paneStack.currentIndex === 1
                    onTriggered: paneStack.currentIndex = 1
                },
                Kirigami.Action {
                    text: i18n("Recents")
                    icon.name: "view-history"
                    checked: paneStack.currentIndex === 2
                    onTriggered: paneStack.currentIndex = 2
                }
            ]
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.smallSpacing

            // The phone gets a row of its own, centred. As a corner overlay it
            // sat on top of whatever the tab underneath put there - the
            // reload button on the recents list, among others.
            Controls.AbstractButton {
                id: phoneChip
                // A bar rather than a pill: it spans the window and sits above
                // everything else in the column, so it cannot cover a control
                // the way the floating chip covered the recents reload button.
                Layout.fillWidth: true
                Layout.topMargin: -Kirigami.Units.largeSpacing
                Layout.leftMargin: -Kirigami.Units.largeSpacing
                Layout.rightMargin: -Kirigami.Units.largeSpacing
                visible: devicesModel.count > 0
                // Only reacts to the pointer when there is a choice to make.
                hoverEnabled: devicesModel.count > 1

                // nameFor(), iconFor() and batteryFor() carry no NOTIFY, so
                // every binding on them has to read bluezRevision or it will
                // keep whatever the cache held before BlueZ answered.
                readonly property string address: telephonyClient.selectedAddress
                readonly property string phoneName: {
                    root.bluezRevision
                    return address.length > 0 ? bluez.nameFor(address) : ""
                }
                readonly property int battery: {
                    root.bluezRevision
                    return address.length > 0 ? bluez.batteryFor(address) : -1
                }

                implicitHeight: chipRow.implicitHeight + Kirigami.Units.smallSpacing * 2
                Accessible.name: devicesModel.count > 1
                    ? i18n("Phone: %1. Choose another phone", phoneName)
                    : i18n("Phone: %1", phoneName)
                onClicked: {
                    if (devicesModel.count > 1)
            phonePopup.open()
                }

                background: Rectangle {
                    // Square, and tinted rather than outlined: a full-width bar
                    // reads as a header, and a border on all four sides would
                    // box in the whole window.
                    color: phoneChip.pressed || phoneChip.hovered
                        ? Kirigami.Theme.alternateBackgroundColor
                        : Kirigami.Theme.backgroundColor

                    // Kirigami.Theme has no separatorColor. Using it resolved to
                    // undefined, which Qt reported as "Unable to assign [undefined] to
                    // QColor" on every frame. This is what Kirigami.Separator draws.
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Qt.alpha(Kirigami.Theme.textColor, 0.15)
                    }
                }

                contentItem: RowLayout {
                    id: chipRow
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: "smartphone"
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: Kirigami.Units.iconSizes.small
                    }
                    // Takes the slack, so the battery sits at the far edge
                    // rather than crowding the name.
                    Controls.Label {
                        Layout.fillWidth: true
                        text: phoneChip.phoneName
                        elide: Text.ElideRight
                    }
                    // BlueZ adds and drops Battery1 as the phone reports, so
                    // -1 is a normal state and not a failure.
                    Controls.Label {
            visible: phoneChip.battery >= 0
            text: i18nc("battery percentage", "%1%", phoneChip.battery)
            color: Kirigami.Theme.disabledTextColor
            font: Kirigami.Theme.smallFont
                    }
                    Kirigami.Icon {
            visible: devicesModel.count > 1
            source: "arrow-down"
            implicitWidth: Kirigami.Units.iconSizes.small
            implicitHeight: Kirigami.Units.iconSizes.small
                    }
                }

                Controls.Popup {
                    id: phonePopup
                    y: phoneChip.height
                    modal: true

                    contentItem: ColumnLayout {
            spacing: 0
            Repeater {
                model: devicesModel
                delegate: Controls.ItemDelegate {
                    required property int index
                    required property string address
                    Layout.fillWidth: true
                    text: {
            root.bluezRevision
            return bluez.nameFor(address)
                    }
                    icon.name: address === telephonyClient.selectedAddress
            ? "dialog-ok" : ""
                    // Same reason as the Device menu: commands follow the selection,
                    // so it must not move while a call is up.
                    enabled: !root.inCall
                    onClicked: {
            telephonyClient.setSelectedDevice(index)
            phonePopup.close()
                    }
                }
            }
                    }
                }
            }

            // ---- Always on screen, whichever tab is open ---------------------

            // A working phone is not news, so it gets no row at all: the window
            // title names it and the live keypad proves it. Only the states that
            // stop a call from happening are worth vertical space.
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                type: Kirigami.MessageType.Warning
                visible: !telephonyClient.available || devicesModel.count === 0
                text: telephonyClient.available
                    ? i18n("No phone is connected.")
                    : i18n("PipeWire telephony is not available.")

                actions: [
                    Kirigami.Action {
                        text: i18n("Bluetooth")
                        icon.name: "preferences-system-bluetooth"
                        visible: telephonyClient.available
                        onTriggered: telephonyClient.openBluetoothSettings()
                    }
                ]
            }

            // One place for every failure.
            Kirigami.InlineMessage {
                id: errorBanner
                Layout.fillWidth: true
                type: Kirigami.MessageType.Error
                showCloseButton: true
                visible: false

                function report(message) {
                    if (!message)
                        return
                    text = message
                    visible = true
                }
            }

            // A service code has no reply channel over HFP, so this is the only
            // acknowledgement the desktop can give.
            Kirigami.InlineMessage {
                id: serviceBanner
                Layout.fillWidth: true
                type: Kirigami.MessageType.Information
                showCloseButton: true
                visible: false
            }

            Connections {
                target: telephonyClient
                function onCommandFailed(reason) {
                    errorBanner.report(i18n("The phone refused the command: %1", reason))
                }
                function onServiceCodeSent(code) {
                    serviceBanner.text = i18n("Sent %1. The network replies on the phone itself.", code)
                    serviceBanner.visible = true
                }
            }
            // Every contacts failure, fatal or not: a phonebook that arrived
            // but could not be cached, a file that could not be written.
            // setLastError emits lastErrorChanged and error together, so
            // handling one is enough - handling both reported each failure
            // twice, and the export call site made it three times.
            Connections {
                target: contactsModel
                function onError(message) {
                    errorBanner.report(message)
                }
            }
            Connections {
                target: recentsModel
                function onLastErrorChanged() {
                    if (recentsModel.lastError.length > 0)
                        errorBanner.report(i18n("Could not import call history: %1",
                                                recentsModel.lastError))
                }
            }

            // ---- The call in progress, reachable from every tab ---------------

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: contentHeight
                interactive: false
                visible: root.inCall
                model: callsModel

                delegate: Rectangle {
                    id: callDelegate
                    // Whole model, not one property per role: "state" would shadow
                    // QQuickItem.state.
                    required property var model

                    width: ListView.view.width
                    height: Kirigami.Units.gridUnit * 2.8
                    color: Kirigami.Theme.alternateBackgroundColor
                    radius: Kirigami.Units.cornerRadius

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.smallSpacing

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            // Selectable: nameForNumber returns the number
                            // itself when the caller is unknown, which is when
                            // someone most wants to copy it down.
                            Controls.TextField {
                                Layout.fillWidth: true
                                text: contactsModel.nameForNumber(
                                    callDelegate.model.line.length > 0 ? callDelegate.model.line
                                                                 : callDelegate.model.incomingLine)
                                readOnly: true
                                background: null
                                padding: 0
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                Controls.Label {
                                    text: callDelegate.model.multiparty
                                        ? i18nc("call state", "%1 (conference)",
                                                root.stateLabel(callDelegate.model.state))
                                        : root.stateLabel(callDelegate.model.state)
                                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                                    color: Kirigami.Theme.disabledTextColor
                                }
                                // Talk time, counted from the instant the call
                                // went active. A call with no answer yet has no
                                // timer rather than a zero, which would read as
                                // connected.
                                Controls.Label {
                                    visible: callDelegate.model.activeSince > 0
                                    text: root.formatDuration(
                                        (root.callTick - callDelegate.model.activeSince) / 1000)
                                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                                    font.features: { "tnum": 1 }
                                    color: Kirigami.Theme.disabledTextColor
                                }
                            }
                        }

                        Controls.Button {
                            icon.name: "call-start"
                            Accessible.name: i18n("Answer")
                            // With a second call up, the sheet asks instead.
                            visible: callDelegate.model.state === "incoming" && callsModel.count === 1
                            onClicked: telephonyClient.answerCall(callDelegate.model.objectPath)
                        }
                        Controls.Button {
                            icon.name: "exchange-positions"
                            Accessible.name: i18n("Swap the active and held calls")
                            // Needs an established pair. hasOtherCall alone is
                            // true when the second call is still ringing, and
                            // there is nothing to swap with an unanswered leg.
                            visible: root.hasOtherCall && root.hasActiveCall
                                     && callDelegate.model.state === "held"
                            onClicked: telephonyClient.swapCalls()
                        }
                        Controls.Button {
                            icon.name: "system-users"
                            Accessible.name: i18n("Merge both calls into a conference")
                            // Both legs have to be established: merging an
                            // active call with one still ringing is refused.
                            visible: root.hasOtherCall && !callDelegate.model.multiparty
                                     && callDelegate.model.state === "active"
                                     && root.hasHeldCall
                            onClicked: telephonyClient.createMultiparty()
                        }
                        Controls.Button {
                            icon.name: "call-stop"
                            Accessible.name: i18n("Hang up")
                            onClicked: telephonyClient.hangupCall(callDelegate.model.objectPath)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: root.inCall

                // No mute method on the AG: muting writes microphone volume 0.
                Controls.Button {
                    icon.name: telephonyClient.muted ? "microphone-sensitivity-muted"
                                                     : "audio-input-microphone"
                    Accessible.name: telephonyClient.muted ? i18n("Unmute") : i18n("Mute")
                    checkable: true
                    checked: telephonyClient.muted
                    onToggled: telephonyClient.setMuted(checked)
                }
                Kirigami.Icon {
                    source: "audio-volume-high"
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small
                }
                Controls.Slider {
                    Layout.fillWidth: true
                    Accessible.name: i18n("Speaker volume")
                    from: 0
                    to: 15
                    stepSize: 1
                    value: 15
                    onMoved: telephonyClient.setSpeakerVolume(value)
                }
            }

            // Ending the call is the one action always worth reaching for, so it
            // gets a bar of its own rather than an icon among the volume
            // controls. Full width, and the only negative-coloured thing here.
            Controls.Button {
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.4
                visible: root.inCall
                text: root.hasOtherCall ? i18n("End all") : i18n("End")
                icon.name: "call-stop"
                // Destructive, so the style's own negative colouring rather
                // than a palette override.
                Kirigami.Theme.colorSet: Kirigami.Theme.Complementary
                Kirigami.Theme.inherit: false
                Accessible.name: root.hasOtherCall ? i18n("End every call")
                                                   : i18n("End the call")
                onClicked: telephonyClient.hangupAll()
            }

            // Where the call audio is. "active" means the phone has opened a
            // SCO link to this machine, so the user is talking into the laptop
            // rather than into a handset lying face-down on a desk.
            RowLayout {
                id: audioRoute
                Layout.fillWidth: true
                visible: root.inCall
                spacing: Kirigami.Units.smallSpacing

                readonly property bool audioHere:
                    telephonyClient.transportState === "active"

                Kirigami.Icon {
                    source: audioRoute.audioHere ? "audio-headphones" : "smartphone"
                    color: audioRoute.audioHere ? Kirigami.Theme.positiveTextColor
                                                : Kirigami.Theme.neutralTextColor
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: Kirigami.Units.iconSizes.small
                }
                Controls.Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: audioRoute.audioHere ? Kirigami.Theme.positiveTextColor
                                                : Kirigami.Theme.neutralTextColor
                    text: audioRoute.audioHere
                        ? i18n("Audio on this computer")
                        : i18n("Audio on the phone (%1)",
                               telephonyClient.transportState.length > 0
                                   ? telephonyClient.transportState : i18n("unknown"))
                }
                // Only offered when the audio is elsewhere. The gateway can
                // open a SCO link on demand but cannot close one:
                // AudioGatewayTransport1 exposes Activate(), Codec, State and a
                // writable RejectSCO, and no Deactivate. Confirmed by
                // introspecting a live gateway. A "move to phone" button could
                // therefore only set a flag for the next call, which is not
                // what its label would promise.
                Controls.Button {
                    visible: !audioRoute.audioHere
                    text: i18n("Move here")
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    Accessible.name: i18n("Move the call audio to this computer")
                    onClicked: telephonyClient.routeAudioHere()
                }
            }

            // ---- The tabs ------------------------------------------------------

            StackLayout {
                id: paneStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: 0

                // Keypad ---------------------------------------------------------
                // Anchored to the bottom: the slack sits above the block, so an
                // empty field leaves no void and needs no placeholder.
                ColumnLayout {
                    spacing: 0

                    Item { Layout.fillHeight: true }

                    // Matches grow down towards the readout and stop just above
                    // it, so the number and the name it resolves to stay together.
                    ColumnLayout {
                        Layout.fillWidth: true
                        // Three rows at roughly 3 grid units each. The contacts
                        // tab lists the rest.
                        Layout.maximumHeight: Kirigami.Units.gridUnit * 9
                        spacing: 0
                        visible: numberField.text.length > 0 && !root.inCall

                        Repeater {
                            model: matchedContacts

                            Controls.ItemDelegate {
                                id: suggestion
                                required property int index
                                required property string name
                                required property string highlightedName
                                required property string phone

                                Layout.fillWidth: true
                                // Only the first few: the contacts tab lists them all.
                                visible: suggestion.index < 3
                                // Fills the field rather than dialling: a row
                                // that places a call the instant it is touched
                                // cannot be read from or copied out of, and a
                                // misplaced tap costs a real phone call.
                                Accessible.name: i18n("Use the number for %1", suggestion.name)
                                onClicked: numberField.text = suggestion.phone

                                contentItem: RowLayout {
                                    spacing: Kirigami.Units.largeSpacing

                                    Rectangle {
                                        implicitWidth: Kirigami.Units.gridUnit * 1.9
                                        implicitHeight: Kirigami.Units.gridUnit * 1.9
                                        radius: width / 2
                                        color: Kirigami.Theme.highlightColor
                                        Controls.Label {
                                            anchors.centerIn: parent
                                            text: suggestion.name.charAt(0)
                                            color: Kirigami.Theme.highlightedTextColor
                                        }
                                    }
                                    Controls.Label {
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        textFormat: Text.StyledText
                                        // The matched run drawn bold, so it is
                                        // clear why this contact came up at all.
                                        text: suggestion.highlightedName
                                    }
                                    Controls.Label {
                                        text: suggestion.phone
                                        color: Kirigami.Theme.disabledTextColor
                                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                                    }
                                }
                            }
                        }
                    }

                    // A reserved block rather than slack: the readout and the
                    // name it resolves to always occupy the same height, so the
                    // keypad does not shift when a name appears under a number.
                    Controls.TextField {
                        id: numberField
                        Layout.minimumHeight: Kirigami.Units.gridUnit * 3
                        Layout.fillWidth: true
                        Layout.topMargin: Kirigami.Units.smallSpacing
                        Layout.bottomMargin: numberName.visible
                            ? 0 : Kirigami.Units.largeSpacing

                        placeholderText: root.hasActiveCall ? i18n("Keys send tones to the call")
                                                     : i18n("Search or enter number")
                        Accessible.name: i18n("Search contacts or enter a phone number")
                        inputMethodHints: Qt.ImhDialableCharactersOnly
                        KeyNavigation.tab: callButton
                        horizontalAlignment: Text.AlignHCenter
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize
                                        * (text.length > 0 ? 2.6 : 1.4)
                        font.weight: Font.Light
                        background: null
                        onTextChanged: contactsModel.query = text
                        onAccepted: {
                            if (callButton.enabled)
                                callButton.clicked()
                        }
                    }

                    Controls.Label {
                        id: numberName
                        Layout.fillWidth: true
                        Layout.bottomMargin: Kirigami.Units.gridUnit
                        horizontalAlignment: Text.AlignHCenter
                        color: Kirigami.Theme.positiveTextColor
                        elide: Text.ElideRight
                        // Only when the number resolves to somebody.
                        readonly property string resolved:
                            contactsModel.nameForNumber(numberField.text)
                        visible: numberField.text.length > 0
                                 && resolved !== numberField.text
                        text: resolved
                    }

                    Keypad {
                        Layout.fillWidth: true
                        // Same inset as the Call bar below, so the key grid and
                        // the bar share one edge instead of stepping apart.
                        Layout.leftMargin: Kirigami.Units.largeSpacing
                        Layout.rightMargin: Kirigami.Units.largeSpacing
                        // During a call the keypad sends tones, otherwise it types.
                        onKeyPressed: (key) => {
                            if (root.hasActiveCall)
                                telephonyClient.sendTones(key)
                            else
                                numberField.insert(numberField.length, key)
                        }
                    }

                    // A wide Call bar rather than a round icon: it is the one
                    // action this screen exists for, it can carry a label, and
                    // it no longer competes with the tab bar for the corner.
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Kirigami.Units.largeSpacing
                        Layout.leftMargin: Kirigami.Units.largeSpacing
                        Layout.rightMargin: Kirigami.Units.largeSpacing
                        spacing: Kirigami.Units.largeSpacing

                        Controls.Button {
                            id: callButton
                            readonly property bool serviceCode:
                                telephonyClient.isServiceCode(numberField.text)

                            Layout.fillWidth: true
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 2.4
                            // Not during a call: the gateway refuses a second
                            // Dial, and the refusal now reaches the user as an
                            // error rather than passing silently. A control
                            // that can only fail should not be pressable.
                            enabled: devicesModel.count > 0
                                     && !root.inCall
                                     && root.isDialable(numberField.text)
                            text: serviceCode ? i18n("Send") : i18n("Call")
                            icon.name: serviceCode ? "document-send" : "call-start"
                            // Breeze draws the default button with its own
                            // accent treatment, so this is prominent without
                            // overriding the palette. Forcing button colours
                            // fought the style's disabled rendering, which left
                            // the bar flat and grey with no depth at all.
                            Accessible.defaultButton: true
                            Accessible.name: serviceCode
                                ? i18n("Send the service code you typed")
                                : i18n("Call the number you typed")
                            KeyNavigation.tab: tabs
                            onClicked: {
                                telephonyClient.dial(numberField.text)
                                numberField.clear()
                            }
                        }

                        Controls.Button {
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 2.4
                            Layout.preferredHeight: Kirigami.Units.gridUnit * 2.4
                            // Absent, not merely transparent: an invisible slot
                            // still takes its width, which left the Call bar
                            // stopping short of the key grid's right edge with
                            // nothing to explain the gap.
                            visible: numberField.text.length > 0
                            icon.name: "edit-clear"
                            display: Controls.Button.IconOnly
                            Accessible.name: i18n("Delete the last digit")
                            onClicked: numberField.remove(numberField.length - 1,
                                                          numberField.length)
                            onPressAndHold: numberField.clear()
                        }
                    }

                    Item { Layout.preferredHeight: Kirigami.Units.largeSpacing }
                }

                // Contacts -------------------------------------------------------
                ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    // The same query as the keypad, not a second one: typing a
                    // name on the keypad and switching here used to leave the
                    // list filtered by text with nowhere to see or clear it,
                    // which also greyed out the sort button.
                    Controls.TextField {
                        Layout.fillWidth: true
                        placeholderText: i18n("Search contacts")
                        text: numberField.text
                        onTextEdited: numberField.text = text
                        Accessible.name: i18n("Search contacts")
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Controls.Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: numberField.text.length > 0 && contactsModel.count > 0
                                ? i18n("%1 of %2 contacts", matchedContacts.count,
                                       contactsModel.count)
                                : i18np("%1 contact", "%1 contacts", contactsModel.count)
                        }
                        Controls.BusyIndicator {
                            running: contactsModel.loading
                            visible: running
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small
                        }
                        Controls.Button {
                            icon.name: "view-refresh"
                            Accessible.name: i18n("Load contacts from the phone")
                            enabled: devicesModel.count > 0 && !contactsModel.loading
                            onClicked: contactsModel.loadFromPhone(telephonyClient.selectedAddress)
                        }
                        Controls.Button {
                            icon.name: "view-sort"
                            checkable: true
                            checked: settings.sortByLastCalled
                            enabled: numberField.text.length === 0
                            Accessible.name: settings.sortByLastCalled
                                ? i18n("Sorted by last called. Sort by name instead")
                                : i18n("Sorted by name. Sort by last called instead")
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.text: settings.sortByLastCalled
                                ? i18n("Sorted by last called")
                                : i18n("Sorted by name")
                            onToggled: settings.sortByLastCalled = checked
                        }
                        Controls.Button {
                            icon.name: "document-open"
                            Accessible.name: i18n("Open a vCard file")
                            onClicked: vcfDialog.open()
                        }
                    }

                    Kirigami.PlaceholderMessage {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignCenter
                        visible: contactsModel.count === 0 && !contactsModel.loading
                        icon.name: "im-user"
                        text: i18n("No contacts yet")
                        explanation: devicesModel.count > 0
                            ? i18n("Load them from your phone, or open a vCard file.")
                            : i18n("Connect a phone to load its contacts.")
                    }

                    Kirigami.PlaceholderMessage {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignCenter
                        visible: contactsModel.count > 0 && matchedContacts.count === 0
                        icon.name: "system-search"
                        text: i18n("No contact matches")
                        explanation: i18n("You can still call the number as you typed it.")
                    }

                    ListView {
                        id: contactList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        visible: matchedContacts.count > 0
                        model: matchedContacts
                        activeFocusOnTab: true
                        keyNavigationEnabled: true

                        // -1 means all collapsed.
                        property int expandedIndex: -1
                        onModelChanged: expandedIndex = -1

                        delegate: Rectangle {
                            id: contactDelegate
                            required property int index
                            required property string name
                            required property var numbers

                            readonly property bool expanded: contactList.expandedIndex === index

                            width: ListView.view.width
                            height: contactHeader.height + (expanded ? numberColumn.height : 0)
                            // A flat list with hairlines rather than stripes:
                            // zebra rows fight the highlight and read as noise
                            // once every row carries two lines of its own.
                            color: Kirigami.Theme.backgroundColor

                            Kirigami.Separator {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                            }

                            ColumnLayout {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                spacing: 0

                                Controls.ItemDelegate {
                                    id: contactHeader
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Kirigami.Units.gridUnit * 1.8

                                    focus: contactDelegate.ListView.isCurrentItem
                                    highlighted: contactDelegate.ListView.isCurrentItem
                                    Accessible.name: contactDelegate.expanded
                                        ? i18n("%1, collapse", contactDelegate.name)
                                        : i18np("%1, expand to show %2 number",
                                                "%1, expand to show %2 numbers",
                                                contactDelegate.name,
                                                contactDelegate.numbers.length)
                                    onClicked: contactList.expandedIndex =
                                        contactDelegate.expanded ? -1 : contactDelegate.index

                                    // Colours are left to the delegate. Kirigami's
                                    // ItemDelegate already binds its label to
                                    // highlightedTextColor when highlighted, and
                                    // overriding that would only make this list
                                    // disagree with the rest of the desktop.
                                    contentItem: RowLayout {
                                        Controls.Label {
                                            Layout.fillWidth: true
                                            verticalAlignment: Text.AlignVCenter
                                            text: contactDelegate.name
                                            elide: Text.ElideRight
                                            color: contactHeader.highlighted
                                                ? Kirigami.Theme.highlightedTextColor
                                                : Kirigami.Theme.textColor
                                        }
                                        Controls.Label {
                                            visible: contactDelegate.numbers.length > 1
                                            text: i18np("%1 number", "%1 numbers",
                                                        contactDelegate.numbers.length)
                                            color: contactHeader.highlighted
                                                ? Kirigami.Theme.highlightedTextColor
                                                : Kirigami.Theme.disabledTextColor
                                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                                        }
                                        Kirigami.Icon {
                                            source: contactDelegate.expanded ? "go-up" : "go-down"
                                            color: contactHeader.highlighted
                                                ? Kirigami.Theme.highlightedTextColor
                                                : Kirigami.Theme.textColor
                                            implicitWidth: Kirigami.Units.iconSizes.small
                                            implicitHeight: Kirigami.Units.iconSizes.small
                                        }
                                    }
                                }

                                ColumnLayout {
                                    id: numberColumn
                                    Layout.fillWidth: true
                                    Layout.leftMargin: Kirigami.Units.largeSpacing
                                    visible: contactDelegate.expanded
                                    spacing: 0

                                    Repeater {
                                        model: contactDelegate.expanded ? contactDelegate.numbers : []

                                        RowLayout {
                                            id: numberRow
                                            required property string modelData
                                            Layout.fillWidth: true

                                            // Selectable: a number nobody can
                                            // copy has to be read aloud and
                                            // typed back in by hand.
                                            Controls.TextField {
                                                Layout.fillWidth: true
                                                text: numberRow.modelData
                                                readOnly: true
                                                background: null
                                                padding: 0
                                                color: Kirigami.Theme.disabledTextColor
                                            }
                                            Controls.Button {
                                                text: i18n("Call")
                                                icon.name: "call-start"
                                                Accessible.name: i18n("Call %1", numberRow.modelData)
                                                enabled: devicesModel.count > 0 && !root.inCall
                                                onClicked: telephonyClient.dial(numberRow.modelData)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Recents --------------------------------------------------------
                ColumnLayout {
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true

                        Controls.Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: i18np("%1 recent call", "%1 recent calls", recentsModel.count)
                        }
                        Controls.BusyIndicator {
                            running: recentsModel.loading
                            visible: running
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small
                        }
                        Controls.Button {
                            icon.name: "view-refresh"
                            Accessible.name: i18n("Import call history from the phone")
                            enabled: devicesModel.count > 0 && !recentsModel.loading
                            onClicked: recentsModel.loadFromPhone(telephonyClient.selectedAddress)
                        }
                    }

                    Kirigami.PlaceholderMessage {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignCenter
                        visible: recentsModel.count === 0 && !recentsModel.loading
                        icon.name: "view-history"
                        text: i18n("No recent calls")
                        explanation: i18n("Calls you make and answer appear here. You can also import the history from your phone.")
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        visible: recentsModel.count > 0
                        model: recentsModel

                        delegate: Rectangle {
                            id: recentDelegate
                            required property string number
                            required property string name
                            required property string direction
                            required property var timestamp
                            required property var duration
                            required property int index

                            width: ListView.view.width
                            height: Kirigami.Units.gridUnit * 2.4
                            // Hairlines rather than stripes, matching the
                            // contacts list.
                            color: Kirigami.Theme.backgroundColor

                            Kirigami.Separator {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Kirigami.Units.smallSpacing
                                anchors.rightMargin: Kirigami.Units.smallSpacing

                                Kirigami.Icon {
                                    source: recentDelegate.direction === "missed"
                                        ? "call-missed"
                                        : (recentDelegate.direction === "incoming"
                                            ? "call-incoming" : "call-outgoing")
                                    color: recentDelegate.direction === "missed"
                                        ? Kirigami.Theme.negativeTextColor
                                        : Kirigami.Theme.textColor
                                    implicitWidth: Kirigami.Units.iconSizes.small
                                    implicitHeight: Kirigami.Units.iconSizes.small
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing

                                        // Selectable, because this falls back to
                                        // the raw number for a caller who is not
                                        // in the contacts - the case where
                                        // copying it is the whole point.
                                        Controls.TextField {
                                            Layout.fillWidth: true
                                            text: recentDelegate.name.length > 0
                                                ? recentDelegate.name : recentDelegate.number
                                            readOnly: true
                                            background: null
                                            padding: 0
                                        }
                                        // Says "Missed" as well as colouring it:
                                        // a red arrow alone is invisible to a
                                        // red-green colour blind reader, and to
                                        // anyone reading the row too fast.
                                        Controls.Label {
                                            visible: recentDelegate.direction === "missed"
                                            text: i18nc("a missed call", "Missed")
                                            font.pointSize: Kirigami.Theme.smallFont.pointSize
                                            font.weight: Font.DemiBold
                                            color: Kirigami.Theme.negativeTextColor
                                        }
                                    }
                                    Controls.Label {
                                        Layout.fillWidth: true
                                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                                        color: Kirigami.Theme.disabledTextColor
                                        elide: Text.ElideRight
                                        text: {
                                            const t = recentDelegate.timestamp
                                            const when = t && !isNaN(t)
                                                ? Qt.formatDateTime(t, Locale.ShortFormat) : ""
                                            const d = recentDelegate.duration
                                            return d > 0
                                                ? i18nc("call time and length", "%1 · %2",
                                                        when, root.formatDuration(d))
                                                : when
                                        }
                                    }
                                }

                                Controls.Button {
                                    icon.name: "call-start"
                                    Accessible.name: i18n("Call %1", recentDelegate.number)
                                    enabled: devicesModel.count > 0 && !root.inCall
                                    onClicked: telephonyClient.dial(recentDelegate.number)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // A number from a tel: link fills the field rather than dialling.
    Connections {
        target: telephonyClient
        function onNumberRequested(number) {
            numberField.text = number
            numberField.forceActiveFocus()
        }
    }

    Dialogs.FileDialog {
        id: vcfDialog
        title: i18n("Open a vCard file")
        nameFilters: [i18n("vCard files (*.vcf)"), i18n("All files (*)")]
        onAccepted: contactsModel.loadFromVcfFile(selectedFile)
    }

    Dialogs.FileDialog {
        id: exportDialog
        title: i18n("Export contacts to a vCard file")
        fileMode: Dialogs.FileDialog.SaveFile
        defaultSuffix: "vcf"
        // No currentFile: it is obsolete on QtQuick.Dialogs.FileDialog and is
        // url-typed, so a bare "contacts.vcf" would resolve against the QML
        // document's qrc: base rather than naming the saved file. defaultSuffix
        // supplies the extension and the dialog remembers the folder.
        nameFilters: [i18n("vCard files (*.vcf)"), i18n("All files (*)")]
        onAccepted: {
            // No error branch: exportVcfFile reports through setLastError,
            // which the Connections above already turns into a banner.
            if (contactsModel.exportVcfFile(selectedFile))
                errorBanner.visible = false
        }
    }

    Kirigami.OverlaySheet {
        id: settingsSheet

        parent: root.overlay
        implicitWidth: Kirigami.Units.gridUnit * 22
        title: i18n("Settings")

        ColumnLayout {
            spacing: Kirigami.Units.smallSpacing

            Keys.onEscapePressed: (event) => {
                settingsSheet.close()
                event.accepted = true
            }

            Kirigami.Heading {
                text: i18n("Window")
                level: 4
            }
            Controls.CheckBox {
                text: i18n("Keep running in the tray when the window is closed")
                Accessible.name: i18n("Keep running in the tray when the window is closed")
                checked: settings.closeToTray
                onToggled: settings.closeToTray = checked
            }
            Controls.Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pointSize: Kirigami.Theme.smallFont.pointSize
                color: Kirigami.Theme.disabledTextColor
                text: i18n("Foner can only announce an incoming call while it is running.")
            }
            Controls.CheckBox {
                text: i18n("Start hidden in the tray")
                Accessible.name: i18n("Start hidden in the tray")
                checked: settings.startMinimised
                onToggled: settings.startMinimised = checked
            }

            Kirigami.Separator { Layout.fillWidth: true }

            Kirigami.Heading {
                text: i18n("Notifications")
                level: 4
            }
            Controls.CheckBox {
                text: i18n("Incoming calls")
                Accessible.name: i18n("Notify me about incoming calls")
                checked: settings.notifyIncoming
                onToggled: settings.notifyIncoming = checked
            }
            Controls.CheckBox {
                text: i18n("Missed calls")
                Accessible.name: i18n("Notify me about missed calls")
                checked: settings.notifyMissed
                onToggled: settings.notifyMissed = checked
            }
            Controls.CheckBox {
                text: i18n("Ended calls, with their length")
                Accessible.name: i18n("Notify me about ended calls")
                checked: settings.notifyEnded
                onToggled: settings.notifyEnded = checked
            }
            Controls.CheckBox {
                text: i18n("Failures, such as a call the phone refused")
                Accessible.name: i18n("Notify me about failures")
                checked: settings.notifyErrors
                onToggled: settings.notifyErrors = checked
            }
            Controls.CheckBox {
                text: i18n("Phone connecting and disconnecting")
                Accessible.name: i18n("Notify me when the phone connects or disconnects")
                checked: settings.notifyDevice
                onToggled: settings.notifyDevice = checked
            }

            Kirigami.Separator { Layout.fillWidth: true }

            Kirigami.Heading {
                text: i18n("Contacts")
                level: 4
            }
            Controls.CheckBox {
                text: i18n("Load contacts and call history when the phone connects")
                Accessible.name: i18n("Load contacts and call history when the phone connects")
                checked: settings.autoLoadContacts
                onToggled: settings.autoLoadContacts = checked
            }
        }
    }

    Kirigami.OverlaySheet {
        id: incomingSheet

        parent: root.overlay
        implicitWidth: Kirigami.Units.gridUnit * 18
        showCloseButton: false
        onOpened: answerButton.forceActiveFocus()

        title: root.hasOtherCall ? i18n("Second incoming call") : i18n("Incoming call")

        ColumnLayout {
            spacing: Kirigami.Units.largeSpacing

            // Escape declines rather than dismissing the sheet.
            Keys.onEscapePressed: (event) => {
                if (root.incomingIndex >= 0)
                    telephonyClient.hangupCall(callsModel.get(root.incomingIndex).objectPath)
                event.accepted = true
            }

            Kirigami.Icon {
                Layout.alignment: Qt.AlignHCenter
                source: "call-incoming"
                implicitWidth: Kirigami.Units.iconSizes.huge
                implicitHeight: Kirigami.Units.iconSizes.huge
                color: Kirigami.Theme.positiveTextColor
            }

            Controls.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                font.pointSize: Kirigami.Theme.defaultFont.pointSize + 2
                text: {
                    if (root.incomingIndex < 0)
                        return ""
                    const call = callsModel.get(root.incomingIndex)
                    const number = call.line.length > 0 ? call.line : call.incomingLine
                    return contactsModel.nameForNumber(number)
                }
                elide: Text.ElideRight
            }

            // With a call already up the AG offers two ways to answer:
            // hold the current call, or end it.
            Controls.Button {
                id: answerButton
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.4
                text: root.hasOtherCall ? i18n("Hold current call and answer") : i18n("Answer")
                icon.name: "call-start"
                // Answer green, decline red, as a handset draws them. This is
                // the one screen the user reads under time pressure, so the two
                // must not be distinguishable by their labels alone.
                palette.button: Kirigami.Theme.positiveTextColor
                palette.buttonText: Kirigami.Theme.highlightedTextColor
                font.weight: Font.DemiBold
                onClicked: {
                    if (root.hasOtherCall)
                        telephonyClient.holdAndAnswer()
                    else
                        telephonyClient.answerCall(callsModel.get(root.incomingIndex).objectPath)
                }
            }
            Controls.Button {
                Layout.fillWidth: true
                // Needs a call that is actually up to release. With the other
                // leg still dialling there is nothing established to end, and
                // the gateway refuses ReleaseAndAnswer.
                visible: root.hasOtherCall && (root.hasActiveCall || root.hasHeldCall)
                text: i18n("End current call and answer")
                icon.name: "exchange-positions"
                onClicked: telephonyClient.releaseAndAnswer()
            }
            Controls.Button {
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.4
                text: i18n("Decline")
                icon.name: "call-stop"
                palette.button: Kirigami.Theme.negativeTextColor
                palette.buttonText: Kirigami.Theme.highlightedTextColor
                font.weight: Font.DemiBold
                onClicked: telephonyClient.hangupCall(callsModel.get(root.incomingIndex).objectPath)
            }
        }
    }

    // Bound, so a call answered on the handset closes the sheet too.
    onIncomingIndexChanged: {
        if (incomingIndex >= 0) {
            root.show()
            root.raise()
            root.requestActivate()
            incomingSheet.open()
        } else {
            incomingSheet.close()
        }
    }
}
