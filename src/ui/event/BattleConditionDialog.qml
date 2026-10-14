// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "BattlePageConditionDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * The triggers of a battle page, as RPG Maker's "Define Battle Event
 * Triggers" window: a row per condition, whose check box enables its
 * fields. The conditions of RPG Maker 2003 stay visible (and keep their
 * data) in 2000 games, locked. It edits a copy (BattlePageConditions),
 * which the C++ that opened it (CommandEditing::editBattleTrigger) writes
 * into the page on OK.
 */
Ez.DialogWindow {
    id: root

    required property Ez.BattlePageConditions conditions
    readonly property var database: Ez.ProjectData.database()

    /** A condition: its check box (the row of BattlePageConditions), then its fields */
    component Condition: Ez.CheckBox {
        id: check

        property int row: 0

        enabled: root.conditions.allowedRows[row]
        checked: root.conditions.checkedRows[row]
        onToggled: root.conditions.setChecked(row, checked)
    }
    /** The fields of a condition, enabled while it is checked */
    component Fields: RowLayout {
        property int row: 0

        spacing: Ez.Metrics.columnSpacing
        enabled: root.conditions.enabledRows[row]
        Layout.fillWidth: true
    }
    component Words: Ez.Label {
        font: Ez.Metrics.font
    }
    component NumberField: Ez.SpinBox {
        id: number

        property int shown: 0
        signal chosen(int value)

        Binding on value {
            value: number.shown
            restoreMode: Binding.RestoreNone
        }
        onValueModified: chosen(value)
    }
    component Member: Ez.ComboBox {
        id: member

        property int shown: 0
        signal chosen(int value)

        model: root.conditions.members
        Binding on currentIndex {
            value: {
                member.count
                return member.indexOfValue(member.shown)
            }
            restoreMode: Binding.RestoreNone
        }
        onActivated: chosen(currentValue)
    }

    objectName: "BattlePageConditionDialog"
    title: qsTr("Define Battle Event Triggers")

    Ez.Section {
        title: qsTr("Conditions")
        Layout.fillWidth: true

        GridLayout {
            columns: 2
            rowSpacing: Ez.Metrics.rowSpacing
            columnSpacing: Ez.Metrics.columnSpacing
            Layout.fillWidth: true

            Condition {
                objectName: "checkSwitchA"
                text: qsTr("Switch")
                row: Ez.BattlePageConditions.SwitchA
            }
            Fields {
                row: Ez.BattlePageConditions.SwitchA
                RefChoice {
                    objectName: "comboSwitchA"
                    database: root.database
                    listKey: "switches"
                    value: root.conditions.switchAId
                    onChosen: (id) => root.conditions.switchAId = id
                }
                Words {
                    text: qsTr("is ON")
                }
            }

            Condition {
                objectName: "checkSwitchB"
                text: qsTr("Switch")
                row: Ez.BattlePageConditions.SwitchB
            }
            Fields {
                row: Ez.BattlePageConditions.SwitchB
                RefChoice {
                    objectName: "comboSwitchB"
                    database: root.database
                    listKey: "switches"
                    value: root.conditions.switchBId
                    onChosen: (id) => root.conditions.switchBId = id
                }
                Words {
                    text: qsTr("is ON")
                }
            }

            // the Player compares with >=
            Condition {
                objectName: "checkVariable"
                text: qsTr("Variable")
                row: Ez.BattlePageConditions.Variable
            }
            Fields {
                row: Ez.BattlePageConditions.Variable
                RefChoice {
                    objectName: "comboVariable"
                    database: root.database
                    listKey: "variables"
                    value: root.conditions.variableId
                    onChosen: (id) => root.conditions.variableId = id
                }
                Words {
                    text: qsTr("is at least")
                }
                NumberField {
                    objectName: "spinVariableValue"
                    from: -root.conditions.variableMaximum
                    to: root.conditions.variableMaximum
                    shown: root.conditions.variableValue
                    onChosen: (value) => root.conditions.variableValue = value
                }
            }

            // turns a * n + b of the battle (b alone when a is 0)
            Condition {
                objectName: "checkTurn"
                text: qsTr("Turns Elapsed")
                row: Ez.BattlePageConditions.Turn
            }
            Fields {
                row: Ez.BattlePageConditions.Turn
                NumberField {
                    objectName: "spinTurnA"
                    from: 0
                    to: 9999
                    shown: root.conditions.turnA
                    onChosen: (value) => root.conditions.turnA = value
                }
                Words {
                    text: qsTr("x +")
                }
                NumberField {
                    objectName: "spinTurnB"
                    from: 0
                    to: 9999
                    shown: root.conditions.turnB
                    onChosen: (value) => root.conditions.turnB = value
                }
                Item {
                    Layout.fillWidth: true
                }
            }

            Condition {
                objectName: "checkFatigue"
                text: qsTr("Party Exhaustion is between")
                row: Ez.BattlePageConditions.Fatigue
            }
            Fields {
                row: Ez.BattlePageConditions.Fatigue
                NumberField {
                    objectName: "spinFatigueMin"
                    from: 0
                    to: 100
                    shown: root.conditions.fatigueMin
                    onChosen: (value) => root.conditions.fatigueMin = value
                }
                Words {
                    text: qsTr("% and")
                }
                NumberField {
                    objectName: "spinFatigueMax"
                    from: 0
                    to: 100
                    shown: root.conditions.fatigueMax
                    onChosen: (value) => root.conditions.fatigueMax = value
                }
                Words {
                    text: qsTr("%")
                }
                Item {
                    Layout.fillWidth: true
                }
            }
            Item {
                implicitWidth: 1
            }
            Fields {
                row: Ez.BattlePageConditions.Fatigue
                Words {
                    text: qsTr("* Exhaustion: 100 - 100 x ((HP / Max HP) x 2 + MP / Max MP) / 3")
                }
            }

            Condition {
                objectName: "checkEnemyHp"
                text: qsTr("Monster")
                row: Ez.BattlePageConditions.EnemyHp
            }
            Fields {
                row: Ez.BattlePageConditions.EnemyHp
                Member {
                    objectName: "comboEnemyHp"
                    shown: root.conditions.enemyId
                    onChosen: (value) => root.conditions.enemyId = value
                    Layout.fillWidth: true
                }
                Words {
                    text: qsTr("HP is between")
                }
                NumberField {
                    objectName: "spinEnemyHpMin"
                    from: 0
                    to: 100
                    shown: root.conditions.enemyHpMin
                    onChosen: (value) => root.conditions.enemyHpMin = value
                }
                Words {
                    text: qsTr("% and")
                }
                NumberField {
                    objectName: "spinEnemyHpMax"
                    from: 0
                    to: 100
                    shown: root.conditions.enemyHpMax
                    onChosen: (value) => root.conditions.enemyHpMax = value
                }
                Words {
                    text: qsTr("%")
                }
            }

            Condition {
                objectName: "checkActorHp"
                text: qsTr("Hero")
                row: Ez.BattlePageConditions.ActorHp
            }
            Fields {
                row: Ez.BattlePageConditions.ActorHp
                RefChoice {
                    objectName: "comboActorHp"
                    database: root.database
                    listKey: "actors"
                    value: root.conditions.actorId
                    onChosen: (id) => root.conditions.actorId = id
                }
                Words {
                    text: qsTr("HP is between")
                }
                NumberField {
                    objectName: "spinActorHpMin"
                    from: 0
                    to: 100
                    shown: root.conditions.actorHpMin
                    onChosen: (value) => root.conditions.actorHpMin = value
                }
                Words {
                    text: qsTr("% and")
                }
                NumberField {
                    objectName: "spinActorHpMax"
                    from: 0
                    to: 100
                    shown: root.conditions.actorHpMax
                    onChosen: (value) => root.conditions.actorHpMax = value
                }
                Words {
                    text: qsTr("%")
                }
            }

            // RPG Maker 2003: the turns of one battler, and the command a hero chose
            Condition {
                objectName: "checkTurnEnemy"
                text: qsTr("Monster")
                row: Ez.BattlePageConditions.TurnEnemy
            }
            Fields {
                row: Ez.BattlePageConditions.TurnEnemy
                Member {
                    objectName: "comboTurnEnemy"
                    shown: root.conditions.turnEnemyId
                    onChosen: (value) => root.conditions.turnEnemyId = value
                    Layout.fillWidth: true
                }
                Words {
                    text: qsTr("has taken")
                }
                NumberField {
                    objectName: "spinTurnEnemyA"
                    from: 0
                    to: 9999
                    shown: root.conditions.turnEnemyA
                    onChosen: (value) => root.conditions.turnEnemyA = value
                }
                Words {
                    text: qsTr("x +")
                }
                NumberField {
                    objectName: "spinTurnEnemyB"
                    from: 0
                    to: 9999
                    shown: root.conditions.turnEnemyB
                    onChosen: (value) => root.conditions.turnEnemyB = value
                }
                Words {
                    text: qsTr("turns")
                }
            }

            Condition {
                objectName: "checkTurnActor"
                text: qsTr("Hero")
                row: Ez.BattlePageConditions.TurnActor
            }
            Fields {
                row: Ez.BattlePageConditions.TurnActor
                RefChoice {
                    objectName: "comboTurnActor"
                    database: root.database
                    listKey: "actors"
                    value: root.conditions.turnActorId
                    onChosen: (id) => root.conditions.turnActorId = id
                }
                Words {
                    text: qsTr("has taken")
                }
                NumberField {
                    objectName: "spinTurnActorA"
                    from: 0
                    to: 9999
                    shown: root.conditions.turnActorA
                    onChosen: (value) => root.conditions.turnActorA = value
                }
                Words {
                    text: qsTr("x +")
                }
                NumberField {
                    objectName: "spinTurnActorB"
                    from: 0
                    to: 9999
                    shown: root.conditions.turnActorB
                    onChosen: (value) => root.conditions.turnActorB = value
                }
                Words {
                    text: qsTr("turns")
                }
            }

            Condition {
                objectName: "checkCommand"
                text: qsTr("Hero")
                row: Ez.BattlePageConditions.Command
            }
            Fields {
                row: Ez.BattlePageConditions.Command
                RefChoice {
                    objectName: "comboCommandActor"
                    database: root.database
                    listKey: "actors"
                    value: root.conditions.commandActorId
                    onChosen: (id) => root.conditions.commandActorId = id
                }
                Words {
                    text: qsTr("uses the")
                }
                Ez.ComboBox {
                    id: command
                    objectName: "comboCommand"
                    model: root.conditions.battleCommands
                    Layout.fillWidth: true
                    Binding on currentIndex {
                        value: {
                            command.count
                            return command.indexOfValue(root.conditions.commandId)
                        }
                        restoreMode: Binding.RestoreNone
                    }
                    onActivated: root.conditions.commandId = currentValue
                }
                Words {
                    text: qsTr("battle command")
                }
            }
        }
    }
}
