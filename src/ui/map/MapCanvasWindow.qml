// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MainWindow"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The Qt Quick map canvas on a map of the project with its palette, in a
 * window of its own apart from the main window's tabs (MapCanvasHost, the
 * automation's qml-map). Its tool bar has the layers,
 * the tools, undo and the zoom (actions named as the main window's); its
 * status bar the cell under the pointer, the zoom and the last request of
 * the editor (a new event...), which only the main window answers.
 */
Controls.ApplicationWindow {
    id: root

    /** The editing of the map (MapCanvasHost makes it) */
    property Ez.MapEditor editor
    readonly property Ez.EditorState editorState: editor ? editor.state : null
    readonly property alias canvas: canvas
    /** The Editor's icons (resources/icons) for the colors shown, light or dark */
    readonly property string icons: colors.window.hslLightness >= 0.5 ? "qrc:/bright/" : "qrc:/dark/"
    readonly property alias tilePalette: tilePalette
    /** The last request of the editor, for the scripts (not translated: a test window) */
    property string lastRequest

    function showLayer() {
        if (!editorState) {
            return
        }
        const actions = [layerLower, layerUpper, layerEvents]
        actions[editorState.layer].checked = true
    }
    function showTool() {
        if (!editorState) {
            return
        }
        const actions = [toolZoom, toolPen, toolRectangle, toolCircle, toolFill, toolSelect]
        actions[editorState.tool].checked = true
    }

    objectName: "mapCanvasWindow"
    title: editor ? editor.mapName : ""
    width: 1000
    height: 700
    font: Ez.Metrics.font

    onEditorStateChanged: {
        showLayer()
        showTool()
    }
    // its map closed (the project, the tab): the window goes too
    onEditorChanged: {
        if (!editor) {
            close()
        }
    }
    Component.onCompleted: {
        showLayer()
        showTool()
        canvas.forceActiveFocus()
    }

    Connections {
        target: root.editorState
        function onLayerChanged() {
            root.showLayer()
        }
        function onToolChanged() {
            root.showTool()
        }
    }

    SystemPalette {
        id: colors
    }
    Controls.ActionGroup {
        id: layers
    }
    Controls.ActionGroup {
        id: tools
    }

    Controls.Action {
        id: layerLower
        objectName: "actionLayerLower"
        text: qsTr("&Lower Layer")
        icon.source: root.icons + "layer-low"
        checkable: true
        Controls.ActionGroup.group: layers
        onTriggered: root.editorState.layer = Ez.EditorState.LOWER
    }
    Controls.Action {
        id: layerUpper
        objectName: "actionLayerUpper"
        text: qsTr("&Upper Layer")
        icon.source: root.icons + "layer-high"
        checkable: true
        Controls.ActionGroup.group: layers
        onTriggered: root.editorState.layer = Ez.EditorState.UPPER
    }
    Controls.Action {
        id: layerEvents
        objectName: "actionLayerEvents"
        text: qsTr("&Events")
        icon.source: root.icons + "layer-event"
        checkable: true
        Controls.ActionGroup.group: layers
        onTriggered: root.editorState.layer = Ez.EditorState.EVENT
    }
    Controls.Action {
        id: toolSelect
        objectName: "actionDrawSelect"
        text: qsTr("Select")
        icon.source: root.icons + "select-area"
        checkable: true
        Controls.ActionGroup.group: tools
        onTriggered: root.editorState.tool = Ez.EditorState.SELECT
    }
    Controls.Action {
        id: toolZoom
        objectName: "actionZoom"
        text: qsTr("Zoom")
        icon.source: root.icons + "zoom"
        checkable: true
        Controls.ActionGroup.group: tools
        onTriggered: root.editorState.tool = Ez.EditorState.ZOOM
    }
    Controls.Action {
        id: toolPen
        objectName: "actionDrawPen"
        text: qsTr("Draw")
        icon.source: root.icons + "draw-pen"
        checkable: true
        Controls.ActionGroup.group: tools
        onTriggered: root.editorState.tool = Ez.EditorState.PENCIL
    }
    Controls.Action {
        id: toolRectangle
        objectName: "actionDrawRectangle"
        text: qsTr("Rectangle")
        icon.source: root.icons + "draw-rectangle"
        checkable: true
        Controls.ActionGroup.group: tools
        onTriggered: root.editorState.tool = Ez.EditorState.RECTANGLE
    }
    Controls.Action {
        id: toolCircle
        objectName: "actionDrawCircle"
        text: qsTr("Circle")
        icon.source: root.icons + "draw-circle"
        checkable: true
        Controls.ActionGroup.group: tools
        onTriggered: root.editorState.tool = Ez.EditorState.CIRCLE
    }
    Controls.Action {
        id: toolFill
        objectName: "actionDrawFill"
        text: qsTr("Fill")
        icon.source: root.icons + "draw-fill"
        checkable: true
        Controls.ActionGroup.group: tools
        onTriggered: root.editorState.tool = Ez.EditorState.FILL
    }
    Controls.Action {
        id: undo
        objectName: "actionUndo"
        text: qsTr("Undo")
        icon.source: root.icons + "edit-undo"
        shortcut: StandardKey.Undo
        enabled: root.editor !== null && root.editor.canUndo
        onTriggered: root.editor.undo()
    }
    Controls.Action {
        id: redo
        objectName: "actionRedo"
        text: qsTr("Redo")
        icon.source: root.icons + "edit-redo"
        shortcut: StandardKey.Redo
        enabled: root.editor !== null && root.editor.canRedo
        onTriggered: root.editor.redo()
    }
    Controls.Action {
        id: zoomIn
        objectName: "actionZoomIn"
        text: qsTr("Zoom &In")
        icon.source: root.icons + "zoom-in"
        shortcut: StandardKey.ZoomIn
        enabled: root.editor !== null && root.editor.scale < 8
        onTriggered: root.editor.zoomIn()
    }
    Controls.Action {
        id: zoomOut
        objectName: "actionZoomOut"
        text: qsTr("Zoom &Out")
        icon.source: root.icons + "zoom-out"
        shortcut: StandardKey.ZoomOut
        enabled: root.editor !== null && root.editor.scale > 0.125
        onTriggered: root.editor.zoomOut()
    }

    // a button of the tool bar: the icon of its action, its text as a tip
    component ToolButton: Controls.ToolButton {
        display: Controls.AbstractButton.IconOnly
        icon.width: Ez.Metrics.toolIconSize
        icon.height: Ez.Metrics.toolIconSize
        padding: Ez.Metrics.toolButtonPadding
        Controls.ToolTip.text: text.replace("&", "")
        Controls.ToolTip.visible: hovered
        Controls.ToolTip.delay: 600
    }

    header: Controls.ToolBar {
        RowLayout {
            spacing: 0

            ToolButton { action: layerLower }
            ToolButton { action: layerUpper }
            ToolButton { action: layerEvents }
            Controls.ToolSeparator {}
            ToolButton { action: undo }
            ToolButton { action: redo }
            Controls.ToolSeparator {}
            ToolButton { action: toolSelect }
            ToolButton { action: toolZoom }
            ToolButton { action: toolPen }
            ToolButton { action: toolRectangle }
            ToolButton { action: toolCircle }
            ToolButton { action: toolFill }
            Controls.ToolSeparator {}
            ToolButton { action: zoomIn }
            ToolButton { action: zoomOut }
        }
    }

    Controls.SplitView {
        anchors.fill: parent

        // Ez.: QtQuick has a Palette too (the colors of the controls)
        Ez.Palette {
            id: tilePalette
            // its 6 columns and its scroll bar, as the dock of the palette
            Controls.SplitView.preferredWidth: implicitWidth
            Controls.SplitView.minimumWidth: 64
            editorState: root.editorState
            chipsetId: root.editor ? root.editor.chipsetId : -1
        }

        MapCanvas {
            id: canvas
            Controls.SplitView.fillWidth: true
            editor: root.editor
            focus: true

            onNewEventRequested: (cell) => root.lastRequest = "new event " + cell.x + "," + cell.y
            onEditEventRequested: (id) => root.lastRequest = "edit event " + id
            onPlaceTeleportRequested: (cell) => root.lastRequest = "place teleport " + cell.x + "," + cell.y
            onRunHereRequested: (cell) => root.lastRequest = "start game " + cell.x + "," + cell.y
        }
    }

    footer: Controls.ToolBar {
        position: Controls.ToolBar.Footer

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: 6

            Ez.Label {
                objectName: "labelRequest"
                text: root.lastRequest
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Ez.Label {
                objectName: "labelZoom"
                text: "x" + canvas.zoom
            }
            Ez.Label {
                objectName: "labelCell"
                text: canvas.hoverText
            }
        }
    }
}
