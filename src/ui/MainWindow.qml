// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MainWindow"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
// KDDockWidgets registers its types from C++, with no qmldir: qmllint cannot
// know them
// qmllint disable import
import com.kdab.dockwidgets as KDDW
// qmllint enable import
import org.easyrpg.editor as Ez

/**
 * The main window of the QML interface (its C++ side: QmlShell), as RPG
 * Maker's: the menu bar (native on macOS), the toolbar, the map tabs in the
 * middle, the palette and the map tree in docks on the left (KDDockWidgets:
 * they can be moved, floated, and are kept with the workspace), and the
 * status bar. Every action is the catalog's (Actions.qml), by its name.
 */
Kirigami.ApplicationWindow {
    id: root

    property Ez.Shell shell
    /** The menu bar of macOS (NativeMenuBar), else one in the window */
    readonly property bool nativeMenuBar: Qt.platform.pluginName === "cocoa"
    readonly property alias actions: windowActions
    readonly property alias mapTabs: mapTabs
    readonly property alias mapTree: mapTree
    readonly property alias tilePalette: tilePalette
    /** The native menu bar (NativeMenuBar), null without one */
    property NativeMenuBar nativeMenus: null

    /** What only the window knows of the workspace (QmlShell::viewState) */
    function viewState() {
        return {
            expanded: mapTree.expandedIds(),
            treeScroll: Math.round(mapTree.view.contentY),
            paletteX: Math.round(tilePalette.view.contentX),
            paletteY: Math.round(tilePalette.view.contentY)
        }
    }
    function dockOf(name) {
        return name === "dockPalette" ? dockPalette : name === "dockMapTree" ? dockMapTree : null
    }
    function isDockOpen(name) {
        const dock = dockOf(name)
        return dock !== null && dock.isOpen
    }
    /** For the scripts: "docked", "floating" (in a window of its own) or "closed", "" for no such dock */
    function dockState(name) {
        const dock = dockOf(name)
        return dock === null ? "" : !dock.isOpen ? "closed" : dock.isFloating ? "floating" : "docked"
    }
    /** A dock in a window of its own, or back in the main window, as a double click on its title bar */
    function setDockFloating(name, floating) {
        const dock = dockOf(name)
        if (dock === null) {
            return false
        }
        dock.isFloating = floating
        return true
    }
    /** The map tree takes the keys (a click on it) */
    function focusTree() {
        mapTree.forceActiveFocus()
    }
    function showDock(dock, open) {
        if (open && !dock.isOpen) {
            dock.open()
        } else if (!open && dock.isOpen) {
            dock.forceClose()
        }
    }

    objectName: "mainWindow"
    title: shell ? shell.controller.windowTitle : ""
    width: 1024
    height: 640
    minimumWidth: 400
    minimumHeight: 300
    font: Ez.Metrics.font
    pageStack.visible: false
    pageStack.enabled: false

    onClosing: (close) => {
        close.accepted = root.shell.requestClose()
    }
    Component.onCompleted: {
        if (nativeMenuBar) {
            nativeMenus = nativeMenuComponent.createObject(root, { actions: windowActions }) as NativeMenuBar
        }
    }

    /**
     * For the scripts, the native menu bar of macOS being neither drawn nor
     * shown offscreen (one is made for them there): where an action is in
     * both menu bars. native: the native menu bar has an item of it with
     * that role ("quit", "about", "preferences": macOS moves it to the
     * application menu; "application": a copy there; "": none); menu: the
     * menu of the window's menu bar holding it (macOS: of the native one,
     * where an item with a role stays in Qt's menus), "" for none; shown:
     * the platform shows the native menu bar (not offscreen).
     */
    function menuPlaces(name, role) {
        if (!nativeMenus) {
            nativeMenus = nativeMenuComponent.createObject(root, { actions: windowActions }) as NativeMenuBar
        }
        nativeMenus.build()
        // the window's menu bar, else (macOS) the native one's menus
        const inWindow = menuBarLoader.item ? (menuBarLoader.item as MainMenuBar).entry(name) : nativeMenus.entry(name)
        return { native: nativeMenus.entry(name, role) !== null, menu: inWindow ? inWindow.menu : "",
            shown: nativeMenus.shown }
    }
    /** The item of the native menu bar chosen (menuPlaces); false for none */
    function triggerNativeItem(name, role) {
        const found = nativeMenus ? nativeMenus.entry(name, role) : null
        if (found) {
            found.item.triggered()
        }
        return found !== null
    }

    Connections {
        target: root.shell
        function onDockRequested(dock, open) {
            root.showDock(dock === "dockPalette" ? dockPalette : dockMapTree, open)
        }
        function onViewRestoreRequested(view) {
            if (view.expanded !== undefined) {
                mapTree.applyExpanded(view.expanded)
            }
            mapTree.reveal(view.selected)
            // after the layout: the views have their sizes
            Qt.callLater(() => {
                mapTree.view.contentY = Math.max(0, Math.min(view.treeScroll, mapTree.view.contentHeight - mapTree.view.height))
                tilePalette.view.contentX = Math.max(0, view.paletteX)
                tilePalette.view.contentY = Math.max(0, Math.min(view.paletteY, tilePalette.view.contentHeight - tilePalette.view.height))
            })
        }
        function onViewResetRequested() {
            mapTree.applyExpanded([0])
            mapTree.view.contentY = 0
            tilePalette.view.contentX = 0
            tilePalette.view.contentY = 0
        }
    }

    SystemPalette {
        id: colors
    }

    Actions {
        id: windowActions
        shell: root.shell
        iconFolder: colors.window.hslLightness >= 0.5 ? "qrc:/bright/" : "qrc:/dark/"
    }

    Component {
        id: nativeMenuComponent
        NativeMenuBar {}
    }

    menuBar: Loader {
        id: menuBarLoader
        active: !root.nativeMenuBar
        visible: active
        sourceComponent: MainMenuBar {
            actions: windowActions
        }
    }
    header: MainToolBar {
        actions: windowActions
        toolbarName: "mainToolBar"
        // a newer Editor (Ez.Updater): at the right edge until it is installed
        trailing: Component {
            Ez.Button {
                id: updateButton

                readonly property string tip: Ez.Updater.ready
                    ? qsTr("Restart the Editor to install version %1").arg(Ez.Updater.latestVersion)
                    : Ez.Updater.busy ? qsTr("Downloading version %1").arg(Ez.Updater.latestVersion)
                    : Ez.Updater.installsInPlace ? qsTr("Install version %1").arg(Ez.Updater.latestVersion)
                    : qsTr("Download version %1").arg(Ez.Updater.latestVersion)

                objectName: "updateButton"
                visible: Ez.Updater.updateAvailable
                accent: true
                text: Ez.Updater.ready ? qsTr("Restart to Update")
                    : !Ez.Updater.busy ? qsTr("Update")
                    : Ez.Updater.progress >= 0 ? qsTr("Updating %1%").arg(Ez.Updater.progress)
                    : qsTr("Updating...")
                // as high as the buttons of the toolbar
                implicitHeight: Ez.Metrics.toolIconSize + 2 * Ez.Metrics.toolButtonPadding
                verticalPadding: 0
                focusPolicy: Qt.NoFocus
                Controls.ToolTip.text: updateButton.tip
                Controls.ToolTip.visible: updateButton.hovered
                Controls.ToolTip.delay: 600
                onHoveredChanged: windowActions.hint = updateButton.hovered ? updateButton.tip : ""
                // nothing while it downloads
                onClicked: Ez.Updater.activate()
            }
        }
    }
    footer: StatusBar {
        shell: root.shell
        hint: windowActions.hint
        cellText: mapTabs.currentCanvas ? mapTabs.currentCanvas.hoverText : ""
    }

    // qmllint disable import unqualified unresolved-type
    KDDW.DockingArea {
        id: dockingArea

        anchors.fill: parent
        uniqueName: "mainLayout"
        options: KDDW.KDDockWidgets.MainWindowOption_HasCentralWidget
        persistentCentralItemFileName: ":/qt/qml/org/easyrpg/editor/CentralArea.qml"

        KDDW.DockWidget {
            id: dockPalette
            uniqueName: "dockPalette"
            title: windowActions.action("actionPaletteToggle")?.text ?? ""
            options: KDDW.KDDockWidgets.DockWidgetOption_NotClosable

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                MainToolBar {
                    id: paletteToolBar
                    actions: windowActions
                    toolbarName: "toolBar"
                    Layout.fillWidth: true
                }
                // Ez.: QtQuick has a Palette too (the colors of the controls)
                Ez.Palette {
                    id: tilePalette
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    editorState: root.shell ? root.shell.editorState : null
                    chipsetId: root.shell && root.shell.currentEditor ? root.shell.currentEditor.chipsetId : -1
                }
            }
        }
        KDDW.DockWidget {
            id: dockMapTree
            uniqueName: "dockMapTree"
            title: windowActions.action("actionMapTreeToggle")?.text ?? ""
            options: KDDW.KDDockWidgets.DockWidgetOption_NotClosable

            MapTree {
                id: mapTree
                anchors.fill: parent
                shell: root.shell
                actions: windowActions
            }
        }

        Component.onCompleted: {
            // the palette above the map tree, on the left,
            // as wide as the tiles or its toolbar (its buttons all shown)
            const width = Math.max(tilePalette.implicitWidth, paletteToolBar.fullWidth()) + 2
            addDockWidget(dockPalette, KDDW.KDDockWidgets.Location_OnLeft, null, Qt.size(width, 360))
            addDockWidget(dockMapTree, KDDW.KDDockWidgets.Location_OnBottom, dockPalette, Qt.size(width, 200))
            // the map tabs in the central area KDDockWidgets made
            const queue = [dockingArea]
            while (queue.length > 0) {
                const item = queue.shift()
                if (item.objectName === "centralArea") {
                    mapTabs.parent = item
                    break
                }
                for (const child of item.children) {
                    queue.push(child)
                }
            }
        }
    }
    // qmllint enable import unqualified unresolved-type

    MapTabs {
        id: mapTabs
        anchors.fill: parent
        shell: root.shell
        // the keys go to the map shown
        focus: true
    }
}
