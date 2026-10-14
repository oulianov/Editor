// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ManiacShowStringPictureWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Show String Picture (Maniac Patch), in four tabs:
 * the fields of the schema, and the three parts of the string of the
 * command, "\x01text\x01system\x01font", each after its mode + 1: a text, or
 * the ID of a string (or of a variable holding it). The text's string ID is
 * parameter 22; the system graphic's and the font's are written in their
 * part, 5 bits per letter (ManiacPatch::DecodeStringToInt).
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property string title: qsTranslate("EventCommandPalette", "Show String Picture")
    readonly property int revision: session.revision
    /** The modes and texts of the parts: the text, the system graphic, the font */
    property var modes: [0, 0, 0]
    property var parts: ["", "", ""]

    readonly property var stringModes: [{ value: 0, text: qsTranslate("ManiacCommandWidget", "Text") },
                                        { value: 1, text: qsTranslate("ManiacCommandWidget", "String") },
                                        { value: 2, text: qsTranslate("ManiacCommandWidget", "String pointer") }]

    /** The objectName of a field (scripts use them): "stringPicture" and its id */
    function nameOf(id) {
        const special = { picture: "stringPictureId", pictureMode: "stringPictureIdMode",
                          pictureVariable: "stringPictureIdVariable", heightMode: "stringPictureHeightMagnificationMode" }
        return special[id] ?? "stringPicture" + id.charAt(0).toUpperCase() + id.slice(1)
    }
    function namesOf(ids) {
        const names = {}
        for (const id of ids) {
            for (const one of [id, id + "Variable", id + "String", id + "Mode"]) {
                names[one] = nameOf(one)
            }
        }
        return names
    }

    /** A string ID written in a part, 5 bits per letter */
    function encodeId(id) {
        let result = String.fromCharCode(65 + (id & 31) - 1)
        for (id = Math.floor(id / 32); id > 0; id = Math.floor(id / 32)) {
            result = String.fromCharCode(97 + (id & 31) - 1) + result
        }
        return result
    }
    function decodeId(text) {
        if (text.length === 0 || text.length > 6) {
            return 0
        }
        let result = text.charCodeAt(text.length - 1) - 65 + 1
        for (let i = 0; i < text.length - 1; ++i) {
            const digit = text.charCodeAt(i) - 97 + 1
            if (digit < 0 || digit >= 32) {
                return 0
            }
            result += digit * Math.pow(32, text.length - 1 - i)
        }
        return result >= 0 && result < Math.pow(32, 6) ? result : 0
    }

    function writeString() {
        let string = ""
        for (let i = 0; i < 3; ++i) {
            string += String.fromCharCode(modes[i] + 1) + parts[i]
        }
        session.set("/string", string)
    }
    function setPart(part, mode, text) {
        const newModes = modes.slice()
        const newParts = parts.slice()
        newModes[part] = mode
        newParts[part] = text
        modes = newModes
        parts = newParts
        writeString()
    }

    // the parts of the string, as opened (a string without separators: the text)
    Component.onCompleted: {
        const string = session.value("/string") ?? ""
        const found = [""]
        const separators = []
        for (const c of string) {
            const code = c.charCodeAt(0)
            if (c.length === 1 && code >= 1 && code <= 3) {
                separators.push(code - 1)
                found.push("")
            } else {
                found[found.length - 1] += c
            }
        }
        if (found.length >= 4) {
            modes = separators.slice(0, 3)
            parts = found.slice(1, 4)
        } else {
            parts = [string, "", ""]
        }
    }

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    /** A column of a tab */
    component Column: ColumnLayout {
        spacing: Ez.Metrics.sectionSpacing
        Layout.alignment: Qt.AlignTop
        Layout.fillWidth: true
    }

    /** A section of fields of the schema */
    component FieldSection: Ez.Section {
        id: section
        property var ids: []
        default property alias extra: top.data
        ColumnLayout {
            id: top
            spacing: Ez.Metrics.rowSpacing
            Layout.fillWidth: true
        }
        SchemaRows {
            session: root.session
            ids: section.ids
            names: root.namesOf(section.ids)
            hides: true
        }
    }

    /** The system graphic or the font: a name, or the ID of a string written in the part */
    component NamePart: ColumnLayout {
        id: namePart
        required property int part
        required property string name
        required property string label
        spacing: Ez.Metrics.rowSpacing
        Layout.fillWidth: true
        Ez.FormLabel {
            text: namePart.label
        }
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.ComboBox {
                objectName: namePart.name + "Mode"
                model: root.stringModes
                currentIndex: root.modes[namePart.part]
                Layout.preferredWidth: 150
                onActivated: (index) => {
                    // a name becomes an ID (or the reverse): the value shown
                    root.setPart(namePart.part, index, index === 0 ? edit.text : root.encodeId(id.value))
                }
            }
            Ez.TextField {
                id: edit
                objectName: namePart.name
                visible: root.modes[namePart.part] === 0
                text: root.modes[namePart.part] === 0 ? root.parts[namePart.part] : ""
                Layout.fillWidth: true
                onTextEdited: root.setPart(namePart.part, 0, text)
            }
            Ez.SpinBox {
                id: id
                objectName: namePart.name + "Id"
                visible: root.modes[namePart.part] !== 0
                from: 0
                to: 99999
                value: root.modes[namePart.part] === 0 ? 0 : root.decodeId(root.parts[namePart.part])
                onValueModified: root.setPart(namePart.part, root.modes[namePart.part], root.encodeId(value))
            }
            Item {
                visible: id.visible
                Layout.fillWidth: true
            }
        }
    }

    Controls.TabBar {
        id: tabs
        objectName: "stringPictureTabs"
        Layout.fillWidth: true
        font: Ez.Metrics.font
        Repeater {
            model: [qsTr("Text"), qsTr("Window"), qsTr("Picture"), qsTr("Effects")]
            Controls.TabButton {
                required property string modelData
                text: modelData
                width: implicitWidth
                font: Ez.Metrics.font
            }
        }
    }

    StackLayout {
        currentIndex: tabs.currentIndex
        Layout.fillWidth: true

        // Text
        RowLayout {
            spacing: 2 * Ez.Metrics.columnSpacing
            Column {
                Ez.Section {
                    title: qsTr("Text")
                    Ez.Form {
                        Ez.FormLabel {
                            text: qsTr("Given as")
                        }
                        Ez.ComboBox {
                            objectName: "stringPictureTextMode"
                            model: root.stringModes
                            currentIndex: root.modes[0]
                            Layout.fillWidth: true
                            onActivated: (index) => root.setPart(0, index, root.parts[0])
                        }
                    }
                    Ez.TextArea {
                        objectName: "stringPictureText"
                        visible: root.modes[0] === 0
                        text: root.parts[0]
                        font: Ez.Metrics.monospaceFont
                        wrapMode: TextEdit.NoWrap
                        Layout.fillWidth: true
                        Layout.preferredHeight: 120
                        onTextChanged: if (activeFocus && text !== root.parts[0]) root.setPart(0, 0, text)
                    }
                    RowLayout {
                        visible: root.modes[0] !== 0
                        spacing: Ez.Metrics.columnSpacing
                        Ez.SpinBox {
                            objectName: "stringPictureTextIdString"
                            visible: root.modes[0] === 1
                            prefix: qsTranslate("ManiacCommandWidget", "String ")
                            from: 0
                            to: 99999
                            jsonData: root.session
                            key: visible ? "/parameters/22" : ""
                        }
                        Ez.RefComboBox {
                            objectName: "stringPictureTextIdVariable"
                            visible: root.modes[0] === 2
                            database: root.session.database
                            listKey: "variables"
                            jsonData: root.session
                            key: visible ? "/parameters/22" : ""
                            Layout.fillWidth: true
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                    }
                }
                Item {
                    Layout.fillHeight: true
                }
            }
            Column {
                FieldSection {
                    title: qsTr("Font")
                    ids: ["fontSizeMode", "fontSize", "fontSizeVariable", "letterSpacing", "lineSpacing", "bold", "noShadow",
                          "noGradient"]
                    NamePart {
                        part: 2
                        name: "stringPictureFont"
                        label: qsTr("Font (empty: default)")
                    }
                }
                Item {
                    Layout.fillHeight: true
                }
            }
        }

        // Window
        RowLayout {
            spacing: 2 * Ez.Metrics.columnSpacing
            Column {
                FieldSection {
                    title: qsTr("Window")
                    ids: ["sizeMode", "width", "widthVariable", "height", "heightVariable", "background", "translucent",
                          "noFrame", "noPadding"]
                    NamePart {
                        part: 1
                        name: "stringPictureSystem"
                        label: qsTr("System graphic (empty: default)")
                    }
                }
                Item {
                    Layout.fillHeight: true
                }
            }
            Column {
                Item {
                    Layout.fillHeight: true
                }
            }
        }

        // Picture
        RowLayout {
            spacing: 2 * Ez.Metrics.columnSpacing
            Column {
                FieldSection {
                    title: qsTr("Picture")
                    ids: ["pictureMode", "picture", "pictureVariable", "positionMode", "x", "xVariable", "y", "yVariable",
                          "origin", "scroll", "mapLayer", "battleLayer"]
                }
                FieldSection {
                    title: qsTr("Magnification and Transparency")
                    ids: ["magnificationMode", "magnification", "magnificationVariable", "separateHeight", "heightMode",
                          "heightMagnification", "heightMagnificationVariable", "transparencyMode", "transparency",
                          "transparencyVariable"]
                }
                Item {
                    Layout.fillHeight: true
                }
            }
            Column {
                FieldSection {
                    title: qsTr("Options")
                    ids: ["eraseOnMapChange", "eraseOnBattleEnd", "tint", "flash", "shake"]
                }
                Item {
                    Layout.fillHeight: true
                }
            }
        }

        // Effects
        RowLayout {
            spacing: 2 * Ez.Metrics.columnSpacing
            Column {
                FieldSection {
                    title: qsTr("Color")
                    ids: ["red", "green", "blue", "saturation", "blend", "flipX", "flipY"]
                }
                Item {
                    Layout.fillHeight: true
                }
            }
            Column {
                FieldSection {
                    title: qsTr("Effect")
                    ids: ["effect", "power", "angleMode", "angle", "angleVariable", "divisorMode", "divisor", "divisorVariable"]
                }
                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
