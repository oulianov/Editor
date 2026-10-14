// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "DatabaseSplitWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The entries of a database list beside their page ("0001: Alex"), as
 * RPG Maker shows them: a search over the numbers and names (listFilter),
 * the list (list), Array Size (pushMax), the menu of the list (Copy,
 * Paste, Clear, Cut, Multiple Copy, also on the keys of the list) and
 * F4/F5 for the previous and next entry from anywhere in the page.
 *
 *   Ez.EntryListPane { id: pane; list: session.database.list("attributes") }
 *   AttributePage { entry: pane.currentEntry }
 *
 * currentRow is the index of the entry shown in the list (its ID - 1),
 * whatever the search hides: renaming it does not lose it.
 */
FocusScope {
    id: root

    /** The entries (a JsonListView) */
    property var list: null
    /** The entry shown: its index in the list (-1: none) */
    property int currentRow: -1
    readonly property int currentId: currentRow + 1
    /** The data of the entry shown (a JsonView), null for none */
    readonly property var currentEntry: list && currentRow >= 0 && currentRow < list.count ? list.entry(currentRow) : null
    /** The search typed */
    property alias filterText: filter.text
    /** The list itself (its rows: the entries the search shows) */
    readonly property alias view: view

    /** Shows the entry of this index; false when there is none */
    function selectRow(row) {
        if (!list || row < 0 || row >= list.count) {
            return false
        }
        currentRow = row
        syncView()
        view.positionViewAtIndex(view.currentIndex, ListView.Contain)
        return true
    }
    function selectId(id) {
        return selectRow(id - 1)
    }
    /** The previous (-1) or next (1) entry the search shows (F4, F5) */
    function step(delta) {
        if (found.rowCount() === 0) {
            return
        }
        const shown = found.rowOf(currentRow)
        const row = Math.max(0, Math.min(found.rowCount() - 1, (shown < 0 ? 0 : shown + delta)))
        currentRow = found.sourceRow(row)
        syncView()
        view.positionViewAtIndex(view.currentIndex, ListView.Contain)
    }

    // wide enough for the number and a name of RPG Maker's length ("0002:
    // High Potion", Ez.Metrics.entryListText)
    implicitWidth: Math.ceil(Math.max(arraySize.implicitWidth, listMetrics.advanceWidth(Ez.Metrics.entryListText)
                                      + scrollBar.width + 2 * frame.border.width + 2 * Ez.Metrics.listTextPadding + 4))
    implicitHeight: layout.implicitHeight

    // the list follows the entry shown, not the other way round
    property bool syncing: false
    function syncView() {
        syncing = true
        view.currentIndex = found.rowOf(currentRow)
        syncing = false
    }

    // the first entry, as RPG Maker, instead of an empty page
    onListChanged: {
        currentRow = list && list.count > 0 ? 0 : -1
        Qt.callLater(syncView)
    }
    Component.onCompleted: Qt.callLater(syncView)
    Connections {
        target: root.list
        // Array Size, a merge: the entry shown stays while it is there
        function onCountChanged() {
            if (root.currentRow >= root.list.count) {
                root.currentRow = root.list.count - 1
            }
            Qt.callLater(root.syncView)
        }
    }
    Connections {
        target: found
        // a rename the search no longer finds: the page stays on its entry
        function onRowsInserted() { Qt.callLater(root.syncView) }
        function onRowsRemoved() { Qt.callLater(root.syncView) }
        function onLayoutChanged() { Qt.callLater(root.syncView) }
        function onModelReset() { Qt.callLater(root.syncView) }
    }

    Ez.EntryFilterModel {
        id: found
        sourceModel: root.list
    }
    FontMetrics {
        id: listMetrics
        font: Ez.Metrics.font
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        spacing: Ez.Metrics.rowSpacing

        Ez.TextField {
            id: filter
            objectName: "listFilter"
            placeholderText: qsTr("Filter")
            Layout.fillWidth: true
            Layout.minimumWidth: 40
            onTextChanged: {
                found.filterText = text
                const typed = text.trim()
                let row = found.rowOf(root.currentRow)
                if (/^\d+$/.test(typed)) {
                    // a number: its entry
                    row = found.bestRow()
                } else if (row < 0) {
                    // the entry shown when it matches, else the first match
                    row = found.rowCount() > 0 ? 0 : -1
                }
                if (row >= 0) {
                    root.currentRow = found.sourceRow(row)
                }
                root.syncView()
                view.positionViewAtIndex(Math.max(view.currentIndex, 0), ListView.Contain)
            }
            Keys.onDownPressed: {
                view.forceActiveFocus()
                root.step(1)
            }
        }

        Rectangle {
            id: frame
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitHeight: 120
            // the colors of the views (dark mode too)
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor
            border.color: Ez.Metrics.sectionBorderColor
            border.width: 1

            ListView {
                id: view
                objectName: "list"
                anchors.fill: parent
                anchors.margins: frame.border.width
                clip: true
                focus: true
                activeFocusOnTab: true
                model: found
                boundsBehavior: Flickable.StopAtBounds
                keyNavigationEnabled: true
                highlightMoveDuration: 0
                reuseItems: true
                Controls.ScrollBar.vertical: Controls.ScrollBar {
                    id: scrollBar
                }

                onCurrentIndexChanged: {
                    if (!root.syncing && currentIndex >= 0) {
                        root.currentRow = found.sourceRow(currentIndex)
                    }
                }

                // RPG Maker's keys of the list (the fields of the page have their own copy and paste)
                Keys.onPressed: (event) => {
                    if (event.matches(StandardKey.Copy)) {
                        copyAction.trigger()
                    } else if (event.matches(StandardKey.Paste)) {
                        pasteAction.trigger()
                    } else if (event.matches(StandardKey.Cut)) {
                        cutAction.trigger()
                    } else if (event.key === Qt.Key_Delete
                               || (event.key === Qt.Key_Backspace && Qt.platform.os === "osx")) {
                        clearAction.trigger()
                    } else if (event.key === Qt.Key_Home || event.key === Qt.Key_End) {
                        if (count > 0) {
                            currentIndex = event.key === Qt.Key_Home ? 0 : count - 1
                            positionViewAtIndex(currentIndex, ListView.Contain)
                        }
                    } else if (event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown) {
                        const page = Math.max(1, Math.floor(height / Math.max(1, currentItem ? currentItem.height : 16)) - 1)
                        if (count > 0) {
                            currentIndex = Math.max(0, Math.min(count - 1,
                                currentIndex + (event.key === Qt.Key_PageUp ? -page : page)))
                            positionViewAtIndex(currentIndex, ListView.Contain)
                        }
                    } else if (event.key === Qt.Key_Menu) {
                        root.showMenu()
                    } else {
                        return
                    }
                    event.accepted = true
                }

                delegate: Item {
                    id: line

                    required property int index
                    required property string text
                    readonly property bool current: ListView.isCurrentItem

                    width: ListView.view.width
                    implicitHeight: label.implicitHeight + 2 * Ez.Metrics.listRowPadding
                    height: implicitHeight

                    Rectangle {
                        anchors.fill: parent
                        visible: line.current
                        // the list in the background: a paler line
                        color: view.activeFocus || menu.opened ? Kirigami.Theme.highlightColor
                                                               : Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g,
                                                                         Kirigami.Theme.highlightColor.b, 0.35)
                    }
                    Ez.Label {
                        id: label
                        x: Ez.Metrics.listTextPadding
                        width: parent.width - 2 * Ez.Metrics.listTextPadding
                        anchors.verticalCenter: parent.verticalCenter
                        text: line.text
                        elide: Text.ElideRight
                        font: Ez.Metrics.font
                        color: line.current && (view.activeFocus || menu.opened) ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                    }
                    MouseArea {
                        id: lineMouse
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onPressed: (mouse) => {
                            view.forceActiveFocus()
                            view.currentIndex = line.index
                            if (mouse.button === Qt.RightButton) {
                                root.showMenu(lineMouse, mouse.x, mouse.y)
                            }
                        }
                    }
                }
            }
        }

        Ez.Button {
            id: arraySize
            objectName: "pushMax"
            text: qsTr("Array Size...")
            Layout.fillWidth: true
            enabled: !!root.list
            onClicked: arraySizeDialog.ask()
        }
    }

    // F4/F5 (and Ctrl+F4/Ctrl+F5, the same in RPG Maker): from anywhere in the page shown
    Shortcut {
        sequences: ["F4", "Ctrl+F4"]
        enabled: root.visible && !!root.list
        onActivated: root.step(-1)
    }
    Shortcut {
        sequences: ["F5", "Ctrl+F5"]
        enabled: root.visible && !!root.list
        onActivated: root.step(1)
    }

    /** The menu of the list at the entry shown (RPG Maker's order): at x, y
     *  of the item at (where it was clicked), else on the line (the Menu key) */
    function showMenu(at, x, y) {
        if (currentRow < 0) {
            return
        }
        menu.clipboard = list.clipboardSize()
        menu.clearable = list.canClear(currentRow)
        const item = view.currentItem
        if (at) {
            // where it was clicked
            menu.popupAt(at, x, y)
        } else if (item) {
            menu.popupAt(item, item.width / 2, item.height / 2)
        } else {
            menu.popup()
        }
    }

    // the icons drawn for the colors of the menus
    readonly property string iconFolder: Kirigami.Theme.backgroundColor.hslLightness < 0.5 ? "qrc:/dark/" : "qrc:/bright/"

    Controls.Action {
        id: copyAction
        objectName: "actionDataCopy"
        text: qsTranslate("EditActions", "Copy")
        icon.source: root.iconFolder + "edit-copy"
        onTriggered: {
            if (root.currentRow >= 0) {
                root.list.copy(root.currentRow, 1)
            }
        }
    }
    Controls.Action {
        id: pasteAction
        objectName: "actionDataPaste"
        text: qsTranslate("EditActions", "Paste")
        icon.source: root.iconFolder + "edit-paste"
        enabled: !menu.opened || menu.clipboard > 0
        onTriggered: {
            // over the entries from the one shown on, keeping their IDs
            if (root.currentRow >= 0 && root.list.clipboardSize() > 0) {
                root.list.paste(root.currentRow)
            }
        }
    }
    Controls.Action {
        id: clearAction
        objectName: "actionDataClear"
        text: qsTranslate("EditActions", "Clear")
        icon.source: root.iconFolder + "edit-delete"
        enabled: !menu.opened || menu.clearable
        onTriggered: {
            if (root.currentRow >= 0 && root.list.canClear(root.currentRow)) {
                root.list.clear(root.currentRow)
            }
        }
    }
    Controls.Action {
        id: cutAction
        objectName: "actionDataCut"
        text: qsTranslate("EditActions", "Cut")
        icon.source: root.iconFolder + "edit-cut"
        enabled: !menu.opened || menu.clearable
        onTriggered: {
            if (root.currentRow >= 0 && root.list.canClear(root.currentRow)) {
                root.list.copy(root.currentRow, 1)
                root.list.clear(root.currentRow)
            }
        }
    }
    Controls.Action {
        id: multipleCopyAction
        objectName: "actionDataMultiCopy"
        text: qsTranslate("DatabaseSplitWidgetBase", "Multiple Copy...")
        onTriggered: {
            if (root.currentRow < 0) {
                return
            }
            // this many entries from the one shown on; Paste puts them back from the entry shown then
            multipleCopyDialog.value = 1
            multipleCopyDialog.to = root.list.count - root.currentRow
            multipleCopyDialog.open()
        }
    }

    Ez.ContextMenu {
        id: menu

        property int clipboard: 0
        property bool clearable: false

        font: Ez.Metrics.font

        Controls.MenuItem {
            action: copyAction
        }
        Controls.MenuItem {
            action: pasteAction
        }
        Controls.MenuItem {
            action: clearAction
        }
        Controls.MenuItem {
            action: cutAction
        }
        Controls.MenuSeparator {}
        Controls.MenuItem {
            action: multipleCopyAction
        }
    }

    Ez.ArraySizeDialog {
        id: arraySizeDialog
        list: root.list
        onConfirmed: (count) => root.resize(count)
    }
    Ez.NumberDialog {
        id: multipleCopyDialog
        title: qsTranslate("DatabaseSplitWidgetBase", "Multiple Copy")
        label: qsTranslate("DatabaseSplitWidgetBase", "No. of Items")
        spinName: "spinMultipleCopy"
        onChosen: (count) => root.list.copy(root.currentRow, count)
    }
    /** Array Size (a smaller list warned about first): the entry shown stays, else the last one */
    function resize(count) {
        const id = currentId
        list.resize(count)
        // the entry shown, else the last one
        if (!selectId(Math.min(id, count)) && found.rowCount() > 0) {
            currentRow = found.sourceRow(0)
            syncView()
        }
    }
}
