// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MainWindow"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The map tree of the main window (MapTreeModel), as RPG Maker's: the
 * project, its maps and their areas. A click shows a map in the temporary
 * tab (an area on its map), a double-click keeps it; the arrow keys go
 * through the nodes as clicks would. A map dragged onto another node goes
 * above, below or into it (MapTreeDrop), with what it holds. The
 * right-click menu has the actions of the node (menuTreeMap, menuTreeArea,
 * menuTreeProject) and its scripts (rpgsync); Ctrl+C, Ctrl+V, Ctrl+H and
 * Del are its own keys.
 *
 * The nodes shown expanded are the tree's own (the workspace's), given to
 * the map tree's data when it is written (expandedIds()).
 *
 * filter (the search above it, MapSearchField): only the maps whose names
 * hold the text, or of its number, show, with the nodes they are in,
 * expanded; the others are hidden (rows of no height). The nodes expanded
 * before are put back when the filter is emptied: what the search expands
 * is not the tree's.
 */
FocusScope {
    id: root

    required property Ez.Shell shell
    /** The actions of the window (Actions.qml) */
    required property Actions actions
    readonly property int selectedId: shell ? shell.controller.selectedMap : -1
    readonly property alias view: view
    /** Changes when nodes expand or collapse: the arrows follow */
    property int revision: 0
    /** The ids of the nodes expanded, hidden ones too: {id: true} */
    property var expandedSet: ({})
    // a map dragged: its id, the node under the pointer, and where it goes
    // there (0 above, 1 below, 2 on), -1: none
    property int dragId: -1
    property int dropTarget: -1
    property int dropPosition: -1

    /** The text searched (MapSearchField): only the maps found show */
    property string filter: ""
    readonly property bool filtering: filter.trim() !== ""
    /** While filtering: the maps found, the nodes shown ({id: true}), how
     *  many maps, the map of the number typed (-1: none) */
    property var matchSet: ({})
    property var shownSet: ({})
    property int matchCount: 0
    property int exactMatch: -1
    /** The nodes expanded before the filter, put back after it (null: no filter) */
    property var expandedBeforeFilter: null
    /** The search expands nodes: the tree's data is not told */
    property bool holdExpanded: false
    /** The filter is to be emptied (another project, a node selected that it hides) */
    signal clearFilterRequested()

    onFilterChanged: applyFilter()

    /** Shows only the maps of the filter, or all of them again */
    function applyFilter() {
        const typed = filter.trim().toLowerCase()
        if (typed === "") {
            matchSet = {}
            shownSet = {}
            matchCount = 0
            exactMatch = -1
            if (expandedBeforeFilter !== null) {
                const ids = expandedBeforeFilter
                expandedBeforeFilter = null
                holdExpanded = true
                applyExpanded(ids)
                holdExpanded = false
            }
            view.filterChanged = true
            view.forceLayout()
            if (selectedId > 0) {
                reveal(selectedId)
            }
            return
        }
        if (expandedBeforeFilter === null) {
            expandedBeforeFilter = expandedIds()
        }
        const number = /^\d+$/.test(typed) ? Number(typed) : -1
        const matches = {}
        const shown = { 0: true }
        let count = 0
        let exact = -1
        for (const id of shell.treeIds()) {
            // the maps, not the areas
            if (shell.treeType(id) !== 1) {
                continue
            }
            if (id === number || shell.treeName(id).toLowerCase().includes(typed)) {
                matches[id] = true
                count++
                if (id === number) {
                    exact = id
                }
                for (let index = shell.treeIndex(id); index.valid; index = index.parent) {
                    shown[shell.treeId(index)] = true
                }
            }
        }
        matchSet = matches
        shownSet = shown
        matchCount = count
        exactMatch = exact
        // the nodes holding the maps found expanded
        holdExpanded = true
        applyExpanded(Object.keys(shown).map(Number))
        holdExpanded = false
        view.filterChanged = true
        view.forceLayout()
        const row = rowOf(selectedId)
        if (row >= 0 && shownSet[selectedId]) {
            view.positionViewAtRow(row, TableView.Contain)
        }
    }
    /** A row shown: all of them, or those of the filter */
    function rowShown(row) {
        return !filtering || shownSet[idAt(row)] === true
    }
    /** The row shown delta rows after row (before: negative), the maps found
     *  only (onlyFound), row itself when there is none */
    function stepRow(row, delta, onlyFound) {
        for (let next = row + delta; next >= 0 && next < view.rows; next += delta) {
            if (onlyFound ? matchSet[idAt(next)] === true : rowShown(next)) {
                return next
            }
        }
        return row
    }
    /** As an arrow key on the tree: the node shown after (delta 1) or before
     *  (-1) the one selected, selected and shown; while filtering
     *  (onlyFound), the next map found */
    function step(delta, onlyFound) {
        const row = rowOf(selectedId)
        const next = stepRow(row, delta, onlyFound)
        if (next >= 0 && next !== row) {
            select(idAt(next), false)
            view.positionViewAtRow(next, TableView.Contain)
        }
    }
    /** Return in the search: the map found selected (else the map of the
     *  number typed, else the first one found) in a kept tab */
    function openFound() {
        if (!filtering) {
            if (selectedId > 0) {
                select(selectedId, true)
            }
            return
        }
        let id = matchSet[selectedId] ? selectedId : exactMatch
        if (id < 0) {
            const first = stepRow(-1, 1, true)
            id = first >= 0 ? idAt(first) : -1
        }
        if (id > 0) {
            select(id, true)
            view.positionViewAtRow(rowOf(id), TableView.Contain)
        }
    }

    /** The ids of the nodes shown expanded (before the filter, while it is) */
    function expandedIds() {
        if (expandedBeforeFilter !== null) {
            return expandedBeforeFilter
        }
        const ids = []
        for (const id of shell.treeIds()) {
            if (expandedSet[id]) {
                ids.push(id)
            }
        }
        return ids
    }
    /** Expands these nodes (the others collapse), parents first */
    function applyExpanded(ids) {
        view.collapseRecursively()
        const set = {}
        for (const id of ids) {
            set[id] = true
        }
        expandedSet = set
        for (const id of shell.treeIds()) {
            if (set[id]) {
                const row = rowOf(id)
                if (row >= 0) {
                    view.expand(row)
                }
            }
        }
        revision++
    }
    /** Expanded as the map tree's data says, the project always */
    function applyTreeExpanded() {
        const ids = [0]
        for (const id of shell.treeIds()) {
            if (shell.tree.data(shell.treeIndex(id), Ez.MapTreeModel.ExpandedRole)) {
                ids.push(id)
            }
        }
        applyExpanded(ids)
    }
    /** After the map tree was read again (another program changed it): the
     *  nodes show what they showed, moved ones too; a new node as its data
     *  said (onDataChanged). The rows kept stay, so does the scroll. */
    function keepExpanded() {
        for (const id of shell.treeIds()) {
            const row = rowOf(id)
            const expanded = expandedSet[id] === true
            if (row >= 0 && view.isExpanded(row) !== expanded) {
                expanded ? view.expand(row) : view.collapse(row)
            }
        }
        revision++
    }
    function rowOf(id) {
        return view.rowAtIndex(shell.treeIndex(id))
    }
    function idAt(row) {
        return row < 0 ? -1 : shell.treeId(view.index(row, 0))
    }
    /** Shows a node: its parents expanded, scrolled to it */
    function reveal(id) {
        view.expandToIndex(shell.treeIndex(id))
        // its parents are expanded now
        for (const id2 of shell.treeIds()) {
            const row = rowOf(id2)
            if (row >= 0 && view.isExpanded(row)) {
                expandedSet[id2] = true
            }
        }
        revision++
        const row = rowOf(id)
        if (row >= 0) {
            view.positionViewAtRow(row, TableView.Contain)
        }
    }
    /** As a click on a node: selected, its map shown (keep: a kept tab) */
    function select(id, keep) {
        if (!shell || id < 0) {
            return
        }
        shell.controller.selectedMap = id
        shell.openNode(id, keep)
    }
    /** The lines of the tree, for the scripts, the selected one marked
     *  with ">" */
    function automationText() {
        const lines = []
        for (const id of shell.treeIds()) {
            lines.push((id === selectedId ? "> " : "  ") + shell.treeName(id))
        }
        return lines.join("\n")
    }
    /** What the tree shows, for the scripts (log-tree): the nodes expanded,
     *  the scroll, the first node shown, the node selected */
    function automationState() {
        const shown = []
        for (const id of shell.treeIds()) {
            const row = rowOf(id)
            if (row >= 0 && view.isExpanded(row)) {
                shown.push(id)
            }
        }
        const first = view.cellAtPosition(Qt.point(1, view.contentY + 1), true).y
        // the lines drawn that do not show their node
        let wrong = 0
        for (let row = 0; row < view.rows; row++) {
            const item = view.itemAtCell(Qt.point(0, row))
            if (item && (item.nodeId !== idAt(row) || item.model.name !== shell.treeName(idAt(row)))) {
                wrong++
            }
        }
        return "expanded: " + shown.join(" ") + " | rows: " + view.rows
            + " | contentY: " + Math.round(view.contentY) + " of " + Math.round(Math.max(0, view.contentHeight - view.height))
            + " | first shown: " + (first >= 0 && first < view.rows ? shell.treeName(idAt(first)) : "none")
            + " | selected: " + selectedId + " " + (selectedId >= 0 ? shell.treeName(selectedId) : "")
            + " | visible: " + (root.visible && root.height > 0) + " | lines wrong: " + wrong
            + " | filter: " + (filtering ? matchCount + " found, " + shownRows() + " rows shown" : "none")
    }
    /** The rows drawn with a height (the scripts: those the filter shows) */
    function shownRows() {
        let shown = 0
        for (let row = 0; row < view.rows; row++) {
            if (view.rowHeight(row) > 0) {
                shown++
            }
        }
        return shown
    }
    /** The line of a node by its text, scrolled to (the scripts' click-item) */
    function automationItem(text) {
        let found = -1
        for (const id of shell.treeIds()) {
            const name = shell.treeName(id)
            if (name === text) {
                found = id
                break
            }
            if (found < 0 && name.indexOf(text) >= 0) {
                found = id
            }
        }
        if (found < 0) {
            return null
        }
        reveal(found)
        view.forceLayout()
        return view.itemAtCell(Qt.point(0, rowOf(found)))
    }
    /** The right-click menu of a node */
    function showMenu(id, x, y) {
        const type = shell.treeType(id)
        const name = type === 1 ? "menuTreeMap" : type === 2 ? "menuTreeArea" : "menuTreeProject"
        // removeItem() destroys them (the items of addAction() are the menu's:
        // destroy() refuses them, the menu would not open)
        while (menu.count > 0) {
            menu.removeItem(menu.itemAt(0))
        }
        let items = []
        for (const info of shell.actions.menuList()) {
            if (info.name === name) {
                items = info.items
            }
        }
        for (const item of items) {
            if (item === "") {
                menu.addItem(separatorComponent.createObject(menu.contentItem))
            } else {
                const action = actions.action(item)
                if (action) {
                    menu.addAction(action)
                }
            }
        }
        const scripts = shell.scriptItems(id)
        if (scripts.length > 0) {
            menu.addItem(separatorComponent.createObject(menu.contentItem))
            for (const script of scripts) {
                menu.addItem(scriptComponent.createObject(menu.contentItem, { script: script }))
            }
        }
        menu.popupAt(root, x, y)
    }
    function nodeAt(x, y) {
        const point = view.contentItem.mapFromItem(root, x, y)
        const cell = view.cellAtPosition(point.x, point.y, true)
        return cell.y >= 0 && cell.y < view.rows ? cell.y : -1
    }
    // where a map dragged to a point goes: the node there and above, below or on it
    function updateDrop(x, y) {
        const row = nodeAt(x, y)
        const item = row >= 0 ? view.itemAtCell(Qt.point(0, row)) : null
        if (!item || dragId < 0) {
            dropTarget = -1
            dropPosition = -1
            return
        }
        const local = item.mapFromItem(root, x, y)
        const target = idAt(row)
        const position = local.y < item.height / 4 ? 0 : local.y > item.height * 3 / 4 ? 1 : 2
        const ok = shell.canDrop(dragId, target, position)
        dropTarget = ok ? target : -1
        dropPosition = ok ? position : -1
    }

    objectName: "treeMap"
    activeFocusOnTab: true
    clip: true
    // a project opened before the window: as its tree data says
    Component.onCompleted: {
        if (shell && shell.hasProject) {
            applyTreeExpanded()
        }
    }

    Keys.onPressed: (event) => {
        const row = rowOf(selectedId)
        let next = -1
        switch (event.key) {
        case Qt.Key_Up:
            next = row < 0 ? stepRow(-1, 1, false) : stepRow(row, -1, false)
            break
        case Qt.Key_Down:
            next = stepRow(row, 1, false)
            break
        case Qt.Key_Right:
            if (row >= 0 && !view.isExpanded(row)) {
                view.expand(row)
            }
            event.accepted = true
            return
        case Qt.Key_Left:
            if (row >= 0 && view.isExpanded(row)) {
                view.collapse(row)
            } else if (row >= 0) {
                next = view.rowAtIndex(view.index(row, 0).parent)
            }
            break
        case Qt.Key_Return:
        case Qt.Key_Enter:
            select(selectedId, true)
            event.accepted = true
            return
        default:
            return
        }
        event.accepted = true
        if (next >= 0 && next !== row) {
            select(idAt(next), false)
            view.positionViewAtRow(next, TableView.Contain)
        }
    }

    Connections {
        target: root.shell ? root.shell.tree : null
        function onModelReset() {
            // another tree (another project): the search ends
            root.expandedBeforeFilter = null
            root.applyTreeExpanded()
            if (root.filtering) {
                root.clearFilterRequested()
            }
        }
        function onDataChanged(topLeft, bottomRight, roles) {
            // the expanded nodes are the tree's: only an explicit change shows
            if (roles.indexOf(Ez.MapTreeModel.ExpandedRole) < 0) {
                return
            }
            for (let row = topLeft.row; row <= bottomRight.row; row++) {
                const index = root.shell.tree.index(row, 0, topLeft.parent)
                const shown = root.view.rowAtIndex(index)
                const expanded = root.shell.tree.data(index, Ez.MapTreeModel.ExpandedRole)
                root.expandedSet[root.shell.treeId(index)] = expanded
                if (shown >= 0 && root.view.isExpanded(shown) !== expanded) {
                    expanded ? root.view.expand(shown) : root.view.collapse(shown)
                }
            }
            root.revision++
        }
    }
    Connections {
        target: root.shell ? root.shell.controller : null
        function onTreeMapReloaded() {
            root.keepExpanded()
            // the maps found again (renamed, moved)
            if (root.filtering) {
                root.applyFilter()
            }
        }
    }
    Connections {
        target: root.shell
        function onTreeSelectRequested(id) {
            // a node the filter hides (Open Start Map, a tab): all show again
            if (root.filtering && !root.shownSet[id]) {
                root.clearFilterRequested()
            }
            root.reveal(id)
        }
    }

    // its own keys, while it has the focus
    Repeater {
        model: ["actionMapCopy", "actionMapPaste", "actionMapShift", "actionMapDelete", "actionAreaDelete"]
        delegate: Item {
            id: key
            required property string modelData
            readonly property var action: root.actions.action(key.modelData)
            Shortcut {
                sequences: key.action ? key.action.keys : []
                enabled: root.activeFocus && root.shell.windowFocused && key.action !== null && key.action.enabled
                onActivated: key.action.trigger()
            }
        }
    }

    // the background of a list, light or dark
    Rectangle {
        anchors.fill: parent
        color: view.palette.base
    }

    TreeView {
        id: view

        anchors.fill: parent
        model: root.shell ? root.shell.tree : null
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        // a drag moves maps, the wheel and the scroll bar scroll
        acceptedButtons: Qt.NoButton
        pointerNavigationEnabled: false
        keyNavigationEnabled: false
        // the rows the filter hides: no height; laid out again once the
        // rows of the nodes it expanded or collapsed are made
        rowHeightProvider: row => root.rowShown(row) ? -1 : 0
        /** The filter changed: laid out again once the rows are made (they
         *  keep the heights they had otherwise) */
        property bool filterChanged: false
        onLayoutChanged: {
            if (filterChanged) {
                filterChanged = false
                forceLayout()
            }
        }
        Controls.ScrollBar.vertical: Controls.ScrollBar {
            objectName: "treeMapScrollY"
        }
        Controls.ScrollBar.horizontal: Controls.ScrollBar {}

        onExpanded: (row, depth) => {
            // the search's own: not the tree's
            if (root.holdExpanded || root.filtering) {
                root.revision++
                return
            }
            const id = root.idAt(row)
            root.expandedSet[id] = true
            root.shell.setExpanded(id, true)
            // its children expanded before it collapsed show so again
            for (const child of root.shell.treeIds()) {
                const shown = root.rowOf(child)
                if (shown >= 0 && root.expandedSet[child] && !view.isExpanded(shown)) {
                    view.expand(shown)
                }
            }
            root.revision++
        }
        onCollapsed: (row, recursively) => {
            if (root.holdExpanded || root.filtering) {
                root.revision++
                return
            }
            const id = root.idAt(row)
            root.expandedSet[id] = false
            root.shell.setExpanded(id, false)
            root.revision++
        }

        delegate: Item {
            id: node

            required property TreeView treeView
            required property int row
            required property int depth
            required property bool hasChildren
            required property var model
            readonly property int nodeId: model.mapId
            // revision >= 0: read again when the tree changes
            readonly property bool open: root.revision >= 0 && treeView.isExpanded(row)
            readonly property bool selected: nodeId === root.selectedId
            readonly property int indent: 14

            implicitWidth: Math.max(root.width, label.x + label.implicitWidth + 6)
            implicitHeight: label.implicitHeight + 2 * Ez.Metrics.listRowPadding

            Rectangle {
                anchors.fill: parent
                color: node.selected ? node.treeView.palette.highlight : "transparent"
                border.width: root.dropTarget === node.nodeId && root.dropPosition === 2 ? 1 : 0
                border.color: node.treeView.palette.highlight
            }
            // where a map dragged goes: a line above or below the node
            Rectangle {
                visible: root.dropTarget === node.nodeId && root.dropPosition !== 2
                x: label.x
                width: parent.width - x
                height: 2
                y: root.dropPosition === 0 ? 0 : parent.height - height
                color: node.treeView.palette.highlight
            }
            Ez.Label {
                id: arrow
                x: node.depth * node.indent
                width: node.indent
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignHCenter
                visible: node.hasChildren
                text: node.open ? "▾" : "▸"
                color: node.selected ? node.treeView.palette.highlightedText : node.treeView.palette.text
                TapHandler {
                    onTapped: node.treeView.toggleExpanded(node.row)
                }
            }
            Ez.EditorIcon {
                id: icon
                x: arrow.x + arrow.width
                anchors.verticalCenter: parent.verticalCenter
                name: node.model.mapType === 0 ? "folder" : node.model.mapType === 2 ? "select" : ""
                width: name === "" ? 0 : 16
            }
            Ez.Label {
                id: label
                x: icon.x + icon.width + (icon.width > 0 ? 4 : 0)
                anchors.verticalCenter: parent.verticalCenter
                text: node.model.name
                color: node.selected ? node.treeView.palette.highlightedText : node.treeView.palette.text
            }
        }
    }

    /** As a click on the node at x, y (beside the maps: the project); its id */
    function clickAt(x, y) {
        forceActiveFocus()
        const row = nodeAt(x, y)
        const id = row >= 0 ? idAt(row) : 0
        select(id, false)
        return id
    }

    // the right-click menu, on the press (the node pressed selected first)
    Ez.ContextMenuHandler {
        onRequested: (x, y) => root.showMenu(root.clickAt(x, y), x, y)
    }
    // the mouse on the nodes: a click, a double-click
    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        onTapped: (eventPoint, button) => root.clickAt(eventPoint.position.x, eventPoint.position.y)
        onDoubleTapped: (eventPoint, button) => {
            const row = root.nodeAt(eventPoint.position.x, eventPoint.position.y)
            if (button === Qt.LeftButton && row >= 0) {
                root.select(root.idAt(row), true)
            }
        }
    }
    DragHandler {
        id: drag
        target: null
        acceptedButtons: Qt.LeftButton
        onActiveChanged: {
            if (active) {
                const row = root.nodeAt(centroid.pressPosition.x, centroid.pressPosition.y)
                const id = root.idAt(row)
                // only maps move
                root.dragId = id > 0 && root.shell.treeType(id) === 1 ? id : -1
                return
            }
            if (root.dragId >= 0 && root.dropTarget >= 0) {
                root.shell.dropMap(root.dragId, root.dropTarget, root.dropPosition)
            }
            root.dragId = -1
            root.dropTarget = -1
            root.dropPosition = -1
        }
        onCentroidChanged: {
            if (active) {
                root.updateDrop(centroid.position.x, centroid.position.y)
            }
        }
    }

    Ez.ContextMenu {
        id: menu
        objectName: "treeMapMenu"
    }
    Component {
        id: separatorComponent
        Controls.MenuSeparator {}
    }
    Component {
        id: scriptComponent
        Controls.MenuItem {
            required property var script
            text: script.text
            enabled: script.enabled
            icon.source: root.actions.iconFolder + "code"
            Controls.ToolTip.text: script.toolTip
            Controls.ToolTip.visible: hovered && script.toolTip !== ""
            Controls.ToolTip.delay: 600
            onTriggered: root.shell.openScript(script.location)
        }
    }
}
