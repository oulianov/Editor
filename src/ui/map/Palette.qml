// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The palette of the tiles to draw with (TilePalette): the
 * tiles of the layer edited in rows of 6 under the eraser row, the water
 * and the animated tiles moving, on grey squares where they are
 * transparent. A drag picks a rectangle of tiles (TilePalette has the
 * rules), framed in yellow, as is a tile the eyedropper of the map picks.
 * Ctrl + wheel zooms it.
 *
 * Wide enough for both, it shows the lower tiles on the left and the upper
 * ones on the right, scrolling together, each with the frame of its own
 * tiles picked: a pick on a side makes its layer the one edited (from the
 * event layer too), and the side of the other layer is faded.
 */
FocusScope {
    id: root

    /** The layer edited and the tiles picked (shared by all maps) */
    property Ez.EditorState editorState
    /** The chipset of the map edited, -1: none (nothing shown) */
    property int chipsetId: -1
    /** Item pixels per pixel of a tile: 2 at first */
    property real zoom: 2
    readonly property real cellSize: lowerSide.cellSize
    /** Between the lower and the upper tiles side by side */
    readonly property real gap: Math.round(cellSize / 4)
    /** The lower and the upper tiles side by side: room for both, the gap
     *  and the scroll bar (counted shown or not, the layout stays put) */
    readonly property bool split: chipsetId >= 0
        && width >= 2 * tilePicker.columns * cellSize + gap + verticalBar.implicitWidth
    /** The cells framed: the tiles picked, the eraser row 0. Side by side,
     *  those of the layer edited, the upper ones from the column 6 (on the
     *  event layer too, as the palette alone shows them) */
    readonly property rect selectionRect: !split || tilePicker.active ? tilePicker.selection
        : upperTilePicker.selection.width > 0 ? Qt.rect(upperTilePicker.selection.x + tilePicker.columns,
            upperTilePicker.selection.y, upperTilePicker.selection.width, upperTilePicker.selection.height)
        : upperTilePicker.selection
    /** The tiles of the layer edited, side by side the lower ones (on the left) */
    readonly property alias picker: tilePicker
    /** Side by side, the upper tiles (on the right) */
    readonly property alias upperPicker: upperTilePicker
    readonly property alias view: view

    /** The middle of a cell in this item, the view scrolled to it
     *  (automation); side by side, the upper tiles from the column 6 */
    function cellPoint(x, y) {
        const top = y * cellSize
        if (top < view.contentY || top + cellSize > view.contentY + view.height) {
            view.contentY = Math.max(0, Math.min(top + cellSize / 2 - view.height / 2, view.contentHeight - view.height))
        }
        const columns = tilePicker.columns
        const left = split && x >= columns ? upperSide.x + (x - columns) * cellSize : x * cellSize
        return content.mapToItem(root, left + cellSize / 2, (y + 0.5) * cellSize)
    }

    objectName: "graphicsPalette"
    // the tiles of a layer: the dock opens that wide
    implicitWidth: lowerSide.tilesWidth + verticalBar.implicitWidth
    implicitHeight: lowerSide.tilesHeight

    Ez.TilePalette {
        id: tilePicker
        state: root.editorState
        layer: root.split ? Ez.EditorState.LOWER : -1
    }
    Ez.TilePalette {
        id: upperTilePicker
        state: root.editorState
        layer: Ez.EditorState.UPPER
    }

    /** The tiles of a picker, the frame of those picked, the pointer on them */
    component Side: Item {
        id: side

        required property Ez.TilePalette picker
        /** Its tiles drawn: the upper side only side by side */
        property bool shown: true
        /** The name of the frame (automation) */
        property string frameName
        /** Past its edges, the pointer still picks its tiles (the gap) */
        property real leftReach: 0
        property real rightReach: 0
        readonly property real cellSize: tiles.cellSize
        readonly property real tilesWidth: tiles.width
        readonly property real tilesHeight: tiles.height
        readonly property alias tiles: tiles

        visible: shown

        // under the transparent tiles, the whole side
        Ez.TransparencyPattern {
            anchors.fill: parent
            eraserCells: root.chipsetId >= 0 ? side.picker.columns : 0
            cellSize: tiles.cellSize
        }

        Ez.MapLayerView {
            id: tiles
            width: implicitWidth
            height: implicitHeight
            chipsetId: root.chipsetId
            gridTiles: root.chipsetId >= 0 && side.shown ? side.picker.tiles : []
            gridColumns: side.picker.columns
            zoom: root.zoom
        }

        // the tiles of the layer not edited, faded as the map fades it
        Rectangle {
            anchors.fill: parent
            visible: !side.picker.active
            color: Ez.Metrics.palette.window
            opacity: 0.5
        }

        // the tiles picked: 3 pixels of yellow over the edges of their cells
        Rectangle {
            objectName: side.frameName
            visible: root.chipsetId >= 0 && side.picker.selection.width > 0
            x: side.picker.selection.x * tiles.cellSize - 1
            y: side.picker.selection.y * tiles.cellSize - 1
            width: side.picker.selection.width * tiles.cellSize + 3
            height: side.picker.selection.height * tiles.cellSize + 3
            // the other layer's tiles picked, fainter
            opacity: side.picker.active ? 1 : 0.6
            color: "transparent"
            border.color: "yellow"
            border.width: 3
        }

        MouseArea {
            function cellAt(x, y) {
                return Qt.point(Math.floor((x - side.leftReach) / tiles.cellSize), Math.floor(y / tiles.cellSize))
            }

            anchors.fill: parent
            anchors.leftMargin: -side.leftReach
            anchors.rightMargin: -side.rightReach
            enabled: root.chipsetId >= 0
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            // a drag picks, it does not scroll; it stays on the side it
            // started on (the tiles of a layer), whose cells TilePalette keeps
            preventStealing: true
            onPressed: (mouse) => {
                root.forceActiveFocus(Qt.MouseFocusReason)
                const cell = cellAt(mouse.x, mouse.y)
                side.picker.press(cell.x, cell.y, mouse.button)
            }
            onPositionChanged: (mouse) => {
                const cell = cellAt(mouse.x, mouse.y)
                side.picker.move(cell.x, cell.y)
            }
            onReleased: (mouse) => {
                const cell = cellAt(mouse.x, mouse.y)
                side.picker.release(cell.x, cell.y, mouse.button)
            }
        }
    }

    Flickable {
        id: view

        anchors.fill: parent
        // the scroll bar beside the tiles, not over them
        anchors.rightMargin: verticalBar.visible ? verticalBar.width : 0
        clip: true
        pixelAligned: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: Math.max(width, root.split ? upperSide.x + upperSide.tilesWidth : lowerSide.tilesWidth)
        contentHeight: Math.max(height, lowerSide.tilesHeight, root.split ? upperSide.tilesHeight : 0)

        Controls.ScrollBar.vertical: Controls.ScrollBar {
            id: verticalBar
            objectName: "paletteScrollY"
            parent: root
            x: root.width - width
            y: 0
            height: root.height
            visible: Math.max(lowerSide.tilesHeight, root.split ? upperSide.tilesHeight : 0) > root.height
            policy: Controls.ScrollBar.AlwaysOn
        }
        Controls.ScrollBar.horizontal: Controls.ScrollBar {
            objectName: "paletteScrollX"
        }

        Item {
            id: content
            width: view.contentWidth
            height: view.contentHeight

            // alone: the whole view, as wide as it is
            Side {
                id: lowerSide
                picker: tilePicker
                frameName: "paletteSelection"
                width: root.split ? tilePicker.columns * root.cellSize : parent.width
                height: parent.height
                rightReach: root.split ? root.gap / 2 : 0
            }

            // between the sides, the background of the window
            Rectangle {
                visible: root.split
                x: lowerSide.width
                width: root.gap
                height: parent.height
                color: Ez.Metrics.palette.window
            }

            Side {
                id: upperSide
                picker: upperTilePicker
                shown: root.split
                frameName: "paletteUpperSelection"
                x: tilePicker.columns * root.cellSize + root.gap
                width: Math.max(0, parent.width - x)
                height: parent.height
                leftReach: root.gap / 2
            }

            WheelHandler {
                // the turns not zoomed yet (a notch is 120)
                property real turned: 0

                target: null
                // the mice of XWayland too, which Qt takes for touchpads (Linux)
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                acceptedModifiers: Qt.ControlModifier
                onWheel: (event) => {
                    turned += event.angleDelta.y
                    // the sharp zooms of the tiles, 1 to 4, a step a notch
                    for (; turned >= 120; turned -= 120) {
                        root.zoom = Math.min(4, lowerSide.tiles.snapZoom(root.zoom * 2))
                    }
                    for (; turned <= -120; turned += 120) {
                        root.zoom = Math.max(1, lowerSide.tiles.snapZoom(root.zoom / 2))
                    }
                }
            }
        }
    }
}
