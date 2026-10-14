// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "TestBattlePartyWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The party of the battle tests, as in RPG Maker's Test Battle window: how
 * many heroes, and for each one the hero, the level and the equipment,
 * with the parameters they give. The EasyRPG Player reads it from the
 * saved database. Its data and rules are party's (Ez.TestBattlePartyModel):
 * the Run Game window and the Test Battle page show it.
 */
ColumnLayout {
    id: root

    required property Ez.TestBattlePartyModel party

    spacing: Ez.Metrics.sectionSpacing

    Ez.Section {
        objectName: "groupPartySize"
        title: qsTr("Number of Party Members")

        RowLayout {
            spacing: 2 * Ez.Metrics.columnSpacing

            Repeater {
                model: 5
                delegate: Ez.RadioButton {
                    id: radio
                    required property int index

                    objectName: "radioPartySize" + index
                    text: index === 0 ? qsTr("None") : String(index)
                    onToggled: {
                        if (checked) {
                            root.party.size = index
                        }
                    }
                    Binding on checked {
                        value: root.party.size === radio.index
                    }
                }
            }
        }
    }

    Ez.Section {
        title: qsTr("Character Parameters")
        Layout.fillHeight: true

        Controls.TabBar {
            id: tabs
            objectName: "tabPartyMembers"
            font: Ez.Metrics.font
            enabled: root.party.size > 0 && root.party.heroes.length > 0
            Layout.fillWidth: true

            Repeater {
                model: 4
                delegate: Controls.TabButton {
                    required property int index

                    text: qsTr("Hero %1").arg(index + 1)
                    width: implicitWidth
                    enabled: index < root.party.size
                }
            }
        }
        // a member taken out of the party: the last one left
        Connections {
            target: root.party
            function onSizeChanged() {
                if (tabs.currentIndex >= root.party.size) {
                    tabs.currentIndex = Math.max(0, root.party.size - 1)
                }
            }
        }

        StackLayout {
            currentIndex: tabs.currentIndex
            enabled: tabs.enabled
            Layout.fillWidth: true
            Layout.fillHeight: true

            Repeater {
                model: root.party
                delegate: RowLayout {
                    id: member

                    required property int index
                    required property int heroIndex
                    required property int level
                    required property int maxLevel
                    required property bool twoWeapons
                    required property var slotChoices
                    required property var slotIndexes
                    required property var parameters
                    required property bool inParty
                    readonly property string suffix: String(index + 1)

                    spacing: Ez.Metrics.columnSpacing
                    enabled: inParty

                    ColumnLayout {
                        spacing: Ez.Metrics.sectionSpacing
                        Layout.alignment: Qt.AlignTop
                        Layout.fillWidth: true

                        Ez.Form {
                            Ez.FormLabel {
                                text: qsTr("Hero")
                            }
                            Ez.ComboBox {
                                id: hero
                                objectName: "comboHero" + member.suffix
                                textRole: ""
                                valueRole: ""
                                model: root.party.heroes
                                Layout.fillWidth: true
                                onActivated: index => root.party.setHero(member.index, index)
                                Binding on currentIndex {
                                    value: hero.count > 0 ? member.heroIndex : -1
                                }
                            }
                            Ez.FormLabel {
                                text: qsTr("Level")
                            }
                            RowLayout {
                                spacing: Ez.Metrics.columnSpacing

                                Ez.SpinBox {
                                    objectName: "spinLevel" + member.suffix
                                    // the party keeps the level in this range
                                    from: 1
                                    to: member.maxLevel
                                    onValueModified: root.party.setLevel(member.index, value)
                                    Binding on value {
                                        value: member.level
                                    }
                                }
                                Ez.Button {
                                    objectName: "pushInitialStats" + member.suffix
                                    text: qsTr("Initial Stats")
                                    Controls.ToolTip.visible: hovered
                                    Controls.ToolTip.text: qsTr("The level and equipment the hero starts the game with")
                                    onClicked: root.party.initialStats(member.index)
                                }
                            }
                        }

                        Ez.Section {
                            title: qsTr("Equipment")

                            // TestBattleParty::Slot: the labels, then their combo boxes
                            Ez.Form {
                                id: equipment
                                readonly property list<string> names: ["comboWeapon", "comboShield", "comboArmor",
                                    "comboHelmet", "comboAccessory"]
                                // heroes with two weapons hold a weapon in the shield slot
                                readonly property list<string> labels: [qsTr("Weapon"),
                                    member.twoWeapons ? qsTr("Weapon") : qsTr("Shield"),
                                    qsTr("Armor"), qsTr("Helmet"), qsTr("Accessory")]

                                flow: GridLayout.TopToBottom
                                rows: names.length

                                Repeater {
                                    model: equipment.names.length
                                    delegate: Ez.FormLabel {
                                        required property int index
                                        text: equipment.labels[index]
                                    }
                                }
                                Repeater {
                                    model: equipment.names.length
                                    delegate: Ez.ComboBox {
                                        id: slot
                                        required property int index

                                        objectName: equipment.names[index] + member.suffix
                                        textRole: ""
                                        valueRole: ""
                                        model: member.slotChoices[index]
                                        Layout.fillWidth: true
                                        onActivated: choice => root.party.setEquipment(member.index, index, choice)
                                        Binding on currentIndex {
                                            value: slot.count > 0 ? member.slotIndexes[slot.index] : -1
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Ez.Section {
                        title: qsTr("Parameters")
                        Layout.alignment: Qt.AlignTop
                        Layout.fillWidth: false

                        // TestBattleParty::Parameter: the labels, then the values
                        Ez.Form {
                            id: stats
                            readonly property list<string> names: [qsTr("Max HP"), qsTr("Max MP"), qsTr("Attack"),
                                qsTr("Defense"), qsTr("Intellect"), qsTr("Agility")]

                            flow: GridLayout.TopToBottom
                            rows: names.length

                            Repeater {
                                model: stats.names
                                delegate: Ez.FormLabel {
                                    required property string modelData
                                    text: modelData
                                }
                            }
                            Repeater {
                                model: stats.names.length
                                delegate: Ez.Label {
                                    required property int index
                                    objectName: "labelParameter" + index + "_" + member.suffix
                                    text: member.parameters[index] ?? ""
                                    font: Ez.Metrics.font
                                    horizontalAlignment: Text.AlignRight
                                    Layout.minimumWidth: 40
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
