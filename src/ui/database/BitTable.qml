// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.easyrpg.editor as Ez

/**
 * A check box per entry of a list for a bit array of an entry (the
 * conditions a skill inflicts, the heroes who can use an item...): RPG
 * Maker stores the arrays up to the last
 * entry they need, the entries past their end read as missing, and a check
 * grows the array with missing bits. The check boxes are named name + row.
 *
 *   BitTable { entry: page.entry; key: "state_effects"; names: page.database.list("states") }
 */
Ez.ArrayTable {
    id: root

    /** The skill or item (a JsonView) */
    property var entry: null
    /** Its bit array */
    property string key
    /** The names of the lines: a list of the database (a JsonListView), or texts */
    property var names: null
    /** The value of the bits past the end of the array */
    property bool missing: false
    /** The check boxes' names, the row after it */
    property string name: ""

    readonly property bool texts: Array.isArray(names)

    rows: !names ? 0 : texts ? names.length : names.count
    source: entry
    showHeader: false
    columns: [
        { name: root.name,
          checked: row => root.entry ? root.entry.bit(root.key, row, root.missing) : false,
          toggle: (row, on) => {
              if (root.entry) {
                  root.entry.setBit(root.key, row, on, root.missing)
              }
          },
          text: row => root.texts ? root.names[row] : root.names.str(row + "/name") },
    ]
    onClicked: (row) => {
        if (entry) {
            entry.setBit(key, row, !entry.bit(key, row, missing), missing)
        }
    }

    // the names change on their pages too
    Connections {
        target: root.texts ? null : root.names
        ignoreUnknownSignals: true
        function onDataChanged() { root.revision++ }
    }
    onNamesChanged: revision++
}
