// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "GoToMapDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Go to Map (Maps menu): a map of the tree chosen by its number or its name.
 * Its list drops down at once, its search focused: what is typed narrows it
 * (the map of the number typed first), Return chooses the map, and Go (or
 * Return again) shows it.
 */
Ez.DialogWindow {
    id: root

    /** The maps of the tree, without the areas: [{text: "0003: Village", value: 3}] */
    property var choices: []
    /** The map chosen, 0 for none */
    readonly property int mapId: combo.currentIndex >= 0 ? Number(combo.currentValue) : 0
    /** The list dropped down once, when the window first shows */
    property bool searched: false

    objectName: "GoToMapDialog"
    title: qsTr("Go to Map")
    preferredWidth: 380
    // room for the list dropped down under the box
    preferredHeight: 360
    acceptable: mapId > 0
    standardButtons: Controls.DialogButtonBox.Cancel
    buttons: [
        Ez.Button {
            objectName: "buttonGo"
            text: qsTr("Go")
            enabled: root.acceptable
            Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.AcceptRole
        }
    ]

    onActiveChanged: {
        if (active && !searched) {
            searched = true
            combo.forceActiveFocus()
            combo.openSearch("")
        }
    }

    Ez.Section {
        objectName: "groupMap"
        title: qsTr("Map")
        Layout.fillHeight: false
        Ez.RefComboBox {
            id: combo
            objectName: "comboMap"
            choices: root.choices
            currentIndex: -1
            focus: true
            Layout.fillWidth: true
            // the map chosen is shown
            Keys.onReturnPressed: root.accept()
            Keys.onEnterPressed: root.accept()
        }
    }
    Ez.Note {
        objectName: "labelHint"
        text: qsTr("Type the number of a map or a part of its name, then choose it in the list.")
        color: Ez.Metrics.mutedTextColor
    }
    Item {
        Layout.fillHeight: true
    }
}
