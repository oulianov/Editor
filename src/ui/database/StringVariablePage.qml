// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

// the texts of the variables' page: their translations stay
pragma Translator: "VariableWidget"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A string variable of the Maniac Patch (T[n]): its name (the event
 * commands show it), as the variables' page
 */
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
            // as the variables'
            limit: 20
            Layout.fillWidth: true
        }
    }
}
