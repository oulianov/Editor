// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "PictureWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Show Picture and Move Picture, in tabs as RPG Maker 2003's: the picture,
 * where and how large and transparent it is (and for Move Picture how long
 * it moves), then its color tint and effect, then the options of the
 * Maniac Patch.
 *
 * With the Maniac Patch a value can be given by a variable: its mode is
 * shown before it. A command written with the Maniac Patch shows them in
 * any game (greyed), others only in its games.
 *
 * The bottom transparency of an older command is the top one: it gets it
 * when the command grows (CommandSchema's copies).
 */
ColumnLayout {
    id: root

    required property Ez.CommandEditSession session
    readonly property bool move: session.code === 11120
    readonly property string title: move ? qsTranslate("MovePictureWidget", "Move Picture")
                                         : qsTranslate("ShowPictureWidget", "Show Picture")
    readonly property int revision: session.revision
    /** The parameter of the bottom transparency, the blend mode and the flips */
    readonly property int flags: move ? 16 : 14
    /** A parameter of the command (0 when missing) */
    function parameter(index) {
        if (revision < 0) {
            return 0
        }
        const parameters = session.value("/parameters") ?? []
        return index < parameters.length ? parameters[index] : 0
    }
    /** The options of the Maniac Patch: in its games, or when the command has them (as opened) */
    readonly property bool showManiac: session.maniac || (parameter(1) >> 8) !== 0 || (parameter(flags) >> 8) !== 0
                                       || parameter(12) === 3 || (move && (parameter(15) & ~1) !== 0)
    readonly property int effect: revision < 0 ? 0 : session.read("effect")

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    /** A value and, with the Maniac Patch, its mode before it (a number or a variable) */
    component ModeValue: RowLayout {
        id: modeValue
        required property string modeId
        required property string valueId
        required property string variableId
        /** The objectName of the value (of its mode: + "Mode") */
        required property string name
        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true
        CommandField {
            session: root.session
            fieldId: modeValue.modeId
            name: modeValue.name + "Mode"
            visible: root.showManiac
            Layout.fillWidth: false
            Layout.preferredWidth: 130
        }
        CommandField {
            session: root.session
            fieldId: modeValue.valueId
            name: modeValue.name
            hides: true
        }
        CommandField {
            session: root.session
            fieldId: modeValue.variableId
            name: modeValue.name + "Variable"
            hides: true
            Layout.fillWidth: true
            Layout.preferredWidth: 180
        }
        Item {
            Layout.fillWidth: true
        }
    }
    component PictureColumn: ColumnLayout {
        spacing: Ez.Metrics.sectionSpacing
        Layout.alignment: Qt.AlignTop
        Layout.fillWidth: true
    }

    Controls.TabBar {
        id: tabs
        objectName: "pictureTabs"
        Layout.fillWidth: true
        font: Ez.Metrics.font
        Repeater {
            model: root.showManiac ? [qsTr("Picture"), qsTr("Additional Attributes"), qsTr("Maniac Patch")]
                                   : [qsTr("Picture"), qsTr("Additional Attributes")]
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

        // Picture
        RowLayout {
            spacing: 2 * Ez.Metrics.columnSpacing
            PictureColumn {
                Ez.Section {
                    title: qsTr("Picture No.")
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        Ez.FormLabel {
                            text: qsTr("Picture No.")
                        }
                        ModeValue {
                            modeId: "pictureMode"
                            valueId: "picture"
                            variableId: "pictureVariable"
                            name: "pictureId"
                        }
                    }
                }
                Ez.Section {
                    title: qsTr("Image File")
                    visible: !root.move
                    Loader {
                        active: !root.move
                        Layout.fillWidth: true
                        sourceComponent: CommandField {
                            session: root.session
                            fieldId: "file"
                            name: "pictureFile"
                        }
                    }
                }
                Ez.Section {
                    title: root.move ? qsTr("Target Coordinates") : qsTr("Display Coordinates")
                    Repeater {
                        model: [{ value: 0, name: "pictureCoordinatesConstant" }, { value: 1, name: "pictureCoordinatesVariable" },
                                { value: 2, name: "pictureCoordinatesPointer" }]
                        CommandRadio {
                            required property var modelData
                            session: root.session
                            fieldId: "position"
                            value: modelData.value
                            objectName: modelData.name
                            // the variables hold the ids of the variables of X and Y: Maniac Patch
                            visible: modelData.value < 2 || root.showManiac
                        }
                    }
                    SchemaRows {
                        session: root.session
                        ids: ["x", "y", "xVariable", "yVariable"]
                        names: ({ x: "pictureX", y: "pictureY", xVariable: "pictureXVariable", yVariable: "pictureYVariable" })
                    }
                    Loader {
                        active: !root.move
                        visible: active
                        sourceComponent: CommandField {
                            session: root.session
                            fieldId: "scroll"
                            name: "pictureScroll"
                        }
                    }
                }
                Ez.Section {
                    title: qsTr("Transparent Color")
                    visible: !root.move
                    Loader {
                        active: !root.move
                        Layout.fillWidth: true
                        sourceComponent: CommandField {
                            session: root.session
                            fieldId: "transparentColor"
                            name: "pictureTransparentColor"
                        }
                    }
                }
                Item {
                    Layout.fillHeight: true
                }
            }
            PictureColumn {
                Ez.Section {
                    title: qsTr("Magnification")
                    ModeValue {
                        modeId: "magnificationMode"
                        valueId: "magnification"
                        variableId: "magnificationVariable"
                        name: "pictureMagnification"
                    }
                    CommandField {
                        session: root.session
                        fieldId: "separateHeight"
                        name: "pictureSeparateHeight"
                        visible: root.showManiac
                    }
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        visible: root.showManiac
                        Ez.FormLabel {
                            text: qsTr("Height")
                        }
                        ModeValue {
                            modeId: "heightMode"
                            valueId: "height"
                            variableId: "heightVariable"
                            name: "pictureHeight"
                        }
                    }
                }
                Ez.Section {
                    title: qsTr("Transparency")
                    GridLayout {
                        columns: 2
                        rowSpacing: Ez.Metrics.rowSpacing
                        columnSpacing: Ez.Metrics.columnSpacing
                        Ez.FormLabel {
                            text: qsTr("Top Half")
                        }
                        ModeValue {
                            modeId: "topMode"
                            valueId: "top"
                            variableId: "topVariable"
                            name: "pictureTopTransparency"
                        }
                        Ez.FormLabel {
                            text: qsTr("Bottom Half")
                        }
                        CommandField {
                            session: root.session
                            fieldId: "bottom"
                            name: "pictureBottomTransparency"
                        }
                    }
                }
                Ez.Section {
                    title: qsTr("Transition Time")
                    visible: root.move
                    Loader {
                        active: root.move
                        Layout.fillWidth: true
                        sourceComponent: RowLayout {
                            spacing: Ez.Metrics.columnSpacing
                            Ez.FormLabel {
                                visible: root.showManiac
                                text: qsTr("Tenths of a second")
                            }
                            ModeValue {
                                modeId: "durationMode"
                                valueId: "duration"
                                variableId: "durationVariable"
                                name: "pictureDuration"
                            }
                        }
                    }
                }
                Ez.Section {
                    title: qsTr("Option")
                    visible: root.move
                    Loader {
                        active: root.move
                        sourceComponent: CommandField {
                            session: root.session
                            fieldId: "wait"
                            name: "pictureWait"
                        }
                    }
                }
                Item {
                    Layout.fillHeight: true
                }
            }
        }

        // Additional Attributes
        PictureColumn {
            Ez.Section {
                title: qsTr("Color Tint")
                GridLayout {
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing
                    Ez.FormLabel {
                        visible: root.showManiac
                        text: qsTr("Given as")
                    }
                    CommandField {
                        session: root.session
                        fieldId: "colorMode"
                        name: "pictureColorMode"
                        visible: root.showManiac
                    }
                    Repeater {
                        model: [{ id: "red", text: qsTr("Red") }, { id: "green", text: qsTr("Green") },
                                { id: "blue", text: qsTr("Blue") }, { id: "saturation", text: qsTr("Saturation") }]
                        delegate: Ez.FormLabel {
                            required property var modelData
                            required property int index
                            text: modelData.text
                            Layout.row: (root.showManiac ? 1 : 0) + index
                            Layout.column: 0
                        }
                    }
                    Repeater {
                        model: ["red", "green", "blue", "saturation"]
                        delegate: RowLayout {
                            id: tint
                            required property string modelData
                            required property int index
                            spacing: Ez.Metrics.columnSpacing
                            Layout.row: (root.showManiac ? 1 : 0) + index
                            Layout.column: 1
                            CommandField {
                                session: root.session
                                fieldId: tint.modelData
                                name: "pictureColor" + tint.index
                                hides: true
                            }
                            CommandField {
                                session: root.session
                                fieldId: tint.modelData + "Variable"
                                name: "pictureColor" + tint.index + "Variable"
                                hides: true
                                Layout.preferredWidth: 180
                            }
                        }
                    }
                }
                // the tint of a constant color: its sample
                Rectangle {
                    readonly property color tint: {
                        if (root.revision < 0) {
                            return "gray"
                        }
                        const shown = root.session.read("colorMode") === 0
                        const level = id => shown ? Math.max(0, Math.min(255, Math.floor(root.session.read(id) * 255 / 200))) : 128
                        return Qt.rgba(level("red") / 255, level("green") / 255, level("blue") / 255, 1)
                    }
                    Layout.fillWidth: true
                    implicitHeight: sample.implicitHeight + 8
                    color: tint
                    border.color: Ez.Metrics.sectionBorderColor
                    Ez.Label {
                        id: sample
                        anchors.centerIn: parent
                        text: qsTr("Sample")
                        font: Ez.Metrics.font
                        color: parent.tint.hslLightness > 0.5 ? "black" : "white"
                    }
                }
            }
            Ez.Section {
                title: qsTr("Effects")
                GridLayout {
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing
                    Ez.FormLabel {
                        text: qsTr("Effect")
                    }
                    Ez.ComboBox {
                        objectName: "pictureEffect"
                        jsonData: root.session
                        key: "/parameters/12"
                        Layout.fillWidth: true
                        model: {
                            const items = [{ value: 0, text: qsTr("None") }, { value: 1, text: qsTr("Rotation") },
                                           { value: 2, text: qsTr("Waver") }]
                            if (root.showManiac) {
                                items.push({ value: 3, text: qsTr("Fixed Angle") })
                            }
                            if (!items.some(item => item.value === root.effect)) {
                                items.push({ value: root.effect, text: qsTr("Unknown (%1)").arg(root.effect) })
                            }
                            return items
                        }
                    }
                    Ez.FormLabel {
                        // the name of the power follows the effect
                        text: root.effect === 1 ? qsTr("Speed") : root.effect === 3 ? qsTr("Angle") : qsTr("Magnitude")
                        enabled: root.effect !== 0
                    }
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        CommandField {
                            session: root.session
                            fieldId: "powerMode"
                            name: "pictureEffectPowerMode"
                            visible: root.showManiac
                            Layout.preferredWidth: 130
                        }
                        Repeater {
                            model: ["speed", "magnitude", "angle"]
                            CommandField {
                                required property string modelData
                                session: root.session
                                fieldId: modelData
                                name: "pictureEffectPower"
                                hides: true
                            }
                        }
                        CommandField {
                            session: root.session
                            fieldId: "powerVariable"
                            name: "pictureEffectPowerVariable"
                            hides: true
                            Layout.preferredWidth: 180
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                    }
                    Ez.FormLabel {
                        visible: root.effect === 3
                        text: qsTr("Divisor")
                    }
                    ModeValue {
                        visible: root.effect === 3
                        modeId: "divisorMode"
                        valueId: "divisor"
                        variableId: "divisorVariable"
                        name: "pictureAngleDivisor"
                    }
                }
            }
            Item {
                Layout.fillHeight: true
            }
        }

        // Maniac Patch: greyed in other games
        RowLayout {
            spacing: 2 * Ez.Metrics.columnSpacing
            enabled: root.session.maniac
            PictureColumn {
                Ez.Section {
                    title: qsTr("Display")
                    SchemaRows {
                        session: root.session
                        ids: ["origin", "blend", "flipX", "flipY"]
                        names: ({ origin: "pictureOrigin", blend: "pictureBlendMode", flipX: "pictureFlipX", flipY: "pictureFlipY" })
                    }
                }
                Ez.Section {
                    title: root.move ? qsTr("Option") : qsTr("Image File")
                    SchemaRows {
                        session: root.session
                        ids: root.move ? ["relative", "keepTint", "keepEffect", "keepBlend", "keepFlips", "keepTime", "magnifiedPosition"]
                                       : ["nameMode", "nameString", "nameVariable"]
                        names: ({ relative: "pictureOption1", keepTint: "pictureOption2", keepEffect: "pictureOption3",
                                  keepBlend: "pictureOption4", keepFlips: "pictureOption5", keepTime: "pictureOption6",
                                  magnifiedPosition: "pictureOption8", nameMode: "pictureNameMode", nameString: "pictureName",
                                  nameVariable: "pictureNameVariable" })
                    }
                    Ez.Note {
                        visible: !root.move && root.session.read("nameMode") === 0
                        text: qsTr("The file of the Picture tab")
                    }
                }
                Item {
                    Layout.fillHeight: true
                }
            }
            PictureColumn {
                visible: !root.move
                Ez.Section {
                    title: qsTr("Spritesheet")
                    SchemaRows {
                        session: root.session
                        ids: root.move ? [] : ["columns", "rows", "frameMode", "frame", "frameVariable", "once"]
                        names: ({ columns: "pictureSheetColumns", rows: "pictureSheetRows", frameMode: "pictureSheetMode",
                                  frame: "pictureSheetFrame", frameVariable: "pictureSheetFrameVariable", once: "pictureSheetOnce" })
                    }
                }
                Ez.Section {
                    title: qsTr("Layer")
                    SchemaRows {
                        session: root.session
                        ids: root.move ? [] : ["mapLayer", "battleLayer", "eraseOnMapChange", "eraseOnBattleEnd", "tint", "flash", "shake"]
                        names: ({ mapLayer: "pictureMapLayer", battleLayer: "pictureBattleLayer",
                                  eraseOnMapChange: "pictureEraseOnMapChange", eraseOnBattleEnd: "pictureEraseOnBattleEnd",
                                  tint: "pictureAffectedByTint", flash: "pictureAffectedByFlash", shake: "pictureAffectedByShake" })
                    }
                }
                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
