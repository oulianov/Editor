// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EasyRpgCommandWidget"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Process JSON (EasyRPG Player): the form of its schema, with the JSON and
 * the path typed here when the command holds them. As EasyRPG Player reads
 * them (Game_Interpreter::CommandEasyRpgProcessJson), the string of the
 * command is the path, then the JSON, each one only when it is given as a
 * text (parameters 8 and 2: 0), its length in characters in parameter 9
 * and 3 (else these hold the string variable or the variable).
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTranslate("EventCommandPalette", "Process JSON")
    readonly property int revision: session.revision
    readonly property bool jsonTyped: revision < 0 ? false : parameter(2) === 0
    readonly property bool pathTyped: revision < 0 ? false : parameter(8) === 0

    /** The texts typed, kept while they are given otherwise (another mode chosen and back) */
    property string jsonText: ""
    property string pathText: ""
    property bool loaded: false

    readonly property string jsonError: {
        if (!jsonTyped || jsonText.trim() === "") {
            return ""
        }
        try {
            JSON.parse(jsonText)
            return ""
        } catch (error) {
            return String(error.message ?? error)
        }
    }

    function parameter(index) {
        return Number(session.value("/parameters/" + index) ?? 0)
    }
    /** Characters as EasyRPG Player counts them (UTF-8 characters, not UTF-16 units) */
    function length(text) {
        return Array.from(text).length
    }
    function slice(text, from, count) {
        return Array.from(text).slice(from, from + count).join("")
    }
    /** The string and the lengths for the texts typed and the modes chosen */
    function sync() {
        if (!loaded) {
            return
        }
        const text = (pathTyped ? pathText : "") + (jsonTyped ? jsonText : "")
        if (String(session.value("/string") ?? "") !== text) {
            session.set("/string", text)
        }
        if (pathTyped && parameter(9) !== length(pathText)) {
            session.set("/parameters/9", length(pathText))
        }
        if (jsonTyped && parameter(3) !== length(jsonText)) {
            session.set("/parameters/3", length(jsonText))
        }
    }

    Component.onCompleted: {
        const text = String(session.value("/string") ?? "")
        let at = 0
        if (parameter(8) === 0) {
            pathText = slice(text, 0, Math.max(0, parameter(9)))
            at = Math.max(0, parameter(9))
        }
        if (parameter(2) === 0) {
            jsonText = slice(text, at, Math.max(0, parameter(3)))
        }
        loaded = true
    }
    // a mode chosen in the form: the string follows
    onRevisionChanged: Qt.callLater(sync)

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true

    SchemaForm {
        session: root.session
        only: [0]
        Layout.fillHeight: false
    }

    Ez.Section {
        title: qsTr("JSON")
        Layout.fillWidth: true
        Layout.fillHeight: root.jsonTyped

        SchemaForm {
            session: root.session
            only: [1]
            Layout.fillHeight: false
        }
        Controls.ScrollView {
            visible: root.jsonTyped
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 420
            Layout.minimumHeight: 120

            Ez.TextArea {
                id: json
                objectName: "textJson"
                font: Ez.Metrics.monospaceFont
                wrapMode: TextEdit.NoWrap
                placeholderText: "{ \"name\": \"value\" }"
                Component.onCompleted: text = root.jsonText
                onTextChanged: {
                    if (activeFocus) {
                        root.jsonText = text
                        root.sync()
                    }
                }
            }
        }
        // EasyRPG Player cannot change a JSON typed in the command
        Ez.Note {
            objectName: "labelJsonConstant"
            visible: root.jsonTyped && root.parameter(0) === 0 && [1, 5, 6].includes(root.parameter(1))
            text: qsTr("This operation changes the JSON: give it as a string variable, or EasyRPG Player skips the command.")
            color: Kirigami.Theme.negativeTextColor
            Layout.fillWidth: true
        }
        Ez.Note {
            objectName: "labelJsonError"
            visible: root.jsonError !== ""
            text: qsTr("Not valid JSON: %1. EasyRPG Player skips the command.").arg(root.jsonError)
            color: Kirigami.Theme.negativeTextColor
            Layout.fillWidth: true
        }
    }

    Ez.Section {
        title: qsTr("Path")
        Layout.fillWidth: true

        SchemaForm {
            session: root.session
            only: [2]
            Layout.fillHeight: false
        }
        Ez.TextField {
            objectName: "linePath"
            visible: root.pathTyped
            font: Ez.Metrics.monospaceFont
            placeholderText: "/key/0/name"
            Layout.fillWidth: true
            Component.onCompleted: text = root.pathText
            onTextEdited: {
                root.pathText = text
                root.sync()
            }
        }
        Ez.Note {
            visible: root.pathTyped
            text: qsTr("A JSON Pointer: / for the whole JSON, /name for a key, /list/0 for the first entry of an array.")
            Layout.fillWidth: true
        }
    }

    // the result (its kind, its switch, variable or string) and the options
    SchemaForm {
        session: root.session
        only: [3, 4, 5]
        Layout.fillHeight: !root.jsonTyped
    }
}
