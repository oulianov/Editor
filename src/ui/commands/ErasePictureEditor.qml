// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ErasePictureWidget"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Erase Picture: its number, or with RPG Maker 2003 1.12 and the Maniac
 * Patch a selection of pictures (a range, from variables, all of them). An
 * older command without the selection does not get it by being opened:
 * it is offered in the games of the Maniac Patch, or when the command has it.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Erase Picture")
    /** The command has the selection (it is offered), as opened */
    readonly property bool selection: session.maniac || session.size("/parameters") > 1

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    Ez.Section {
        title: qsTr("Picture No.")
        SchemaRows {
            session: root.session
            ids: ["selection", "picture", "last", "variable", "lastVariable"]
            names: ({ selection: "erasePictureMode", picture: "erasePictureId", last: "erasePictureLastId",
                      variable: "erasePictureVariable", lastVariable: "erasePictureLastVariable" })
            showRow: id => id !== "selection" || root.selection
        }
    }
    Item {
        Layout.fillHeight: true
    }
}
