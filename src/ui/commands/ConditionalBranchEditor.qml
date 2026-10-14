// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ConditionalBranchWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Conditional Branch and its battle version: the type of the condition,
 * then the settings of that type (the fields of the schema it shows), and
 * whether the block has an else branch (the session's extra "else":
 * CommandEditorRegistry adds or removes the branch on OK).
 *
 * The map version has the conditions of the Maniac Patch (types 12-16, IDs
 * from variables): offered in its games, shown greyed in others when the
 * command has them.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: {
        const text = session.schema.title
        return qsTranslate(text.context, text.text)
    }
    readonly property bool battle: session.code === 13310
    readonly property int revision: session.revision
    readonly property int type: revision < 0 ? 0 : session.read("type")
    /** The settings the IDs given by variables (Maniac Patch) add: shown in its games, or when used */
    readonly property var idModes: ["itemMode", "heroMode", "characterMode", "characterExists"]

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    Ez.Section {
        title: qsTr("Condition")
        Layout.fillHeight: true

        Ez.ComboBox {
            id: type
            objectName: "conditionType"
            jsonData: root.session
            key: "/parameters/0"
            enabledRole: "enabled"
            Layout.fillWidth: true
            model: {
                const field = root.session.schema.field("type")
                const items = []
                for (const item of field.items) {
                    // the Maniac Patch conditions: offered in its games, shown greyed in others
                    if (item.maniac && !root.session.maniac && !(root.type >= 12 && root.type <= 16)) {
                        continue
                    }
                    items.push({ value: item.value, text: qsTranslate(item.text.context, item.text.text),
                                 enabled: (!item.rpg2003 || root.session.rpg2k3) && (!item.maniac || root.session.maniac) })
                }
                // a condition of an engine extension: kept until another is chosen
                if (!items.some(item => item.value === root.type)) {
                    items.push({ value: root.type, text: qsTranslate("QObject", "Unknown (%1)").arg(root.type), enabled: true })
                }
                return items
            }
        }

        SchemaRows {
            session: root.session
            Layout.leftMargin: Ez.Metrics.sectionPadding
            Layout.topMargin: Ez.Metrics.sectionSpacing
            ids: root.battle
                 ? ["switch", "switchState", "variable", "comparison", "operandType", "operandNumber", "operandVariable",
                    "hero", "monster", "target", "commandHero", "battleCommand"]
                 : ["switch", "switchState", "variable", "comparison", "operandType", "operandNumber", "operandVariable",
                    "timer", "timerComparison", "money", "moneyComparison",
                    "itemMode", "item", "itemVariable", "possession",
                    "heroMode", "hero", "heroVariable", "heroTest", "heroName", "heroLevel", "heroHp", "heroSkill",
                    "heroItem", "heroCondition",
                    "characterMode", "character", "characterVariable", "direction", "characterExists",
                    "vehicle", "other", "maniacOther", "switchPointer", "switchPointerState",
                    "variablePointer", "pointerComparison", "pointerOperandMode", "pointerOperand", "pointerOperandVariable",
                    "leftMode", "leftString", "leftVariable", "stringComparison", "rightMode", "rightText", "rightString",
                    "rightVariable", "ignoreCase", "expression"]
            names: ({
                type: "conditionType", "switch": "conditionSwitch", switchState: "conditionSwitchState",
                variable: "conditionVariable", comparison: "conditionComparison", operandType: "conditionOperandType",
                operandNumber: "conditionOperandNumber", operandVariable: "conditionOperandVariable",
                hero: "conditionHero", monster: "conditionMonster", target: "conditionTargetMonster",
                commandHero: "conditionCommandHero", battleCommand: "conditionBattleCommand",
                timer: "conditionTimer", timerComparison: "conditionTimerComparison", money: "conditionMoney",
                moneyComparison: "conditionMoneyComparison", itemMode: "conditionItemMode", item: "conditionItem",
                itemVariable: "conditionItemVariable", possession: "conditionPossession", heroMode: "conditionHeroMode",
                heroVariable: "conditionHeroVariable", heroTest: "conditionHeroTest", heroName: "conditionHeroName",
                heroLevel: "conditionHeroLevel", heroHp: "conditionHeroHP", heroSkill: "conditionHeroSkill",
                heroItem: "conditionHeroEquipment", heroCondition: "conditionHeroCondition",
                characterMode: "conditionCharacterMode", character: "conditionCharacter",
                characterVariable: "conditionCharacterVariable", direction: "conditionDirection",
                characterExists: "conditionCharacterExists", vehicle: "conditionVehicle", other: "conditionOther",
                maniacOther: "conditionManiacOther", switchPointer: "conditionSwitchPointer",
                switchPointerState: "conditionSwitchPointerState", variablePointer: "conditionVariablePointer",
                pointerComparison: "conditionVariablePointerComparison", pointerOperandMode: "conditionVariablePointerOperandMode",
                pointerOperand: "conditionVariablePointerOperand", pointerOperandVariable: "conditionVariablePointerOperandVariable",
                leftMode: "conditionStringLeftMode", leftString: "conditionStringLeft", leftVariable: "conditionStringLeftVariable",
                stringComparison: "conditionStringComparison", rightMode: "conditionStringRightMode",
                rightText: "conditionStringText", rightString: "conditionStringRight", rightVariable: "conditionStringRightVariable",
                ignoreCase: "conditionStringIgnoreCase",
                expression: "conditionExpression"
            })
            // the IDs from variables of the Maniac Patch: in its games, or when the command has them
            showRow: function(id) {
                return !root.idModes.includes(id) || root.session.maniac || root.session.read(id) !== 0
            }
        }

        Ez.Note {
            // the conditions without settings
            visible: !root.battle && (root.type === 8 || root.type === 9 || (root.type === 5 && root.session.read("heroTest") === 0))
            text: root.type === 8 ? qsTr("Event was Started Using Action Key")
                : root.type === 9 ? qsTr("BGM has Played Through Once") : qsTr("Hero is in the party")
            Layout.leftMargin: Ez.Metrics.sectionPadding
        }
        Ez.Note {
            visible: root.battle ? root.type > 5 || root.type < 0 : root.type > 16 || root.type < 0
            text: qsTr("This condition is provided by an engine extension.")
            Layout.leftMargin: Ez.Metrics.sectionPadding
        }
        Item {
            Layout.fillHeight: true
        }
    }

    Ez.Section {
        title: qsTr("Option")
        Ez.CheckBox {
            objectName: "conditionElse"
            text: qsTr("Execute Custom Handler if Condition Not Met")
            checked: !!root.session.extra("else")
            onToggled: {
                root.session.setExtra("else", checked)
                // RPG Maker writes it too
                root.session.write("else", checked ? 1 : 0)
            }
        }
    }
}
