// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.easyrpg.editor as Ez

/** The picker of a FaceSet: its 16 faces, the one chosen (clicked) framed */
PickerBase {
    id: root

    previewContent: Ez.FaceSetPaintedItem {
        id: facesetGrid
        pickerMode: true
        path: root.picker.currentPath
        width: implicitWidth * root.zoom
        height: implicitHeight * root.zoom
        onCellIndexChanged: root.pickerData.index = facesetGrid.cellIndex
        Binding on cellIndex {
            value: Math.max(0, Math.min(15, root.pickerData.index))
        }
    }
}
