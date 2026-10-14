// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MainWindow"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The maps open in the main window (MapTabsController): a tab per map, as
 * wide as its name, the temporary one (a map only looked at) in italics, a
 * close button on each; under them the map of the tab shown, edited by its
 * MapEditor in a MapCanvas. A double-click keeps a temporary tab; a tab can
 * be dragged elsewhere.
 */
FocusScope {
    id: root

    required property Ez.Shell shell
    readonly property Ez.MapTabsController tabs: shell ? shell.tabs : null
    readonly property alias tabBar: bar
    /** The view of the map shown, null for none */
    readonly property Ez.MapCanvas currentCanvas: canvases.count > 0 && tabs && tabs.currentIndex >= 0
        ? canvases.itemAt(tabs.currentIndex) as Ez.MapCanvas : null

    /** The view of an open map, null for none */
    function canvasOf(id) {
        for (let i = 0; i < canvases.count; i++) {
            const canvas = canvases.itemAt(i) as MapView
            if (canvas && canvas.mapId === id) {
                return canvas
            }
        }
        return null
    }

    /** The view of an open map (its tab's: the same index) */
    component MapView: Ez.MapCanvas {
        id: canvas

        required property int index
        required property int mapId

        anchors.fill: parent
        visible: root.tabs !== null && index === root.tabs.currentIndex
        enabled: visible
        focus: visible
        editor: root.shell.editorOf(mapId)

        Connections {
            target: root.shell
            function onShowAreaRequested(map, cells) {
                // once a map just opened has its size and its first view
                if (map === canvas.mapId) {
                    Qt.callLater(canvas.showArea, cells)
                }
            }
            function onCenterRequested(map, x, y) {
                if (map === canvas.mapId) {
                    Qt.callLater(canvas.centerOn, x, y)
                }
            }
        }
    }

    /** A tab: the name of its map, italic while temporary, and its close button */
    component MapTab: Controls.TabButton {
        id: tab

        required property int index
        required property int mapId
        required property string title
        required property string toolTip
        required property bool temporary

        text: title
        width: implicitWidth
        font.italic: temporary
        leftPadding: 8
        rightPadding: 4
        topPadding: 3
        bottomPadding: 3
        Controls.ToolTip.text: toolTip
        Controls.ToolTip.visible: hovered && toolTip !== ""
        Controls.ToolTip.delay: 600

        contentItem: RowLayout {
            spacing: 4

            Ez.Label {
                text: tab.text
                font: tab.font
                color: tab.checked ? tab.palette.windowText : tab.palette.buttonText
            }
            Controls.ToolButton {
                objectName: "closeTab"
                // the cross of a close button
                text: "\u00d7"
                padding: 0
                leftPadding: 3
                rightPadding: 3
                focusPolicy: Qt.NoFocus
                onClicked: bar.closeTab(tab.index)
            }
        }

        TapHandler {
            // a double-click keeps a temporary tab
            onDoubleTapped: root.tabs.keepMap(tab.mapId)
        }
        DragHandler {
            id: drag
            target: null
            xAxis.enabled: true
            yAxis.enabled: false
            onActiveChanged: {
                if (active) {
                    return
                }
                // dropped on another tab: it takes its place
                const point = tab.mapToItem(bar, centroid.position.x, tab.height / 2)
                for (let i = 0; i < bar.count; i++) {
                    const other = bar.itemAt(i)
                    const local = other.mapFromItem(bar, point.x, point.y)
                    if (i !== tab.index && local.x >= 0 && local.x < other.width) {
                        bar.moveTab(tab.index, i)
                        return
                    }
                }
            }
        }
    }

    Controls.TabBar {
        id: bar

        // the tabs as text, for the scripts, the temporary ones marked
        // with "~"
        function automationText() {
            const names = []
            for (let i = 0; i < count; i++) {
                const tab = itemAt(i) as MapTab
                names.push((tab && tab.temporary ? "~" : "") + (tab ? tab.text : ""))
            }
            return names.join(" | ")
        }
        /** The close button of a tab (it asks first when it has changes) */
        function closeTab(index) {
            root.tabs.requestClose(root.tabs.mapAt(index))
        }
        /** A tab dragged to another place */
        function moveTab(from, to) {
            root.tabs.moveTab(from, to)
        }

        objectName: "tabMap"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        currentIndex: root.tabs ? root.tabs.currentIndex : -1
        onCurrentIndexChanged: {
            if (root.tabs && currentIndex >= 0 && currentIndex !== root.tabs.currentIndex) {
                root.tabs.setCurrentIndex(currentIndex)
            }
        }

        Repeater {
            model: root.tabs

            delegate: MapTab {}
        }
    }

    Item {
        id: views

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: bar.bottom
        anchors.bottom: parent.bottom

        Repeater {
            id: canvases
            model: root.tabs

            delegate: MapView {}
        }
    }
}
