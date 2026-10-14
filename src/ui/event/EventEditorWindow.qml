// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EventDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQuick.Window
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The event window of a map event (RPG Maker's event editor): its name, its
 * pages in tabs (New, Copy, Paste, Delete and Clear Page), each page's
 * settings and commands (EventPageEditor), and OK, Cancel, Apply. Over an
 * EventEditorController, which the map made (EventWindow::exec): Apply
 * puts the event on the map at once, OK when it closes.
 *
 * As in RPG Maker 2003, Ctrl+Tab switches pages, and PgUp / PgDn move in
 * the commands from anywhere in the window (away from them, the first
 * press only goes back to them).
 *
 * The event changed by another program while it is open is shown again,
 * asking first when it was changed here; removed, the window closes.
 */
Ez.DialogWindow {
    id: root

    required property Ez.EventEditorController controller
    /** The chipset of the map: the tiles a page can show */
    property int chipsetId: 0
    /** What the window showed last time: {page, views: [a page's line and scroll]} */
    property var viewState: ({})
    /** A line of the page shown to select first (from 0), -1: as last time */
    property int selectLine: -1
    /** The width of the settings of the pages, the splitter's */
    property real settingsWidth: 0
    /** The size the window had last time */
    property size savedSize: Qt.size(0, 0)

    /** The view of each page: its line and scroll */
    property var pageViews: []
    /** The page to show is known (Component.onCompleted): the pages load
     *  from then on, not while the tabs are made (they pass by every page) */
    property bool pagesReady: false
    /** The page shown (its editor), null while it loads */
    readonly property var currentEditor: pageRepeater.count > tabs.currentIndex && tabs.currentIndex >= 0
                                         ? (pageRepeater.itemAt(tabs.currentIndex) as Loader)?.item ?? null : null

    /** What it shows, kept by EventWindow::exec for the next time */
    function saveViewState() {
        captureView()
        return { page: controller.currentPage, views: pageViews }
    }
    function captureView() {
        const page = controller.currentPage
        if (currentEditor && page >= 0) {
            const views = pageViews.slice()
            views[page] = currentEditor.commandList.viewState()
            pageViews = views
        }
    }
    /** The previous (-1) or next (1) page */
    function switchPage(step) {
        if (controller.pageCount > 0) {
            captureView()
            controller.switchPage(step)
        }
    }
    /** PgUp, PgDn: in the commands, which they go back to first */
    function pageKey(up) {
        const commands = currentEditor ? currentEditor.commandList : null
        if (!commands) {
            return
        }
        if (commands.view.activeFocus) {
            commands.pageStep(up)
        }
        commands.view.forceActiveFocus()
    }
    // the pages change: the views follow them
    function newPage() {
        captureView()
        const index = controller.newPage()
        if (index >= 0) {
            const views = pageViews.slice()
            views.splice(index, 0, {})
            pageViews = views
        }
    }
    function pastePage() {
        captureView()
        const index = controller.pastePage()
        if (index >= 0) {
            const views = pageViews.slice()
            views.splice(index, 0, {})
            pageViews = views
        }
    }
    function deletePage() {
        const shown = controller.currentPage
        // the page that takes its place, as RPG Maker (the last one at the end)
        if (controller.deletePage() >= 0) {
            const views = pageViews.slice()
            views.splice(shown, 1)
            pageViews = views
        }
    }
    function clearPage() {
        const index = controller.clearPage()
        if (index >= 0) {
            const views = pageViews.slice()
            views[index] = {}
            pageViews = views
        }
    }

    objectName: "EventDialog"
    // its controller holds the lock of its map while it is open
    // (EditLock::Event): the maps can still be read again meanwhile
    ownEditLock: true
    title: controller.title
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowMinMaxButtonsHint
    standardButtons: Controls.DialogButtonBox.Ok | Controls.DialogButtonBox.Cancel
                     | Controls.DialogButtonBox.Apply | Controls.DialogButtonBox.Help
    // the size of last time, else 980x640, on the screen
    preferredWidth: Math.min(savedSize.width > 0 ? savedSize.width : 980, Screen.desktopAvailableWidth * 0.95)
    height: Math.max(minimumHeight, Math.min(savedSize.height > 0 ? savedSize.height : 640, Screen.desktopAvailableHeight * 0.95))
    buttons: [
        Ez.Button {
            // the script of the page, for games edited with rpgsync
            readonly property string reason: root.controller.mapId > 0 ? root.currentEditor?.commandEditor.scriptUnavailableReason() ?? "" : ""
            objectName: "openScript"
            text: qsTr("Open Script")
            visible: root.controller.mapId > 0
            // greyed without rpgsync, the tooltip says how to set it up
            enabled: reason === ""
            Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.ActionRole
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
            Controls.ToolTip.text: reason !== "" ? reason : qsTr("Open the script of this page at the selected command (Ctrl+Shift+O in the list)")
            onClicked: root.currentEditor?.commandList.openScript()
        }
    ]

    // OK: on the map when the event changed (a new event even unchanged), as RPG Maker
    onAccepting: root.controller.ok()
    Connections {
        target: root.buttonBox
        function onApplied() {
            // on the map now, like RPG Maker; later changes compare to it
            root.controller.apply()
        }
        function onHelpRequested() {
            Ez.Editor.help("event-editor", root.contentItem)
        }
    }

    Component.onCompleted: {
        pageViews = viewState.views ?? []
        const page = Number(viewState.page ?? 0)
        controller.currentPage = Math.max(0, Math.min(page, controller.pageCount - 1))
        pagesReady = true
        Qt.callLater(() => root.lineName.forceActiveFocus())
    }
    readonly property alias lineName: name

    // Another program changed the event while it is open
    Connections {
        target: root.controller
        function onReloadPendingChanged() {
            if (root.controller.reloadPending) {
                root.showMessage("reload", reloadQuestion)
            }
        }
        function onRemoved() {
            root.showMessage("removed", removedMessage)
        }
    }
    // the messages are made the first time they are shown (rarely)
    property var messages: ({})
    function showMessage(name, component) {
        if (!messages[name]) {
            messages[name] = component.createObject(root.contentItem)
        }
        messages[name].open()
    }
    Component {
        id: reloadQuestion
        Ez.MessageDialog {
            title: qsTr("Event changed on disk")
            text: qsTr("This event was changed by another program.\nShow the new version and discard your changes?")
            standardButtons: Controls.Dialog.Yes | Controls.Dialog.No
            onAccepted: root.controller.acceptReload()
            onRejected: root.controller.rejectReload()
        }
    }
    Component {
        id: removedMessage
        Ez.MessageDialog {
            title: qsTr("Event removed")
            text: qsTr("This event was removed from the map by another program.")
            onClosed: root.reject()
        }
    }

    // Ctrl+Tab switches pages, PgUp / PgDn move in the commands
    Shortcut {
        sequence: "PgUp"
        onActivated: root.pageKey(true)
    }
    Shortcut {
        sequence: "PgDown"
        onActivated: root.pageKey(false)
    }
    // Ctrl+Shift+Tab as well as Ctrl+Shift+Backtab (a Shift+Tab typed)
    Shortcut {
        // Qt's Ctrl is Command on macOS (Command+Tab switches applications):
        // the Control key of RPG Maker's Ctrl+Tab is Meta there
        sequences: Qt.platform.os === "osx" ? [StandardKey.PreviousChild, "Ctrl+Shift+Tab", "Meta+Shift+Tab", "Meta+Shift+Backtab"]
                                            : [StandardKey.PreviousChild, "Ctrl+Shift+Tab"]
        onActivated: root.switchPage(-1)
        // a platform whose standard keys have it too
        onActivatedAmbiguously: root.switchPage(-1)
    }
    Shortcut {
        sequences: Qt.platform.os === "osx" ? [StandardKey.NextChild, "Meta+Tab"] : [StandardKey.NextChild]
        onActivated: root.switchPage(1)
    }

    RowLayout {
        spacing: Ez.Metrics.columnSpacing * 2
        Layout.fillWidth: true

        ColumnLayout {
            spacing: 2
            Layout.alignment: Qt.AlignBottom

            Ez.Label {
                objectName: "labelName"
                text: qsTr("Name:")
                font: Ez.Metrics.font
            }
            RowLayout {
                spacing: Ez.Metrics.columnSpacing

                Ez.TextField {
                    id: name
                    objectName: "lineName"
                    Layout.preferredWidth: 141
                    Binding on text {
                        value: root.controller.name
                        restoreMode: Binding.RestoreNone
                    }
                    onTextEdited: root.controller.name = text
                }
                // where it is on the map, as RPG Maker writes cells
                Ez.Label {
                    objectName: "labelPosition"
                    text: "(%1,%2)".arg(String(root.controller.x).padStart(3, "0")).arg(String(root.controller.y).padStart(3, "0"))
                    font: Ez.Metrics.font
                    color: Ez.Metrics.mutedTextColor
                }
            }
        }
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Layout.alignment: Qt.AlignBottom

            Ez.Button {
                objectName: "pushNewPage"
                text: qsTr("New Page")
                enabled: root.controller.canNewPage
                onClicked: root.newPage()
            }
            Ez.Button {
                objectName: "pushCopyPage"
                text: qsTr("Copy Page")
                enabled: root.controller.pageCount > 0
                onClicked: root.controller.copyPage()
            }
            Ez.Button {
                objectName: "pushPastePage"
                text: qsTr("Paste Page")
                enabled: root.controller.canPastePage
                onClicked: root.pastePage()
            }
            Ez.Button {
                objectName: "pushDeletePage"
                text: qsTr("Delete Page")
                enabled: root.controller.canDeletePage
                onClicked: root.deletePage()
            }
            Ez.Button {
                objectName: "pushClearPage"
                text: qsTr("Clear Page")
                enabled: root.controller.pageCount > 0
                onClicked: root.clearPage()
            }
        }
        Item {
            Layout.fillWidth: true
        }
    }

    ColumnLayout {
        spacing: 0
        Layout.fillWidth: true
        Layout.fillHeight: true

        // the pages, numbered from 1 in order (as RPG Maker saves them)
        Controls.TabBar {
            id: tabs
            objectName: "tabEventPages"
            Layout.fillWidth: true
            font: Ez.Metrics.font
            Binding on currentIndex {
                value: root.controller.currentPage
                restoreMode: Binding.RestoreNone
            }

            Repeater {
                model: root.controller.pageCount
                Controls.TabButton {
                    required property int index
                    text: String(index + 1)
                    width: Math.max(implicitWidth, 60)
                    font: Ez.Metrics.font
                    // the user's choice only: the tabs made again when the
                    // pages change pass by other pages
                    onClicked: {
                        if (index !== root.controller.currentPage) {
                            root.captureView()
                            root.controller.currentPage = index
                        }
                    }
                }
            }
        }

        StackLayout {
            currentIndex: tabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: Ez.Metrics.sectionSpacing

            Repeater {
                id: pageRepeater
                model: root.controller.pageCount

                // a page is made when first shown, then kept (its changes
                // are the controller's: a page not made yet loses none)
                Loader {
                    id: loader

                    required property int index
                    property bool loaded: false

                    active: root.pagesReady && (root.controller.currentPage === index || loaded)
                    onLoaded: {
                        loaded = true
                        const page = item as EventPageEditor
                        const view = root.pageViews[index]
                        // the line asked for (a search), else as last time
                        if (root.selectLine >= 0 && index === root.controller.currentPage) {
                            const line = root.selectLine
                            root.selectLine = -1
                            Qt.callLater(() => {
                                page.commandList.selectIndex(line)
                                page.commandList.view.forceActiveFocus()
                            })
                        } else if (view) {
                            Qt.callLater(() => page.commandList.restoreViewState(view))
                        }
                    }
                    sourceComponent: EventPageEditor {
                        controller: root.controller
                        pageIndex: loader.index
                        chipsetId: root.chipsetId
                        settingsWidth: root.settingsWidth
                        onSettingsWidthChanged: root.settingsWidth = settingsWidth
                    }
                }
            }
        }
    }
}
