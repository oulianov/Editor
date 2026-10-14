// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ShowChoiceWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Show Choices: the texts of its 4 choices (the lines of their branches,
 * the session's extra "choices": CommandEditorRegistry rebuilds the
 * branches on OK) and what cancelling does. A choice left empty is none:
 * cancelling cannot select it.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Show Choices")
    /** The texts of the choices */
    property var texts: session.extra("choices") ?? ["", "", "", ""]

    function setText(index, text) {
        const edited = texts.slice()
        edited[index] = text
        texts = edited
        session.setExtra("choices", edited)
        // a choice gone cannot be the one cancelling selects
        if (text === "" && Number(cancel.value) === index + 1) {
            cancel.value = 0
        }
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    Ez.JsonField {
        id: cancel
        source: root.session
        path: "/parameters/0"
    }

    Ez.Section {
        title: qsTr("Choices")
        Ez.Form {
            Repeater {
                model: 4
                delegate: Ez.FormLabel {
                    required property int index
                    text: qsTr("Choice %1").arg(index + 1)
                    Layout.row: index
                    Layout.column: 0
                }
            }
            Repeater {
                model: 4
                delegate: Ez.TextField {
                    required property int index
                    objectName: "choice" + (index + 1)
                    text: root.texts[index] ?? ""
                    Layout.row: index
                    Layout.column: 1
                    Layout.fillWidth: true
                    Layout.preferredWidth: 220
                    onTextEdited: root.setText(index, text)
                }
            }
        }
    }

    Ez.Section {
        title: qsTr("Cancellation Behavior")
        GridLayout {
            columns: 2
            rowSpacing: Ez.Metrics.rowSpacing
            columnSpacing: 2 * Ez.Metrics.columnSpacing
            Repeater {
                model: [qsTr("Ignore"), qsTr("Select Choice 1"), qsTr("Select Choice 2"),
                        qsTr("Select Choice 3"), qsTr("Select Choice 4"), qsTr("Execute Handler")]
                delegate: Ez.RadioButton {
                    required property string modelData
                    required property int index
                    objectName: "choiceCancel" + index
                    text: modelData
                    // a choice left empty is none
                    enabled: index < 1 || index > 4 || (root.texts[index - 1] ?? "") !== ""
                    jsonData: root.session
                    key: "/parameters/0"
                    value: index
                    Layout.fillWidth: true
                }
            }
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
