// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "System2Widget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * The message window of the Maniac Patch (the Message Window group of its
 * Extra dialog, Message Window... on the System 2 page), laid out as there:
 * the font (a font of the game's Font folder or of the system; empty: RPG
 * Maker's RPG2000 font) and its size, then the window's size, the spacing
 * of the text, the position and size of the face and where the text starts
 * without and with a face. Defaults puts back the Maniac Patch's. OK writes
 * them.
 *
 *   Ez.Editor.exec("ManiacMessageDialog", { system: page.system }, button)
 */
Ez.DialogWindow {
    id: root

    /** The system of the database (a JsonView) */
    property var system: null

    /**
     * The numbers: their field, spin box, the Maniac Patch's default, their
     * lowest value (0 by default) and whether it lets the Patch compute it
     * (automatic: "Auto")
     */
    readonly property var numbers: ({
        maniac_message_font_size: { name: "spinFontSize", fallback: 12, from: 1 },
        maniac_message_window_width: { name: "spinWindowWidth", fallback: 320, automatic: true },
        maniac_message_window_height: { name: "spinWindowHeight", fallback: 80, automatic: true },
        maniac_message_spacing_char: { name: "spinSpacingChar", fallback: 0, from: -99 },
        maniac_message_spacing_line: { name: "spinSpacingLine", fallback: 4, from: -99 },
        maniac_message_face_x: { name: "spinFaceX", fallback: 16 },
        maniac_message_face_y: { name: "spinFaceY", fallback: 16 },
        maniac_message_face_width: { name: "spinFaceWidth", fallback: 48 },
        maniac_message_face_height: { name: "spinFaceHeight", fallback: 48 },
        maniac_message_text_x: { name: "spinTextX", fallback: 8 },
        maniac_message_text_y: { name: "spinTextY", fallback: 10 },
        maniac_message_text_face_x: { name: "spinTextFaceX", fallback: 80, from: -1, automatic: true },
        maniac_message_text_face_y: { name: "spinTextFaceY", fallback: 10 },
    })
    /** The values shown (written on OK) */
    property var values: ({})

    /** A value changed in the copy */
    function setValue(key, value) {
        const changed = Object.assign({}, values)
        changed[key] = value
        values = changed
    }
    /** The Maniac Patch's settings */
    function restoreDefaults() {
        const shown = { maniac_message_font_name: "" }
        for (const key in numbers) {
            shown[key] = numbers[key].fallback
        }
        values = shown
    }

    objectName: "ManiacMessageDialog"
    title: qsTr("Message Window")
    // the numbers of both columns at their width (the columns share it)
    preferredWidth: 420

    Component.onCompleted: {
        const shown = {}
        if (system) {
            shown.maniac_message_font_name = system.str("maniac_message_font_name")
            for (const key in numbers) {
                shown[key] = system.num(key)
            }
        }
        values = shown
    }
    onAccepting: (check) => {
        if (!system) {
            return
        }
        system.beginBatch()
        for (const key in values) {
            system.set(key, values[key])
        }
        system.endBatch()
    }

    /** A number of the copy */
    component Setting: Ez.SpinBox {
        id: setting
        required property string key
        /** Its lowest value lets the Maniac Patch compute it ("Auto") */
        readonly property bool automatic: root.numbers[key].automatic === true
        objectName: root.numbers[key].name
        from: root.numbers[key].from ?? 0
        to: 9999
        Layout.fillWidth: true
        // again after Defaults
        Binding on value {
            value: root.values[setting.key] ?? root.numbers[setting.key].fallback
            restoreMode: Binding.RestoreNone
        }
        textFromValue: function(value, locale) {
            return setting.automatic && value <= setting.from ? qsTr("Auto") : String(value)
        }
        valueFromText: function(text, locale) {
            return text === qsTr("Auto") ? setting.from : Number.fromLocaleString(locale, text)
        }
        onValueModified: root.setValue(key, value)
    }

    /** A group of two numbers, as the Maniac Patch's dialog */
    component Pair: Ez.Section {
        id: pair
        required property string firstLabel
        required property string firstKey
        required property string secondLabel
        required property string secondKey
        Layout.fillWidth: true
        Ez.Form {
            Ez.FormLabel {
                text: pair.firstLabel
            }
            Setting {
                key: pair.firstKey
            }
            Ez.FormLabel {
                text: pair.secondLabel
            }
            Setting {
                key: pair.secondKey
            }
        }
    }

    Ez.Section {
        title: qsTr("Font")
        Ez.Form {
            Ez.FormLabel {
                text: qsTr("Name")
            }
            Ez.TextField {
                id: fontName
                objectName: "lineFontName"
                // a font of the Font folder, or the name of a font of the system
                placeholderText: qsTr("RPG2000 or RPG2000G")
                Binding on text {
                    value: root.values.maniac_message_font_name ?? ""
                    restoreMode: Binding.RestoreNone
                }
                onTextEdited: root.setValue("maniac_message_font_name", text)
                Layout.fillWidth: true
                buttons: Ez.FieldButton {
                    objectName: "pushFontName"
                    fused: true
                    onClicked: {
                        const choice = Ez.Pickers.pickFile({ folder: "Font", preview: "None", name: fontName.text,
                                                             title: qsTr("Select Font") }, fontName)
                        if (choice) {
                            root.setValue("maniac_message_font_name", choice.name)
                        }
                    }
                }
            }
            Ez.FormLabel {
                text: qsTr("Size")
            }
            Setting {
                key: "maniac_message_font_size"
                to: 256
                Layout.fillWidth: false
            }
        }
    }
    GridLayout {
        columns: 2
        rowSpacing: Ez.Metrics.sectionSpacing
        columnSpacing: Ez.Metrics.sectionSpacing
        // the two columns as wide
        uniformCellWidths: true
        Layout.fillWidth: true

        // 0: 320 wide; as high as the text start, the font size and the line spacing make it
        Pair {
            title: qsTr("Window Size")
            firstLabel: qsTr("Width")
            firstKey: "maniac_message_window_width"
            secondLabel: qsTr("Height")
            secondKey: "maniac_message_window_height"
        }
        Pair {
            title: qsTr("Spacing")
            firstLabel: qsTr("Characters")
            firstKey: "maniac_message_spacing_char"
            secondLabel: qsTr("Lines")
            secondKey: "maniac_message_spacing_line"
        }
        Pair {
            title: qsTr("Face Position")
            firstLabel: qsTr("X")
            firstKey: "maniac_message_face_x"
            secondLabel: qsTr("Y")
            secondKey: "maniac_message_face_y"
        }
        Pair {
            title: qsTr("Text Position")
            firstLabel: qsTr("X")
            firstKey: "maniac_message_text_x"
            secondLabel: qsTr("Y")
            secondKey: "maniac_message_text_y"
        }
        Pair {
            title: qsTr("Face Size")
            firstLabel: qsTr("Width")
            firstKey: "maniac_message_face_width"
            secondLabel: qsTr("Height")
            secondKey: "maniac_message_face_height"
        }
        // X -1: after the face (its width and twice its X)
        Pair {
            title: qsTr("Text Position (with Face)")
            firstLabel: qsTr("X")
            firstKey: "maniac_message_text_face_x"
            secondLabel: qsTr("Y")
            secondKey: "maniac_message_text_face_y"
        }
    }
    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        Ez.Button {
            objectName: "pushDefaults"
            text: qsTr("Defaults")
            onClicked: root.restoreDefaults()
        }
        Item {
            Layout.fillWidth: true
        }
    }
}
