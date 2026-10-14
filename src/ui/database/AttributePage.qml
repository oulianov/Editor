// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "AttributeWidget"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * An attribute of the database: its name, whether weapons or magic deal
 * it, and the damage it does to each resistance rank (A to E; RPG Maker
 * 2003 allows a negative E rate, which heals).
 */
Ez.EntryPage {
    id: root

    /** The picture of a resistance rank (0: A) */
    component Rank: Image {
        property int rank
        source: "qrc:/ranks/rank" + rank
        sourceSize.width: 16
        sourceSize.height: 16
        Layout.alignment: Qt.AlignVCenter
    }

    Ez.Form {
        Ez.FormLabel {
            text: qsTr("Name:")
        }
        Ez.TextField {
            objectName: "lineName"
            jsonData: root.entry
            key: "name"
            limit: 8
            Layout.fillWidth: true
        }
    }

    Ez.Section {
        objectName: "groupBox_55"
        title: qsTr("Attribute Type")
        Ez.RadioGroup {
            jsonData: root.entry
            key: "type"
            choices: [
                { name: "radioWeapon", text: qsTr("Weapon") },
                { name: "radioMagic", text: qsTr("Magic") },
            ]
        }
    }

    Ez.Section {
        objectName: "groupBox_56"
        title: qsTr("Damage Multiplier")
        // the ranks A to E and the rate of each
        Ez.Form {
            Layout.fillWidth: false
            Rank {
                rank: 0
            }
            Ez.SpinBox {
                objectName: "spinA"
                jsonData: root.entry
                key: "a_rate"
                from: 0
                to: 100000
                suffix: qsTr(" %")
            }
            Rank {
                rank: 1
            }
            Ez.SpinBox {
                objectName: "spinB"
                jsonData: root.entry
                key: "b_rate"
                from: 0
                to: 100000
                suffix: qsTr(" %")
            }
            Rank {
                rank: 2
            }
            Ez.SpinBox {
                objectName: "spinC"
                jsonData: root.entry
                key: "c_rate"
                from: 0
                to: 100000
                suffix: qsTr(" %")
            }
            Rank {
                rank: 3
            }
            Ez.SpinBox {
                objectName: "spinD"
                jsonData: root.entry
                key: "d_rate"
                from: 0
                to: 100000
                suffix: qsTr(" %")
            }
            Rank {
                rank: 4
            }
            Ez.SpinBox {
                objectName: "spinE"
                jsonData: root.entry
                key: "e_rate"
                from: root.rpg2k3 ? -99999 : 0
                to: 100000
                suffix: qsTr(" %")
            }
        }
    }
}
