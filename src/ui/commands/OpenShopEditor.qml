// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "OpenShopWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Call Shop: what can be done in the shop, its messages and its handlers on
 * the left, the goods for sale on the right (the parameters after the
 * fourth, each item once, as RPG Maker). CommandEditorRegistry adds or
 * removes the transaction branches on OK.
 */
RowLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTr("Open Shop Window")
    readonly property int revision: session.revision
    /** The IDs of the items for sale */
    readonly property var goods: revision < 0 ? null : (session.value("/parameters") ?? []).slice(4)

    /** The goods become these */
    function setGoods(ids) {
        const parameters = (session.value("/parameters") ?? []).slice(0, 4)
        while (parameters.length < 4) {
            parameters.push(0)
        }
        session.set("/parameters", parameters.concat(ids))
    }

    spacing: 2 * Ez.Metrics.columnSpacing
    Layout.fillWidth: true
    Layout.fillHeight: true

    ColumnLayout {
        spacing: Ez.Metrics.sectionSpacing
        Layout.alignment: Qt.AlignTop

        Ez.Section {
            title: qsTr("Transactions Available")
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
        Ez.Section {
            title: qsTr("Shop Message Set")
            CommandField {
                session: root.session
                fieldId: "messages"
                name: "shopMessageSet"
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
        Item {
            Layout.fillHeight: true
        }
    }

    Ez.Section {
        title: qsTr("Goods for Sale")
        // a shop that only buys sells nothing
        enabled: root.revision < 0 ? false : root.session.read("transactions") !== 2
        Layout.fillHeight: true
        Layout.fillWidth: true

        Ez.ArrayTable {
            id: list
            objectName: "shopGoods"
            showHeader: false
            rows: root.goods.length
            source: root.session
            visibleRows: 10
            columns: [{ text: row => root.session.entryName("items", root.goods[row]) || "%1: %2".arg(String(root.goods[row]).padStart(4, "0")).arg(qsTr("Unknown")) }]
            Layout.fillWidth: true
            Layout.fillHeight: true
            onRemoveRequested: (row) => remove.clicked()
        }
        Ez.RefComboBox {
            id: item
            objectName: "shopItem"
            database: root.session.database
            listKey: "items"
            Layout.fillWidth: true
            Component.onCompleted: currentIndex = 0
        }
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.Button {
                objectName: "shopAddItem"
                text: qsTr("Add")
                Layout.fillWidth: true
                enabled: item.currentIndex >= 0
                onClicked: {
                    // an item is sold once, like in RPG Maker
                    const id = Number(item.currentValue)
                    const at = root.goods.indexOf(id)
                    if (at >= 0) {
                        list.currentIndex = at
                        return
                    }
                    root.setGoods(root.goods.concat([id]))
                    list.currentIndex = root.goods.length - 1
                }
            }
            Ez.Button {
                id: remove
                objectName: "shopRemoveItem"
                text: qsTr("Remove")
                Layout.fillWidth: true
                enabled: list.currentIndex >= 0 && list.currentIndex < root.goods.length
                onClicked: {
                    if (!enabled) {
                        return
                    }
                    const row = list.currentIndex
                    const ids = root.goods.slice()
                    ids.splice(row, 1)
                    root.setGoods(ids)
                    list.currentIndex = Math.min(row, ids.length - 1)
                }
            }
        }
    }
}
