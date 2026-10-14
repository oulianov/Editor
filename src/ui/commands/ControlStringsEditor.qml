// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ManiacControlStringsWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * Control Strings (Maniac Patch): the strings changed (the schema), the
 * operation and its value (the schema), then the arguments of the operation
 * as EasyRPG Player reads them: values in parameters 4 to 7, their modes in
 * the nibbles 1 to 4 of parameter 0. A literal text is the string of the
 * command; the texts of concatenation, insertion and replacement follow each
 * other in it, their arguments are their lengths.
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTranslate("EventCommandPalette", "Control Strings")
    readonly property int revision: session.revision
    readonly property int operation: revision < 0 ? 0 : session.read("operation")
    readonly property int function_: revision < 0 ? 0 : session.read("function")
    readonly property var args: argumentsOf(operation, function_)
    /** The literal texts of the arguments (the ones in the string, others empty) */
    property var texts: []

    // the modes of the values: a number, a variable holding it, a string, a text...
    readonly property var numberModes: [{ value: 0, text: qsTranslate("ManiacCommandWidget", "Constant"), kind: "Number" },
                                        { value: 1, text: qsTranslate("ManiacCommandWidget", "Variable"), kind: "Variable" },
                                        { value: 2, text: qsTranslate("ManiacCommandWidget", "Variable pointer"), kind: "Variable" }]
    readonly property var targetModes: [{ value: 0, text: qsTranslate("ManiacCommandWidget", "Variable"), kind: "Variable" },
                                        { value: 1, text: qsTranslate("ManiacCommandWidget", "Variable pointer"), kind: "Variable" }]
    readonly property var stringModes: [{ value: 0, text: qsTranslate("ManiacCommandWidget", "Text"), kind: "Text" },
                                        { value: 1, text: qsTranslate("ManiacCommandWidget", "String"), kind: "StringId" },
                                        { value: 2, text: qsTranslate("ManiacCommandWidget", "String pointer"), kind: "Variable" }]

    /** The arguments (parameters 4..7) of an operation, as EasyRPG Player reads them */
    function argumentsOf(operation, fn) {
        const str = label => ({ kind: "Str", label: label, modes: stringModes })
        const num = label => ({ kind: "Num", label: label, modes: numberModes, min: -99999999, max: 99999999 })
        const varArg = label => ({ kind: "Var", label: label, modes: targetModes })
        const choices = (label, items) => ({ kind: "Choice", label: label, items: items })
        if (operation === 0 || operation === 1) {
            switch (fn) {
            case 0: return [str(qsTr("Text")), num(qsTr("Minimum length"))]
            case 1: return [num(qsTr("Number")), num(qsTr("Minimum length"))]
            case 2: return [{ kind: "Switch", label: qsTr("Switch"), min: 1, max: 99999,
                              modes: [{ value: 0, text: qsTr("Switch"), kind: "Number" },
                                      { value: 1, text: qsTr("Switch pointer"), kind: "Variable" }] },
                            num(qsTr("Minimum length"))]
            case 3: return [choices(qsTr("Data"), [qsTr("Hero"), qsTr("Skill"), qsTr("Item"), qsTr("Enemy"), qsTr("Troop"),
                                                   qsTr("Terrain"), qsTr("Attribute"), qsTr("Condition"), qsTr("Battle animation"),
                                                   qsTr("Chipset"), qsTr("Switch"), qsTr("Variable"), qsTr("String"),
                                                   qsTr("Common event"), qsTr("Class"), qsTr("Battler animation"), qsTr("Map"),
                                                   qsTr("Map event"), qsTr("Party member")].map((text, value) => ({ value: value, text: text }))),
                            num(qsTr("ID")), dynamic()]
            case 4: return [choices(qsTr("Data"), [{ value: 0, text: qsTr("Hero (title)") }, { value: 1, text: qsTr("Skill") },
                                                   { value: 2, text: qsTr("Item") }, { value: 18, text: qsTr("Party member (title)") }]),
                            num(qsTr("ID")), dynamic()]
            case 6: return [str(qsTr("Text 1")), str(qsTr("Text 2")), str(qsTr("Text 3"))]
            case 7: return [str(qsTr("Text")), num(qsTr("Position")), str(qsTr("Inserted text"))]
            case 8:
            case 14: return [str(qsTr("Text")), str(qsTr("Search")), str(qsTr("Replace with"))]
            case 9:
            case 13: return [str(qsTr("Text")), num(qsTr("Start")), num(qsTr("Length"))]
            case 10: return [str(qsTr("Delimiter")),
                             { kind: "Join", label: qsTr("Join"),
                               modes: [{ value: 0, text: qsTr("Strings from"), kind: "StringId" },
                                       { value: 1, text: qsTr("Strings from pointer"), kind: "Variable" },
                                       { value: 2, text: qsTr("Variables from"), kind: "Variable" },
                                       { value: 3, text: qsTr("Variables from pointer"), kind: "Variable" }] },
                             num(qsTr("Count"))]
            case 12: return [str(qsTr("File name")), encoding()]
            default: return []
            }
        }
        switch (operation) {
        case 2:
        case 3: return [{ kind: "VarPlain", label: qsTr("Store in") }]
        case 4:
        case 9: return [str(qsTr("Search")), varArg(qsTr("Position in")), num(qsTr("Start"))]
        case 5: return [str(qsTr("Delimiter")),
                        { kind: "StrOut", label: qsTr("Parts in strings from"),
                          modes: [{ value: 0, text: qsTr("String"), kind: "StringId" },
                                  { value: 1, text: qsTr("String pointer"), kind: "Variable" }] },
                        varArg(qsTr("Count in"))]
        case 7: return [str(qsTr("File name")), encoding()]
        case 8: return [{ kind: "StrPlain", label: qsTr("Line in string") }]
        case 10: return [str(qsTr("Pattern")), varArg(qsTr("Position in")), num(qsTr("Start")),
                         { kind: "StrPlain", label: qsTr("Match in string") }]
        default: return []
        }
    }
    function dynamic() {
        return { kind: "Choice", label: qsTr("Value"), items: [{ value: 0, text: qsTr("From the database") },
                                                                { value: 1, text: qsTr("Current (changed in the game)") }] }
    }
    function encoding() {
        return { kind: "Choice", label: qsTr("Encoding"), items: [{ value: 0, text: qsTr("ANSI (code page of the game)") },
                                                                   { value: 1, text: qsTr("UTF-8") }] }
    }

    /** The texts of concatenation, insertion and replacement follow each other in the string */
    function multipleTexts(operation, fn) {
        return (operation === 0 || operation === 1) && [6, 7, 8, 14].includes(fn)
    }
    function parameter(index) {
        return Number(session.value("/parameters/" + index) ?? 0)
    }
    /** The mode of argument i: in nibble i + 1 of parameter 0 */
    function modeOf(i) {
        return Number(session.value("/parameters/0@%1:4".arg(4 * (i + 1))) ?? 0)
    }
    /** The number of characters (code points), as EasyRPG Player counts them */
    function length(text) {
        return Array.from(text).length
    }

    /** The string of the command from the literal texts shown */
    function writeTexts() {
        let string = ""
        let ordinal = 0
        const multiple = multipleTexts(operation, function_)
        for (let i = 0; i < args.length; ++i) {
            if (args[i].kind !== "Str") {
                continue
            }
            const text = texts[ordinal++] ?? ""
            if (modeOf(i) !== 0) {
                continue
            }
            if (multiple) {
                string += text
                session.set("/parameters/" + (4 + i), length(text))
            } else {
                string = text
            }
        }
        session.set("/string", string)
    }

    // the literal texts of the arguments, as opened
    Component.onCompleted: {
        const string = Array.from(session.value("/string") ?? "")
        const list = []
        let position = 0
        for (let i = 0; i < args.length; ++i) {
            if (args[i].kind !== "Str") {
                continue
            }
            if (!multipleTexts(operation, function_)) {
                list.push(string.join(""))
            } else if (modeOf(i) === 0) {
                const count = parameter(4 + i)
                list.push(string.slice(position, position + count).join(""))
                position += count
            } else {
                list.push("")
            }
        }
        texts = list
    }
    // the new arguments read the string another way
    onArgsChanged: writeTexts()

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    SchemaForm {
        session: root.session
        only: [0, 1]
        names: ({ targetMode: "stringTargetMode", firstString: "stringFirstString", firstVariable: "stringFirstVariable",
                  lastString: "stringLastString", lastVariable: "stringLastVariable", operation: "stringOperation",
                  "function": "stringFunction", extract: "stringExtract", hex: "stringHex", firstOnly: "stringFirstOnly" })
        Layout.fillHeight: false
    }

    Ez.Section {
        title: qsTr("Arguments")
        visible: root.args.length > 0
        Ez.Form {
            Repeater {
                model: root.args
                delegate: Ez.FormLabel {
                    required property var modelData
                    required property int index
                    text: modelData.label
                    Layout.row: index
                    Layout.column: 0
                }
            }
            Repeater {
                model: root.args
                delegate: RowLayout {
                    id: row
                    required property var modelData
                    required property int index
                    readonly property string name: "stringArgument" + (index + 1)
                    readonly property string valueKey: "/parameters/" + (4 + index)
                    readonly property int mode: root.revision < 0 || !modelData.modes ? 0 : root.modeOf(index)
                    readonly property string kind: {
                        if (modelData.kind === "VarPlain") {
                            return "Variable"
                        }
                        if (modelData.kind === "StrPlain") {
                            return "StringId"
                        }
                        if (!modelData.modes) {
                            return modelData.kind
                        }
                        const found = modelData.modes.find(one => one.value === row.mode)
                        return found ? found.kind : "Number"
                    }
                    // the ordinal of its text among the texts
                    readonly property int ordinal: root.args.slice(0, index).filter(one => one.kind === "Str").length

                    spacing: Ez.Metrics.columnSpacing
                    Layout.row: index
                    Layout.column: 1
                    Layout.fillWidth: true

                    Ez.ComboBox {
                        objectName: row.name + "Mode"
                        visible: !!row.modelData.modes
                        model: row.modelData.modes ?? []
                        jsonData: root.session
                        key: visible ? "/parameters/0@%1:4".arg(4 * (row.index + 1)) : ""
                        Layout.preferredWidth: 150
                        // a text that becomes a string id (or the reverse) changes the string
                        onActivated: if (row.modelData.kind === "Str") root.writeTexts()
                    }
                    Ez.TextField {
                        objectName: row.name + "Text"
                        visible: row.kind === "Text"
                        text: root.texts[row.ordinal] ?? ""
                        Layout.fillWidth: true
                        Layout.minimumWidth: 200
                        onTextEdited: {
                            const list = root.texts.slice()
                            while (list.length <= row.ordinal) {
                                list.push("")
                            }
                            list[row.ordinal] = text
                            root.texts = list
                            root.writeTexts()
                        }
                    }
                    Ez.SpinBox {
                        objectName: row.name
                        visible: row.kind === "Number"
                        from: Math.min(row.modelData.min ?? 0, value)
                        to: Math.max(row.modelData.max ?? 99999, value)
                        jsonData: root.session
                        key: visible ? row.valueKey : ""
                    }
                    Ez.SpinBox {
                        objectName: row.modelData.kind === "StrPlain" ? row.name : row.name + "String"
                        visible: row.kind === "StringId"
                        prefix: row.modelData.kind === "StrPlain" ? "" : qsTranslate("ManiacCommandWidget", "String ")
                        from: 0
                        to: 99999
                        jsonData: root.session
                        key: visible ? row.valueKey : ""
                    }
                    Ez.RefComboBox {
                        objectName: row.modelData.kind === "VarPlain" ? row.name : row.name + "Variable"
                        visible: row.kind === "Variable"
                        database: root.session.database
                        listKey: "variables"
                        jsonData: root.session
                        key: visible ? row.valueKey : ""
                        Layout.fillWidth: true
                        Layout.preferredWidth: 200
                    }
                    Ez.ComboBox {
                        objectName: row.name
                        visible: row.kind === "Choice"
                        model: row.modelData.items ?? []
                        jsonData: root.session
                        key: visible ? row.valueKey : ""
                        Layout.fillWidth: true
                    }
                    Item {
                        visible: row.kind === "Number" || row.kind === "StringId"
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }

    Item {
        Layout.fillHeight: true
    }
}
