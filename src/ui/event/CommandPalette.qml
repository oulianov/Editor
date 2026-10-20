// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EventCommandPalette"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Picks the kind of command to insert, like RPG Maker's "Event Commands"
 * window: pages of buttons in groups, two columns a page, and a search
 * over all of them (Enter picks the command of that name, else the first
 * match). Opened by CommandEditing::pickCommand, which reads code.
 *
 * pages: [{title, entries: [{code, name, source, rpg2k3Only, maniacOnly}]}], an
 * entry of code 0 is the gap between two groups; the one of code -1 (in a
 * map event, a common event) opens the battle event commands (battlePages)
 * in this window again, over it: Cancel comes back here, a command chosen
 * there is this one's.
 */
Ez.DialogWindow {
    id: root

    property var pages: []
    /** The pages of the battle event commands, which the entry of code -1 opens */
    property var battlePages: []
    /** The window this one was opened from (by its entry of the battle event commands): the command chosen is its too */
    property var opener: null
    /** The text searched at first */
    property string searchText: ""
    /** The commands of a battle */
    property bool battle: false
    /** The game is RPG Maker 2003's, uses the Maniac Patch: their commands can be chosen */
    property bool rpg2k3: true
    property bool maniac: false
    /** The command chosen (0: none) */
    property int code: 0

    /** Chooses the command of an entry; the one of the battle event commands opens them */
    function choose(entry) {
        if (entry.code < 0) {
            openBattleCommands()
            return
        }
        root.code = entry.code
        if (opener) {
            opener.code = entry.code
        }
        root.accept()
    }
    /** The battle event commands over this window; the search goes on there when it found some of them */
    function openBattleCommands() {
        const search = searchField.text.trim()
        const found = battleNamed(folded(search), false).length > 0
        if (Ez.Editor.exec("CommandPalette", { pages: battlePages, battle: true, rpg2k3: rpg2k3, maniac: maniac,
                                               opener: root, searchText: found ? search : "" }, root.contentItem) === 1) {
            root.accept()
        }
    }
    /** Why a command cannot be chosen, empty when it can */
    function whyNot(entry) {
        if (entry.rpg2k3Only && !rpg2k3) {
            return qsTr("Only in RPG Maker 2003")
        }
        if (entry.maniacOnly && !maniac) {
            return qsTr("Only with the Maniac Patch")
        }
        return ""
    }
    /** The groups of a page in two columns: the second one starts at the group that best balances them */
    function columnsOf(page) {
        const groups = [[]]
        for (const entry of page.entries) {
            if (entry.code === 0) {
                groups.push([])
            } else {
                groups[groups.length - 1].push(entry)
            }
        }
        let total = 0
        for (const group of groups) {
            total += group.length
        }
        let split = groups.length
        let best = total
        let before = 0
        for (let i = 0; i < groups.length; ++i) {
            const difference = Math.abs(total - 2 * before)
            if (i > 0 && difference < best) {
                best = difference
                split = i
            }
            before += groups[i].length
        }
        return [groups.slice(0, split), groups.slice(split)]
    }
    /** A text to search in: lower case, without accents ("téléporter": "teleporter") */
    function folded(text) {
        return text.normalize("NFD").replace(/[\u0300-\u036f]/g, "").toLowerCase()
    }
    /** The battle event commands the pages do not have (those of the enemies, the battle's Conditional Branch...) */
    readonly property var battleOnly: {
        const codes = new Set()
        for (const page of pages) {
            for (const entry of page.entries) {
                codes.add(entry.code)
            }
        }
        const found = []
        for (const page of battlePages) {
            for (const entry of page.entries) {
                if (entry.code !== 0 && !codes.has(entry.code)) {
                    found.push(entry)
                }
            }
        }
        return found
    }
    /** Those of them named so: the search finds them through the entry that opens them */
    function battleNamed(search, exact) {
        return search === "" ? [] : battleOnly.filter(entry => named(entry, search, exact))
    }
    /** The entry is named so, in the language of the Editor or in English; that of the battle event commands when one of them is */
    function named(entry, search, exact) {
        if (entry.code < 0 && battleNamed(search, exact).length > 0) {
            return true
        }
        for (const name of [entry.name, entry.source ?? ""]) {
            const text = folded(name)
            if (exact ? text === search : text.includes(search)) {
                return true
            }
        }
        return false
    }
    /** The commands whose name (or English name) has the text, in the order of the pages and of their columns */
    readonly property var matches: {
        const search = folded(searchField.text.trim())
        if (search === "") {
            return []
        }
        const found = []
        for (const page of pages) {
            for (const column of columnsOf(page)) {
                for (const group of column) {
                    for (const entry of group) {
                        if (root.named(entry, search, false)) {
                            // the battle event commands it opens: those found
                            const inside = entry.code < 0 ? root.battleNamed(search, false).map(one => one.name) : []
                            // the title of the page as its tab shows it ("&&": "&")
                            found.push({ entry: entry, page: page.title.replace(/&&/g, "&"), battle: inside })
                        }
                    }
                }
            }
        }
        return found
    }

    objectName: "EventCommandPalette"
    title: battle ? qsTr("Battle Event Commands") : qsTr("Event Commands")
    standardButtons: Controls.DialogButtonBox.Cancel | Controls.DialogButtonBox.Help

    Connections {
        target: root.buttonBox
        function onHelpRequested() {
            Ez.Editor.help("command-palette", root.contentItem)
        }
    }

    Ez.TextField {
        id: searchField
        objectName: "search"
        placeholderText: qsTr("Search a command, Enter picks the best match")
        text: root.searchText
        Layout.fillWidth: true
        focus: true
        Component.onCompleted: forceActiveFocus()
        onAccepted: {
            // the command of that name, else the first result that can be chosen
            const search = root.folded(text.trim())
            let first = null
            for (const match of root.matches) {
                if (root.whyNot(match.entry) !== "") {
                    continue
                }
                if (root.named(match.entry, search, true)) {
                    root.choose(match.entry)
                    return
                }
                first = first ?? match.entry
            }
            if (first) {
                root.choose(first)
            }
        }
    }

    // the results of a search replace the pages, which would hide the
    // matches of the other pages; the window keeps the size of the pages
    StackLayout {
        currentIndex: searchField.text.trim() === "" ? 0 : 1
        Layout.fillWidth: true
        Layout.fillHeight: true

        ColumnLayout {
            spacing: 0

            Controls.TabBar {
                id: tabs
                objectName: "tabPages"
                Layout.fillWidth: true
                font: Ez.Metrics.font

                Repeater {
                    model: root.pages
                    Controls.TabButton {
                        required property var modelData
                        // the titles escape "&" as "&&" (a mnemonic's), which these tabs show as written
                        text: modelData.title.replace(/&&/g, "&")
                        width: implicitWidth
                        font: Ez.Metrics.font
                    }
                }
            }

            StackLayout {
                currentIndex: tabs.currentIndex
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: Ez.Metrics.sectionSpacing

                Repeater {
                    model: root.pages

                    RowLayout {
                        id: page

                        required property var modelData

                        /** The width of the widest column: both have it (their names are not cut) */
                        readonly property real columnWidth: {
                            let widest = 0
                            for (const child of children) {
                                widest = Math.max(widest, child.implicitWidth)
                            }
                            return widest
                        }

                        spacing: 10

                        Repeater {
                            model: root.columnsOf(page.modelData)

                            ColumnLayout {
                                id: column

                                required property var modelData

                                spacing: 10
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.preferredWidth: page.columnWidth
                                Layout.alignment: Qt.AlignTop

                                Repeater {
                                    model: column.modelData

                                    ColumnLayout {
                                        id: group

                                        required property var modelData

                                        spacing: 2
                                        Layout.fillWidth: true

                                        Repeater {
                                            model: group.modelData

                                            Ez.Button {
                                                id: button

                                                required property var modelData
                                                readonly property string why: root.whyNot(modelData)

                                                text: modelData.name + "..."
                                                enabled: why === ""
                                                Layout.fillWidth: true
                                                // a greyed button still says why
                                                Controls.ToolTip.visible: why !== "" && hover.hovered
                                                Controls.ToolTip.text: why
                                                onClicked: root.choose(modelData)
                                                HoverHandler {
                                                    id: hover
                                                }
                                            }
                                        }
                                    }
                                }
                                Item {
                                    Layout.fillHeight: true
                                }
                            }
                        }
                    }
                }
            }
        }

        Controls.ScrollView {
            objectName: "searchResults"
            implicitHeight: 120
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 2

                Repeater {
                    model: root.matches

                    // the button in an item: it has its parent as soon as it
                    // is made (Breeze reads it), which a Repeater's own items
                    // do not, made again at each letter typed
                    Item {
                        id: result

                        required property var modelData
                        readonly property string why: root.whyNot(modelData.entry)

                        implicitWidth: resultButton.implicitWidth
                        implicitHeight: resultButton.implicitHeight
                        Layout.fillWidth: true

                        Ez.Button {
                            id: resultButton
                            anchors.fill: parent
                            text: result.modelData.entry.name + "..."
                            enabled: result.why === ""
                            // the page of the command, as its tooltip (the battle event commands found there)
                            Controls.ToolTip.visible: resultHover.hovered
                            Controls.ToolTip.text: result.why !== "" ? result.why
                                : result.modelData.battle.length > 0 ? result.modelData.battle.join(", ") : result.modelData.page
                            onClicked: root.choose(result.modelData.entry)
                            HoverHandler {
                                id: resultHover
                            }
                        }
                    }
                }
                Ez.Button {
                    visible: root.matches.length === 0
                    enabled: false
                    text: qsTr("No command matches \"%1\"").arg(searchField.text.trim())
                    Layout.fillWidth: true
                }
            }
        }
    }
}
