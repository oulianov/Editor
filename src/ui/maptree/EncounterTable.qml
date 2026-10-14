// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EncounterList"

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The "Enemy Encounters" list of Map Properties and Area Properties: the
 * monster groups met there, edited in Define Encounter (double-click or
 * Return), with Cut, Copy, Paste and Delete (right-click, or their keys
 * while the list has the focus) like RPG Maker. The window places the Add
 * and Delete buttons, which call add() and remove().
 */
Ez.ArrayTable {
    id: root

    property Ez.EncounterListModel encounters

    /** Define Encounter for a new line */
    function add() {
        if (!encounters) {
            return
        }
        if (encounters.troopCount() === 0) {
            noTroop.open()
            return
        }
        define.row = -1
        define.troopId = 1
        define.open()
    }
    /** Define Encounter for a line (none: a new one) */
    function edit(row) {
        if (!encounters) {
            return
        }
        if (row < 0 || row >= encounters.count) {
            add()
            return
        }
        if (encounters.troopCount() === 0) {
            noTroop.open()
            return
        }
        define.row = row
        define.troopId = encounters.troopId(row)
        define.open()
    }
    function remove() {
        if (encounters && currentIndex >= 0 && currentIndex < encounters.count) {
            encounters.remove(currentIndex)
        }
    }
    function cut() {
        if (encounters && currentIndex >= 0 && currentIndex < encounters.count) {
            encounters.cut(currentIndex)
        }
    }
    function copy() {
        if (encounters && currentIndex >= 0 && currentIndex < encounters.count) {
            encounters.copy(currentIndex)
        }
    }
    function paste() {
        if (encounters) {
            const row = encounters.paste(currentIndex)
            if (row >= 0) {
                currentIndex = row
            }
        }
    }

    objectName: "tableEncounters"
    rows: encounters ? encounters.count : 0
    source: encounters
    columns: [{ title: qsTr("Monster Groups"), text: row => root.encounters.text(row) }]
    onActivated: (row) => edit(row)
    onRemoveRequested: (row) => remove()
    contextMenu: menu

    // only while the list has the focus: the window has other fields
    Shortcut {
        sequences: [StandardKey.Cut]
        enabled: root.activeFocus
        onActivated: root.cut()
    }
    Shortcut {
        sequences: [StandardKey.Copy]
        enabled: root.activeFocus
        onActivated: root.copy()
    }
    Shortcut {
        sequences: [StandardKey.Paste]
        enabled: root.activeFocus
        onActivated: root.paste()
    }

    Ez.ContextMenu {
        id: menu

        readonly property bool selected: root.encounters !== null && root.currentIndex >= 0
                                         && root.currentIndex < root.encounters.count
        font: Ez.Metrics.font

        Controls.MenuItem {
            objectName: "actionEncounterEdit"
            text: qsTr("Edit...")
            icon.source: menu.iconFolder + "properties"
            onTriggered: root.edit(root.currentIndex)
        }
        Controls.MenuSeparator {}
        Controls.MenuItem {
            objectName: "actionEncounterCut"
            text: qsTr("Cut")
            icon.source: menu.iconFolder + "edit-cut"
            enabled: menu.selected
            onTriggered: root.cut()
        }
        Controls.MenuItem {
            objectName: "actionEncounterCopy"
            text: qsTr("Copy")
            icon.source: menu.iconFolder + "edit-copy"
            enabled: menu.selected
            onTriggered: root.copy()
        }
        Controls.MenuItem {
            objectName: "actionEncounterPaste"
            text: qsTr("Paste")
            icon.source: menu.iconFolder + "edit-paste"
            enabled: root.encounters !== null && root.encounters.canPaste
            onTriggered: root.paste()
        }
        Controls.MenuItem {
            objectName: "actionEncounterDelete"
            text: qsTr("Delete")
            icon.source: menu.iconFolder + "edit-delete"
            enabled: menu.selected
            onTriggered: root.remove()
        }
    }

    EncounterDialog {
        id: define

        /** The line edited, -1: a new one */
        property int row: -1

        parent: Controls.Overlay.overlay
        onChosen: (troopId) => {
            if (row < 0) {
                root.currentIndex = root.encounters.insert(-1, troopId)
            } else {
                root.encounters.setTroop(row, troopId)
            }
            root.forceActiveFocus()
        }
    }

    Ez.MessageDialog {
        id: noTroop
        parent: Controls.Overlay.overlay
        title: qsTr("Define Encounter")
        text: qsTr("The database has no monster group.")
    }
}
