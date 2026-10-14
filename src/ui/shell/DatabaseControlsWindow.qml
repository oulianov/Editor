// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

// The Ez controls of the database pages bound to the project's data, for
// the scripts (tests/scripts/qml_database_controls.txt: "qml-open
// DatabaseControlsWindow"): an entry of a list searched, a system color,
// choices, tables of arrays. They edit a copy of the database (a session
// never applied): nothing reaches the project.

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

Ez.DialogWindow {
    id: root

    readonly property var database: session.database
    readonly property var commonEvent: database ? database.subtree("commonevents/0") : null
    readonly property var item: database ? database.subtree("items/0") : null
    readonly property var skills: database ? database.list("classes/0/skills") : null
    readonly property var attributes: database ? database.list("attributes") : null

    objectName: "qmlDatabaseControls"
    title: "Ez database controls"
    preferredWidth: 460

    Ez.DatabaseSession {
        id: session
        projectData: Ez.ProjectData
    }

    Ez.Section {
        title: "First common event"
        Ez.Form {
            Ez.FormLabel {
                text: "Switch"
            }
            Ez.RefComboBox {
                objectName: "controlSwitch"
                database: root.database
                listKey: "switches"
                noneText: "(None)"
                jsonData: root.commonEvent
                key: "switch_id"
                pickable: true
                Layout.fillWidth: true
                onPicked: (id) => picked.text = "picked " + id
            }
            Ez.FormLabel {
                text: "Again"
            }
            Ez.RefComboBox {
                objectName: "controlSwitchAgain"
                database: root.database
                listKey: "switches"
                jsonData: root.commonEvent
                key: "switch_id"
                Layout.fillWidth: true
            }
            Ez.FormLabel {
                text: "Picker"
            }
            Ez.Label {
                id: picked
                objectName: "pickedLabel"
                text: "-"
            }
        }
    }

    Ez.Section {
        title: "First condition and attribute"
        Ez.Form {
            Ez.FormLabel {
                text: "Color"
            }
            Ez.SystemColorComboBox {
                objectName: "controlColor"
                database: root.database
                jsonData: root.database ? root.database.subtree("states/0") : null
                key: "color"
            }
            Ez.FormLabel {
                text: "Type"
            }
            Ez.RadioGroup {
                columns: 2
                jsonData: root.database ? root.database.subtree("attributes/0") : null
                key: "type"
                choices: [{ name: "controlWeapon", text: "Weapon" }, { name: "controlMagic", text: "Magic" }]
            }
        }
    }

    Ez.Section {
        title: "Tables"
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Layout.fillWidth: true

            Ez.ArrayTable {
                id: skillTable
                objectName: "controlSkills"
                rows: root.skills ? root.skills.count : 0
                source: root.skills
                addLine: true
                Layout.fillWidth: true
                columns: [
                    { title: "Level", width: 50, text: row => String(root.skills.num(row + "/level")) },
                    { title: "Skill", text: row => {
                        const id = root.skills.num(row + "/skill_id")
                        return root.database.exists("skills/" + (id - 1)) ? root.database.str("skills/" + (id - 1) + "/name") : "<" + id + "?>"
                    } },
                ]
                // a new skill learned at level 1 (the pages ask in a window)
                onActivated: (row) => {
                    if (row === rows && root.skills.insertEntries(row, 1)) {
                        root.skills.set(row + "/level", 1)
                        root.skills.set(row + "/skill_id", 1)
                    }
                }
                onRemoveRequested: (row) => root.skills.removeEntries(row, 1)
            }
            Ez.ArrayTable {
                objectName: "controlItemAttributes"
                rows: root.attributes ? root.attributes.count : 0
                source: root.item
                Layout.fillWidth: true
                columns: [
                    { title: "", width: 24, name: "controlItemAttribute",
                      checked: row => root.item.bit("attribute_set", row),
                      toggle: (row, on) => root.item.setBit("attribute_set", row, on) },
                    { title: "Attribute", text: row => root.attributes.str(row + "/name") },
                ]
            }
        }
    }
}
