// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "VariableOperationsWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Control Variables, as EasyRPG Player reads it: the target (p0, p1, p2),
 * the operation (p3) and the operand (p4, values from p5).
 *
 * In other games it is RPG Maker's window: radio buttons, the values of the
 * one checked enabled. With the Maniac Patch (in its games, or for a command
 * using it): ranges through variables and expressions as targets, bit
 * operations, and more operands (party members, switches, functions,
 * expressions) chosen in lists, whose values can come from variables (their
 * modes 4 bits per value). The expressions are placed by the session.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTranslate("EventCommandPalette", "Control Variables")
    readonly property int revision: session.revision
    /** The options of the Maniac Patch, as the command was opened */
    readonly property bool showManiac: session.maniac || session.read("target") >= 3 || session.read("operation") >= 6
                                       || session.read("operand") >= 9
    readonly property int target: revision < 0 ? 0 : session.read("target")
    readonly property int operand: revision < 0 ? 0 : session.read("operand")

    /** The objectNames of the controls (scripts use them) */
    readonly property var names: ({
        target0: "targetSingle", target1: "targetRange", target2: "targetPointer", target3: "targetPointerRange",
        target4: "targetExpressionRadio", variable: "targetVariable", first: "targetFirst", last: "targetLast",
        pointer: "targetPointerVariable", firstPointer: "targetFirstPointer", lastPointer: "targetLastPointer",
        operation0: "operationSet", operation1: "operationAdd", operation2: "operationSubtract",
        operation3: "operationMultiply", operation4: "operationDivide", operation5: "operationModulus",
        operand0: "operandNumberRadio", operand1: "operandVariableRadio", operand2: "operandPointerRadio",
        operand3: "operandRandomRadio", operand4: "operandItemRadio", operand5: "operandHeroRadio",
        operand6: "operandSpriteRadio", operand7: "operandOtherRadio", operand8: "operandMonsterRadio",
        number: "operandNumber", operandVariable: "operandVariable", operandPointer: "operandPointer",
        randomFrom: "operandRandomFrom", randomTo: "operandRandomTo", item: "operandItem", itemCount: "operandItemCount",
        hero: "operandHero", heroField: "operandHeroField", sprite: "operandSprite", spriteField: "operandSpriteField",
        other: "operandOther", monster: "operandMonster", monsterField: "operandMonsterField", member: "operandParty",
        "switch": "operandSwitch"
    })

    // the modes of a value: a number, or a variable holding it (or its id)
    readonly property var numberModes: [{ value: 0, text: qsTranslate("ManiacCommandWidget", "Constant") },
                                        { value: 1, text: qsTranslate("ManiacCommandWidget", "Variable") },
                                        { value: 2, text: qsTranslate("ManiacCommandWidget", "Variable pointer") }]
    readonly property var switchModes: [{ value: 0, text: qsTr("Switch") }, { value: 1, text: qsTr("Switch of variable") }]

    /**
     * The rows of the operands of the Maniac Patch, as EasyRPG Player reads
     * them: a field of the schema (field), a value after its mode (argument:
     * mode in the slot of parameter modeIndex, value in index), an ID of a
     * field or of a variable (id: mode in modeIndex), or a choice of
     * parameter index (choices)
     */
    readonly property var maniacRows: [
        { operands: [0], label: qsTr("Value"), field: "number" },
        { operands: [1], label: qsTr("Variable"), field: "operandVariable" },
        { operands: [2], label: qsTr("Variable"), field: "operandPointer" },
        { operands: [3], label: qsTr("From"), name: "operandRandomFrom", modeIndex: 7, slot: 0, index: 5 },
        { operands: [3], label: qsTr("To"), name: "operandRandomTo", modeIndex: 7, slot: 1, index: 6 },
        { operands: [4], label: qsTr("Item"), id: "item", modeIndex: 7 },
        { operands: [4], label: qsTr("Count"), field: "itemCount" },
        { operands: [5], label: qsTr("Hero"), id: "hero", modeIndex: 7 },
        { operands: [5], label: qsTr("Value"), field: "heroField" },
        { operands: [6], label: qsTr("Sprite"), id: "sprite", modeIndex: 7 },
        { operands: [6], label: qsTr("Value"), field: "spriteField" },
        { operands: [7], label: qsTr("Value"), field: "other" },
        { operands: [8], label: qsTr("Monster"), id: "monster", modeIndex: 7 },
        { operands: [8], label: qsTr("Value"), field: "monsterField" },
        { operands: [9], label: qsTr("Party member"), id: "member", modeIndex: 7 },
        { operands: [9], label: qsTr("Value"), field: "memberField", name: "operandPartyField" },
        { operands: [10], label: qsTr("Switch"), id: "switch", modeIndex: 6, switchModes: true },
        { operands: [11], label: qsTr("Base"), name: "argumentBase", modeIndex: 7, slot: 0, index: 5 },
        { operands: [11], label: qsTr("Exponent"), name: "argumentExponent", modeIndex: 7, slot: 1, index: 6 },
        { operands: [12], label: qsTr("Value"), name: "argumentSquare", modeIndex: 7, slot: 0, index: 5 },
        { operands: [12], label: qsTr("Multiplier"), name: "argumentSquareMultiplier", index: 6 },
        { operands: [13, 14], label: qsTr("Angle"), name: "argumentAngle", modeIndex: 7, slot: 0, index: 5 },
        { operands: [13, 14], label: qsTr("Angle divisor"), name: "argumentAngleDivisor", modeIndex: 7, slot: 1, index: 8 },
        { operands: [13, 14], label: qsTr("Multiplier"), name: "argumentAngleMultiplier", index: 6 },
        { operands: [15], label: qsTr("Y"), name: "argumentY", modeIndex: 8, slot: 0, index: 5 },
        { operands: [15], label: qsTr("X"), name: "argumentX", modeIndex: 8, slot: 1, index: 6 },
        { operands: [15], label: qsTr("Multiplier"), name: "argumentAtanMultiplier", index: 7 },
        { operands: [16, 17], label: qsTr("A"), name: "argumentA", modeIndex: 7, slot: 0, index: 5 },
        { operands: [16, 17], label: qsTr("B"), name: "argumentB", modeIndex: 7, slot: 1, index: 6 },
        { operands: [18], label: qsTr("Value"), name: "argumentAbsolute", modeIndex: 6, slot: 0, index: 5 },
        { operands: [19], label: qsTr("A"), name: "argumentBinaryA", modeIndex: 8, slot: 0, index: 6 },
        { operands: [19], label: qsTr("Operation"), name: "argumentBinaryOperation", index: 5,
          choices: [{ value: 1, text: "+" }, { value: 2, text: "-" }, { value: 3, text: "*" }, { value: 4, text: "/" },
                    { value: 5, text: "%" }, { value: 6, text: qsTr("OR") }, { value: 7, text: qsTr("AND") },
                    { value: 8, text: qsTr("XOR") }, { value: 9, text: qsTr("Shift left") }, { value: 10, text: qsTr("Shift right") }] },
        { operands: [19], label: qsTr("B"), name: "argumentBinaryB", modeIndex: 8, slot: 1, index: 7 },
        { operands: [20], label: qsTr("A"), name: "argumentTernaryA", modeIndex: 10, slot: 0, index: 6 },
        { operands: [20], label: qsTr("Comparison"), name: "argumentTernaryComparison", index: 5,
          choices: [{ value: 0, text: "==" }, { value: 1, text: ">=" }, { value: 2, text: "<=" }, { value: 3, text: ">" },
                    { value: 4, text: "<" }, { value: 5, text: "!=" }] },
        { operands: [20], label: qsTr("B"), name: "argumentTernaryB", modeIndex: 10, slot: 1, index: 7 },
        { operands: [20], label: qsTr("If true"), name: "argumentTernaryTrue", modeIndex: 10, slot: 2, index: 8 },
        { operands: [20], label: qsTr("If false"), name: "argumentTernaryFalse", modeIndex: 10, slot: 3, index: 9 },
        { operands: [21], label: qsTr("Expression"), expression: true },
        { operands: [22], label: qsTr("From"), name: "argumentLerpFrom", modeIndex: 5, slot: 0, index: 6 },
        { operands: [22], label: qsTr("To"), name: "argumentLerpTo", modeIndex: 5, slot: 1, index: 7 },
        { operands: [22], label: qsTr("Numerator"), name: "argumentLerpNumerator", modeIndex: 5, slot: 2, index: 8 },
        { operands: [22], label: qsTr("Denominator"), name: "argumentLerpDenominator", modeIndex: 5, slot: 3, index: 9 },
        // a number is the ID of the first variable
        { operands: [23, 24, 25], label: qsTr("First variable ID"), name: "argumentArrayFirst", modeIndex: 5, slot: 0, index: 6 },
        { operands: [23, 24, 25], label: qsTr("Count"), name: "argumentArrayCount", modeIndex: 5, slot: 1, index: 7 }
    ]

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    /** A Maniac Patch expression of the command, the target's or the operand's */
    component ExpressionField: ColumnLayout {
        id: expression
        required property bool target
        required property string name
        readonly property var read: root.revision < 0 ? null : root.session.variableExpression(target)
        property int error: -1
        spacing: Ez.Metrics.rowSpacing
        Layout.fillWidth: true
        Ez.TextField {
            id: text
            objectName: expression.name
            font: Ez.Metrics.monospaceFont
            readOnly: !expression.read.readable
            Layout.fillWidth: true
            Layout.minimumWidth: 240
            Binding on text {
                value: expression.read.text ?? ""
                when: !text.activeFocus
                restoreMode: Binding.RestoreNone
            }
            onTextEdited: expression.error = root.session.setVariableExpression(expression.target, text)
        }
        Ez.Note {
            visible: !expression.read.readable || expression.error >= 0
            text: !expression.read.readable
                  ? qsTranslate("ManiacCommandWidget", "This expression cannot be read: it is kept as it is, use Edit raw to change it.")
                  : qsTranslate("ManiacCommandWidget", "Not a valid expression (at character %1): it is not saved.").arg(expression.error + 1)
            Layout.fillWidth: true
        }
    }

    /** A value after its mode (number, variable, pointer), or a number, or a choice */
    component Argument: RowLayout {
        id: argument
        required property var row
        readonly property string modeKey: row.slot !== undefined ? "/parameters/%1@%2:4".arg(row.modeIndex).arg(4 * row.slot)
                                                                  : "/parameters/%1".arg(row.modeIndex)
        readonly property int mode: row.modeIndex === undefined || root.revision < 0 ? 0 : Number(root.session.value(modeKey) ?? 0)
        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true
        Ez.ComboBox {
            objectName: argument.row.name + "Mode"
            visible: argument.row.modeIndex !== undefined && argument.row.choices === undefined
            // the modes are Maniac Patch values
            enabled: root.session.maniac
            model: root.numberModes
            jsonData: root.session
            key: visible ? argument.modeKey : ""
            Layout.preferredWidth: 130
        }
        Ez.SpinBox {
            objectName: argument.row.name
            visible: argument.mode === 0 && argument.row.choices === undefined
            from: -2147483647
            to: 2147483647
            jsonData: root.session
            key: visible ? "/parameters/" + argument.row.index : ""
        }
        Ez.RefComboBox {
            objectName: argument.row.name + "Variable"
            visible: argument.mode !== 0 && argument.row.choices === undefined
            database: root.session.database
            listKey: "variables"
            jsonData: root.session
            key: visible ? "/parameters/" + argument.row.index : ""
            Layout.fillWidth: true
            Layout.preferredWidth: 200
        }
        Ez.ComboBox {
            objectName: argument.row.name
            visible: argument.row.choices !== undefined
            model: argument.row.choices ?? []
            jsonData: root.session
            key: visible ? "/parameters/" + argument.row.index : ""
            Layout.fillWidth: true
        }
        Item {
            Layout.fillWidth: true
        }
    }

    /** An ID: of the list of the field, or with the Maniac Patch in a variable */
    component IdArgument: RowLayout {
        id: idArgument
        required property var row
        readonly property string modeKey: "/parameters/" + row.modeIndex
        readonly property int mode: root.revision < 0 ? 0 : Number(root.session.value(modeKey) ?? 0)
        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true
        Ez.ComboBox {
            objectName: (root.names[idArgument.row.id] ?? idArgument.row.id) + "Mode"
            enabled: root.session.maniac
            model: idArgument.row.switchModes ? root.switchModes : root.numberModes
            jsonData: root.session
            key: idArgument.modeKey
            Layout.preferredWidth: 130
        }
        CommandField {
            session: root.session
            fieldId: idArgument.row.id
            name: root.names[idArgument.row.id] ?? idArgument.row.id
            visible: idArgument.mode === 0
            Layout.fillWidth: true
            Layout.preferredWidth: 200
        }
        Ez.RefComboBox {
            objectName: (root.names[idArgument.row.id] ?? idArgument.row.id) + "Variable"
            visible: idArgument.mode !== 0
            database: root.session.database
            listKey: "variables"
            jsonData: root.session
            key: visible ? "/parameters/5" : ""
            Layout.fillWidth: true
            Layout.preferredWidth: 200
        }
    }

    // Variable to change
    SchemaForm {
        session: root.session
        only: [0]
        names: root.names
        hideManiacItems: !root.showManiac
        Layout.fillHeight: false
    }
    ExpressionField {
        visible: root.showManiac && root.target === 4
        target: true
        name: "targetExpression"
        Layout.leftMargin: Ez.Metrics.sectionPadding
    }

    // RPG Maker's window: radio buttons
    Loader {
        active: !root.showManiac
        visible: active
        Layout.fillWidth: true
        sourceComponent: SchemaForm {
            session: root.session
            only: [1, 2]
            names: root.names
            hides: ["expression"]
            hideManiacItems: true
            Layout.fillHeight: false
        }
    }

    // the Maniac Patch: too many choices for radio buttons; the rows of the operand chosen are shown
    Loader {
        active: root.showManiac
        visible: active
        Layout.fillWidth: true
        sourceComponent: ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Ez.Section {
                title: qsTr("Operation")
                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Operation")
                    }
                    Ez.ComboBox {
                        objectName: "operation"
                        jsonData: root.session
                        key: "/parameters/3"
                        enabledRole: "enabled"
                        Layout.fillWidth: true
                        model: [qsTr("Set equal"), qsTr("Add"), qsTr("Subtract"), qsTr("Multiply"), qsTr("Divide"),
                                qsTr("Modulus"), qsTr("Bitwise OR"), qsTr("Bitwise AND"), qsTr("Bitwise XOR"),
                                qsTr("Shift left"), qsTr("Shift right")]
                               .map((text, value) => ({ value: value, text: text, enabled: value < 6 || root.session.maniac }))
                    }
                }
            }
            Ez.Section {
                title: qsTr("Operand")
                Ez.Form {
                    Ez.FormLabel {
                        text: qsTr("Operand")
                    }
                    Ez.ComboBox {
                        objectName: "operandType"
                        jsonData: root.session
                        key: "/parameters/4"
                        enabledRole: "enabled"
                        Layout.fillWidth: true
                        model: [qsTr("Number"), qsTr("Value of variable"), qsTr("Value stored in index"), qsTr("Random number"),
                                qsTr("Item"), qsTr("Hero"), qsTr("Sprite"), qsTr("Other"), qsTr("Monster"), qsTr("Party member"),
                                qsTr("Switch (0 or 1)"), qsTr("Power"), qsTr("Square root"), qsTr("Sine"), qsTr("Cosine"),
                                qsTr("Arc tangent (atan2)"), qsTr("Minimum"), qsTr("Maximum"), qsTr("Absolute value"),
                                qsTr("Binary operation"), qsTr("Comparison (ternary)"), qsTr("Expression"),
                                qsTr("Linear interpolation"), qsTr("Sum of variables"), qsTr("Minimum of variables"),
                                qsTr("Maximum of variables")]
                               .map((text, value) => ({ value: value, text: text, enabled: value < 9 || root.session.maniac }))
                    }
                    Repeater {
                        model: root.maniacRows
                        delegate: Ez.FormLabel {
                            required property var modelData
                            required property int index
                            text: modelData.label
                            visible: modelData.operands.includes(root.operand)
                            enabled: root.session.maniac || modelData.operands[0] < 9
                            Layout.row: 1 + index
                            Layout.column: 0
                        }
                    }
                    Repeater {
                        model: root.maniacRows
                        delegate: Loader {
                            id: rowLoader
                            required property var modelData
                            required property int index
                            visible: modelData.operands.includes(root.operand)
                            active: visible
                            enabled: root.session.maniac || modelData.operands[0] < 9
                            Layout.row: 1 + index
                            Layout.column: 1
                            Layout.fillWidth: true
                            sourceComponent: modelData.expression ? expressionRow
                                           : modelData.field ? fieldRow
                                           : modelData.id ? idRow : argumentRow
                            Component {
                                id: fieldRow
                                RowLayout {
                                    CommandField {
                                        session: root.session
                                        fieldId: rowLoader.modelData.field
                                        name: rowLoader.modelData.name ?? root.names[rowLoader.modelData.field] ?? rowLoader.modelData.field
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                            Component {
                                id: idRow
                                IdArgument {
                                    row: rowLoader.modelData
                                }
                            }
                            Component {
                                id: argumentRow
                                Argument {
                                    row: rowLoader.modelData
                                }
                            }
                            Component {
                                id: expressionRow
                                ExpressionField {
                                    target: false
                                    name: "operandExpression"
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
