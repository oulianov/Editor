// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ManiacCommandWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Script (Maniac Patch): JavaScript the engine runs. Its code is typed here
 * (the string of the command, a line per Maniac_Script_2 command after the
 * first one) or is in a string (parameter 0: its mode, 1: the string or
 * the variable). The value it ends with goes to the targets (an array: a
 * value to each, in their order): their count in parameter 4, then a kind
 * and a number per target from parameter 5. The code typed is kept while
 * it is in a string, until OK.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTranslate("EventCommandPalette", "Script")
    readonly property int revision: session.revision
    readonly property int sourceMode: revision < 0 ? 0 : parameter(0)
    readonly property int count: revision < 0 ? 0 : Math.max(0, parameter(4))
    /** The code typed, while the code is in a string */
    property string typed: ""

    readonly property var modes: [{ value: 0, text: qsTr("Text") },
                                  { value: 1, text: qsTr("String") },
                                  { value: 2, text: qsTr("String pointer") }]
    // the kinds of targets, as Maniac's TPC writes them: s[1], s[v[2]], v[3], v[v[4]], t[5], t[v[6]]
    readonly property var kinds: [{ value: 0, text: qsTr("Switch") },
                                  { value: 1, text: qsTr("Switch pointer") },
                                  { value: 2, text: qsTr("Variable") },
                                  { value: 3, text: qsTr("Variable pointer") },
                                  { value: 4, text: qsTr("String") },
                                  { value: 5, text: qsTr("String pointer") }]

    function parameter(index) {
        return Number(session.value("/parameters/" + index) ?? 0)
    }
    /** The parameters up to the targets, then the targets ({kind, id}) */
    function writeTargets(targets) {
        const parameters = []
        for (let i = 0; i < 4; ++i) {
            parameters.push(parameter(i))
        }
        parameters.push(targets.length)
        for (const one of targets) {
            parameters.push(one.kind, one.id)
        }
        session.set("/parameters", parameters)
    }
    function targets() {
        const list = []
        for (let i = 0; i < count; ++i) {
            list.push({ kind: parameter(5 + 2 * i), id: parameter(6 + 2 * i) })
        }
        return list
    }
    function addTarget() {
        const list = targets()
        // after the last one: the same kind, the next number
        const last = list.length > 0 ? list[list.length - 1] : { kind: 2, id: 0 }
        list.push({ kind: last.kind, id: last.id + 1 })
        writeTargets(list)
    }
    function removeTarget(index) {
        const list = targets()
        list.splice(index, 1)
        writeTargets(list)
    }
    function moveTarget(index, by) {
        const list = targets()
        const moved = list.splice(index, 1)[0]
        list.splice(index + by, 0, moved)
        writeTargets(list)
    }

    Component.onCompleted: {
        if (sourceMode === 0) {
            // typing at once, at the end
            code.cursorPosition = code.length
            code.forceActiveFocus()
        }
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true

    Ez.Section {
        title: qsTr("Code")
        Layout.fillHeight: root.sourceMode === 0

        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Layout.fillWidth: true

            Ez.FormLabel {
                text: qsTr("Given as")
            }
            Ez.ComboBox {
                objectName: "sourceMode"
                model: root.modes
                jsonData: root.session
                key: "/parameters/0"
                Layout.preferredWidth: 150
                onActivated: {
                    // the code typed goes while it is in a string, and comes back
                    const lines = root.session.value("/lines") ?? ""
                    if (currentValue !== 0 && lines !== "") {
                        root.typed = lines
                        root.session.set("/lines", "")
                    } else if (currentValue === 0 && lines === "" && root.typed !== "") {
                        root.session.set("/lines", root.typed)
                    }
                }
            }
            Ez.RefComboBox {
                objectName: root.sourceMode === 1 ? "sourceString" : "sourceVariable"
                visible: root.sourceMode !== 0
                database: root.session.database
                listKey: root.sourceMode === 1 ? "maniac_string_variables" : "variables"
                pickable: true
                jsonData: root.session
                key: visible ? "/parameters/1" : ""
                Layout.fillWidth: true
                Layout.preferredWidth: 200
            }
            Item {
                visible: root.sourceMode === 0
                Layout.fillWidth: true
            }
        }

        Controls.ScrollView {
            visible: root.sourceMode === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 480
            Layout.minimumHeight: 180

            Ez.TextArea {
                id: code
                objectName: "code"
                font: Ez.Metrics.monospaceFont
                wrapMode: TextEdit.NoWrap
                jsonData: root.session
                key: "/lines"
            }
        }
    }

    Ez.Section {
        title: qsTr("Returned Values")

        Ez.Note {
            text: qsTr("The value the code ends with goes to the first target; an array gives a value to each target, in their order.")
            Layout.fillWidth: true
        }

        Ez.Form {
            visible: root.count > 0

            Repeater {
                model: root.count
                delegate: Ez.FormLabel {
                    required property int index
                    text: qsTr("Target %1").arg(index + 1)
                    Layout.row: index
                    Layout.column: 0
                }
            }
            Repeater {
                model: root.count
                delegate: RowLayout {
                    id: target
                    required property int index
                    readonly property string name: "target" + (index + 1)
                    readonly property int kind: root.revision < 0 ? 0 : root.parameter(5 + 2 * index)
                    readonly property string idKey: "/parameters/" + (6 + 2 * index)

                    spacing: Ez.Metrics.columnSpacing
                    Layout.row: index
                    Layout.column: 1
                    Layout.fillWidth: true

                    Ez.ComboBox {
                        objectName: target.name + "Kind"
                        model: root.kinds
                        jsonData: root.session
                        key: "/parameters/" + (5 + 2 * target.index)
                        Layout.preferredWidth: 150
                    }
                    Ez.RefComboBox {
                        objectName: target.name + (target.kind === 0 ? "Switch" : target.kind === 4 ? "String" : "Variable")
                        database: root.session.database
                        listKey: target.kind === 0 ? "switches" : target.kind === 4 ? "maniac_string_variables" : "variables"
                        pickable: true
                        jsonData: root.session
                        key: target.idKey
                        Layout.fillWidth: true
                        Layout.preferredWidth: 200
                    }
                    Ez.Button {
                        objectName: target.name + "Up"
                        text: "▲"
                        enabled: target.index > 0
                        onClicked: root.moveTarget(target.index, -1)
                    }
                    Ez.Button {
                        objectName: target.name + "Down"
                        text: "▼"
                        enabled: target.index < root.count - 1
                        onClicked: root.moveTarget(target.index, 1)
                    }
                    Ez.Button {
                        objectName: target.name + "Remove"
                        text: qsTr("Remove")
                        onClicked: root.removeTarget(target.index)
                    }
                }
            }
        }

        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.Button {
                objectName: "addTarget"
                text: qsTr("Add Target")
                onClicked: root.addTarget()
            }
            Item {
                Layout.fillWidth: true
            }
        }
    }

    // the sections at the top while the code is in a string
    Item {
        visible: root.sourceMode !== 0
        Layout.fillHeight: true
    }
}
