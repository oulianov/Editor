// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ManiacCommandWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Call Command (Maniac Patch): the form of its schema (the command called,
 * its text, where its parameters come from), then its parameters written
 * in the command, as EasyRPG Player reads them:
 *
 * - values: their count in parameter 3, the values from parameter 5, the
 *   mode of each (a constant, a variable, a pointer) in a nibble, four per
 *   parameter from the one parameter 2 names (here: after the values);
 * - expressions: from parameter 5, one after the other. They are typed one
 *   per line (the session's extra "expressions"): OK writes them
 *   (CommandEditorRegistry), or shows the error of one.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTranslate("EventCommandPalette", "Call Command")
    readonly property int revision: session.revision
    readonly property int given: revision < 0 ? 0 : session.read("firstMode")
    readonly property int count: revision < 0 || given !== 3 ? 0 : Math.max(0, parameter(3))
    readonly property int modesAt: revision < 0 ? 5 : parameter(2)

    readonly property var modes: [{ value: 0, text: qsTr("Constant") },
                                  { value: 1, text: qsTr("Variable") },
                                  { value: 2, text: qsTr("Variable pointer") }]

    function parameter(index) {
        return Number(session.value("/parameters/" + index) ?? 0)
    }
    /** The values written in the command: {mode, value} */
    function values() {
        const list = []
        const modesAt = parameter(2)
        for (let i = 0; i < count; ++i) {
            const modes = parameter(modesAt + Math.floor(i / 4))
            list.push({ mode: (modes >>> (4 * (i % 4))) & 15, value: parameter(5 + i) })
        }
        return list
    }
    /** The values from parameter 5, their modes after them */
    function writeValues(list) {
        const parameters = [parameter(0), parameter(1), 5 + list.length, list.length, parameter(4)]
        for (const one of list) {
            parameters.push(one.value)
        }
        for (let i = 0; i < list.length; i += 4) {
            let modes = 0
            for (let k = 0; k < 4 && i + k < list.length; ++k) {
                modes |= (list[i + k].mode & 15) << (4 * k)
            }
            parameters.push(modes)
        }
        session.set("/parameters", parameters)
    }
    function addValue() {
        const list = values()
        list.push({ mode: 0, value: 0 })
        writeValues(list)
    }
    function removeValue(index) {
        const list = values()
        list.splice(index, 1)
        writeValues(list)
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true

    SchemaForm {
        session: root.session
        only: [0]
        Layout.fillHeight: false
    }

    Ez.Section {
        title: qsTr("Parameters")
        Layout.fillHeight: root.given === 4

        SchemaForm {
            session: root.session
            only: [1]
            Layout.fillHeight: false
        }

        // values written in the command
        Ez.Form {
            visible: root.given === 3 && root.count > 0

            Repeater {
                model: root.count
                delegate: Ez.FormLabel {
                    required property int index
                    text: qsTr("Parameter %1").arg(index)
                    Layout.row: index
                    Layout.column: 0
                }
            }
            Repeater {
                model: root.count
                delegate: RowLayout {
                    id: entry
                    required property int index
                    readonly property string name: "value" + index
                    // the mode in a nibble of the parameters after the values
                    readonly property string modeKey: "/parameters/%1@%2:4".arg(root.modesAt + Math.floor(index / 4)).arg(4 * (index % 4))
                    readonly property string valueKey: "/parameters/" + (5 + index)
                    readonly property int mode: root.revision < 0 ? 0 : Number(root.session.value(modeKey) ?? 0)

                    spacing: Ez.Metrics.columnSpacing
                    Layout.row: index
                    Layout.column: 1
                    Layout.fillWidth: true

                    Ez.ComboBox {
                        objectName: entry.name + "Mode"
                        model: root.modes
                        jsonData: root.session
                        key: entry.modeKey
                        Layout.preferredWidth: 150
                        // a variable from the first one, not V[0]
                        onActivated: if (currentValue !== 0 && root.parameter(5 + entry.index) < 1) root.session.set(entry.valueKey, 1)
                    }
                    Ez.SpinBox {
                        objectName: entry.name
                        visible: entry.mode === 0
                        from: -2147483648
                        to: 2147483647
                        jsonData: root.session
                        key: visible ? entry.valueKey : ""
                    }
                    Ez.RefComboBox {
                        objectName: entry.name + "Variable"
                        visible: entry.mode !== 0
                        database: root.session.database
                        listKey: "variables"
                        jsonData: root.session
                        key: visible ? entry.valueKey : ""
                        Layout.fillWidth: true
                        Layout.preferredWidth: 200
                    }
                    Item {
                        visible: entry.mode === 0
                        Layout.fillWidth: true
                    }
                    Ez.Button {
                        objectName: entry.name + "Remove"
                        text: qsTr("Remove")
                        onClicked: root.removeValue(entry.index)
                    }
                }
            }
        }

        RowLayout {
            visible: root.given === 3
            spacing: Ez.Metrics.columnSpacing
            Ez.Button {
                objectName: "addValue"
                text: qsTr("Add Parameter")
                onClicked: root.addValue()
            }
            Item {
                Layout.fillWidth: true
            }
        }

        // expressions, one per line
        Ez.Note {
            visible: root.given === 4
            text: root.session.extra("expressionsReadable") === false
                  ? qsTr("The expressions could not be read: they are kept as they are unless new ones are typed.")
                  : qsTr("One expression per line, a parameter each: V[1] + 1, rand(1, 6)...")
            Layout.fillWidth: true
        }
        Controls.ScrollView {
            visible: root.given === 4
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 420
            Layout.minimumHeight: 100

            Ez.TextArea {
                objectName: "expressions"
                font: Ez.Metrics.monospaceFont
                wrapMode: TextEdit.NoWrap
                Component.onCompleted: text = String(root.session.extra("expressions") ?? "")
                onTextChanged: {
                    if (activeFocus) {
                        root.session.setExtra("expressions", text)
                    }
                }
            }
        }
    }

    Item {
        visible: root.given !== 4
        Layout.fillHeight: true
    }
}
