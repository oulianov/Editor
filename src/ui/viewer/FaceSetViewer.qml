// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.easyrpg.editor as Ez

/** A face of a FaceSet, which opens the FaceSet picker (FaceSetPicker) */
ViewerBase {
    id: root

    implicitWidth: 48
    implicitHeight: 48

    folder: "FaceSet"
    preview: "FaceSet"

    property alias filename: paintedItem.filename
    property alias cellIndex: paintedItem.cellIndex

    Ez.FaceSetPaintedItem {
        id: paintedItem

        anchors.fill: parent

        projectData: Ez.ProjectData
    }
}
