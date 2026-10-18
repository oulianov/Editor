// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MapPreview"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A map to pick cells on (RPG Maker's TFORMMAPVIEWER): the map as the map
 * view draws it (MapPreview.qml), or a grid of tiles (gridTiles: the palette
 * of a chipset), with the cell picked or the rectangle dragged framed in
 * black and white. A click picks a cell, a drag a rectangle (rectangleMode,
 * up to maxWidth x maxHeight), past the edge it scrolls; a double-click
 * activates the cell. The arrows move the frame, the wheel scrolls, Ctrl +
 * wheel zooms (1/1 to 1/8: MapZoomButtons).
 */
FocusScope {
    id: root

    /** The map shown */
    property Ez.MapDocument document: null
    /** Grid mode: these tile ids in rows of gridColumns, of chipsetId */
    property var gridTiles: []
    property int gridColumns: 6
    property int chipsetId: -1
    /** The pixels of a cell: 32 is the map at 1/1 */
    property int cellSize: 32
    /** A drag selects a rectangle, else it moves the cell */
    property bool rectangleMode: false
    /** The largest rectangle (0: no limit) */
    property int maxWidth: 0
    property int maxHeight: 0
    /** The cell or rectangle selected, in cells; none when its width is 0 */
    property rect selection: Qt.rect(0, 0, 0, 0)
    readonly property bool hasSelection: selection.width > 0 && selection.height > 0

    readonly property bool gridMode: gridTiles.length > 0
    readonly property int tileSize: gridMode ? grid.tileSize : preview.lowerView.tileSize
    readonly property int columns: gridMode ? grid.cells.width : (document ? preview.lowerView.cells.width : 0)
    readonly property int rows: gridMode ? grid.cells.height : (document ? preview.lowerView.cells.height : 0)
    readonly property alias view: view
    // the scroll bars the cells need (one makes room for the other): set
    // once the sizes settle, the size of a bar depending on what it shows
    readonly property real contentWidthNeeded: columns * cellSize
    readonly property real contentHeightNeeded: rows * cellSize
    property bool needsVerticalBar: false
    property bool needsHorizontalBar: false
    function updateBars() {
        const vertical = contentHeightNeeded > height - 2
            || (contentWidthNeeded > width - 2 && contentHeightNeeded > height - 2 - horizontalBar.implicitHeight)
        needsHorizontalBar = contentWidthNeeded > width - 2
            || (contentHeightNeeded > height - 2 && contentWidthNeeded > width - 2 - verticalBar.implicitWidth)
        needsVerticalBar = vertical
    }
    onWidthChanged: Qt.callLater(updateBars)
    onHeightChanged: Qt.callLater(updateBars)
    onContentWidthNeededChanged: Qt.callLater(updateBars)
    onContentHeightNeededChanged: Qt.callLater(updateBars)
    /** Beside the cells when they are higher than the view */
    readonly property alias verticalScrollBar: verticalBar

    /** A double-click on a cell */
    signal cellActivated(int x, int y)

    /** Selects cells, kept inside the map (once it is shown) */
    function select(cells) {
        if (cells.width > 0 && cells.height > 0 && (columns <= 0 || rows <= 0)) {
            pendingSelection = cells
            return
        }
        pendingSelection = Qt.rect(0, 0, 0, 0)
        if (columns <= 0 || rows <= 0 || cells.width <= 0 || cells.height <= 0) {
            selection = Qt.rect(0, 0, 0, 0)
            return
        }
        const left = Math.max(0, Math.min(cells.x, columns - 1))
        const top = Math.max(0, Math.min(cells.y, rows - 1))
        const right = Math.min(cells.x + cells.width, columns)
        const bottom = Math.min(cells.y + cells.height, rows)
        selection = Qt.rect(left, top, Math.max(1, right - left), Math.max(1, bottom - top))
    }
    function setCell(x, y) {
        select(Qt.rect(x, y, 1, 1))
    }
    /** Scrolls the cell to the middle of the view (once it has its size) */
    function centerOn(x, y) {
        pendingCenter = Qt.point(x, y)
        applyCenter()
    }
    /** The middle of a cell in this item, scrolled into view first (scripts click there) */
    function cellPoint(x, y) {
        const left = x * cellSize - view.contentX
        const top = y * cellSize - view.contentY
        if (left < 0 || top < 0 || left + cellSize > view.width || top + cellSize > view.height) {
            centerOn(x, y)
        }
        return content.mapToItem(root, (x + 0.5) * cellSize, (y + 0.5) * cellSize)
    }
    /** The cell under a point of the content, inside the map */
    function cellAt(x, y) {
        return Qt.point(Math.max(0, Math.min(Math.floor(x / cellSize), columns - 1)),
                        Math.max(0, Math.min(Math.floor(y / cellSize), rows - 1)))
    }
    function zoomTo(size) {
        size = Math.max(4, Math.min(64, size))
        if (size === cellSize) {
            return
        }
        // the middle of the view stays where it is
        const middleX = (view.contentX + view.width / 2) / cellSize
        const middleY = (view.contentY + view.height / 2) / cellSize
        cellSize = size
        view.contentX = Math.max(0, Math.min(middleX * size - view.width / 2, view.contentWidth - view.width))
        view.contentY = Math.max(0, Math.min(middleY * size - view.height / 2, view.contentHeight - view.height))
    }

    property point pendingCenter: Qt.point(-1, -1)
    property rect pendingSelection: Qt.rect(0, 0, 0, 0)
    function applyCenter() {
        if (pendingCenter.x < 0 || view.width <= 0 || view.height <= 0 || columns <= 0) {
            return
        }
        const x = pendingCenter.x
        const y = pendingCenter.y
        pendingCenter = Qt.point(-1, -1)
        view.contentX = Math.max(0, Math.min(x * cellSize + cellSize / 2 - view.width / 2, view.contentWidth - view.width))
        view.contentY = Math.max(0, Math.min(y * cellSize + cellSize / 2 - view.height / 2, view.contentHeight - view.height))
    }

    // a new map: the selection stays, inside it
    function fitSelection() {
        if (pendingSelection.width > 0) {
            select(pendingSelection)
        } else if (hasSelection) {
            select(selection)
        }
        applyCenter()
    }
    onColumnsChanged: fitSelection()
    onRowsChanged: fitSelection()

    implicitWidth: 240
    implicitHeight: 180
    activeFocusOnTab: true

    // the arrows move the cell, or the rectangle with its size
    Keys.onPressed: (event) => {
        let dx = 0
        let dy = 0
        switch (event.key) {
        case Qt.Key_Left: dx = -1; break
        case Qt.Key_Right: dx = 1; break
        case Qt.Key_Up: dy = -1; break
        case Qt.Key_Down: dy = 1; break
        default: return
        }
        event.accepted = true
        if (columns <= 0) {
            return
        }
        const moved = hasSelection ? Qt.rect(selection.x + dx, selection.y + dy, selection.width, selection.height)
                                   : Qt.rect(0, 0, 1, 1)
        moved.x = Math.max(0, Math.min(moved.x, columns - moved.width))
        moved.y = Math.max(0, Math.min(moved.y, rows - moved.height))
        selection = moved
        cellPoint(moved.x, moved.y)
    }

    Rectangle {
        anchors.fill: parent
        // the map on a dark ground, in light and dark mode
        color: Qt.darker(Kirigami.Theme.backgroundColor, Kirigami.Theme.backgroundColor.hslLightness > 0.5 ? 1.5 : 1.3)
        border.color: root.activeFocus ? Kirigami.Theme.focusColor : Ez.Metrics.sectionBorderColor
        border.width: 1
    }

    Flickable {
        id: view

        anchors.fill: parent
        anchors.margins: 1
        anchors.rightMargin: 1 + (verticalBar.visible ? verticalBar.width : 0)
        anchors.bottomMargin: 1 + (horizontalBar.visible ? horizontalBar.height : 0)
        clip: true
        pixelAligned: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: Math.max(width, root.columns * root.cellSize)
        contentHeight: Math.max(height, root.rows * root.cellSize)
        onWidthChanged: root.applyCenter()
        onHeightChanged: root.applyCenter()

        Controls.ScrollBar.vertical: Controls.ScrollBar {
            id: verticalBar
            parent: root
            x: root.width - width - 1
            y: 1
            height: view.height
            visible: root.needsVerticalBar
            policy: Controls.ScrollBar.AlwaysOn
        }
        Controls.ScrollBar.horizontal: Controls.ScrollBar {
            id: horizontalBar
            parent: root
            x: 1
            y: root.height - height - 1
            width: view.width
            visible: root.needsHorizontalBar
            policy: Controls.ScrollBar.AlwaysOn
        }

        Item {
            id: content
            width: view.contentWidth
            height: view.contentHeight

            // under the transparent tiles of the grid: grey squares
            Canvas {
                id: squares
                visible: root.gridMode
                width: root.columns * root.cellSize
                height: root.rows * root.cellSize
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                onPaint: {
                    const context = getContext("2d")
                    context.fillStyle = "#808080"
                    context.fillRect(0, 0, width, height)
                    context.fillStyle = "#666666"
                    for (let y = 0; y < height; y += 8) {
                        for (let x = (y / 8) % 2 * 8; x < width; x += 16) {
                            context.fillRect(x, y, 8, 8)
                        }
                    }
                }
            }

            Ez.MapLayerView {
                id: grid
                visible: root.gridMode
                width: implicitWidth
                height: implicitHeight
                chipsetId: root.chipsetId
                gridTiles: root.gridTiles
                gridColumns: root.gridColumns
                zoom: root.cellSize / Math.max(1, tileSize)
            }

            MapPreview {
                id: preview
                visible: !root.gridMode && root.document !== null
                document: root.gridMode ? null : root.document
                zoom: root.cellSize / Math.max(1, lowerView.tileSize)
            }

            // the cell or rectangle: black and white, seen on any tile
            Rectangle {
                objectName: "cellFrame"
                visible: root.hasSelection && root.columns > 0
                x: root.selection.x * root.cellSize
                y: root.selection.y * root.cellSize
                width: root.selection.width * root.cellSize
                height: root.selection.height * root.cellSize
                color: "transparent"
                border.color: "black"
                border.width: 3

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    color: "transparent"
                    border.color: "white"
                    border.width: 1
                }
            }

            MouseArea {
                id: mouse

                // the cell the drag started on, the last point in the view
                property point anchor
                property point viewPoint
                property bool dragging: false

                function dragTo() {
                    const at = root.cellAt(viewPoint.x + view.contentX, viewPoint.y + view.contentY)
                    if (!root.rectangleMode) {
                        root.setCell(at.x, at.y)
                        return
                    }
                    let x = at.x
                    let y = at.y
                    if (root.maxWidth > 0) {
                        x = Math.max(anchor.x - root.maxWidth + 1, Math.min(x, anchor.x + root.maxWidth - 1))
                    }
                    if (root.maxHeight > 0) {
                        y = Math.max(anchor.y - root.maxHeight + 1, Math.min(y, anchor.y + root.maxHeight - 1))
                    }
                    root.select(Qt.rect(Math.min(anchor.x, x), Math.min(anchor.y, y),
                                        Math.abs(x - anchor.x) + 1, Math.abs(y - anchor.y) + 1))
                }

                anchors.fill: parent
                enabled: root.columns > 0
                // a drag picks, it does not scroll
                preventStealing: true
                onPressed: (event) => {
                    root.forceActiveFocus(Qt.MouseFocusReason)
                    anchor = root.cellAt(event.x, event.y)
                    viewPoint = mapToItem(view, event.x, event.y)
                    dragging = true
                    root.setCell(anchor.x, anchor.y)
                }
                onPositionChanged: (event) => {
                    if (!dragging) {
                        return
                    }
                    viewPoint = mapToItem(view, event.x, event.y)
                    dragTo()
                }
                onReleased: dragging = false
                onCanceled: dragging = false
                onDoubleClicked: (event) => {
                    const cell = root.cellAt(event.x, event.y)
                    if (cell.x < root.selection.x || cell.y < root.selection.y
                            || cell.x >= root.selection.x + root.selection.width
                            || cell.y >= root.selection.y + root.selection.height) {
                        root.setCell(cell.x, cell.y)
                    }
                    root.cellActivated(cell.x, cell.y)
                }
            }
        }

        // past the edge while dragging: a cell every 40 ms, as RPG Maker
        Timer {
            interval: 40
            repeat: true
            running: mouse.dragging && (mouse.viewPoint.x < 0 || mouse.viewPoint.y < 0
                                        || mouse.viewPoint.x > view.width || mouse.viewPoint.y > view.height)
            onTriggered: {
                const dx = mouse.viewPoint.x < 0 ? -root.cellSize : mouse.viewPoint.x > view.width ? root.cellSize : 0
                const dy = mouse.viewPoint.y < 0 ? -root.cellSize : mouse.viewPoint.y > view.height ? root.cellSize : 0
                view.contentX = Math.max(0, Math.min(view.contentX + dx, view.contentWidth - view.width))
                view.contentY = Math.max(0, Math.min(view.contentY + dy, view.contentHeight - view.height))
                mouse.dragTo()
            }
        }

        WheelHandler {
            // the turns not zoomed yet (a notch is 120)
            property real turned: 0

            target: null
            acceptedModifiers: Qt.ControlModifier
            onWheel: (event) => {
                turned += event.angleDelta.y
                for (; turned >= 120; turned -= 120) {
                    root.zoomTo(root.cellSize * 2)
                }
                for (; turned <= -120; turned += 120) {
                    root.zoomTo(root.cellSize / 2)
                }
            }
        }
    }
}
