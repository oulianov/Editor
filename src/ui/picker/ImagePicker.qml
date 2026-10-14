// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.easyrpg.editor as Ez

/**
 * The picker of an image shown whole, as in its file: backdrops, panoramas,
 * pictures, battle animations and charsets, chipsets, system graphics,
 * title and game over screens...
 */
PickerBase {
    id: root

    previewContent: Ez.SpritePaintedItem {
        id: sprite
        pickerMode: true
        directory: root.directory
        path: root.picker.currentPath
        width: implicitWidth * root.zoom
        height: implicitHeight * root.zoom
    }
}
