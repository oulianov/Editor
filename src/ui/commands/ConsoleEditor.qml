// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ManiacCommandWidget"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Console (Maniac Patch): the form of its schema, the text written in the
 * console typed over several lines (the string of the command, its lines
 * ending with "\r\n" as the Maniac Patch writes them).
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTranslate("EventCommandPalette", "Console")
    readonly property int revision: session.revision
    readonly property bool writes: revision < 0 ? false : session.read("operation") === 2
    readonly property bool typed: revision < 0 ? false : writes && session.read("textMode") === 0

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true

    SchemaForm {
        session: root.session
        only: [0]
        Layout.fillHeight: false
    }

    Ez.Section {
        title: qsTr("Text")
        visible: root.writes
        Layout.fillHeight: root.typed

        SchemaForm {
            session: root.session
            only: [1]
            hides: ["textText"]
            Layout.fillHeight: false
        }

        Controls.ScrollView {
            visible: root.typed
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 420
            Layout.minimumHeight: 100

            Ez.TextArea {
                id: typedText
                objectName: "textText"
                wrapMode: TextEdit.NoWrap
                Component.onCompleted: typedText.text = String(root.session.value("/string") ?? "").replace(/\r\n/g, "\n")
                onTextChanged: {
                    if (activeFocus) {
                        root.session.set("/string", typedText.text.replace(/\n/g, "\r\n"))
                    }
                }
            }
        }
    }

    SchemaForm {
        session: root.session
        only: [2, 3, 4]
        Layout.fillHeight: !root.typed
    }
}
