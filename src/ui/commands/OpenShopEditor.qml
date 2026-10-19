// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "OpenShopWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Call Shop, as RPG Maker 2003's: what can be done in the shop, its
 * handlers and its messages, then the goods for sale (the parameters after
 * the fourth, each item once) beside all the items, "< Add" and "Remove >"
 * between them (or a double-click on an item). CommandEditorRegistry adds or
 * removes the transaction branches on OK.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Open Shop Window")
    readonly property int revision: session.revision
    /** The IDs of the items for sale */
    readonly property var goods: revision < 0 ? null : (session.value("/parameters") ?? []).slice(4)
    readonly property var items: session.database ? session.database.list("items") : null

    /** The goods become these */
    function setGoods(ids) {
        const parameters = (session.value("/parameters") ?? []).slice(0, 4)
        while (parameters.length < 4) {
            parameters.push(0)
        }
        session.set("/parameters", parameters.concat(ids))
    }

    /** The item chosen among all of them becomes one of the goods (once, as RPG Maker) */
    function addItem() {
        if (all.currentIndex < 0) {
            return
        }
        const id = all.currentIndex + 1
        const at = goods.indexOf(id)
        if (at < 0) {
            setGoods(goods.concat([id]))
        }
        list.currentIndex = at >= 0 ? at : goods.length - 1
    }

    function removeItem() {
        const row = list.currentIndex
        if (row < 0 || row >= goods.length) {
            return
        }
        const ids = goods.slice()
        ids.splice(row, 1)
        setGoods(ids)
        list.currentIndex = Math.min(row, ids.length - 1)
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true

    Ez.Section {
        title: qsTr("Transactions Available")
        RowLayout {
            spacing: 2 * Ez.Metrics.columnSpacing
            Repeater {
                model: 3
                CommandRadio {
                    required property int index
                    session: root.session
                    fieldId: "transactions"
                    value: index
                    objectName: "shopTransactions" + index
                }
            }
        }
    }
    Ez.Section {
        title: qsTr("Option")
        CommandField {
            session: root.session
            fieldId: "handlers"
            name: "shopHandlers"
        }
    }
    Ez.Section {
        title: qsTr("Shop Message Set")
        CommandField {
            session: root.session
            fieldId: "messages"
            name: "shopMessageSet"
            Layout.fillWidth: true
        }
    }

    Ez.Section {
        title: qsTr("Goods for Sale")
        // a shop that only buys sells nothing
        enabled: root.revision < 0 ? false : root.session.read("transactions") !== 2
        Layout.fillHeight: true
        Layout.fillWidth: true

        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Layout.fillWidth: true
            Layout.fillHeight: true

            Ez.ArrayTable {
                id: list
                objectName: "shopGoods"
                showHeader: false
                rows: root.goods.length
                source: root.session
                visibleRows: 8
                columns: [{ text: row => root.session.entryName("items", root.goods[row]) || "%1: %2".arg(String(root.goods[row]).padStart(4, "0")).arg(qsTr("Unknown")) }]
                Layout.preferredWidth: 200
                Layout.fillWidth: true
                Layout.fillHeight: true
                onActivated: (row) => root.removeItem()
                onRemoveRequested: (row) => root.removeItem()
            }
            ColumnLayout {
                spacing: Ez.Metrics.rowSpacing
                Layout.alignment: Qt.AlignVCenter
                Ez.Button {
                    objectName: "shopAddItem"
                    text: qsTr("< Add")
                    Layout.fillWidth: true
                    enabled: all.currentIndex >= 0
                    onClicked: root.addItem()
                }
                Ez.Button {
                    objectName: "shopRemoveItem"
                    text: qsTr("Remove >")
                    Layout.fillWidth: true
                    enabled: list.currentIndex >= 0 && list.currentIndex < root.goods.length
                    onClicked: root.removeItem()
                }
            }
            Ez.ArrayTable {
                id: all
                objectName: "shopItems"
                showHeader: false
                rows: root.items ? root.items.count : 0
                source: root.items
                visibleRows: 8
                columns: [{ text: row => root.session.entryName("items", row + 1) }]
                Layout.preferredWidth: 170
                Layout.fillWidth: true
                Layout.fillHeight: true
                onActivated: (row) => root.addItem()
                Component.onCompleted: currentIndex = 0
            }
        }
    }
}
