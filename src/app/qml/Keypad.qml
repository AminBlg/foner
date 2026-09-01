// SPDX-FileCopyrightText: 2026 AminBlg
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Bare keys: the digit and its letters drawn on the ground, the way a handset
// draws them. The hit area is still the whole cell.
GridLayout {
    id: keypad

    signal keyPressed(string key)

    columns: 3
    // Framed keys need air between them, or the borders read as one table.
    columnSpacing: Kirigami.Units.largeSpacing
    rowSpacing: Kirigami.Units.largeSpacing

    Repeater {
        // The letters make the T9 search discoverable.
        model: [
            { digit: "1", letters: "" },
            { digit: "2", letters: "ABC" },
            { digit: "3", letters: "DEF" },
            { digit: "4", letters: "GHI" },
            { digit: "5", letters: "JKL" },
            { digit: "6", letters: "MNO" },
            { digit: "7", letters: "PQRS" },
            { digit: "8", letters: "TUV" },
            { digit: "9", letters: "WXYZ" },
            { digit: "*", letters: "" },
            { digit: "0", letters: "+" },
            { digit: "#", letters: "" }
        ]

        Controls.Button {
            id: key
            required property var modelData

            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 3.4
            // No background of our own: the style draws a real button, with the
            // gradient, hover and pressed depth the desktop theme provides.
            // Hand-drawing a rectangle here is what made these look flat.
            flat: false
            Accessible.name: key.modelData.digit === "*" ? i18n("Star")
                : (key.modelData.digit === "#" ? i18n("Hash") : key.modelData.digit)
            onClicked: keypad.keyPressed(key.modelData.digit)
            // Long-press 0 types +.
            onPressAndHold: keypad.keyPressed(key.modelData.digit === "0"
                                              ? "+" : key.modelData.digit)

            contentItem: Item {
                // Centred as one block. A column of two labels puts the digit
                // off-centre on the keys that carry no letters.
                Column {
                    anchors.centerIn: parent
                    spacing: 0

                    Controls.Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: key.modelData.digit
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 2.2
                    }
                    Controls.Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: key.modelData.letters.length > 0
                        text: key.modelData.letters
                        font.pointSize: Kirigami.Theme.smallFont.pointSize - 1
                        font.letterSpacing: 1.5
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }
    }
}
