// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "VariableWidget"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/** A variable of the game: its name (the event commands show it) */
DatabaseEntryPage {
    id: root

    ColumnLayout {
        spacing: Ez.Metrics.rowSpacing
        Layout.fillWidth: true

        Ez.Label {
            text: qsTr("Name:")
            font: Ez.Metrics.font
        }
        Ez.TextField {
            objectName: "lineName"
            jsonData: root.entry
            key: "name"
            limit: 20
            Layout.fillWidth: true
        }
    }
}
