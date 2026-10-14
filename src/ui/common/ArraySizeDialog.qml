// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * RPG Maker's Array Size of a database list (a JsonListView): the number of
 * entries asked (spinArraySize), then, for fewer, a warning naming the
 * entries holding data it deletes (arraySizeWarning). confirmed(count)
 * once the size is accepted; the owner resizes the list (list.resize(count)) and
 * selects what it shows.
 *
 *   Ez.ArraySizeDialog { id: arraySize; list: pane.list; onConfirmed: (count) => pane.resize(count) }
 *   ...
 *   onClicked: arraySize.ask()
 */
Ez.NumberDialog {
    id: root

    /** The list resized */
    property var list: null
    /** The number or the warning is shown */
    readonly property bool busy: visible || warning.visible

    /** The size accepted, and the warning passed when it deletes entries with data */
    signal confirmed(int count)

    /** Opens it on the size of the list */
    function ask() {
        if (!list) {
            return
        }
        value = list.count
        // RPG Maker allows 5000 entries (larger lists made by patches stay possible)
        to = Math.max(5000, list.count)
        open()
    }

    title: qsTranslate("DatabaseSplitWidgetBase", "Array Size")
    label: qsTranslate("DatabaseSplitWidgetBase", "Array Size")
    spinName: "spinArraySize"

    Ez.MessageDialog {
        id: warning

        property int count: 0

        objectName: "arraySizeWarning"
        parent: Controls.Overlay.overlay
        title: qsTranslate("DatabaseSplitWidgetBase", "Array Size")
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        onAccepted: root.confirmed(count)
    }

    onChosen: (count) => {
        const rows = list ? list.count : 0
        if (count < 1 || count === rows) {
            return
        }
        if (count < rows) {
            const names = list.entriesWithData(count)
            if (names.length > 0) {
                const shown = 15
                let text = qsTranslate("DatabaseSplitWidgetBase", "Reducing the array size from %1 to %2 deletes %3 entries, %n of them with data:",
                                       "", names.length).arg(rows).arg(count).arg(rows - count)
                text += "\n\n" + names.slice(0, shown).join("\n")
                if (names.length > shown) {
                    text += "\n" + qsTranslate("DatabaseSplitWidgetBase", "and %n more", "", names.length - shown)
                }
                warning.text = text
                warning.count = count
                warning.open()
                return
            }
        }
        confirmed(count)
    }
}
