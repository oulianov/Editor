// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ShiftMapDialog"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Shift Map (RPG Maker's TFORMMAPSHIFT): a direction and a number of cells
 * to move the whole map by; what leaves one side comes back on the other.
 */
Ez.DialogWindow {
    id: root

    /** 0 up, 1 left, 2 right, 3 down */
    property int direction: 0
    /** The shift in cells */
    readonly property int dx: direction === 1 ? -units.value : direction === 2 ? units.value : 0
    readonly property int dy: direction === 0 ? -units.value : direction === 3 ? units.value : 0

    objectName: "ShiftMapDialog"
    title: qsTr("Shift Map")

    // the buttons placed like the arrows they stand for
    Ez.Section {
        objectName: "groupDirection"
        title: qsTr("Direction")
        Layout.fillHeight: false
        GridLayout {
            columns: 3
            rowSpacing: 0
            columnSpacing: 2 * Ez.Metrics.columnSpacing
            Layout.alignment: Qt.AlignHCenter

            Item {}
            Ez.RadioButton {
                objectName: "radioUp"
                text: qsTr("Up")
                checked: root.direction === 0
                onToggled: root.direction = 0
            }
            Item {}
            Ez.RadioButton {
                objectName: "radioLeft"
                text: qsTr("Left")
                checked: root.direction === 1
                onToggled: root.direction = 1
            }
            Item {}
            Ez.RadioButton {
                objectName: "radioRight"
                text: qsTr("Right")
                checked: root.direction === 2
                onToggled: root.direction = 2
            }
            Item {}
            Ez.RadioButton {
                objectName: "radioDown"
                text: qsTr("Down")
                checked: root.direction === 3
                onToggled: root.direction = 3
            }
            Item {}
        }
    }
    Ez.Section {
        objectName: "groupUnits"
        title: qsTr("Number of Units")
        Layout.fillHeight: false
        Ez.SpinBox {
            id: units
            objectName: "spinUnits"
            from: 1
            to: 100
            value: 1
            Layout.fillWidth: true
        }
    }
}
