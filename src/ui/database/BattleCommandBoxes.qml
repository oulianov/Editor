// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ClassEditors"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * The battle commands of a hero or a class (RPG Maker 2003): six boxes
 * (comboBattleCommand1 to 6) of the commands of the database, then the Row
 * command RPG Maker always adds. The array is written as RPG Maker writes
 * it: the commands chosen, the Row command (0), -1 up to 7 entries.
 */
Ez.Section {
    id: root

    /** The hero or class (a JsonView) */
    property var entry: null
    property var database: null
    /** The boxes on a line (1: a column) */
    property int columns: 1

    readonly property var commands: database ? database.choices("battlecommands/commands", qsTr("<none>"), 0) : null
    /** The commands chosen, without the Row command and the padding */
    readonly property var chosen: (field.valid && field.value ? Array.from(field.value) : []).filter(id => Number(id) > 0)

    /** A box chose a command: the array again from the boxes */
    function write() {
        const ids = []
        for (let slot = 0; slot < boxes.count; ++slot) {
            const box = boxes.itemAt(slot) as Ez.ComboBox
            const id = box && box.currentIndex >= 0 ? Number(box.currentValue) : 0
            if (id > 0) {
                ids.push(id)
            }
        }
        ids.push(0)
        while (ids.length < 7) {
            ids.push(-1)
        }
        field.value = ids
    }

    objectName: "groupBattleCommands"
    title: qsTranslate("ClassWidget", "Battle Commands")

    Ez.JsonField {
        id: field
        source: root.entry
        path: "battle_commands"
    }

    GridLayout {
        columns: root.columns
        rowSpacing: Ez.Metrics.rowSpacing
        columnSpacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true

        Repeater {
            id: boxes
            model: 6

            Ez.ComboBox {
                id: box

                required property int index

                objectName: "comboBattleCommand" + (index + 1)
                model: root.commands
                Layout.fillWidth: true
                Layout.minimumWidth: 100
                onActivated: root.write()

                Binding {
                    target: box
                    property: "currentIndex"
                    // again once the commands are listed
                    value: box.count > 0 ? Math.max(0, box.indexOfValue(Number(root.chosen[box.index] ?? 0))) : -1
                    restoreMode: Binding.RestoreNone
                }
            }
        }
        Ez.Label {
            objectName: "labelRow"
            text: qsTranslate("ClassWidget", "Row")
            font: Ez.Metrics.font
            leftPadding: Ez.Metrics.columnSpacing
        }
    }
}
