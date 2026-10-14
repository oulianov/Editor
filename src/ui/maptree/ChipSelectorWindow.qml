// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ChipSelectorDialog"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Select Tile (RPG Maker's TFORMCHIPSELECTOR): the lower or upper tiles of
 * a chipset in the order of the palette, 6 a row. A click picks a tile, a
 * drag a block up to the largest size, a double-click is OK.
 */
Ez.DialogWindow {
    id: root

    property int chipsetId: -1
    /** The tile ids of the palette's cells */
    property var tiles: []
    property int maxWidth: 1
    property int maxHeight: 1
    /** The cells picked: x 0 to 5, y the row */
    property rect selection

    objectName: "ChipSelectorDialog"
    title: qsTr("Select Tile")
    acceptable: view.hasSelection
    height: Math.max(minimumHeight, 14 * 32)

    Component.onCompleted: {
        view.select(selection)
        if (selection.width > 0) {
            view.centerOn(selection.x, selection.y)
        }
        view.forceActiveFocus()
    }
    onAccepted: selection = view.selection

    Ez.Note {
        visible: root.maxWidth * root.maxHeight > 1
        text: qsTr("Drag to select up to %1 x %2 tiles.").arg(root.maxWidth).arg(root.maxHeight)
    }
    MapCellView {
        id: view
        objectName: "chipSelector"
        chipsetId: root.chipsetId
        gridTiles: root.tiles
        rectangleMode: true
        maxWidth: root.maxWidth
        maxHeight: root.maxHeight
        // exactly the 6 columns, the scroll bar beside them
        Layout.preferredWidth: 6 * cellSize + 2 + verticalScrollBar.implicitWidth
        Layout.minimumWidth: Layout.preferredWidth
        Layout.fillHeight: true
        Layout.minimumHeight: 8 * cellSize
        onCellActivated: root.accept()
    }
}
