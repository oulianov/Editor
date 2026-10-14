// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.easyrpg.editor as Ez

/** An image of the game, which opens the image picker (ImagePicker) on its folder */
ViewerBase {
    id: root

    implicitWidth: sprite.implicitWidth
    implicitHeight: sprite.implicitHeight

    folder: sprite.directory
    preview: "Image"

    property alias directory: sprite.directory
    property alias filename: sprite.filename

    Ez.SpritePaintedItem {
        id: sprite

        anchors.fill: parent

        projectData: Ez.ProjectData
    }
}
