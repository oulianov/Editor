// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * An entry of a list of the database (a switch, an item...) for data that
 * is not JSON (the settings of an event page, the triggers of a battle
 * page): the ID is given (value), the choice is told (chosen), and "..."
 * picks the entry in the window of the list.
 *
 *   RefChoice { database: Ez.ProjectData.database(); listKey: "switches"
 *               value: page.switchAId; onChosen: (id) => page.switchAId = id }
 */
Ez.RefComboBox {
    id: root

    /** The ID shown */
    property int value: 0
    /** The user chose an ID */
    signal chosen(int value)

    pickable: true
    Layout.fillWidth: true
    Binding on currentIndex {
        value: {
            // again once the entries come
            root.count
            return root.indexOfValue(root.value)
        }
        restoreMode: Binding.RestoreNone
    }
    onActivated: chosen(currentValue)
    // the ID is not JSON: told, not written (the picker edits the copy of
    // the database shown, the project's or a database window's)
    pickHandler: function() {
        const id = Ez.Pickers.pickEntry({ list: listKey, id: value, database: database }, root)
        if (id !== null && id !== undefined) {
            chosen(id)
        }
        return id
    }
}
