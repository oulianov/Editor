// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "EnemyWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A monster: its name, graphic, statistics, spoils, its behavior (the
 * actions it chooses in battle, edited in EnemyActionDialog.qml: the empty
 * last line adds one) and its resistances (a click gives a line the next
 * rank, A to E).
 */
Ez.EntryPage {
    id: root

    readonly property var actions: entry ? entry.list("actions") : null
    /** An action copied (Copy, Cut), pasted before the current line */
    property var clipboard: null
    readonly property var actionKeys: ["kind", "basic", "skill_id", "enemy_id", "condition_type", "condition_param1",
        "condition_param2", "switch_id", "switch_on", "switch_on_id", "switch_off", "switch_off_id", "rating"]

    readonly property var basicNames: [
        qsTranslate("EnemyActionDialog", "Attack"), qsTranslate("EnemyActionDialog", "Double Attack"),
        qsTranslate("EnemyActionDialog", "Defend"), qsTranslate("EnemyActionDialog", "Observe Battle"),
        qsTranslate("EnemyActionDialog", "Charge Up"), qsTranslate("EnemyActionDialog", "Self-Destruct"),
        qsTranslate("EnemyActionDialog", "Escape"), qsTranslate("EnemyActionDialog", "No Action"),
    ]

    /** The "Action" column of the behavior list */
    function actionText(row) {
        if (!actions || row < 0 || row >= actions.count) {
            return ""
        }
        const prefix = "actions/" + row + "/"
        switch (entry.num(prefix + "kind")) {
        case 1:
            return nameOf("skills", entry.num(prefix + "skill_id"))
        case 2:
            return qsTranslate("EnemyActionDialog", "Transform into %1").arg(nameOf("enemies", entry.num(prefix + "enemy_id")))
        default: {
            const basic = entry.num(prefix + "basic")
            const names = basicNames
            return basic >= 0 && basic < names.length ? names[basic] : "<" + basic + "?>"
        }
        }
    }
    /** The "Precondition" column of the behavior list */
    function conditionText(row) {
        if (!actions || row < 0 || row >= actions.count) {
            return ""
        }
        const prefix = "actions/" + row + "/"
        const p1 = entry.num(prefix + "condition_param1")
        const p2 = entry.num(prefix + "condition_param2")
        const range = p1 + "-" + p2
        switch (entry.num(prefix + "condition_type")) {
        case 1: {
            const id = entry.num(prefix + "switch_id")
            return qsTranslate("EnemyActionDialog", "Switch [%1] ON").arg(String(id).padStart(4, "0") + ":" + nameOf("switches", id))
        }
        case 2:
            // the same forms as RPG Maker: only the parts that are set
            if (p1 === 0) {
                return qsTranslate("EnemyActionDialog", "Turns Elapsed [%1]").arg(p2)
            } else if (p2 === 0) {
                return qsTranslate("EnemyActionDialog", "Turns Elapsed [%1x]").arg(p1)
            }
            return qsTranslate("EnemyActionDialog", "Turns Elapsed [%1x+%2]").arg(p1).arg(p2)
        case 3:
            return qsTranslate("EnemyActionDialog", "Monsters Present [%1]").arg(range)
        case 4:
            return qsTranslate("EnemyActionDialog", "Monster HP between [%1]%").arg(range)
        case 5:
            return qsTranslate("EnemyActionDialog", "Monster MP between [%1]%").arg(range)
        case 6:
            return qsTranslate("EnemyActionDialog", "Average Character Level [%1]").arg(range)
        case 7:
            return qsTranslate("EnemyActionDialog", "Party Exhaustion [%1]%").arg(range)
        default:
            return qsTranslate("EnemyActionDialog", "No Precondition")
        }
    }
    function nameOf(list, id) {
        const entries = database ? database.list(list) : null
        return entries && id >= 1 && id <= entries.count ? entries.str((id - 1) + "/name") : "<" + id + "?>"
    }

    /** Edits the action of a line (the empty last line adds one) */
    function editAction(row) {
        if (!actions || row < 0) {
            return
        }
        const added = row >= actions.count
        if (added) {
            row = actions.count
            actions.insertEntries(row, 1)
        }
        const accepted = Ez.Editor.exec("EnemyActionDialog", { database: database, action: entry.subtree("actions/" + row) }, actionTable)
        if (!accepted && added) {
            actions.removeEntries(row, 1)
        }
        actionTable.currentIndex = row
    }
    function copyAction(cut) {
        const row = actionTable.currentIndex
        if (!actions || row < 0 || row >= actions.count) {
            return
        }
        const copy = {}
        for (const key of actionKeys) {
            copy[key] = entry.value("actions/" + row + "/" + key)
        }
        clipboard = copy
        if (cut) {
            deleteAction()
        }
    }
    function pasteAction() {
        if (!actions || !clipboard) {
            return
        }
        // before the current line, as RPG Maker
        const row = Math.max(0, Math.min(actionTable.currentIndex < 0 ? actions.count : actionTable.currentIndex, actions.count))
        actions.insertEntries(row, 1)
        entry.beginBatch()
        for (const key of actionKeys) {
            entry.set("actions/" + row + "/" + key, clipboard[key])
        }
        entry.endBatch()
        actionTable.currentIndex = row
    }
    function deleteAction() {
        const row = actionTable.currentIndex
        if (actions && row >= 0 && row < actions.count) {
            actions.removeEntries(row, 1)
        }
    }

    fillWidth: true

    GridLayout {
        columns: 3
        rowSpacing: Ez.Metrics.sectionSpacing
        columnSpacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true

        // the name, the graphic, the statistics
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.fillHeight: true

            Ez.Section {
                objectName: "groupName"
                title: qsTr("Name")
                Ez.TextField {
                    objectName: "lineName"
                    jsonData: root.entry
                    key: "name"
                    limit: 20
                    Layout.fillWidth: true
                }
            }

            Ez.Section {
                objectName: "groupGraphic"
                title: qsTr("Monster Graphic")
                Layout.fillHeight: true

                Ez.SpriteField {
                    id: graphic
                    objectName: "graphicsMonster"
                    preview: "Monster"
                    jsonData: root.entry
                    key: "battler_name"
                    hueKey: "battler_hue"
                    title: qsTr("Select Monster Graphic")
                    transparent: transparent.checked
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 128
                    Controls.ToolTip.visible: graphicHover.hovered
                    Controls.ToolTip.delay: 600
                    Controls.ToolTip.text: qsTr("Click to choose the graphic")
                    HoverHandler {
                        id: graphicHover
                    }
                }
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true
                    Ez.Button {
                        objectName: "pushSetGraphic"
                        text: qsTr("Set...")
                        onClicked: graphic.pick()
                    }
                    Ez.CheckBox {
                        id: transparent
                        objectName: "checkTransparent"
                        text: qsTr("Transparent")
                        jsonData: root.entry
                        key: "transparent"
                    }
                    Ez.CheckBox {
                        objectName: "checkFlying"
                        text: qsTr("Flying")
                        jsonData: root.entry
                        key: "levitate"
                    }
                }
            }

            Ez.Section {
                objectName: "groupStats"
                title: qsTr("Base Statistics")

                // limits of RPG Maker's monster editor
                GridLayout {
                    columns: 2
                    rowSpacing: 2
                    columnSpacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true

                    Ez.Label { objectName: "labelMaxHp"; text: qsTr("Max HP"); font: Ez.Metrics.font }
                    Ez.Label { objectName: "labelMaxSp"; text: qsTr("Max MP"); font: Ez.Metrics.font }
                    Ez.SpinBox {
                        objectName: "spinMaxHp"
                        jsonData: root.entry
                        key: "max_hp"
                        from: 1
                        to: root.rpg2k3 ? 99999 : 9999
                        Layout.fillWidth: true
                    }
                    Ez.SpinBox {
                        objectName: "spinMaxSp"
                        jsonData: root.entry
                        key: "max_sp"
                        from: 0
                        to: root.rpg2k3 ? 9999 : 999
                        Layout.fillWidth: true
                    }
                    Ez.Label { objectName: "labelAttack"; text: qsTr("Attack"); font: Ez.Metrics.font }
                    Ez.Label { objectName: "labelDefense"; text: qsTr("Defense"); font: Ez.Metrics.font }
                    Ez.SpinBox {
                        objectName: "spinAttack"
                        jsonData: root.entry
                        key: "attack"
                        from: 1
                        to: 999
                        Layout.fillWidth: true
                    }
                    Ez.SpinBox {
                        objectName: "spinDefense"
                        jsonData: root.entry
                        key: "defense"
                        from: 1
                        to: 999
                        Layout.fillWidth: true
                    }
                    Ez.Label { objectName: "labelSpirit"; text: qsTr("Intellect"); font: Ez.Metrics.font }
                    Ez.Label { objectName: "labelAgility"; text: qsTr("Agility"); font: Ez.Metrics.font }
                    Ez.SpinBox {
                        objectName: "spinSpirit"
                        jsonData: root.entry
                        key: "spirit"
                        from: 1
                        to: 999
                        Layout.fillWidth: true
                    }
                    Ez.SpinBox {
                        objectName: "spinAgility"
                        jsonData: root.entry
                        key: "agility"
                        from: 1
                        to: 999
                        Layout.fillWidth: true
                    }
                }
            }
        }

        // the spoils, the options, the behavior
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.fillHeight: true

            Ez.Section {
                objectName: "groupSpoils"
                title: qsTr("Spoils")
                Ez.Form {
                    Ez.FormLabel { objectName: "labelExp"; text: qsTr("Experience") }
                    Ez.SpinBox {
                        objectName: "spinExp"
                        jsonData: root.entry
                        key: "exp"
                        from: 0
                        to: 999999
                    }
                    Ez.FormLabel { objectName: "labelGold"; text: qsTr("Money") }
                    Ez.SpinBox {
                        objectName: "spinGold"
                        jsonData: root.entry
                        key: "gold"
                        from: 0
                        to: 999999
                    }
                    Ez.FormLabel { objectName: "labelDropItem"; text: qsTr("Item") }
                    Ez.RefComboBox {
                        objectName: "comboDropItem"
                        database: root.database
                        listKey: "items"
                        noneText: qsTr("(None)")
                        jsonData: root.entry
                        key: "drop_id"
                        Layout.fillWidth: true
                    }
                    Ez.FormLabel { objectName: "labelDropProb"; text: qsTr("Probability") }
                    Ez.SpinBox {
                        objectName: "spinDropProb"
                        jsonData: root.entry
                        key: "drop_prob"
                        from: 0
                        to: 100
                        suffix: qsTr("%")
                    }
                }
            }

            RowLayout {
                spacing: Ez.Metrics.sectionSpacing
                Layout.fillWidth: true

                Ez.Section {
                    objectName: "groupCritical"
                    title: qsTr("Critical Hit Probability")
                    Layout.fillHeight: true
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        Ez.CheckBox {
                            id: critical
                            objectName: "checkCritical"
                            text: qsTr("One in")
                            jsonData: root.entry
                            key: "critical_hit"
                        }
                        Ez.SpinBox {
                            objectName: "spinCritChance"
                            jsonData: root.entry
                            key: "critical_hit_chance"
                            from: 1
                            to: 100
                            enabled: critical.checked
                        }
                    }
                }
                Ez.Section {
                    objectName: "groupOption"
                    title: qsTr("Option")
                    Layout.fillHeight: true
                    Ez.CheckBox {
                        objectName: "checkMiss"
                        text: qsTr("Attacks often Miss")
                        jsonData: root.entry
                        key: "miss"
                    }
                }
            }

            Ez.Section {
                objectName: "groupActions"
                title: qsTr("Behavior")
                Layout.fillHeight: true

                Ez.ArrayTable {
                    id: actionTable
                    objectName: "treeActions"
                    rows: root.actions ? root.actions.count : 0
                    source: root.entry
                    addLine: true
                    visibleRows: 8
                    columns: [
                        { title: qsTr("Action"), text: row => root.actionText(row) },
                        { title: qsTr("Precondition"), text: row => root.conditionText(row) },
                        { title: qsTr("Priority"), width: 50, text: row => row >= 0 && row < root.actions.count ? String(root.entry.num("actions/" + row + "/rating")) : "" },
                    ]
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onActivated: (row) => root.editAction(row)
                    onRemoveRequested: (row) => root.deleteAction()

                    Keys.onPressed: (event) => {
                        if (event.matches(StandardKey.Copy)) {
                            root.copyAction(false)
                        } else if (event.matches(StandardKey.Cut)) {
                            root.copyAction(true)
                        } else if (event.matches(StandardKey.Paste)) {
                            root.pasteAction()
                        } else {
                            return
                        }
                        event.accepted = true
                    }
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: (eventPoint) => actionMenu.popupAt(parent, eventPoint.position.x, eventPoint.position.y)
                    }

                    Controls.ToolTip.visible: actionHover.hovered && !actionMenu.visible
                    Controls.ToolTip.delay: 600
                    Controls.ToolTip.text: qsTr("Double-click a line to edit it, or the empty line to add an action")
                    HoverHandler {
                        id: actionHover
                    }

                    Ez.ContextMenu {
                        id: actionMenu
                        readonly property bool onAction: !!root.actions && actionTable.currentIndex >= 0
                                                         && actionTable.currentIndex < root.actions.count
                        font: Ez.Metrics.font
                        Controls.MenuItem {
                            objectName: "actionEdit"
                            text: qsTranslate("EditActions", "Edit...")
                            onTriggered: root.editAction(actionTable.currentIndex < 0 ? root.actions.count : actionTable.currentIndex)
                        }
                        Controls.MenuSeparator {}
                        Controls.MenuItem {
                            objectName: "actionCut"
                            text: qsTranslate("EditActions", "Cut")
                            enabled: actionMenu.onAction
                            onTriggered: root.copyAction(true)
                        }
                        Controls.MenuItem {
                            objectName: "actionCopy"
                            text: qsTranslate("EditActions", "Copy")
                            enabled: actionMenu.onAction
                            onTriggered: root.copyAction(false)
                        }
                        Controls.MenuItem {
                            objectName: "actionPaste"
                            text: qsTranslate("EditActions", "Paste")
                            enabled: !!root.clipboard
                            onTriggered: root.pasteAction()
                        }
                        Controls.MenuItem {
                            objectName: "actionDelete"
                            text: qsTranslate("EditActions", "Delete")
                            enabled: actionMenu.onAction
                            onTriggered: root.deleteAction()
                        }
                    }
                }
            }
        }

        // the resistances
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.fillHeight: true

            Ez.Section {
                objectName: "groupAttributeRanks"
                title: qsTr("Attribute Resist")
                Layout.fillHeight: true
                RankTable {
                    objectName: "listAttributeRanks"
                    entry: root.entry
                    key: "attribute_ranks"
                    implicitWidth: 130
                    names: root.database ? root.database.list("attributes") : null
                    visibleRows: 12
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
            Ez.Section {
                objectName: "groupStateRanks"
                title: qsTr("Condition Resist")
                Layout.fillHeight: true
                RankTable {
                    objectName: "listStateRanks"
                    entry: root.entry
                    key: "state_ranks"
                    implicitWidth: 130
                    names: root.database ? root.database.list("states") : null
                    visibleRows: 8
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
    }
}
