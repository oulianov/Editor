// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "SearchComboBox"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * An entry of a database list ("0012: Potion"), "(None)" first when
 * noneText is set: with jsonData and key the number of the data is the
 * ID of the entry chosen (noneValue for none).
 *
 *   Ez.RefComboBox {
 *       database: page.database; listKey: "items"
 *       noneText: qsTr("(None)")
 *       jsonData: page.entry; key: "item_id"
 *   }
 *
 * Its drop-down (the arrow, Space, Alt+Down, F4) is the entry picker of
 * RPG Maker's lists: a search over the numbers and names (searchField,
 * focused) above the list (searchList, the entry of the box marked and
 * shown), and Array Size at the bottom (searchArraySize, the list of the
 * database the box shows: the window's copy, as the list pane resizes it).
 * In the list: Up, Down, Page Up, Page Down, Home and End move, Return
 * chooses (the entry of the number typed, else the first match), Escape
 * leaves the entry as it is.
 *
 * pickable puts a "..." in the frame of the box, between its text and its
 * arrow (pickName, pickText, pickToolTip): the entry picker on the list
 * (Ez.Pickers.pickEntry, on the copy of the database window, or
 * pickHandler), where the entries are also renamed; the one chosen is the
 * box's (picked(id)). There (renamable), typing on the box renames the
 * entry it shows, as the picker does (F2: the name as it is): its text
 * becomes a field (objectName + "Name") after the number, Return or
 * leaving it writes the name to the list, Escape leaves it as it was.
 * Elsewhere typing opens the search with what is typed.
 */
Ez.ComboBox {
    id: root

    /** The database (a JsonView) and the pointer of the list in it */
    property var database: null
    property string listKey: ""
    /** The first line standing for none ("": none) and its ID */
    property string noneText: ""
    property int noneValue: 0
    /** A "..." in the frame of the box, opening the entry picker */
    property bool pickable: false
    /** The objectName of the "..." */
    property string pickName: objectName + "Pick"
    /** The text of the "...", for a pickHandler doing something else than the entry picker */
    property string pickText: "..."
    /** Its tool tip, a sentence */
    property string pickToolTip: listKey === "switches" ? qsTr("Opens the window of the switches, to choose one or to rename them")
        : listKey === "variables" ? qsTr("Opens the window of the variables, to choose one or to rename them")
        : qsTr("Opens the window of the list, to choose an entry or to rename the entries")
    /** The title of the picker (empty: the name of the list) */
    property string pickTitle: ""
    readonly property alias pickButton: pick
    /**
     * A function that picks the entry instead (a command editor's session,
     * whose database is its own): it writes the entry and returns its ID,
     * null when cancelled
     */
    property var pickHandler: null
    /** Typing renames the entry shown: where the picker renames them, a list of the database */
    property bool renamable: pickable && database !== null && listKey !== "" && !listKey.includes("/")
    /** Array Size in the drop-down: the whole list of the database is shown */
    property bool resizable: database !== null && listKey !== "" && !listKey.includes("/")
                             && choices === undefined && ids === undefined

    /** Only the entries of these IDs (the equipment among the items...); undefined: all */
    property var ids: undefined
    /** Entries of another list than the database's ([{text, value}]), searched the same way */
    property var choices: undefined

    /** The name of an entry is being typed */
    readonly property bool renaming: nameEditor.visible
    /** The ID of the entry shown (-1: none shown) */
    readonly property int shownId: currentIndex >= 0 ? Number(currentValue) : -1
    /** The list renamed */
    readonly property var nameList: renamable && database ? database.list(listKey) : null
    /** The names RPG Maker lets type, per list (the fields of the database pages) */
    readonly property var nameLimits: ({
        actors: 12, classes: 12, skills: 20, items: 20, enemies: 20, troops: 20, terrains: 16,
        attributes: 8, states: 8, animations: 20, chipsets: 20, commonevents: 20,
        switches: 20, variables: 20, battleranimations: 16
    })

    /** An entry was chosen in the picker (its ID, noneValue for "(None)") */
    signal picked(int id)

    /** Opens the entry picker on the entry shown; the one chosen replaces it */
    function pickEntry() {
        if (pickHandler) {
            const handled = pickHandler()
            if (handled !== null && handled !== undefined) {
                picked(handled)
            }
            return
        }
        if (!database || listKey === "") {
            return
        }
        const shown = currentIndex >= 0 ? Number(currentValue) : noneValue
        const chosen = Ez.Pickers.pickEntry({ list: listKey, id: shown === noneValue && noneText !== "" ? 0 : shown,
                                              none: noneText !== "", title: pickTitle, database: database }, root)
        if (chosen === null || chosen === undefined) {
            return
        }
        const id = chosen === 0 && noneText !== "" ? noneValue : chosen
        if (jsonData && key !== "") {
            jsonData.set(key, id)
        } else {
            currentIndex = indexOfValue(id)
            activated(currentIndex)
        }
        picked(id)
    }

    /** Opens the search, with text typed in it */
    function openSearch(text) {
        search.seed = text
        if (search.opened) {
            searchField.text += text
        } else {
            search.open()
        }
    }
    /** Chooses the entry at a row of the list, as the user does */
    function choose(row) {
        // nothing matches: the search stays open
        if (row < 0) {
            return
        }
        search.close()
        if (row !== currentIndex) {
            currentIndex = row
            activated(row)
        }
    }
    /** Array Size of the list: the new entries (or the last one left) shown in the drop-down */
    function resizeList(count) {
        const list = database ? database.list(listKey) : null
        if (!list) {
            return
        }
        const before = list.count
        if (!list.resize(count)) {
            return
        }
        // the whole list, at the first new entry (the last one left)
        searchField.text = ""
        const offset = noneText !== "" ? 1 : 0
        const shown = found.rowOf((count > before ? before : count - 1) + offset)
        searchList.currentIndex = shown
        searchList.positionViewAtIndex(Math.max(0, shown), ListView.Center)
        // the pointer left where the dialog was does not choose a line
        search.pointer = Qt.point(-1, -1)
    }
    /**
     * Starts typing the name of the entry shown, text after it
     *
     * @return false where it cannot be renamed ("(None)", another list)
     */
    function startRename(text) {
        const id = shownId
        if (!renamable || !enabled || !nameList || id < 1 || id > nameList.count || (noneText !== "" && id === noneValue)) {
            return false
        }
        nameEditor.entryId = id
        nameEditor.prefix = String(id).padStart(4, "0") + ": "
        nameEditor.text = nameList.str((id - 1) + "/name") + text
        nameEditor.visible = true
        nameEditor.forceActiveFocus()
        nameEditor.cursorPosition = nameEditor.length
        return true
    }
    /** Ends typing the name: written (commit), else left as it was */
    function finishRename(commit) {
        if (!nameEditor.visible) {
            return
        }
        nameEditor.visible = false
        const list = nameList
        if (commit && list && nameEditor.entryId >= 1 && nameEditor.text !== list.str((nameEditor.entryId - 1) + "/name")) {
            list.rename(nameEditor.entryId - 1, nameEditor.text)
        }
        if (nameEditor.activeFocus) {
            root.forceActiveFocus()
        }
    }

    model: choices !== undefined ? choiceList : ids === undefined ? entries : allowed

    // the "..." between the text and the arrow, in the frame of the box
    readonly property real pickPart: pickable ? pick.implicitWidth + spacing : 0
    readonly property real indicatorPart: indicator && indicator.visible ? indicator.width + 2 * spacing : 0
    rightPadding: horizontalPadding + indicatorPart + pickPart
    Binding {
        target: root.indicator
        property: "x"
        value: root.width - root.horizontalPadding - root.spacing - root.indicator.width
        when: root.indicator !== null && root.pickable
    }

    // F4 and Alt+Down drop the list down, before the window's shortcuts
    // (F4 of a database page: the previous entry)
    Keys.onShortcutOverride: (event) => {
        const modifiers = event.modifiers & (Qt.ControlModifier | Qt.MetaModifier | Qt.AltModifier | Qt.ShiftModifier)
        event.accepted = event.key === Qt.Key_F4 && modifiers === 0 || event.key === Qt.Key_Down && modifiers === Qt.AltModifier
    }
    Keys.onPressed: (event) => {
        const shortcut = event.modifiers & (Qt.ControlModifier | Qt.MetaModifier | Qt.AltModifier)
        if (event.key === Qt.Key_F4 && !shortcut || event.key === Qt.Key_Down && (event.modifiers & Qt.AltModifier)) {
            search.open()
            event.accepted = true
        } else if (event.key === Qt.Key_F2 && !shortcut) {
            event.accepted = root.startRename("")
        } else if (!shortcut && event.text.length > 0 && event.text.trim().length > 0 && event.text.charCodeAt(0) >= 32) {
            // a letter or a digit renames the entry, else starts a search (a space opens it)
            if (!root.startRename(event.text)) {
                root.openSearch(event.text)
            }
            event.accepted = true
        }
    }

    Controls.AbstractButton {
        id: pick
        objectName: root.pickName
        visible: root.pickable
        x: root.width - root.rightPadding + root.spacing
        y: 1
        height: root.height - 2
        implicitWidth: Math.max(Math.ceil(pickLabel.implicitWidth) + 2 * root.spacing, Math.round(root.height * 0.9))
        width: implicitWidth
        hoverEnabled: true
        // the box keeps the focus
        focusPolicy: Qt.NoFocus
        text: root.pickText
        Controls.ToolTip.visible: hovered && root.pickToolTip !== ""
        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
        Controls.ToolTip.text: root.pickToolTip
        onClicked: {
            root.finishRename(true)
            root.pickEntry()
        }

        background: Rectangle {
            color: pick.down ? Qt.rgba(Kirigami.Theme.focusColor.r, Kirigami.Theme.focusColor.g, Kirigami.Theme.focusColor.b, 0.3)
                 : pick.hovered ? Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.1)
                 : "transparent"
            // the lines between the text, the "..." and the arrow
            Rectangle {
                x: 0
                y: Ez.Metrics.fieldPadding - 1
                width: 1
                height: parent.height - 2 * y
                color: Ez.Metrics.sectionBorderColor
            }
            Rectangle {
                x: parent.width - 1
                y: Ez.Metrics.fieldPadding - 1
                width: 1
                height: parent.height - 2 * y
                color: Ez.Metrics.sectionBorderColor
            }
        }
        contentItem: Ez.Label {
            id: pickLabel
            text: pick.text
            font: Ez.Metrics.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // the name typed over the text of the box, after the number
    Ez.TextField {
        id: nameEditor

        /** The entry renamed and its number ("0003: ") */
        property int entryId: 0
        property string prefix: ""
        /** Where the text of the box starts */
        readonly property real textStart: root.leftPadding + (root.contentItem instanceof TextInput ? (root.contentItem as TextInput).leftPadding : 0)

        objectName: root.objectName + "Name"
        visible: false
        x: 0
        y: 0
        width: root.pickable ? pick.x : root.width - root.rightPadding
        height: root.height
        leftPadding: textStart + prefixLabel.implicitWidth
        limit: root.nameLimits[root.listKey] ?? 0

        Ez.Label {
            id: prefixLabel
            x: nameEditor.textStart
            anchors.verticalCenter: parent.verticalCenter
            text: nameEditor.prefix
            font: nameEditor.font
        }

        // Return and Escape are the field's, not the window's buttons
        Keys.onShortcutOverride: (event) => {
            event.accepted = event.key === Qt.Key_Escape || event.key === Qt.Key_Return || event.key === Qt.Key_Enter
        }
        Keys.onReturnPressed: root.finishRename(true)
        Keys.onEnterPressed: root.finishRename(true)
        Keys.onEscapePressed: root.finishRename(false)
        onActiveFocusChanged: {
            if (!activeFocus) {
                root.finishRename(true)
            }
        }
    }

    /** The entries of the list, "(None)" first */
    readonly property var entries: !database || listKey === "" ? null
         : noneText !== "" ? database.choices(listKey, noneText, noneValue) : database.list(listKey)
    Ez.ChoiceListModel {
        id: choiceList
        entries: root.choices ?? []
    }
    Ez.EntryFilterModel {
        id: allowed
        sourceModel: root.ids === undefined ? null : root.entries
        // "(None)" stays
        ids: root.ids === undefined ? undefined : (root.noneText !== "" ? [root.noneValue].concat(root.ids) : root.ids)
    }
    Ez.EntryFilterModel {
        id: found
        sourceModel: root.model
    }
    FontMetrics {
        id: lineMetrics
        font: Ez.Metrics.font
    }

    Ez.ArraySizeDialog {
        id: arraySize
        list: root.resizable && root.database ? root.database.list(root.listKey) : null
        onConfirmed: (count) => root.resizeList(count)
        // back in the search once answered
        onBusyChanged: if (!busy && search.opened) searchField.forceActiveFocus()
    }

    popup: Controls.Popup {
        id: search

        /** What was typed to open it */
        property string seed: ""
        /** The height of a line of the list */
        readonly property real rowHeight: lineMetrics.height + 2 * Ez.Metrics.listRowPadding
        /** The lines shown at most: a long list keeps a tall window, scrolled */
        readonly property int maximumRows: 20
        /** The window's height and the top of the box in it, measured when it opens */
        property real windowHeight: 600
        property real boxTop: 0
        /** Where the pointer was last seen over the lines, in the window (-1: not since it opened) */
        property point pointer: Qt.point(-1, -1)

        // below the box, else above it, else as low as the window lets it
        // (a small window: a command editor's)
        y: {
            const below = boxTop + root.height
            if (below + height + margins <= windowHeight) {
                return root.height
            }
            if (boxTop - height - margins >= 0) {
                return -height
            }
            return windowHeight - margins - height - boxTop
        }
        width: Math.max(root.width, 260)
        height: Math.min(implicitHeight, windowHeight - 2 * margins)
        implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
        // in the window, whatever its size
        margins: 4
        padding: 0
        focus: true
        font: Ez.Metrics.font
        // the Array Size dialog over it is not a click outside
        closePolicy: arraySize.busy ? Controls.Popup.NoAutoClose
                                    : Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutsideParent
        // drawn by its content, flat and opaque, as RPG Maker's lists
        background: null

        onAboutToShow: {
            const overlay = Controls.Overlay.overlay
            windowHeight = overlay ? overlay.height : 600
            boxTop = root.mapToItem(null, 0, 0).y
            pointer = Qt.point(-1, -1)
            searchField.text = ""
            found.filterText = ""
            // the whole list first: the window keeps its size while searching
            searchList.currentIndex = found.rowOf(root.currentIndex)
        }
        onOpened: {
            searchField.forceActiveFocus()
            if (seed !== "") {
                searchField.text = seed
                seed = ""
            } else {
                searchList.positionViewAtIndex(Math.max(0, searchList.currentIndex), ListView.Center)
            }
        }
        // back on the box (not once a name is being typed in it)
        onClosed: if (!root.renaming) root.forceActiveFocus()

        contentItem: Item {
            implicitWidth: column.implicitWidth + 2 * column.anchors.margins
            implicitHeight: column.implicitHeight + 2 * column.anchors.margins

            Rectangle {
                anchors.fill: parent
                color: Ez.Metrics.palette.window
                border.color: Ez.Metrics.sectionBorderColor
            }

            ColumnLayout {
                id: column
                anchors.fill: parent
                anchors.margins: 4
                spacing: 4

                Ez.TextField {
                    id: searchField
                    objectName: "searchField"
                    placeholderText: qsTr("Search by number or name")
                    Layout.fillWidth: true
                    onTextChanged: {
                        found.filterText = text
                        if (text.trim() === "") {
                            searchList.currentIndex = found.rowOf(root.currentIndex)
                        } else {
                            // the entry of the number typed, else the first one
                            searchList.currentIndex = found.bestRow()
                        }
                        searchList.positionViewAtIndex(Math.max(0, searchList.currentIndex), ListView.Contain)
                    }
                    Keys.onPressed: (event) => {
                        const rows = searchList.count
                        const page = Math.max(1, Math.floor(searchList.height / search.rowHeight) - 1)
                        // Home and End of the list; with Shift they select in the text
                        const plain = !(event.modifiers & Qt.ShiftModifier)
                        let row = searchList.currentIndex
                        if (event.key === Qt.Key_Up) {
                            row = Math.max(0, row - 1)
                        } else if (event.key === Qt.Key_Down) {
                            row = Math.min(rows - 1, row + 1)
                        } else if (event.key === Qt.Key_PageUp) {
                            row = Math.max(0, row - page)
                        } else if (event.key === Qt.Key_PageDown) {
                            row = Math.min(rows - 1, row + page)
                        } else if (event.key === Qt.Key_Home && plain) {
                            row = 0
                        } else if (event.key === Qt.Key_End && plain) {
                            row = rows - 1
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            root.choose(found.sourceRow(searchList.currentIndex))
                            event.accepted = true
                            return
                        } else if (event.key === Qt.Key_Escape) {
                            search.close()
                            event.accepted = true
                            return
                        } else {
                            return
                        }
                        event.accepted = true
                        if (rows === 0) {
                            return
                        }
                        searchList.currentIndex = row
                        searchList.positionViewAtIndex(row, ListView.Contain)
                    }
                }

                ListView {
                    id: searchList
                    objectName: "searchList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    implicitHeight: Math.min(Math.max(1, root.count), search.maximumRows) * search.rowHeight
                    clip: true
                    model: found
                    highlightMoveDuration: 0
                    boundsBehavior: Flickable.StopAtBounds
                    Controls.ScrollBar.vertical: Controls.ScrollBar {}

                    // the lines on the color of a list
                    Rectangle {
                        z: -1
                        parent: searchList
                        anchors.fill: parent
                        color: Ez.Metrics.palette.base
                    }

                    delegate: Item {
                        id: line

                        required property string text
                        // the ID: the index of a line is not told again when a search
                        // removes the lines before it
                        required property var value
                        /** The row of the entry in the box */
                        readonly property int row: root.indexOfValue(value)
                        readonly property bool current: ListView.isCurrentItem
                        // the entry of the box
                        readonly property bool chosen: row === root.currentIndex

                        width: ListView.view.width
                        height: search.rowHeight

                        Rectangle {
                            anchors.fill: parent
                            visible: line.current
                            color: Kirigami.Theme.highlightColor
                        }
                        Ez.Label {
                            x: Ez.Metrics.listTextPadding
                            width: parent.width - 2 * Ez.Metrics.listTextPadding
                            anchors.verticalCenter: parent.verticalCenter
                            text: line.text
                            elide: Text.ElideRight
                            font.family: Ez.Metrics.font.family
                            font.pointSize: Ez.Metrics.font.pointSize
                            font.bold: line.chosen
                            color: line.current ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            // the line under the pointer is the one Return chooses, as in a
                            // menu, once the pointer moves over the lines: not the line a
                            // search, Array Size or the window opening brings under it
                            onPositionChanged: (mouse) => {
                                const point = mapToItem(null, mouse.x, mouse.y)
                                const moved = search.pointer.x >= 0 && (point.x !== search.pointer.x || point.y !== search.pointer.y)
                                search.pointer = point
                                if (moved) {
                                    searchList.currentIndex = found.rowOf(line.row)
                                }
                            }
                            onClicked: root.choose(line.row)
                        }
                    }

                    Ez.Label {
                        objectName: "searchNothing"
                        anchors.centerIn: parent
                        width: parent.width - 2 * Ez.Metrics.listTextPadding
                        visible: searchList.count === 0
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: qsTr("No entry matches")
                        color: Ez.Metrics.mutedTextColor
                    }
                }

                RowLayout {
                    visible: root.resizable
                    spacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true

                    Ez.Button {
                        objectName: "searchArraySize"
                        text: qsTranslate("DatabaseSplitWidget", "Array Size...")
                        enabled: arraySize.list !== null
                        // the button only: Return stays the entry's
                        focusPolicy: Qt.NoFocus
                        onClicked: arraySize.ask()
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Ez.Label {
                        // the entries of the list (the matches while searching)
                        text: searchField.text.trim() === "" ? qsTr("%n entries", "", arraySize.list ? arraySize.list.count : 0)
                                                             : qsTr("%1 of %n", "", arraySize.list ? arraySize.list.count : 0).arg(searchList.count)
                        color: Ez.Metrics.mutedTextColor
                    }
                }
            }
        }
    }
}
