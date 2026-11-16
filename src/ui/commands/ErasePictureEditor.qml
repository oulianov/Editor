// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ErasePictureWidget"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Erase Picture: its number, or with RPG Maker 2003 1.12 and the Maniac
 * Patch a selection of pictures (a range, from variables, all of them),
 * greyed with its tag in the games that do not run it. An older command
 * without the selection does not get it by being opened: only once one is
 * chosen.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Erase Picture")

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    Ez.Section {
        title: qsTr("Picture No.")
        SchemaRows {
            session: root.session
            ids: ["selection", "picture", "last", "variable", "lastVariable"]
            names: ({ selection: "erasePictureMode", picture: "erasePictureId", last: "erasePictureLastId",
                      variable: "erasePictureVariable", lastVariable: "erasePictureLastVariable" })
        }
    }
    Item {
        Layout.fillHeight: true
    }
}
