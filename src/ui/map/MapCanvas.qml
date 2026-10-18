// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MapScene"

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A map being edited, in a view that scrolls: the map as MapPreview draws it,
 * the pasted tiles floating over it, and what its MapEditor shows over them
 * (MapOverlay: the brush, the selection and its marching ants, the selected
 * cells, the grid of the event layer, an area).
 *
 * The mouse and the keys go to the editor as cells: the keys of the map
 * before the shortcuts of the window, as RPG Maker. Ctrl + wheel, a pinch
 * and the zoom tool zoom by the editor's steps, snapped to whole pixels of
 * the screen, the cell under the pointer staying in place. The view scrolls
 * while a drag goes past its edges. The editor's requests (a new or edited
 * event, a teleport, a test play) come out as signals for the host.
 */
FocusScope {
    id: root

    /** The editing of the map shown (its MapDocument, the shared EditorState) */
    property Ez.MapEditor editor
    readonly property Ez.MapDocument document: editor ? editor.document : null
    /** The layer edited (Ez.EditorState.LOWER, UPPER or EVENT) */
    readonly property int editedLayer: editor ? editor.state.layer : Ez.EditorState.EVENT
    /** The zoom shown: item pixels per pixel of a tile */
    readonly property real zoom: d.zoom
    /** The pixels of a cell */
    readonly property real cellSize: preview.lowerView.cellSize
    /** The boundaries of an area of this map, dashed; empty: none (showArea) */
    property rect area
    /** The cell under the pointer and its events, for the status bar */
    readonly property string hoverText: editor ? editor.hoverText : ""
    /** The zoom a map opens at, as the map tabs (RPG Maker's 1/2) */
    property real defaultScale: 2
    /** Past an edge by less than this, a drag scrolls the view */
    property int scrollMargin: 8

    readonly property alias view: view
    readonly property alias preview: preview
    readonly property alias overlay: overlay
    readonly property alias contextMenu: menu

    /** The event dialog of a new event on a cell, or of an event (id) */
    signal newEventRequested(point cell)
    signal editEventRequested(int id)
    /** Where a teleport event on a free cell goes (MapDocument::placeTeleport) */
    signal placeTeleportRequested(point cell)
    /** Test play from a cell */
    signal runHereRequested(point cell)

    objectName: "mapView"
    activeFocusOnTab: true

    /** The cell at a point of the view's content, any cell outside the map */
    function cellAt(x, y) {
        const size = cellSize > 0 ? cellSize : 1
        return Qt.point(Math.floor((x - mapItem.x) / size), Math.floor((y - mapItem.y) / size))
    }
    /** The cell at a point of the window (the scene) */
    function cellAtScene(x, y) {
        const point = pointer.mapFromItem(null, x, y)
        return cellAt(point.x, point.y)
    }
    /** The middle of a cell in this item; reveal: the view scrolls to it first */
    function cellPoint(x, y, reveal) {
        if (reveal) {
            root.reveal(x, y)
        }
        return mapItem.mapToItem(root, (x + 0.5) * cellSize, (y + 0.5) * cellSize)
    }
    /** Scrolls as little as needed to show a cell */
    function reveal(x, y) {
        const left = mapItem.x + x * cellSize
        const top = mapItem.y + y * cellSize
        let contentX = view.contentX
        let contentY = view.contentY
        if (left < contentX) {
            contentX = left
        } else if (left + cellSize > contentX + view.width) {
            contentX = left + cellSize - view.width
        }
        if (top < contentY) {
            contentY = top
        } else if (top + cellSize > contentY + view.height) {
            contentY = top + cellSize - view.height
        }
        scrollTo(contentX, contentY, true)
    }
    /** Scrolls a cell to the middle of the view */
    function centerOn(x, y) {
        scrollTo(mapItem.x + (x + 0.5) * cellSize - view.width / 2, mapItem.y + (y + 0.5) * cellSize - view.height / 2, true)
    }
    /** Dashes the boundaries of an area (cells) and centers it; empty: hidden */
    function showArea(cells) {
        area = cells
        if (cells.width > 0 && cells.height > 0) {
            centerOn(cells.x + (cells.width - 1) / 2, cells.y + (cells.height - 1) / 2)
        }
    }
    /** The view at a point of its content, inside it; save: kept in the map
     *  info, as the user scrolled there */
    function scrollTo(x, y, save) {
        d.saving = save
        view.contentX = Math.round(Math.max(0, Math.min(x, view.contentWidth - view.width)))
        view.contentY = Math.round(Math.max(0, Math.min(y, view.contentHeight - view.height)))
        d.saving = true
    }

    // the zoom of the editor, the point of the map under the anchor (the
    // pointer, else the middle of the view) staying there
    function applyZoom() {
        const next = editor && editor.scale > 0 ? editor.scale : 1
        const old = d.zoom
        if (!d.placed || next === old) {
            d.zoom = next
            return
        }
        const anchorX = d.anchor.x >= 0 ? d.anchor.x : view.width / 2
        const anchorY = d.anchor.y >= 0 ? d.anchor.y : view.height / 2
        const mapX = (view.contentX + anchorX - mapItem.x) / old
        const mapY = (view.contentY + anchorY - mapItem.y) / old
        // the sizes follow at once
        d.zoom = next
        scrollTo(mapX * next + mapItem.x - anchorX, mapY * next + mapItem.y - anchorY, false)
    }
    // the first zoom and where the view was scrolled to (the map info), once
    // the view has a size
    function place() {
        if (d.placed || !editor || view.width <= 0 || view.height <= 0) {
            return
        }
        editor.devicePixelRatio = Screen.devicePixelRatio
        if (editor.scale <= 0) {
            editor.scale = defaultScale
        }
        d.zoom = editor.scale
        d.placed = true
        const scroll = editor.scrollPosition
        scrollTo(scroll.x * d.zoom, scroll.y * d.zoom, false)
    }
    function saveScroll() {
        if (!d.saving || !d.placed || !editor) {
            return
        }
        // the scroll position along the axes with a scroll bar shown (else as it was)
        const scroll = editor.scrollPosition
        editor.scrollPosition = Qt.point(view.contentWidth > view.width ? Math.floor(view.contentX / d.zoom) : scroll.x,
                                         view.contentHeight > view.height ? Math.floor(view.contentY / d.zoom) : scroll.y)
    }
    // where the pointer is in the view, and how far past its edges
    function track(mouse) {
        const x = mouse.x - view.contentX
        const y = mouse.y - view.contentY
        d.pointer = Qt.point(x, y)
        d.buttons = mouse.buttons
        const margin = scrollMargin
        d.edge = Qt.point(x < margin ? x - margin : x > view.width - margin ? x - view.width + margin : 0,
                          y < margin ? y - margin : y > view.height - margin ? y - view.height + margin : 0)
    }
    // the keys the map does not take scroll it, as a scroll view does
    function scrollKey(event) {
        if (event.modifiers !== Qt.NoModifier && event.modifiers !== Qt.KeypadModifier) {
            return false
        }
        const step = 20
        switch (event.key) {
        case Qt.Key_Left: scrollTo(view.contentX - step, view.contentY, true); return true
        case Qt.Key_Right: scrollTo(view.contentX + step, view.contentY, true); return true
        case Qt.Key_Up: scrollTo(view.contentX, view.contentY - step, true); return true
        case Qt.Key_Down: scrollTo(view.contentX, view.contentY + step, true); return true
        case Qt.Key_PageUp: scrollTo(view.contentX, view.contentY - view.height, true); return true
        case Qt.Key_PageDown: scrollTo(view.contentX, view.contentY + view.height, true); return true
        case Qt.Key_Home: scrollTo(view.contentX, 0, true); return true
        case Qt.Key_End: scrollTo(view.contentX, view.contentHeight, true); return true
        }
        return false
    }

    onEditorChanged: {
        d.placed = false
        place()
    }
    Component.onCompleted: place()

    // Take the keys of the map before the shortcuts of the window (Ctrl+X,
    // Del...), as RPG Maker does in its map view
    Keys.onShortcutOverride: (event) => {
        event.accepted = root.editor !== null && root.editor.handleKey(event.key, event.modifiers, false)
    }
    Keys.onPressed: (event) => {
        if (root.editor && root.editor.handleKey(event.key, event.modifiers, true)) {
            event.accepted = true
            return
        }
        event.accepted = root.scrollKey(event)
    }

    QtObject {
        id: d

        property real zoom: 1
        property bool placed: false
        property bool saving: true
        // in the view: where the zoom keeps the map, (-1, -1) its middle
        property point anchor: Qt.point(-1, -1)
        // the pointer in the view, its buttons, how far past the edges
        property point pointer: Qt.point(-1, -1)
        property int buttons: 0
        property point edge: Qt.point(0, 0)
        // the last press, in the content: where a menu opens
        property point press: Qt.point(0, 0)
        // the last press of the left button, in the view, and whether the
        // pointer went past the drag distance since
        property point pressAt: Qt.point(0, 0)
        property bool dragStarted: false
        // Ctrl + wheel: the turns not zoomed yet (a notch is 120)
        property real wheel: 0
        // the scroll bars, while the map is larger than the view
        // (from a bar of their own: the bars shown change with the view)
        readonly property bool horizontalBar: preview.implicitWidth > root.width
            || (preview.implicitHeight > root.height && preview.implicitWidth > root.width - barSize.implicitWidth)
        readonly property bool verticalBar: preview.implicitHeight > root.height
            || (preview.implicitWidth > root.width && preview.implicitHeight > root.height - barSize.implicitWidth)
    }

    SystemPalette {
        id: colors
    }
    // how thick the scroll bars are
    Controls.ScrollBar {
        id: barSize
        orientation: Qt.Vertical
        policy: Controls.ScrollBar.AlwaysOn
        visible: false
    }

    // the view around a map smaller than it
    Rectangle {
        anchors.fill: parent
        color: colors.base
    }

    Binding {
        target: root.editor
        property: "devicePixelRatio"
        value: Screen.devicePixelRatio
        when: root.editor !== null
    }
    // the part of the map shown, in cells: where a paste goes and where the
    // arrow keys start
    Binding {
        target: root.editor
        property: "viewCells"
        when: root.editor !== null && root.cellSize > 0
        value: Qt.rect((view.contentX - mapItem.x) / root.cellSize, (view.contentY - mapItem.y) / root.cellSize,
                       view.width / root.cellSize, view.height / root.cellSize)
    }

    Connections {
        target: root.editor

        function onScaleChanged() {
            root.applyZoom()
        }
        function onDevicePixelRatioChanged() {
            // snapped again on this screen
            if (root.editor.scale > 0) {
                root.editor.scale = root.editor.scale
            }
        }
        function onRevealRequested(cell) {
            root.reveal(cell.x, cell.y)
        }
        function onContextMenuRequested(kind, cell) {
            // where the click was
            menu.popup(kind, pointer, d.press.x, d.press.y)
        }
        function onNewEventRequested(cell) {
            root.newEventRequested(cell)
        }
        function onEditEventRequested(id) {
            root.editEventRequested(id)
        }
        function onPlaceTeleportRequested(cell) {
            root.placeTeleportRequested(cell)
        }
        function onRunHereRequested(cell) {
            root.runHereRequested(cell)
        }
    }

    Flickable {
        id: view

        anchors.fill: parent
        // the scroll bars beside it, not over the map
        anchors.rightMargin: d.verticalBar ? barSize.implicitWidth : 0
        anchors.bottomMargin: d.horizontalBar ? barSize.implicitWidth : 0
        clip: true
        pixelAligned: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: Math.max(width, preview.implicitWidth)
        contentHeight: Math.max(height, preview.implicitHeight)
        onWidthChanged: root.place()
        onHeightChanged: root.place()
        onContentXChanged: root.saveScroll()
        onContentYChanged: root.saveScroll()

        Controls.ScrollBar.horizontal: Controls.ScrollBar {
            id: horizontalBar
            objectName: "mapScrollX"
            parent: root
            x: 0
            y: root.height - height
            width: view.width
            height: barSize.implicitWidth
            visible: d.horizontalBar
            policy: Controls.ScrollBar.AlwaysOn
        }
        Controls.ScrollBar.vertical: Controls.ScrollBar {
            id: verticalBar
            objectName: "mapScrollY"
            parent: root
            x: root.width - width
            y: 0
            width: barSize.implicitWidth
            height: view.height
            visible: d.verticalBar
            policy: Controls.ScrollBar.AlwaysOn
        }

        Item {
            id: mapItem

            // in the middle of the view when smaller
            x: Math.round((view.contentWidth - width) / 2)
            y: Math.round((view.contentHeight - height) / 2)
            width: preview.implicitWidth
            height: preview.implicitHeight

            MapPreview {
                id: preview
                document: root.document
                zoom: d.zoom
                // the editor snaps its scale already
                snap: false
                editedLayer: root.editedLayer
                // the selection tool shows both layers as they are
                tool: root.editor ? root.editor.state.tool : -1
                // the selected items where the selection tool drags them
                movingEvents: root.editor ? root.editor.selectedEvents : []
                movingStarts: root.editor ? root.editor.selectedStarts : []
                moveOffset: root.editor ? root.editor.itemsOffset : Qt.point(0, 0)
            }

            // The pasted tiles floating over the map until a click outside
            // them puts them down: both layers, the panorama under them
            Item {
                id: floating

                readonly property rect cells: root.editor ? root.editor.floatingRect : Qt.rect(0, 0, 0, 0)

                objectName: "floatingTiles"
                visible: cells.width > 0 && cells.height > 0
                x: cells.x * root.cellSize
                y: cells.y * root.cellSize
                width: cells.width * root.cellSize
                height: cells.height * root.cellSize
                clip: true

                Ez.MapPanoramaView {
                    x: -floating.x
                    y: -floating.y
                    width: implicitWidth
                    height: implicitHeight
                    document: floating.visible ? root.document : null
                    zoom: d.zoom
                }
                Ez.MapLayerView {
                    width: implicitWidth
                    height: implicitHeight
                    chipsetId: root.editor ? root.editor.chipsetId : -1
                    gridTiles: root.editor ? root.editor.floatingLower : []
                    gridColumns: Math.max(1, floating.cells.width)
                    zoom: d.zoom
                }
                Ez.MapLayerView {
                    width: implicitWidth
                    height: implicitHeight
                    chipsetId: root.editor ? root.editor.chipsetId : -1
                    gridTiles: root.editor ? root.editor.floatingUpper : []
                    gridColumns: Math.max(1, floating.cells.width)
                    zoom: d.zoom
                }
            }

            Ez.MapOverlay {
                id: overlay
                objectName: "mapOverlay"
                anchors.fill: parent
                editor: root.editor
                zoom: d.zoom
                cellSize: root.cellSize
                grid: root.editedLayer === Ez.EditorState.EVENT
                area: root.area
            }
        }

        // the mouse over the whole view, in cells (outside the map: none);
        // the cursor is the overlay's, under it
        MouseArea {
            id: pointer

            width: view.contentWidth
            height: view.contentHeight
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            // a drag draws, it does not scroll the view
            preventStealing: true

            onPressed: (mouse) => {
                root.forceActiveFocus(Qt.MouseFocusReason)
                root.track(mouse)
                d.press = Qt.point(mouse.x, mouse.y)
                if (mouse.button === Qt.LeftButton) {
                    d.pressAt = d.pointer
                    d.dragStarted = false
                }
                if (!root.editor) {
                    return
                }
                const cell = root.cellAt(mouse.x, mouse.y)
                // the zoom tool keeps the cell clicked under the pointer
                d.anchor = d.pointer
                root.editor.press(cell.x, cell.y, mouse.button)
                d.anchor = Qt.point(-1, -1)
            }
            onPositionChanged: (mouse) => {
                root.track(mouse)
                if (root.editor) {
                    const cell = root.cellAt(mouse.x, mouse.y)
                    root.editor.move(cell.x, cell.y, mouse.buttons)
                    // a press is a click until the pointer goes past the
                    // platform's drag distance: then what it grabbed moves
                    if (pressed && (mouse.buttons & Qt.LeftButton) && !d.dragStarted
                            && Math.abs(d.pointer.x - d.pressAt.x) + Math.abs(d.pointer.y - d.pressAt.y)
                               >= Qt.styleHints.startDragDistance) {
                        d.dragStarted = true
                        root.editor.startDrag()
                    }
                }
            }
            onReleased: (mouse) => {
                root.track(mouse)
                d.edge = Qt.point(0, 0)
                if (root.editor) {
                    const cell = root.cellAt(mouse.x, mouse.y)
                    root.editor.release(cell.x, cell.y, mouse.button, mouse.buttons)
                }
            }
            // another window took the pointer before the release (the event
            // window opened by a double-click): nothing dragged is dropped
            onCanceled: {
                d.edge = Qt.point(0, 0)
                if (root.editor) {
                    root.editor.cancelPress()
                }
            }
            onDoubleClicked: (mouse) => {
                if (root.editor) {
                    const cell = root.cellAt(mouse.x, mouse.y)
                    root.editor.doubleClick(cell.x, cell.y)
                }
            }
            onExited: {
                // the frame of the tool goes with the pointer
                if (root.editor && !pressed) {
                    root.editor.leave()
                }
            }

            // Ctrl + wheel zooms by the steps of the zoom tool, a step a notch
            WheelHandler {
                id: wheelZoom
                target: null
                acceptedModifiers: Qt.ControlModifier
                onWheel: (event) => {
                    if (!root.editor) {
                        return
                    }
                    d.wheel += event.angleDelta.y
                    const steps = Math.trunc(d.wheel / 120)
                    d.wheel -= steps * 120
                    d.anchor = Qt.point(wheelZoom.point.position.x - view.contentX, wheelZoom.point.position.y - view.contentY)
                    for (let i = 0; i < Math.abs(steps); i++) {
                        root.editor.wheel(steps > 0 ? 120 : -120, event.modifiers)
                    }
                    d.anchor = Qt.point(-1, -1)
                }
            }
            // a pinch too, a step each time the fingers spread by about 1.4
            PinchHandler {
                id: pinchZoom
                target: null
                onActiveChanged: {
                    if (root.editor) {
                        root.editor.resetPinch()
                    }
                }
                onScaleChanged: (delta) => {
                    if (!root.editor) {
                        return
                    }
                    d.anchor = Qt.point(pinchZoom.centroid.position.x - view.contentX, pinchZoom.centroid.position.y - view.contentY)
                    root.editor.pinch(delta)
                    d.anchor = Qt.point(-1, -1)
                }
            }
        }
    }

    // A drag past the edges scrolls the view, faster farther from it; the
    // cell under the pointer changes meanwhile
    Timer {
        id: autoScroll
        interval: 16
        repeat: true
        running: root.editor !== null && root.editor.dragging && pointer.pressed && (d.edge.x !== 0 || d.edge.y !== 0)
        onTriggered: {
            const step = (past) => past === 0 ? 0 : Math.sign(past) * Math.min(48, Math.max(2, Math.abs(past) / 2))
            const x = view.contentX
            const y = view.contentY
            root.scrollTo(x + step(d.edge.x), y + step(d.edge.y), true)
            if (view.contentX === x && view.contentY === y) {
                return
            }
            const cell = root.cellAt(d.pointer.x + view.contentX, d.pointer.y + view.contentY)
            root.editor.move(cell.x, cell.y, d.buttons)
        }
    }

    MapContextMenu {
        id: menu
        editor: root.editor
    }
}
