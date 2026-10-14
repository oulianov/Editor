// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

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
 */
FocusScope {
    id: root

    /** The layer edited and the tiles picked (shared by all maps) */
    property Ez.EditorState editorState
    /** The chipset of the map edited, -1: none (nothing shown) */
    property int chipsetId: -1
    /** Item pixels per pixel of a tile: 2 at first */
    property real zoom: 2
    readonly property real cellSize: tiles.cellSize
    /** The cells framed: the tiles picked, the eraser row 0 */
    readonly property rect selectionRect: picker.selection
    readonly property alias picker: picker
    readonly property alias view: view

    /** The middle of a cell in this item, the view scrolled to it (automation) */
    function cellPoint(x, y) {
        const top = y * cellSize
        if (top < view.contentY || top + cellSize > view.contentY + view.height) {
            view.contentY = Math.max(0, Math.min(top + cellSize / 2 - view.height / 2, view.contentHeight - view.height))
        }
        return content.mapToItem(root, (x + 0.5) * cellSize, (y + 0.5) * cellSize)
    }
    function cellAt(x, y) {
        return Qt.point(Math.floor(x / cellSize), Math.floor(y / cellSize))
    }

    objectName: "graphicsPalette"
    implicitWidth: tiles.width + verticalBar.implicitWidth
    implicitHeight: tiles.height

    Ez.TilePalette {
        id: picker
        state: root.editorState
    }

    Flickable {
        id: view

        anchors.fill: parent
        // the scroll bar beside the tiles, not over them
        anchors.rightMargin: verticalBar.visible ? verticalBar.width : 0
        clip: true
        pixelAligned: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: Math.max(width, tiles.width)
        contentHeight: Math.max(height, tiles.height)

        Controls.ScrollBar.vertical: Controls.ScrollBar {
            id: verticalBar
            objectName: "paletteScrollY"
            parent: root
            x: root.width - width
            y: 0
            height: root.height
            visible: tiles.height > root.height
            policy: Controls.ScrollBar.AlwaysOn
        }
        Controls.ScrollBar.horizontal: Controls.ScrollBar {
            objectName: "paletteScrollX"
        }

        Item {
            id: content
            width: view.contentWidth
            height: view.contentHeight

            // under the transparent tiles, the whole view
            Ez.TransparencyPattern {
                anchors.fill: parent
                eraserCells: root.chipsetId >= 0 ? picker.columns : 0
                cellSize: tiles.cellSize
            }

            Ez.MapLayerView {
                id: tiles
                width: implicitWidth
                height: implicitHeight
                chipsetId: root.chipsetId
                gridTiles: root.chipsetId >= 0 ? picker.tiles : []
                gridColumns: picker.columns
                zoom: root.zoom
            }

            // the tiles picked: 3 pixels of yellow over the edges of their cells
            Rectangle {
                objectName: "paletteSelection"
                visible: root.chipsetId >= 0 && picker.selection.width > 0
                x: picker.selection.x * tiles.cellSize - 1
                y: picker.selection.y * tiles.cellSize - 1
                width: picker.selection.width * tiles.cellSize + 3
                height: picker.selection.height * tiles.cellSize + 3
                color: "transparent"
                border.color: "yellow"
                border.width: 3
            }

            MouseArea {
                anchors.fill: parent
                enabled: root.chipsetId >= 0
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                // a drag picks, it does not scroll
                preventStealing: true
                onPressed: (mouse) => {
                    root.forceActiveFocus(Qt.MouseFocusReason)
                    const cell = root.cellAt(mouse.x, mouse.y)
                    picker.press(cell.x, cell.y, mouse.button)
                }
                onPositionChanged: (mouse) => {
                    const cell = root.cellAt(mouse.x, mouse.y)
                    picker.move(cell.x, cell.y)
                }
                onReleased: (mouse) => {
                    const cell = root.cellAt(mouse.x, mouse.y)
                    picker.release(cell.x, cell.y, mouse.button)
                }

                WheelHandler {
                    // the turns not zoomed yet (a notch is 120)
                    property real turned: 0

                    target: null
                    acceptedModifiers: Qt.ControlModifier
                    onWheel: (event) => {
                        turned += event.angleDelta.y
                        // the sharp zooms of the tiles, 1 to 4, a step a notch
                        for (; turned >= 120; turned -= 120) {
                            root.zoom = Math.min(4, tiles.snapZoom(root.zoom * 2))
                        }
                        for (; turned <= -120; turned += 120) {
                            root.zoom = Math.max(1, tiles.snapZoom(root.zoom / 2))
                        }
                    }
                }
            }
        }
    }
}
