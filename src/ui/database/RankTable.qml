// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ClassWidget"

import QtQuick
import org.easyrpg.editor as Ez

/**
 * The resistance of a hero or a class to each attribute or condition: a
 * line per entry of the list (names) with its rank, A to E. A click (or
 * Space) gives a line the next rank, as RPG Maker does; RPG Maker reads a
 * rank missing at the end of the array (key) as C.
 *
 *   RankTable { entry: page.entry; key: "attribute_ranks"; names: page.database.list("attributes") }
 */
Ez.ArrayTable {
    id: root

    /** The hero or class (a JsonView) */
    property var entry: null
    /** Its array of ranks (0: A ... 4: E) */
    property string key
    /** The attributes or conditions (a JsonListView) */
    property var names: null

    /** The rank of a line (C when missing) */
    function rank(row) {
        const ranks = entry ? entry.value(key) : null
        return ranks && row < ranks.length ? Number(ranks[row]) : 2
    }
    /** The next rank of a line (A after E); the array grows with C up to it */
    function cycle(row) {
        if (!entry || row < 0) {
            return
        }
        const ranks = Array.from(entry.value(key) ?? [])
        while (ranks.length <= row) {
            ranks.push(2)
        }
        ranks[row] = (Number(ranks[row]) + 1) % 5
        entry.set(key, ranks)
    }

    rows: names ? names.count : 0
    source: entry
    showHeader: false
    columns: [
        { icon: row => "qrc:/ranks/rank" + root.rank(row),
          text: row => root.names.str(row + "/name") },
    ]
    onClicked: (row) => cycle(row)

    // the names change on their pages too
    Connections {
        target: root.names
        function onDataChanged() { root.revision++ }
    }
}
