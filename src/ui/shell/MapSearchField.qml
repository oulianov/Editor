// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MainWindow"

import QtQuick
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The search above the map tree: the number of a map or a part of its name.
 * Return shows the first map found as a click on it in the tree (selected,
 * in the temporary tab): the map of the number typed, then the maps whose
 * names hold the text, in the order of the tree. Return again, the text
 * unchanged, shows the next one. Nothing found: the text is red. Escape empties it and
 * gives the keys back to the tree.
 */
Ez.TextField {
    id: root

    /** The map tree it searches */
    required property MapTree tree
    /** The text of the last search, the next Return goes on from its map */
    property string searched: ""
    /** The last Return found no map */
    property bool notFound: false

    /** The maps found, in the order Return shows them */
    function found(): var {
        const typed = text.trim().toLowerCase()
        const shell = tree.shell
        if (typed === "" || !shell) {
            return []
        }
        const number = /^\d+$/.test(typed) ? Number(typed) : -1
        const ids = []
        for (const id of shell.treeIds()) {
            // the maps, not the areas
            if (shell.treeType(id) !== 1) {
                continue
            }
            if (id === number) {
                ids.unshift(id)
            } else if (shell.treeName(id).toLowerCase().includes(typed)) {
                ids.push(id)
            }
        }
        return ids
    }
    function go() {
        const ids = found()
        notFound = text.trim() !== "" && ids.length === 0
        if (ids.length === 0) {
            return
        }
        const at = ids.indexOf(tree.selectedId)
        const id = text === searched && at >= 0 ? ids[(at + 1) % ids.length] : ids[0]
        searched = text
        tree.select(id, false)
        tree.reveal(id)
    }

    objectName: "treeMapSearch"
    placeholderText: qsTr("Map name or ID")
    enabled: tree.view.rows > 0
    color: notFound ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
    onTextChanged: notFound = false
    onAccepted: go()
    Keys.onEscapePressed: {
        text = ""
        tree.forceActiveFocus()
    }
}
