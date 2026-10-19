// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ConditionalBranchWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Conditional Branch and its battle version, as RPG Maker 2003's: every type
 * of condition at once, a radio button each with its settings on its line,
 * greyed with their defaults until it is chosen (the fields of the schema).
 * The map version spreads them on two pages, "1" and "2" as RPG Maker; a
 * third holds the conditions RPG Maker's window lacks (Other of RPG Maker
 * 2003, those of the Maniac Patch: offered in its games, shown greyed in
 * others when the command has them). The option: whether the block has an
 * else branch (the session's extra "else": CommandEditorRegistry adds or
 * removes the branch on OK).
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
    readonly property var typeField: session.schema.field("type")
    /** The third page: RPG Maker 2003's Other, the Maniac Patch's */
    readonly property bool extraPage: !battle && (session.rpg2k3 || session.maniac || type >= 11)
    /** The conditions of the Maniac Patch: in its games, or the command's */
    readonly property bool maniacShown: session.maniac || (type >= 12 && type <= 16)

    /** The objectNames of the controls of fields (the scripts use them) */
    readonly property var names: ({
        "switch": "conditionSwitch", switchState: "conditionSwitchState",
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

    /** The page of a type of condition */
    function pageOf(value) {
        if (battle || [0, 1, 2, 3, 10].includes(value)) {
            return 0
        }
        return value >= 4 && value <= 9 ? 1 : 2
    }

    /** The field of the alternatives (an ID or its variable) shown: the
     *  command's when its type is chosen, else the default one */
    function shownOf(id, value, isDefault) {
        return revision < 0 ? false : type === value ? session.isVisible(id) : isDefault
    }

    /** A setting given by a variable (Maniac Patch): in its games, or the command's */
    function modeShown(id, value) {
        return revision < 0 ? false : session.maniac || (type === value && session.read(id) !== 0)
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    /** The radio button of a type of condition */
    component TypeRadio: Ez.RadioButton {
        readonly property var item: (root.typeField.items ?? []).find(one => one.value === value) ?? null
        objectName: "conditionType" + value
        text: item ? qsTranslate(item.text.context, item.text.text) : ""
        // another engine's: greyed unless it is the command's
        enabled: !item || ((!item.rpg2003 || root.session.rpg2k3) && (!item.maniac || root.session.maniac)) || checked
        jsonData: root.session
        key: "/parameters/0"
        Layout.alignment: Qt.AlignLeft | Qt.AlignTop
    }

    /** The control of a field, greyed with its default unless its type is chosen */
    component Field: CommandField {
        /** Its type of condition: the field is the command's only when it is chosen (-1: its rules decide) */
        property int forType: -1
        session: root.session
        name: root.names[fieldId] ?? fieldId
        // the fields of the other version (map, battle): none
        active: !!field.id
        bound: ruleVisible && (forType < 0 || root.type === forType)
        enabled: revision < 0 ? false : bound && session.isEnabled(fieldId)
    }

    /** A radio button of a choice of a type's settings, greyed with its default */
    component OptionRadio: Ez.RadioButton {
        required property string fieldId
        readonly property var field: root.session.schema.field(fieldId)
        readonly property var item: (field.items ?? []).find(one => one.value === value) ?? null
        readonly property bool bound: root.revision < 0 ? false : root.session.isVisible(fieldId)
        objectName: (root.names[fieldId] ?? fieldId) + value
        text: item ? qsTranslate(item.text.context, item.text.text) : ""
        enabled: bound && root.session.isEnabled(fieldId) && (!item || !item.maniac || root.session.maniac || checked)
        jsonData: root.session
        key: bound ? field.path : "/defaults/" + fieldId
    }

    /** The words between the settings of a type ("is"), greyed with them */
    component Words: Ez.Label {
        required property int forType
        enabled: root.type === forType
    }

    Ez.Section {
        title: qsTr("Condition")
        Layout.fillHeight: true

        Controls.TabBar {
            id: pages
            objectName: "conditionPages"
            visible: !root.battle
            font: Ez.Metrics.font
            Layout.fillWidth: true
            Controls.TabButton {
                text: qsTr("1")
                width: implicitWidth
                font: Ez.Metrics.font
            }
            Controls.TabButton {
                text: qsTr("2")
                width: implicitWidth
                font: Ez.Metrics.font
            }
            Controls.TabButton {
                text: qsTr("3")
                width: root.extraPage ? implicitWidth : 0
                visible: root.extraPage
                font: Ez.Metrics.font
            }
            // the page of the condition of the command
            Component.onCompleted: currentIndex = root.pageOf(root.type)
        }

        StackLayout {
            currentIndex: root.battle ? 0 : pages.currentIndex
            Layout.fillWidth: true
            Layout.topMargin: Ez.Metrics.rowSpacing

            // Page 1 (and the battle's only page): switch, variable, then
            // timers and money, or the heroes and monsters of the battle
            GridLayout {
                columns: 2
                rowSpacing: Ez.Metrics.rowSpacing
                columnSpacing: Ez.Metrics.columnSpacing
                Layout.alignment: Qt.AlignTop

                TypeRadio {
                    value: 0
                }
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Field {
                        fieldId: "switch"
                        Layout.fillWidth: true
                    }
                    Words {
                        forType: 0
                        text: qsTr("is")
                    }
                    Field {
                        fieldId: "switchState"
                    }
                }

                TypeRadio {
                    value: 1
                }
                ColumnLayout {
                    spacing: Ez.Metrics.rowSpacing
                    Field {
                        fieldId: "variable"
                        Layout.fillWidth: true
                    }
                    GridLayout {
                        columns: 2
                        rowSpacing: Ez.Metrics.rowSpacing
                        columnSpacing: Ez.Metrics.columnSpacing
                        OptionRadio {
                            fieldId: "operandType"
                            value: 0
                        }
                        Field {
                            fieldId: "operandNumber"
                            forType: 1
                            bound: root.type === 1 && root.revision >= 0 && root.session.read("operandType") === 0
                        }
                        RowLayout {
                            spacing: Ez.Metrics.columnSpacing
                            OptionRadio {
                                fieldId: "operandType"
                                value: 1
                            }
                            OptionRadio {
                                fieldId: "operandType"
                                value: 2
                                visible: root.session.maniac || checked
                            }
                        }
                        Field {
                            fieldId: "operandVariable"
                            forType: 1
                            // shown for both modes, greyed for a number
                            visible: true
                            bound: root.type === 1 && root.revision >= 0 && root.session.read("operandType") !== 0
                            Layout.fillWidth: true
                        }
                    }
                    Field {
                        fieldId: "comparison"
                    }
                }

                // the map: timers and money
                TypeRadio {
                    value: 2
                    visible: !root.battle
                }
                RowLayout {
                    visible: !root.battle
                    spacing: Ez.Metrics.columnSpacing
                    Field {
                        fieldId: "timer"
                        forType: 2
                    }
                    Field {
                        fieldId: "timerComparison"
                        forType: 2
                    }
                }
                TypeRadio {
                    value: 10
                    visible: !root.battle
                }
                RowLayout {
                    visible: !root.battle
                    spacing: Ez.Metrics.columnSpacing
                    Field {
                        fieldId: "timer"
                        forType: 10
                        name: "conditionTimer2"
                    }
                    Field {
                        fieldId: "timerComparison"
                        forType: 10
                        name: "conditionTimer2Comparison"
                    }
                }
                TypeRadio {
                    value: 3
                    visible: !root.battle
                    text: qsTr("Money is")
                }
                RowLayout {
                    visible: !root.battle
                    spacing: Ez.Metrics.columnSpacing
                    Field {
                        fieldId: "moneyComparison"
                        Layout.fillWidth: false
                    }
                    Field {
                        fieldId: "money"
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }

                // the battle: heroes and monsters
                TypeRadio {
                    value: 2
                    visible: root.battle
                    text: qsTr("Hero")
                }
                RowLayout {
                    visible: root.battle
                    spacing: Ez.Metrics.columnSpacing
                    Field {
                        fieldId: "hero"
                        Layout.fillWidth: true
                    }
                    Words {
                        forType: 2
                        text: qsTr("is able to act")
                    }
                }
                TypeRadio {
                    value: 3
                    visible: root.battle
                    text: qsTr("Monster")
                }
                RowLayout {
                    visible: root.battle
                    spacing: Ez.Metrics.columnSpacing
                    Field {
                        fieldId: "monster"
                        Layout.fillWidth: true
                    }
                    Words {
                        forType: 3
                        text: qsTr("is able to act")
                    }
                }
                TypeRadio {
                    value: 4
                    visible: root.battle
                    text: qsTr("Monster")
                }
                RowLayout {
                    visible: root.battle
                    spacing: Ez.Metrics.columnSpacing
                    Field {
                        fieldId: "target"
                        Layout.fillWidth: true
                    }
                    Words {
                        forType: 4
                        text: qsTr("is the current target")
                    }
                }
                TypeRadio {
                    value: 5
                    visible: root.battle
                    text: qsTr("Hero")
                }
                GridLayout {
                    visible: root.battle
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing
                    Field {
                        fieldId: "commandHero"
                        Layout.fillWidth: true
                    }
                    Words {
                        forType: 5
                        text: qsTr("uses the")
                    }
                    Field {
                        fieldId: "battleCommand"
                        Layout.fillWidth: true
                    }
                    Words {
                        forType: 5
                        text: qsTr("battle command")
                    }
                }
            }

            // Page 2: item, hero, sprite, vehicle, action key, music
            GridLayout {
                columns: 2
                rowSpacing: Ez.Metrics.rowSpacing
                columnSpacing: Ez.Metrics.columnSpacing
                Layout.alignment: Qt.AlignTop

                TypeRadio {
                    value: 4
                }
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Field {
                        fieldId: "itemMode"
                        visible: root.modeShown("itemMode", 4)
                    }
                    Field {
                        fieldId: "item"
                        visible: root.shownOf("item", 4, true)
                        Layout.fillWidth: true
                    }
                    Field {
                        fieldId: "itemVariable"
                        visible: root.shownOf("itemVariable", 4, false)
                        Layout.fillWidth: true
                    }
                    Field {
                        fieldId: "possession"
                    }
                }

                TypeRadio {
                    value: 5
                }
                ColumnLayout {
                    spacing: Ez.Metrics.rowSpacing
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        Field {
                            fieldId: "heroMode"
                            visible: root.modeShown("heroMode", 5)
                        }
                        Field {
                            fieldId: "hero"
                            visible: root.shownOf("hero", 5, true)
                            Layout.fillWidth: true
                        }
                        Field {
                            fieldId: "heroVariable"
                            visible: root.shownOf("heroVariable", 5, false)
                            Layout.fillWidth: true
                        }
                    }
                    // what is tested, and its value
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        Field {
                            fieldId: "heroTest"
                        }
                        Repeater {
                            model: ["heroName", "heroLevel", "heroHp", "heroSkill", "heroItem", "heroCondition"]
                            Field {
                                required property string modelData
                                fieldId: modelData
                                visible: root.shownOf(modelData, 5, false)
                                Layout.fillWidth: wide
                            }
                        }
                    }
                }

                TypeRadio {
                    value: 6
                    text: qsTr("Sprite")
                }
                ColumnLayout {
                    spacing: Ez.Metrics.rowSpacing
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        Field {
                            fieldId: "characterMode"
                            visible: root.modeShown("characterMode", 6)
                        }
                        Field {
                            fieldId: "character"
                            visible: root.shownOf("character", 6, true)
                            Layout.fillWidth: true
                        }
                        Field {
                            fieldId: "characterVariable"
                            visible: root.shownOf("characterVariable", 6, false)
                            Layout.fillWidth: true
                        }
                        Words {
                            forType: 6
                            text: qsTr("is Facing")
                        }
                        Field {
                            fieldId: "direction"
                        }
                    }
                    Field {
                        fieldId: "characterExists"
                        visible: root.modeShown("characterExists", 6)
                    }
                }

                TypeRadio {
                    value: 7
                }
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Field {
                        fieldId: "vehicle"
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }

                TypeRadio {
                    value: 8
                    Layout.columnSpan: 2
                }
                TypeRadio {
                    value: 9
                    Layout.columnSpan: 2
                }
            }

            // Page 3: the conditions RPG Maker's window lacks; the settings
            // of the Maniac Patch's under the one chosen
            ColumnLayout {
                spacing: Ez.Metrics.rowSpacing
                Layout.alignment: Qt.AlignTop

                RowLayout {
                    visible: !root.battle
                    spacing: Ez.Metrics.columnSpacing
                    TypeRadio {
                        value: 11
                    }
                    Field {
                        fieldId: "other"
                        Layout.fillWidth: true
                    }
                }
                Repeater {
                    model: root.battle ? [] : [
                        { value: 12, ids: ["maniacOther"] },
                        { value: 13, ids: ["switchPointer", "switchPointerState"] },
                        { value: 14, ids: ["variablePointer", "pointerComparison", "pointerOperandMode", "pointerOperand",
                                           "pointerOperandVariable"] },
                        { value: 15, ids: ["leftMode", "leftString", "leftVariable", "stringComparison", "rightMode",
                                           "rightText", "rightString", "rightVariable", "ignoreCase"] },
                        { value: 16, ids: ["expression"] }]
                    ColumnLayout {
                        id: maniacType
                        required property var modelData
                        visible: root.maniacShown
                        spacing: Ez.Metrics.rowSpacing
                        Layout.fillWidth: true
                        TypeRadio {
                            value: maniacType.modelData.value
                        }
                        SchemaRows {
                            visible: root.type === maniacType.modelData.value
                            session: root.session
                            ids: maniacType.modelData.ids
                            names: root.names
                            Layout.leftMargin: 2 * Ez.Metrics.sectionPadding
                        }
                    }
                }
            }
        }

        Ez.Note {
            visible: root.battle ? root.type > 5 || root.type < 0 : root.type > 16 || root.type < 0
            text: qsTr("This condition is provided by an engine extension.")
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
