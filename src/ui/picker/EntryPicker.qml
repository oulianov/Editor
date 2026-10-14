// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "DatabaseSplitWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * An entry of a list of the database (Pickers::pickEntry, Ez.Pickers.pickEntry,
 * the "..." beside a switch, an item...): the list, searched by number or
 * name, "(None)" first where the field allows none. As RPG Maker's switch
 * and variable windows, the entry's name can be changed, and the number of
 * entries (Array Size); these changes are in a copy of the database
 * (session), given to the window that opened the picker on OK or Apply.
 *
 * chosenId: the ID of the entry selected (0: "(None)").
 */
Ez.DialogWindow {
    id: root

    required property Ez.DatabaseSession session
    /** The list, as JSON names it ("switches") */
    required property string listName
    /** The entry selected first (unknown: "(None)" when offered, else the first) */
    property int currentId: 0
    /** "(None)" is offered first */
    property bool none: false
    /** The title asked for (empty: the name of the list) */
    property string customTitle
    /** Hears Apply (PickerRelay) */
    property var relay: null
    property int chosenId: currentId

    readonly property var entries: session.database ? session.database.list(listName) : null
    /** The row of the list (the fallback row counted) selected */
    property int sourceRow: -1
    readonly property int offset: none ? 1 : 0

    /** The ID of a row of the list: RPG Maker numbers the entries from 1 */
    function idOf(row) {
        return row < offset ? 0 : row - offset + 1
    }
    function rowOfId(id) {
        if (!entries) {
            return -1
        }
        if (id >= 1 && id <= entries.count) {
            return id - 1 + offset
        }
        return 0
    }
    /** Selects a row of the list (filtered out: none) */
    function select(row) {
        sourceRow = row
        if (row >= 0) {
            chosenId = idOf(row)
        }
        files.show(found.rowOf(row))
    }

    readonly property var titles: ({
        actors: qsTranslate("DatabaseDialog", "Characters"),
        classes: qsTranslate("DatabaseDialog", "Professions"),
        skills: qsTranslate("DatabaseDialog", "Skills"),
        items: qsTranslate("DatabaseDialog", "Items"),
        enemies: qsTranslate("DatabaseDialog", "Monsters"),
        troops: qsTranslate("DatabaseDialog", "Monster Groups"),
        attributes: qsTranslate("DatabaseDialog", "Attributes"),
        states: qsTranslate("DatabaseDialog", "Hero Status"),
        animations: qsTranslate("DatabaseDialog", "Battle Animation"),
        battleranimations: qsTranslate("DatabaseDialog", "Battle Animation 2"),
        terrains: qsTranslate("DatabaseDialog", "Terrain"),
        chipsets: qsTranslate("DatabaseDialog", "Chipset"),
        commonevents: qsTranslate("DatabaseDialog", "Common events"),
        switches: qsTranslate("DatabaseDialog", "Switches"),
        variables: qsTranslate("DatabaseDialog", "Variables")
    })

    objectName: "EntryPicker"
    title: customTitle !== "" ? customTitle : (titles[listName] ?? qsTranslate("DatabaseDialog", "Database"))
    standardButtons: Controls.DialogButtonBox.Ok | Controls.DialogButtonBox.Cancel
                     | Controls.DialogButtonBox.Apply | Controls.DialogButtonBox.Help
    preferredWidth: 360
    height: Math.max(minimumHeight, 480)

    Component.onCompleted: {
        if (entries && none) {
            entries.fallbackString = qsTr("(None)")
            entries.fallbackValue = 0
        }
        select(rowOfId(currentId))
        files.view.forceActiveFocus()
    }

    Connections {
        target: root.buttonBox
        function onApplied() {
            if (root.relay) {
                root.relay.apply(root.chosenId)
            }
        }
        function onHelpRequested() {
            Ez.Editor.help(root.listName === "switches" || root.listName === "variables"
                           ? "database-switches" : "database", root.contentItem)
        }
    }

    Ez.EntryFilterModel {
        id: found
        sourceModel: root.entries
    }

    Ez.TextField {
        id: filter
        objectName: "listFilter"
        placeholderText: qsTr("Filter")
        Layout.fillWidth: true
        // the search shows its best match; the entry selected stays when it matches
        onTextChanged: {
            const selected = root.sourceRow
            found.filterText = text
            root.select(found.rowOf(selected) >= 0 ? selected : found.sourceRow(found.bestRow()))
        }
        Keys.onDownPressed: files.view.forceActiveFocus()
        Keys.onReturnPressed: root.accept()
    }

    Ez.PickerList {
        id: files
        listName: "list"
        model: found
        textRole: "text"
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 200
        onPicked: row => {
            root.sourceRow = found.sourceRow(row)
            root.chosenId = root.idOf(root.sourceRow)
        }
        onActivated: row => {
            if (row >= 0) {
                root.accept()
            }
        }
    }

    Ez.Form {
        Ez.FormLabel {
            text: qsTranslate("SwitchWidget", "Name:")
        }
        Ez.TextField {
            objectName: "lineName"
            enabled: root.entries !== null && root.sourceRow >= root.offset
            jsonData: enabled ? root.entries.entry(root.sourceRow - root.offset) : null
            key: "name"
            Layout.fillWidth: true
        }
    }

    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        Ez.Button {
            objectName: "pushMax"
            text: qsTr("Array Size...")
            enabled: root.entries !== null
            onClicked: arraySize.ask()
        }
        Item {
            Layout.fillWidth: true
        }
    }

    Ez.ArraySizeDialog {
        id: arraySize
        list: root.entries
        onConfirmed: (count) => root.resize(count)
    }

    function resize(count) {
        const id = chosenId
        entries.resize(count)
        // the entry selected, else the last one left
        select(rowOfId(Math.min(id, count)))
    }
}
