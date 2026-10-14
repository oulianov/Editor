// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "CommonEventWidget"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A common event: its name, what starts it (called from Call Event, or
 * automatically while its switch is on) and its commands, edited in the
 * window's copy of the database. The command list fills the page.
 */
DatabaseEntryPage {
    id: root
    // its canvas or list as large as the page
    fillHeight: true

    /** The command list (its keys, its line selected) */
    readonly property alias commandList: commands

    /** What the window remembers of it: the line selected */
    function viewState() {
        return commands.viewState()
    }
    function restoreViewState(state) {
        commands.restoreViewState(state)
    }
    /** A line of its commands selected (the Search's results: start.line; -1: none) */
    function showLine(start) {
        if (start.line >= 0) {
            commands.selectIndex(start.line)
            commands.view.forceActiveFocus()
        }
    }


    // the commands of the event shown, again when its list moved (an
    // entry pasted, the list resized: the vectors went elsewhere)
    function showCommands() {
        editor.showCommonEvent(root.entry)
    }
    onEntryChanged: showCommands()
    Connections {
        target: root.entry
        function onValueChanged(jsonPtr) {
            if (jsonPtr === "" || jsonPtr === "/event_commands") {
                root.showCommands()
            }
        }
    }
    Ez.CommandListEditor {
        id: editor
        projectData: Ez.ProjectData
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Ez.Metrics.sectionSpacing

        RowLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.fillWidth: true

            Ez.Form {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignBottom

                Ez.FormLabel {
                    text: qsTr("Name:")
                }
                Ez.TextField {
                    objectName: "lineName"
                    jsonData: root.entry
                    key: "name"
                    limit: 20
                    Layout.fillWidth: true
                }
                Ez.FormLabel {
                    text: qsTr("Trigger:")
                }
                Ez.ComboBox {
                    id: trigger
                    objectName: "comboTrigger"
                    jsonData: root.entry
                    key: "trigger"
                    // RPG Maker's names: a called event runs only from Call Event
                    model: [
                        { text: qsTr("Call"), value: 5 },
                        { text: qsTr("Auto Start"), value: 3 },
                        { text: qsTr("Parallel Process"), value: 4 },
                    ]
                    Layout.fillWidth: true
                }
            }

            Ez.Section {
                title: qsTr("Trigger Switch")
                checkable: true
                checkName: "groupSwitch"
                jsonData: root.entry
                key: "switch_flag"
                enabled: trigger.currentValue !== 5
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignBottom

                Ez.RefComboBox {
                    objectName: "comboSwitch"
                    database: root.database
                    listKey: "switches"
                    pickable: true
                    jsonData: root.entry
                    key: "switch_id"
                    Layout.fillWidth: true
                }
            }
        }

        CommandList {
            id: commands
            editor: editor
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
