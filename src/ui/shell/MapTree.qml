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

    /** The ids of the nodes shown expanded */
    function expandedIds() {
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
            next = Math.max(0, row - 1)
            break
        case Qt.Key_Down:
            next = Math.min(view.rows - 1, row + 1)
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
            root.applyTreeExpanded()
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
        target: root.shell
        function onTreeSelectRequested(id) {
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
        Controls.ScrollBar.vertical: Controls.ScrollBar {
            objectName: "treeMapScrollY"
        }
        Controls.ScrollBar.horizontal: Controls.ScrollBar {}

        onExpanded: (row, depth) => {
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

    // the right-click menu, on the press as the menus of macOS: a click with
    // two fingers on a trackpad scrolls a little before its release, which
    // cancels a tap
    TapHandler {
        acceptedButtons: Qt.RightButton
        onPressedChanged: {
            if (pressed) {
                const at = point.pressPosition
                root.showMenu(root.clickAt(at.x, at.y), at.x, at.y)
            }
        }
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
