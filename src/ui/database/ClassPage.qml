// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ClassWidget"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A class of the database (Professions, RPG Maker 2003), in three
 * columns: its name, battle graphics, options and battle
 * commands; its parameter curves (a double-click edits one) and experience
 * curve; the skills it learns and its resistances. A hero takes all this
 * from its class with Apply on the Characters page.
 */
Ez.EntryPage {
    id: root

    readonly property int maxLevel: rpg2k3 ? 99 : 50
    readonly property int maxHp: rpg2k3 ? 9999 : 999

    fillWidth: true

    RowLayout {
        id: columns
        spacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true
        Layout.fillHeight: true

        // the name, graphics, options and battle commands
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.preferredWidth: (root.column.room - 2 * columns.spacing) / 3
            Layout.minimumWidth: implicitWidth

            Ez.Section {
                objectName: "groupName"
                title: qsTr("Name")
                Ez.TextField {
                    objectName: "lineName"
                    jsonData: root.entry
                    key: "name"
                    limit: 12
                    Layout.fillWidth: true
                }
            }
            Ez.Section {
                objectName: "groupAnimation"
                title: qsTr("Animation Group")
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    BattlerView {
                        objectName: "graphicsBattler"
                        database: root.database
                        battler: battlerField.valid ? Number(battlerField.value) : 0

                        Ez.JsonField {
                            id: battlerField
                            source: root.entry
                            path: "battler_animation"
                        }
                    }
                    Ez.ComboBox {
                        objectName: "comboBattler"
                        jsonData: root.entry
                        key: "battler_animation"
                        model: root.database ? root.database.choices("battleranimations", qsTr("<none>"), 0) : null
                        Layout.fillWidth: true
                    }
                }
            }
            Ez.Section {
                objectName: "groupOptions"
                title: qsTr("Options")
                Ez.CheckBox {
                    objectName: "checkAutoBattle"
                    text: qsTr("AI Control")
                    jsonData: root.entry
                    key: "auto_battle"
                }
                Ez.CheckBox {
                    objectName: "checkTwoWeapons"
                    text: qsTr("Two Weapons")
                    jsonData: root.entry
                    key: "two_weapon"
                }
                Ez.CheckBox {
                    objectName: "checkLockEquipment"
                    text: qsTr("Lock Equipment")
                    jsonData: root.entry
                    key: "lock_equipment"
                }
                Ez.CheckBox {
                    objectName: "checkMightyGuard"
                    text: qsTr("Mighty Guard")
                    jsonData: root.entry
                    key: "super_guard"
                }
            }
            BattleCommandBoxes {
                entry: root.entry
                database: root.database
            }
        }

        // the parameter and experience curves
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            Layout.preferredWidth: (root.column.room - 2 * columns.spacing) / 3
            Layout.minimumWidth: implicitWidth

            Ez.Section {
                objectName: "groupStatistics"
                title: qsTr("Basic Statistics")
                ParameterGraphs {
                    id: graphs
                    entry: root.entry
                    maxLevel: root.maxLevel
                    maxHp: root.maxHp
                    graphHeight: 36
                    names: ["graphicsHp", "graphicsMp", "graphicsAttack", "graphicsDefense",
                            "graphicsIntelligence", "graphicsAgility"]
                    titles: [qsTr("Maximum HP"), qsTr("Maximum MP"), qsTr("Attack Power"), qsTr("Defense Power"),
                             qsTr("Intelligence"), qsTr("Agility")]
                    Layout.fillWidth: true
                }
                Ez.Button {
                    objectName: "pushEditCurves"
                    text: qsTr("Edit Curves...")
                    Layout.fillWidth: true
                    onClicked: graphs.edit(0)
                }
            }
            ExperienceSummary {
                objectName: "groupExperience"
                entry: root.entry
                database: root.database
            }
        }

        // the skills learned and the resistances
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: (root.column.room - 2 * columns.spacing) / 3
            Layout.minimumWidth: implicitWidth

            Ez.Section {
                objectName: "groupSkills"
                title: qsTr("Skill Progression")
                Layout.fillHeight: true
                SkillTable {
                    objectName: "tableSkills"
                    entry: root.entry
                    database: root.database
                    maxLevel: root.maxLevel
                    visibleRows: 5
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
            Ez.Section {
                objectName: "groupStates"
                title: qsTr("Condition Resist")
                Layout.fillHeight: true
                RankTable {
                    objectName: "listStates"
                    entry: root.entry
                    key: "state_ranks"
                    names: root.database ? root.database.list("states") : null
                    visibleRows: 4
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
            Ez.Section {
                objectName: "groupAttributes"
                title: qsTr("Attribute Resist")
                Layout.fillHeight: true
                RankTable {
                    objectName: "listAttributes"
                    entry: root.entry
                    key: "attribute_ranks"
                    names: root.database ? root.database.list("attributes") : null
                    visibleRows: 4
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
            }
        }
    }
}
