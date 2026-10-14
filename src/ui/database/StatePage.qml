// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "StateWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A condition of the database (Hero Status):
 * its name, how long it lasts, its color and priority, what it prevents,
 * how it is resisted and recovered from, its effects on HP and MP, its
 * battle messages (RPG Maker 2000) and the pose of the heroes (RPG Maker
 * 2003). As in RPG Maker, most of the first condition (Death) is fixed.
 */
DatabaseEntryPage {
    id: root

    readonly property int entryId: entry ? entry.num("ID") : 0
    /** The first condition is Death: the game fixes most of it */
    readonly property bool death: entryId === 1

    /** The battler animation of the first hero, whose poses are listed */
    readonly property int firstBattlerId: database && revision >= 0 && database.size("actors") > 0
                                          ? database.num("actors/0/battler_animation") : 0
    // the poses read again when the database changes (a renamed pose...)
    property int revision: 0
    Connections {
        target: root.database
        ignoreUnknownSignals: true
        function onValueChanged(pointer) {
            if (pointer === "" || pointer.startsWith("/actors/0") || pointer.startsWith("/battleranimations")) {
                root.revision++
            }
        }
    }

    /** The 32 poses of RPG Maker 2003, named as the first hero's (the defaults else) */
    readonly property var poseChoices: {
        const defaults = [qsTr("Idle Stance"), qsTr("Right Hand"), qsTr("Left Hand"), qsTr("Skill Use"),
                          qsTr("Dead"), qsTr("Damage"), qsTr("Bad Status"), qsTr("Defending"), qsTr("Walking Left"),
                          qsTr("Walking Right"), qsTr("Victory"), qsTr("Item")]
        const id = firstBattlerId
        const battlers = database ? database.list("battleranimations") : null
        const poses = battlers && id >= 1 && id <= battlers.count ? battlers.list((id - 1) + "/poses") : null
        const choices = []
        for (let i = 0; i < 32; ++i) {
            let name = poses && i < poses.count ? poses.str(i + "/name") : ""
            if (name === "") {
                name = defaults[i] ?? ""
            }
            choices.push({ text: String(i + 1).padStart(2, "0") + ": " + name, value: i })
        }
        // a pose out of the range stays visible
        if (poseOutside) {
            choices.push({ text: qsTr("<%1?>").arg(poseValue + 1), value: poseValue })
        }
        return choices
    }
    readonly property int poseValue: poseField.valid ? Number(poseField.value) : 0
    // the choices change only for a pose out of the range
    readonly property bool poseOutside: poseValue < 0 || poseValue >= 32

    fillWidth: true

    Ez.JsonField {
        id: poseField
        source: root.entry
        path: "battler_animation_id"
    }

    GridLayout {
        columns: 5
        rowSpacing: Ez.Metrics.sectionSpacing
        columnSpacing: Ez.Metrics.sectionSpacing
        Layout.fillWidth: true

        Ez.Section {
            objectName: "groupBox_257"
            title: qsTr("State Name")
            Layout.row: 0
            Layout.column: 0
            Ez.TextField {
                objectName: "lineName"
                jsonData: root.entry
                key: "name"
                limit: 8
                Layout.fillWidth: true
            }
        }
        Ez.Section {
            objectName: "groupBox_281"
            title: qsTr("Extents of effect")
            Layout.row: 0
            Layout.column: 1
            Ez.ComboBox {
                objectName: "comboExtents"
                jsonData: root.entry
                key: "type"
                model: [{ text: qsTr("Ends after battle"), value: 0 }, { text: qsTr("Persists after battle"), value: 1 }]
                Layout.fillWidth: true
            }
        }
        Ez.Section {
            objectName: "groupBox_282"
            title: qsTr("Color")
            Layout.row: 0
            Layout.column: 2
            Ez.SystemColorComboBox {
                objectName: "comboColor"
                database: root.database
                jsonData: root.entry
                key: "color"
                enabled: !root.death
                Layout.fillWidth: true
            }
        }
        Ez.Section {
            objectName: "groupBox_258"
            title: qsTr("Priority")
            Layout.row: 0
            Layout.column: 3
            Ez.SpinBox {
                objectName: "spinPriority"
                jsonData: root.entry
                key: "priority"
                from: 1
                to: 100
                enabled: !root.death
                Layout.fillWidth: true
            }
        }
        Ez.Section {
            objectName: "groupBox_283"
            title: qsTr("Action restriction")
            Layout.row: 0
            Layout.column: 4
            Ez.ComboBox {
                objectName: "comboRestriction"
                jsonData: root.entry
                key: "restriction"
                model: [{ text: qsTr("No restriction"), value: 0 }, { text: qsTr("No action allowed"), value: 1 },
                        { text: qsTr("Attack enemies randomly"), value: 2 }, { text: qsTr("Attack allies randomly"), value: 3 }]
                enabled: !root.death
                Layout.fillWidth: true
            }
        }

        Ez.Section {
            objectName: "groupBox_284"
            title: qsTr("Hit percentage")
            Layout.row: 1
            Layout.column: 0
            Ez.SpinBox {
                objectName: "spinAccuracy"
                jsonData: root.entry
                key: "reduce_hit_ratio"
                from: 0
                to: 100
                suffix: qsTr(" %")
                enabled: !root.death
                Layout.fillWidth: true
            }
        }
        Ez.Section {
            objectName: "groupBox_285"
            title: qsTr("Additional Effects")
            Layout.row: 1
            Layout.column: 1
            Layout.columnSpan: 4
            Layout.fillHeight: true
            // RPG Maker 2003's
            RowLayout {
                spacing: 2 * Ez.Metrics.columnSpacing
                enabled: root.rpg2k3 && !root.death
                Ez.CheckBox {
                    objectName: "checkEvadeAll"
                    text: qsTr("Evade All Attacks")
                    jsonData: root.entry
                    key: "avoid_attacks"
                    Layout.fillWidth: true
                }
                Ez.CheckBox {
                    objectName: "checkReflectSkills"
                    text: qsTr("Reflect Skills")
                    jsonData: root.entry
                    key: "reflect_magic"
                    Layout.fillWidth: true
                }
                Ez.CheckBox {
                    objectName: "checkNoEquipmentChange"
                    text: qsTr("Equipment cannot be changed")
                    jsonData: root.entry
                    key: "cursed"
                    Layout.fillWidth: true
                }
            }
        }

        Ez.Section {
            objectName: "groupBox_286"
            title: qsTr("Susceptibility")
            Layout.row: 2
            Layout.column: 0
            Layout.fillHeight: true
            // the chance of each resistance rank, A to E
            Repeater {
                model: 5
                RowLayout {
                    id: rateRow
                    required property int index
                    spacing: Ez.Metrics.columnSpacing
                    Image {
                        source: "qrc:/ranks/rank" + rateRow.index
                        sourceSize.width: 16
                        sourceSize.height: 16
                    }
                    Ez.SpinBox {
                        objectName: "spinRate" + "ABCDE"[rateRow.index]
                        jsonData: root.entry
                        key: "abcde"[rateRow.index] + "_rate"
                        from: 0
                        to: 100
                        suffix: qsTr(" %")
                        Layout.fillWidth: true
                    }
                }
            }
        }
        Ez.Section {
            objectName: "groupBox_289"
            title: qsTr("Recovery")
            Layout.row: 2
            Layout.column: 1
            Layout.columnSpan: 3
            Layout.fillHeight: true

            Ez.Section {
                objectName: "groupBox_296"
                title: qsTr("Recovery over time")
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.Label {
                        objectName: "labelRecoverTurns_1"
                        text: qsTr("State lasts at least")
                        font: Ez.Metrics.font
                    }
                    Ez.SpinBox {
                        enabled: !root.death
                        objectName: "spinRecoverTurns"
                        jsonData: root.entry
                        key: "hold_turn"
                        from: 0
                        to: 99
                        Layout.fillWidth: true
                    }
                    Ez.Label {
                        objectName: "labelRecoverTurns_2"
                        text: qsTr("turns, then")
                        font: Ez.Metrics.font
                    }
                }
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.SpinBox {
                        enabled: !root.death
                        objectName: "spinRecoverChance"
                        jsonData: root.entry
                        key: "auto_release_prob"
                        from: 0
                        to: 100
                        suffix: qsTr(" %")
                        Layout.fillWidth: true
                    }
                    Ez.Label {
                        objectName: "labelRecoverChance"
                        text: qsTr("chance to recover each turn")
                        font: Ez.Metrics.font
                    }
                }
            }
            Ez.Section {
                objectName: "groupBox_297"
                title: qsTr("Recovery from physical damage")
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.SpinBox {
                        enabled: !root.death
                        objectName: "spinRecoverDamageChance"
                        jsonData: root.entry
                        key: "release_by_damage"
                        from: 0
                        to: 100
                        suffix: qsTr(" %")
                        Layout.fillWidth: true
                    }
                    Ez.Label {
                        objectName: "labelRecoverDamageChance"
                        text: qsTr("chance to recover after taking a hit")
                        font: Ez.Metrics.font
                    }
                }
            }
        }
        Ez.Section {
            objectName: "groupBox_288"
            title: qsTr("Base Stat Alteration")
            Layout.row: 2
            Layout.column: 4
            Layout.fillHeight: true

            Ez.RadioGroup {
                jsonData: root.entry
                key: "affect_type"
                enabled: root.rpg2k3 && !root.death
                choices: [
                    { name: "radioStatAlterationNone", text: qsTr("None"), value: 2 },
                    { name: "radioStatAlterationHalf", text: qsTr("Half"), value: 0 },
                    { name: "radioStatAlterationDouble", text: qsTr("Double"), value: 1 },
                ]
            }
            Ez.CheckBox {
                enabled: !root.death
                objectName: "checkStatAffectAttack"
                text: qsTr("Attack")
                jsonData: root.entry
                key: "affect_attack"
            }
            Ez.CheckBox {
                enabled: !root.death
                objectName: "checkStatAffectDefense"
                text: qsTr("Defense")
                jsonData: root.entry
                key: "affect_defense"
            }
            Ez.CheckBox {
                enabled: !root.death
                objectName: "checkStatAffectIntelligence"
                text: qsTr("Intelligence")
                jsonData: root.entry
                key: "affect_spirit"
            }
            Ez.CheckBox {
                enabled: !root.death
                objectName: "checkStatAffectAgility"
                text: qsTr("Agility")
                jsonData: root.entry
                key: "affect_agility"
            }
        }

        Ez.Section {
            objectName: "groupBox_298"
            title: qsTr("Battle Messages")
            Layout.row: 3
            Layout.column: 0
            Layout.rowSpan: 2
            Layout.fillHeight: true

            Ez.Section {
                objectName: "groupBox_299"
                title: qsTr("Infliction (Actor)")
                Ez.TextField {
                    objectName: "lineMessageInflictionActor"
                    jsonData: root.entry
                    key: "message_actor"
                    enabled: !root.rpg2k3
                    Layout.fillWidth: true
                }
            }
            Ez.Section {
                objectName: "groupBox_300"
                title: qsTr("Infliction (Enemy)")
                Ez.TextField {
                    objectName: "lineMessageInflictionEnemy"
                    jsonData: root.entry
                    key: "message_enemy"
                    enabled: !root.rpg2k3
                    Layout.fillWidth: true
                }
            }
            Ez.Section {
                objectName: "groupBox_301"
                title: qsTr("Already affected")
                Ez.TextField {
                    objectName: "lineMessageAlreadyAffected"
                    jsonData: root.entry
                    key: "message_already"
                    enabled: !root.rpg2k3 && !root.death
                    Layout.fillWidth: true
                }
            }
            Ez.Section {
                objectName: "groupBox_302"
                title: qsTr("Per turn message")
                Ez.TextField {
                    objectName: "lineMessagePerTurn"
                    jsonData: root.entry
                    key: "message_affected"
                    enabled: !root.rpg2k3 && !root.death
                    Layout.fillWidth: true
                }
            }
            Ez.Section {
                objectName: "groupBox_303"
                title: qsTr("Recovery")
                Ez.TextField {
                    objectName: "lineMessageRecovery"
                    jsonData: root.entry
                    key: "message_recovery"
                    enabled: !root.rpg2k3
                    Layout.fillWidth: true
                }
            }
        }
        Ez.Section {
            objectName: "groupBox_287"
            title: qsTr("Prevent the following Skills from being invoked")
            Layout.row: 3
            Layout.column: 1
            Layout.columnSpan: 3
            Layout.fillHeight: true

            Ez.Form {
                Ez.CheckBox {
                    enabled: !root.death
                    objectName: "checkRestrictAttackInfluence"
                    text: qsTr("Skills with Attack Influence of at least")
                    jsonData: root.entry
                    key: "restrict_skill"
                }
                Ez.SpinBox {
                    enabled: !root.death
                    objectName: "spinRestrictAttackInfluence"
                    jsonData: root.entry
                    key: "restrict_skill_level"
                    from: 0
                    to: 10
                    Layout.fillWidth: true
                }
                Ez.CheckBox {
                    enabled: !root.death
                    objectName: "checkRestrictIntellectInfluence"
                    text: qsTr("Skills with Intellect Influence of at least")
                    jsonData: root.entry
                    key: "restrict_magic"
                }
                Ez.SpinBox {
                    enabled: !root.death
                    objectName: "spinRestrictIntellectInfluence"
                    jsonData: root.entry
                    key: "restrict_magic_level"
                    from: 0
                    to: 10
                    Layout.fillWidth: true
                }
            }
        }
        Ez.Section {
            objectName: "groupBox_304"
            title: qsTr("Animation")
            Layout.row: 3
            Layout.column: 4
            Layout.fillHeight: true

            // the pose of the heroes under the condition (RPG Maker 2003)
            Ez.ComboBox {
                objectName: "comboBattlerPose"
                jsonData: root.entry
                key: "battler_animation_id"
                model: root.poseChoices
                enabled: root.rpg2k3 && !root.death
                Layout.fillWidth: true
            }
            BattlerView {
                objectName: "labelBattlerPose"
                database: root.database
                battler: root.firstBattlerId
                pose: poseField.valid ? Number(poseField.value) : 0
                Layout.alignment: Qt.AlignHCenter
            }
        }

        Ez.Section {
            objectName: "groupBox_290"
            title: qsTr("HP effects")
            Layout.row: 4
            Layout.column: 1
            Layout.columnSpan: 2
            Layout.fillHeight: true

            Ez.RadioGroup {
                jsonData: root.entry
                key: "hp_change_type"
                columns: 3
                enabled: root.rpg2k3 && !root.death
                choices: [
                    { name: "radioHPEffectNone", text: qsTr("None"), value: 2 },
                    { name: "radioHPEffectRecovery", text: qsTr("Recovery"), value: 1 },
                    { name: "radioHPEffectDamage", text: qsTr("Damage"), value: 0 },
                ]
            }
            Ez.Section {
                objectName: "groupBox_291"
                title: qsTr("Effect per turn in Battle")
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.SpinBox {
                        enabled: !root.death
                        objectName: "spinHPEffectPercentage"
                        jsonData: root.entry
                        key: "hp_change_max"
                        from: 0
                        to: 100
                        suffix: qsTr(" %")
                    }
                    Ez.Label {
                        objectName: "labelHPEffectBattle_1"
                        text: qsTr("of maximum HP +")
                        font: Ez.Metrics.font
                    }
                    Ez.SpinBox {
                        enabled: !root.death
                        objectName: "spinHPEffectValue"
                        jsonData: root.entry
                        key: "hp_change_val"
                        from: 0
                        to: 999
                    }
                    Ez.Label {
                        objectName: "labelHPEffectBattle_2"
                        text: qsTr("HP")
                        font: Ez.Metrics.font
                    }
                }
            }
            Ez.Section {
                objectName: "groupBox_292"
                title: qsTr("Effect on Field Map")
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.SpinBox {
                        enabled: !root.death
                        objectName: "spinHPEffectFieldValue"
                        jsonData: root.entry
                        key: "hp_change_map_val"
                        from: 0
                        to: 999
                    }
                    Ez.Label {
                        objectName: "labelHPEffectField_1"
                        text: qsTr("HP every")
                        font: Ez.Metrics.font
                    }
                    Ez.SpinBox {
                        enabled: !root.death
                        objectName: "spinHPEffectFieldSteps"
                        jsonData: root.entry
                        key: "hp_change_map_steps"
                        from: 0
                        to: 999
                    }
                    Ez.Label {
                        objectName: "labelHPEffectField_2"
                        text: qsTr("steps")
                        font: Ez.Metrics.font
                    }
                }
            }
        }
        Ez.Section {
            objectName: "groupBox_293"
            title: qsTr("MP effects")
            Layout.row: 4
            Layout.column: 3
            Layout.columnSpan: 2
            Layout.fillHeight: true

            Ez.RadioGroup {
                jsonData: root.entry
                key: "sp_change_type"
                columns: 3
                enabled: root.rpg2k3 && !root.death
                choices: [
                    { name: "radioMPEffectNone", text: qsTr("None"), value: 2 },
                    { name: "radioMPEffectRecovery", text: qsTr("Recovery"), value: 1 },
                    { name: "radioMPEffectDamage", text: qsTr("Damage"), value: 0 },
                ]
            }
            Ez.Section {
                objectName: "groupBox_294"
                title: qsTr("Effect per turn in Battle")
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.SpinBox {
                        enabled: !root.death
                        objectName: "spinMPEffectPercentage"
                        jsonData: root.entry
                        key: "sp_change_max"
                        from: 0
                        to: 100
                        suffix: qsTr(" %")
                    }
                    Ez.Label {
                        objectName: "labelMPEffectBattle_1"
                        text: qsTr("of maximum MP +")
                        font: Ez.Metrics.font
                    }
                    Ez.SpinBox {
                        enabled: !root.death
                        objectName: "spinMPEffectValue"
                        jsonData: root.entry
                        key: "sp_change_val"
                        from: 0
                        to: 999
                    }
                    Ez.Label {
                        objectName: "labelMPEffectBattle_2"
                        text: qsTr("MP")
                        font: Ez.Metrics.font
                    }
                }
            }
            Ez.Section {
                objectName: "groupBox_295"
                title: qsTr("Effect on Field Map")
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.SpinBox {
                        enabled: !root.death
                        objectName: "spinMPEffectFieldValue"
                        jsonData: root.entry
                        key: "sp_change_map_val"
                        from: 0
                        to: 999
                    }
                    Ez.Label {
                        objectName: "labelMPEffectField_1"
                        text: qsTr("MP every")
                        font: Ez.Metrics.font
                    }
                    Ez.SpinBox {
                        enabled: !root.death
                        objectName: "spinMPEffectFieldSteps"
                        jsonData: root.entry
                        key: "sp_change_map_steps"
                        from: 0
                        to: 999
                    }
                    Ez.Label {
                        objectName: "labelMPEffectField_2"
                        text: qsTr("steps")
                        font: Ez.Metrics.font
                    }
                }
            }
        }
    }
}
