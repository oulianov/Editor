// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.easyrpg.editor as Ez

/** A character of a CharSet, which opens the CharSet picker (CharSetPicker) */
ViewerBase {
    id: root

    implicitWidth: 48
    implicitHeight: 64

    folder: "CharSet"
    preview: "CharSet"

    property alias filename: paintedItem.filename
    property alias cellIndex: paintedItem.cellIndex
    property alias spin: paintedItem.spin
    property alias walk: paintedItem.walk
    property alias transparent: paintedItem.transparent

    Ez.CharSetPaintedItem {
        id: paintedItem

        anchors.fill: parent

        projectData: Ez.ProjectData
    }
}
