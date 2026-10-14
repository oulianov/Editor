// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * The choices of one number of the project's data, a radio button each:
 * choices are {text, name, value},
 * value being the index when left out, name the objectName of the button.
 *
 *   Ez.RadioGroup {
 *       jsonData: page.entry
 *       key: "type"
 *       choices: [{name: "radioWeapon", text: qsTr("Weapon")}, {name: "radioMagic", text: qsTr("Magic")}]
 *   }
 *
 * columns: the buttons on one line (choices.length) or in a column (1).
 */
GridLayout {
    id: root

    property var jsonData: null
    property string key: ""
    property var choices: []
    /** The number of the data (-1 when none is shown) */
    readonly property int value: field.valid ? Number(field.value) : -1

    /** A choice made by the user (not by the data) */
    signal activated(int value)

    columns: 1
    rowSpacing: Ez.Metrics.rowSpacing
    columnSpacing: 2 * Ez.Metrics.columnSpacing

    Ez.JsonField {
        id: field
        source: root.jsonData
        path: root.key
    }

    Repeater {
        model: root.choices
        Ez.RadioButton {
            required property var modelData
            required property int index
            objectName: modelData.name ?? ""
            text: modelData.text ?? ""
            jsonData: root.jsonData
            key: root.key
            value: modelData.value ?? index
            onToggled: {
                if (checked) {
                    root.activated(value)
                }
            }
        }
    }
}
