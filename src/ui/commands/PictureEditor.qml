// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "PictureWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Show Picture and Move Picture on one page. At the left the picture: for
 * Show Picture its preview (a click chooses the file) and under it two tabs,
 * File (its number, its name, its transparent color) and Advanced (the
 * spritesheet and the layers of RPG Maker 2003 1.12, the tab's tag); for
 * Move Picture its number, its time and its options. At the right where it
 * is, its size and how it looks.
 *
 * With the Maniac Patch a value can be given by a variable: its mode is
 * shown before it. A command written with the Maniac Patch shows them in
 * any game (greyed), others only in its games. RPG Maker 2003 1.12 gives
 * the picture, its magnification and its transparency by a variable too,
 * and the end of the name of the file: those modes are shown with their
 * tag. The other options of an add-on are there in every game, greyed with
 * their tag where the game does not run them, or hidden (Editor Settings)
 * unless the command has them: the Advanced tab too when none is left.
 *
 * The transparency of the bottom half is RPG Maker 2003's before 1.10 (its
 * later versions draw the whole picture with the top's): shown in those
 * games only. Of an older command it is the top one: it gets it when the
 * command grows (CommandSchema's copies).
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
    /** The value of a field (0 before the session is ready) */
    function read(id) {
        return revision < 0 ? 0 : session.read(id)
    }
    /** The modes of the values of the Maniac Patch: in its games, or when the command has them (as opened) */
    readonly property bool showManiac: session.maniac || (parameter(1) >> 8) !== 0 || (parameter(flags) >> 8) !== 0
                                       || parameter(12) === 3 || (move && (parameter(15) & ~1) !== 0)
    readonly property int effect: read("effect")
    /** The field is offered (the session's originShown: of an add-on the game runs, shown greyed, or the command's) */
    function shown(id) {
        return revision < 0 || (session.path(id) !== "" && session.originShown(id))
    }
    /** One of the fields is offered */
    function anyShown(ids) {
        return ids.some(id => shown(id))
    }
    /** The fields of the Advanced tab (Show Picture): RPG Maker 2003 1.12's spritesheet and layers */
    readonly property var advancedIds: ["columns", "rows", "frameMode", "frame", "frameVariable", "once", "mapLayer", "battleLayer",
                                         "eraseOnMapChange", "eraseOnBattleEnd", "tint", "flash", "shake"]
    readonly property bool advancedShown: !move && anyShown(advancedIds)
    /** The game runs RPG Maker 2003 1.12's options (the labels of the Advanced tab greyed else, as its fields) */
    readonly property bool runs112: Ez.Origins.available("rpg2003e")
    /** The bottom half has a transparency of its own: RPG Maker 2003 before 1.10 */
    readonly property bool bottomShown: Ez.Game.rpg2k3 && !Ez.Game.commands2k3e
    /** The options of Move Picture after waiting: the Maniac Patch's */
    readonly property var moveOptionIds: ["relative", "keepTint", "keepEffect", "keepBlend", "keepFlips", "keepTime", "magnifiedPosition"]

    spacing: Ez.Metrics.sectionSpacing
    Layout.fillWidth: true

    /** A value and its mode before it (a number or a variable): the Maniac
     *  Patch's in its games, RPG Maker 2003 1.12's with their tag */
    component ModeValue: RowLayout {
        id: modeValue
        required property string modeId
        required property string valueId
        required property string variableId
        /** The objectName of the value (of its mode: + "Mode") */
        required property string name
        /** The origin of the mode: "rpg2003e" (RPG Maker 2003 1.12), else the Maniac Patch's */
        readonly property string origin: root.session.schema.field(modeId).origin ?? ""
        readonly property bool version: origin === "rpg2003e"
        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true
        CommandField {
            id: mode
            session: root.session
            fieldId: modeValue.modeId
            name: modeValue.name + "Mode"
            // 1.12's: offered as the options of the add-ons (greyed where the
            // game does not run it, or hidden unless the command has it)
            visible: root.showManiac || (modeValue.version && root.shown(modeValue.modeId))
            Layout.fillWidth: false
            Layout.preferredWidth: 130
        }
        Ez.OriginTag {
            objectName: modeValue.name + "ModeTag"
            origin: modeValue.version && mode.visible ? modeValue.origin : ""
            warning: !Ez.Origins.available(origin) && root.revision >= 0 && root.session.read(modeValue.modeId) !== 0
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
    /** The tag of the add-on of a field after its control: a warning when
     *  the command has a value of it the game does not run */
    component FieldTag: Ez.OriginTag {
        required property string fieldId
        origin: root.session.schema.field(fieldId).origin ?? ""
        warning: !Ez.Origins.available(origin) && root.revision >= 0 && root.session.isSet(fieldId)
    }
    /** A column of sections, from the top */
    component PictureColumn: ColumnLayout {
        spacing: Ez.Metrics.sectionSpacing
        Layout.alignment: Qt.AlignTop
    }

    RowLayout {
        spacing: 2 * Ez.Metrics.columnSpacing
        Layout.fillWidth: true

        // the picture
        PictureColumn {
            Layout.preferredWidth: 360
            Layout.fillWidth: false

            Loader {
                active: !root.move
                visible: active
                Layout.fillWidth: true
                sourceComponent: preview
            }

            Controls.TabBar {
                id: tabs
                objectName: "pictureTabs"
                visible: root.advancedShown
                Layout.fillWidth: true
                font: Ez.Metrics.font
                Controls.TabButton {
                    text: qsTr("File")
                    width: implicitWidth
                    font: Ez.Metrics.font
                }
                Controls.TabButton {
                    id: advancedButton
                    text: qsTr("Advanced")
                    width: implicitWidth
                    font: Ez.Metrics.font
                    // its tag after the text: RPG Maker 2003 1.12's
                    rightPadding: horizontalPadding + (advancedTag.width > 0 ? advancedTag.width + Ez.Metrics.columnSpacing : 0)
                    Ez.OriginTag {
                        id: advancedTag
                        objectName: "pictureAdvancedTag"
                        origin: "rpg2003e"
                        warning: !Ez.Origins.available(origin) && root.revision >= 0 && root.advancedIds.some(id => root.session.isSet(id))
                        x: advancedButton.leftPadding + advancedButton.contentItem.implicitWidth + Ez.Metrics.columnSpacing
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                onVisibleChanged: if (!visible) currentIndex = 0
            }
            Ez.Section {
                title: tabs.visible ? "" : qsTr("Picture")
                StackLayout {
                    currentIndex: tabs.visible ? tabs.currentIndex : 0
                    Layout.fillWidth: true
                    Loader {
                        sourceComponent: root.move ? movePicture : fileTab
                    }
                    Loader {
                        active: !root.move
                        sourceComponent: advancedTab
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
                    Layout.fillWidth: true
                    sourceComponent: ColumnLayout {
                        spacing: Ez.Metrics.rowSpacing
                        CommandField {
                            session: root.session
                            fieldId: "wait"
                            name: "pictureWait"
                        }
                        SchemaRows {
                            session: root.session
                            visible: root.anyShown(root.moveOptionIds)
                            ids: root.moveOptionIds
                            names: ({ relative: "pictureOption1", keepTint: "pictureOption2", keepEffect: "pictureOption3",
                                      keepBlend: "pictureOption4", keepFlips: "pictureOption5", keepTime: "pictureOption6",
                                      magnifiedPosition: "pictureOption8" })
                        }
                    }
                }
            }
            Item {
                Layout.fillHeight: true
            }
        }

        // where it is, how large, how it looks
        PictureColumn {
            Layout.fillWidth: true

            Ez.Section {
                title: root.move ? qsTr("Target Coordinates") : qsTr("Display Coordinates")
                RowLayout {
                    spacing: 2 * Ez.Metrics.columnSpacing
                    Repeater {
                        model: [{ value: 0, name: "pictureCoordinatesConstant" }, { value: 1, name: "pictureCoordinatesVariable" },
                                { value: 2, name: "pictureCoordinatesPointer" }]
                        CommandRadio {
                            required property var modelData
                            session: root.session
                            fieldId: "position"
                            value: modelData.value
                            objectName: modelData.name
                        }
                    }
                }
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.FormLabel {
                        text: qsTr("X")
                    }
                    CommandField {
                        session: root.session
                        fieldId: "x"
                        name: "pictureX"
                        hides: true
                    }
                    CommandField {
                        session: root.session
                        fieldId: "xVariable"
                        name: "pictureXVariable"
                        hides: true
                        Layout.preferredWidth: 180
                    }
                    Ez.FormLabel {
                        text: qsTr("Y")
                        Layout.leftMargin: 2 * Ez.Metrics.columnSpacing
                    }
                    CommandField {
                        session: root.session
                        fieldId: "y"
                        name: "pictureY"
                        hides: true
                    }
                    CommandField {
                        session: root.session
                        fieldId: "yVariable"
                        name: "pictureYVariable"
                        hides: true
                        Layout.preferredWidth: 180
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
                // the Maniac Patch: the point of the picture X and Y put there
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    visible: root.shown("origin")
                    Ez.FormLabel {
                        text: qsTr("Origin")
                        enabled: origins.enabled
                    }
                    Grid {
                        id: origins
                        objectName: "pictureOrigin"
                        columns: 3
                        spacing: 2
                        enabled: root.revision >= 0 && root.session.isEnabled("origin")
                        opacity: enabled ? 1 : Ez.Metrics.disabledOpacity
                        // the cells from the top left, row by row: the values of the Maniac Patch
                        Repeater {
                            model: [1, 5, 3, 7, 0, 8, 2, 6, 4]
                            Rectangle {
                                id: cell
                                required property int modelData
                                objectName: "pictureOrigin" + modelData
                                width: 14
                                height: 14
                                radius: 2
                                Kirigami.Theme.colorSet: Kirigami.Theme.View
                                Kirigami.Theme.inherit: false
                                color: root.read("origin") === modelData ? Kirigami.Theme.highlightColor : Kirigami.Theme.backgroundColor
                                border.color: Ez.Metrics.sectionBorderColor
                                TapHandler {
                                    onTapped: root.session.write("origin", cell.modelData)
                                }
                                HoverHandler {
                                    id: cellHover
                                    cursorShape: Qt.PointingHandCursor
                                }
                                Controls.ToolTip.visible: cellHover.hovered
                                Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                                Controls.ToolTip.text: originLabel.names[cell.modelData]
                            }
                        }
                    }
                    Ez.Label {
                        id: originLabel
                        readonly property var names: [qsTr("Center"), qsTr("Top left"), qsTr("Bottom left"), qsTr("Top right"),
                                                      qsTr("Bottom right"), qsTr("Top"), qsTr("Bottom"), qsTr("Left"), qsTr("Right")]
                        text: names[Math.max(0, Math.min(8, root.read("origin")))] ?? ""
                        color: Ez.Metrics.mutedTextColor
                        enabled: origins.enabled
                    }
                    FieldTag {
                        objectName: "pictureOriginTag"
                        fieldId: "origin"
                    }
                    Item {
                        Layout.fillWidth: true
                    }
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
                title: qsTr("Magnification")
                GridLayout {
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing
                    Ez.FormLabel {
                        text: heightRow.visible ? qsTr("Width") : qsTr("Magnification")
                    }
                    ModeValue {
                        modeId: "magnificationMode"
                        valueId: "magnification"
                        variableId: "magnificationVariable"
                        name: "pictureMagnification"
                    }
                    // the Maniac Patch's: not offered (Editor Settings) unless the command has it
                    Ez.FormLabel {
                        text: qsTr("Height")
                        visible: heightRow.visible
                        enabled: Ez.Origins.available("maniac")
                    }
                    RowLayout {
                        id: heightRow
                        spacing: Ez.Metrics.columnSpacing
                        visible: root.shown("separateHeight")
                        CommandField {
                            session: root.session
                            fieldId: "separateHeight"
                            name: "pictureSeparateHeight"
                            checkText: qsTr("Separate")
                        }
                        ModeValue {
                            modeId: "heightMode"
                            valueId: "height"
                            variableId: "heightVariable"
                            name: "pictureHeight"
                        }
                    }
                    Ez.FormLabel {
                        text: qsTr("Flip")
                        visible: flipRow.visible
                        enabled: Ez.Origins.available("maniac")
                    }
                    RowLayout {
                        id: flipRow
                        spacing: 2 * Ez.Metrics.columnSpacing
                        visible: root.anyShown(["flipX", "flipY"])
                        CommandField {
                            session: root.session
                            fieldId: "flipX"
                            name: "pictureFlipX"
                            checkText: qsTr("Horizontally")
                        }
                        CommandField {
                            session: root.session
                            fieldId: "flipY"
                            name: "pictureFlipY"
                            checkText: qsTr("Vertically")
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            Ez.Section {
                title: qsTr("Appearance")
                GridLayout {
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing
                    Ez.FormLabel {
                        text: root.bottomShown ? qsTr("Top Half") : qsTr("Transparency")
                    }
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        ModeValue {
                            modeId: "topMode"
                            valueId: "top"
                            variableId: "topVariable"
                            name: "pictureTopTransparency"
                            Layout.fillWidth: false
                        }
                        Ez.FormLabel {
                            text: qsTr("Bottom Half")
                            visible: root.bottomShown
                            Layout.leftMargin: 2 * Ez.Metrics.columnSpacing
                        }
                        CommandField {
                            session: root.session
                            fieldId: "bottom"
                            name: "pictureBottomTransparency"
                            visible: root.bottomShown
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    // the Maniac Patch
                    Ez.FormLabel {
                        text: qsTr("Blend mode")
                        visible: blendRow.visible
                        enabled: Ez.Origins.available("maniac")
                    }
                    RowLayout {
                        id: blendRow
                        spacing: Ez.Metrics.columnSpacing
                        visible: root.shown("blend")
                        CommandField {
                            session: root.session
                            fieldId: "blend"
                            name: "pictureBlendMode"
                            Layout.fillWidth: false
                            Layout.preferredWidth: 150
                        }
                        FieldTag {
                            objectName: "pictureBlendModeTag"
                            fieldId: "blend"
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                    }

                    Ez.FormLabel {
                        text: qsTr("Color Tint")
                    }
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        CommandField {
                            session: root.session
                            fieldId: "colorMode"
                            name: "pictureColorMode"
                            visible: root.showManiac
                            Layout.fillWidth: false
                            Layout.preferredWidth: 130
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
                            implicitWidth: 120
                            implicitHeight: sample.implicitHeight + 6
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
                        Item {
                            Layout.fillWidth: true
                        }
                    }
                    Item {
                        implicitWidth: 1
                    }
                    GridLayout {
                        columns: 2
                        rowSpacing: Ez.Metrics.rowSpacing
                        columnSpacing: 3 * Ez.Metrics.columnSpacing
                        Repeater {
                            model: [{ id: "red", text: qsTr("Red") }, { id: "green", text: qsTr("Green") },
                                    { id: "blue", text: qsTr("Blue") }, { id: "saturation", text: qsTr("Saturation") }]
                            delegate: RowLayout {
                                id: channel
                                required property var modelData
                                required property int index
                                spacing: Ez.Metrics.columnSpacing
                                Ez.FormLabel {
                                    text: channel.modelData.text
                                    Layout.preferredWidth: 64
                                }
                                CommandField {
                                    session: root.session
                                    fieldId: channel.modelData.id
                                    name: "pictureColor" + channel.index
                                    hides: true
                                }
                                CommandField {
                                    session: root.session
                                    fieldId: channel.modelData.id + "Variable"
                                    name: "pictureColor" + channel.index + "Variable"
                                    hides: true
                                    Layout.preferredWidth: 180
                                }
                            }
                        }
                    }

                    Ez.FormLabel {
                        text: qsTr("Effect")
                    }
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        Ez.ComboBox {
                            objectName: "pictureEffect"
                            jsonData: root.session
                            key: "/parameters/12"
                            Layout.preferredWidth: 130
                            // the Maniac Patch's greyed in the other games, saying what they need
                            enabledRole: "enabled"
                            toolTipRole: "why"
                            originRole: "origin"
                            model: {
                                const items = [{ value: 0, text: qsTr("None") }, { value: 1, text: qsTr("Rotation") },
                                               { value: 2, text: qsTr("Waver") }]
                                // not offered (Editor Settings) unless it is the command's
                                if (Ez.Origins.shown("maniac") || root.effect === 3) {
                                    items.push({ value: 3, text: qsTr("Fixed Angle"), origin: "maniac", enabled: Ez.Origins.available("maniac"),
                                                 why: Ez.Origins.available("maniac") ? "" : Ez.Origins.need("maniac") })
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
                            Layout.leftMargin: Ez.Metrics.columnSpacing
                        }
                        CommandField {
                            session: root.session
                            fieldId: "powerMode"
                            name: "pictureEffectPowerMode"
                            visible: root.showManiac
                            Layout.fillWidth: false
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
                    // the Maniac Patch, fixed angle: the angle divided by it
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
    }

    // Show Picture: the picture, a click choosing its file; under it its name
    Component {
        id: preview
        ColumnLayout {
            spacing: Ez.Metrics.sectionTitleGap
            Ez.SpriteField {
                id: picture
                objectName: "picturePreview"
                preview: "Image"
                folder: "Picture"
                jsonData: root.session
                key: root.session.schema.field("file").path ?? ""
                enabled: root.revision >= 0
                // as the Player draws it: the transparent color, the
                // transparency (of each half before RPG Maker 2003 1.10), the
                // tint and saturation of a constant color, the flips, the
                // magnification and the effect when constant (not the blend mode)
                imageQuery: {
                    if (root.revision < 0) {
                        return ""
                    }
                    const parts = []
                    if (root.read("transparentColor") === 0) {
                        parts.push("transparent=0")
                    }
                    const top = root.read("topMode") === 0 ? root.read("top") : 0
                    const bottom = root.bottomShown ? root.read("bottom") : top
                    if (top !== 0 || bottom !== 0) {
                        parts.push("opacity=" + (100 - top) + "," + (100 - bottom))
                    }
                    if (root.read("colorMode") === 0) {
                        const tone = ["red", "green", "blue", "saturation"].map(id => root.read(id))
                        if (tone.some(one => one !== 100)) {
                            parts.push("tone=" + tone.join(","))
                        }
                    }
                    const flip = (root.read("flipX") !== 0 ? "x" : "") + (root.read("flipY") !== 0 ? "y" : "")
                    if (flip !== "") {
                        parts.push("flip=" + flip)
                    }
                    return parts.join("&")
                }
                // the power of the effect, unless a variable's (its modes:
                // read by the Player in Maniac Patch games only)
                readonly property int power: root.session.maniac && root.read("powerMode") !== 0 ? 0 : root.parameter(13)
                // Rotation: the speed in 256ths of a turn a frame, 60 frames a second
                turnSpeed: root.effect === 1 ? power * 360 / 256 * 60 : 0
                // Waver: rows swinging by 4 pixels per magnitude
                waver: root.effect === 2 ? power * 4 : 0
                // Fixed Angle (the Maniac Patch): degrees, the angle divided by the divisor (0: 1)
                angle: root.effect === 3 && root.read("divisorMode") === 0 ? Math.trunc(power / (root.read("divisor") || 1)) : 0
                // the magnification, unless a variable's (its mode: read in
                // RPG Maker 2003 1.12 and Maniac Patch games); a height of its
                // own in Maniac Patch games only, when the command has it
                readonly property int widthPercent: (root.runs112 || root.session.maniac) && root.read("magnificationMode") !== 0
                                                    ? 100 : root.read("magnification")
                readonly property bool separateHeight: root.session.maniac && root.parameter(20) >= 16
                                                       && root.revision >= 0 && (root.session.value("/parameters") ?? []).length > 31
                readonly property int heightPercent: !separateHeight ? widthPercent : root.read("heightMode") !== 0 ? 100 : root.read("height")
                scaleX: Math.max(0, widthPercent) / 100
                scaleY: Math.max(0, heightPercent) / 100
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                Controls.ToolTip.visible: pictureHover.hovered
                Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                Controls.ToolTip.text: qsTr("Click to choose the picture")
                HoverHandler {
                    id: pictureHover
                    cursorShape: Qt.PointingHandCursor
                }

                // the frames of its spritesheet (RPG Maker 2003 1.12). The rows
                // count only with columns, as the Player reads them: without,
                // they hold what older versions left there (millions in real
                // games). A line per frame only where frames are 2 pixels or
                // wider in the preview, so never more lines than it has pixels
                readonly property int columns: Math.max(0, root.read("columns"))
                readonly property int rows: columns > 0 ? Math.max(0, root.read("rows")) : 0
                readonly property int columnLines: columns > 1 && pictureRect.width / columns >= 2 ? columns - 1 : 0
                readonly property int rowLines: rows > 1 && pictureRect.height / rows >= 2 ? rows - 1 : 0
                // what is drawn over the picture turns with it (around its
                // center, the field's)
                Item {
                    anchors.fill: parent
                    rotation: picture.shownAngle
                    Repeater {
                        model: picture.columnLines
                        Rectangle {
                            required property int index
                            x: picture.pictureRect.x + Math.round((index + 1) * picture.pictureRect.width / picture.columns)
                            y: picture.pictureRect.y
                            width: 1
                            height: picture.pictureRect.height
                            color: Kirigami.Theme.highlightColor
                        }
                    }
                    Repeater {
                        model: picture.rowLines
                        Rectangle {
                            required property int index
                            x: picture.pictureRect.x
                            y: picture.pictureRect.y + Math.round((index + 1) * picture.pictureRect.height / picture.rows)
                            width: picture.pictureRect.width
                            height: 1
                            color: Kirigami.Theme.highlightColor
                        }
                    }
                    // where X and Y put it, when not its center: the origin of the Maniac Patch
                    Rectangle {
                        objectName: "pictureOriginPoint"
                        readonly property int origin: Math.max(0, Math.min(8, root.read("origin")))
                        readonly property var place: [[0.5, 0.5], [0, 0], [0, 1], [1, 0], [1, 1], [0.5, 0], [0.5, 1], [0, 0.5], [1, 0.5]][origin]
                        visible: origin !== 0 && picture.pictureRect.width > 0
                        x: picture.pictureRect.x + place[0] * picture.pictureRect.width - width / 2
                        y: picture.pictureRect.y + place[1] * picture.pictureRect.height - height / 2
                        width: 8
                        height: 8
                        radius: 4
                        color: Kirigami.Theme.highlightColor
                        border.color: Kirigami.Theme.textColor
                    }
                }
            }
            Ez.Label {
                objectName: "pictureFileName"
                text: {
                    const name = picture.fileName
                    if (name === "") {
                        return qsTr("No picture")
                    }
                    if (root.read("replace") === 0) {
                        return name
                    }
                    return qsTr("%1, last %2 characters: V[%3]").arg(name).arg(root.read("replaceCount"))
                        .arg(String(root.read("replaceVariable")).padStart(4, "0"))
                }
                color: Ez.Metrics.mutedTextColor
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }

    // the File tab: the number of the picture, its name, its transparent color
    Component {
        id: fileTab
        ColumnLayout {
            spacing: Ez.Metrics.rowSpacing
            GridLayout {
                columns: 2
                rowSpacing: Ez.Metrics.rowSpacing
                columnSpacing: Ez.Metrics.columnSpacing
                Layout.fillWidth: true
                Ez.FormLabel {
                    text: qsTr("Picture No.")
                }
                ModeValue {
                    modeId: "pictureMode"
                    valueId: "picture"
                    variableId: "pictureVariable"
                    name: "pictureId"
                }
                // the Maniac Patch: the name from a string variable
                Ez.FormLabel {
                    text: qsTr("Name given as")
                    visible: nameRow.visible
                    enabled: Ez.Origins.available("maniac")
                }
                RowLayout {
                    id: nameRow
                    spacing: Ez.Metrics.columnSpacing
                    visible: root.shown("nameMode")
                    CommandField {
                        session: root.session
                        fieldId: "nameMode"
                        name: "pictureNameMode"
                        Layout.fillWidth: false
                        Layout.preferredWidth: 130
                    }
                    FieldTag {
                        objectName: "pictureNameModeTag"
                        fieldId: "nameMode"
                    }
                    CommandField {
                        session: root.session
                        fieldId: "nameString"
                        name: "pictureName"
                        hides: true
                    }
                    CommandField {
                        session: root.session
                        fieldId: "nameVariable"
                        name: "pictureNameVariable"
                        hides: true
                        Layout.preferredWidth: 180
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
                // RPG Maker 2003 1.12: its last characters, the value of a variable
                CommandField {
                    id: replace
                    session: root.session
                    fieldId: "replace"
                    name: "pictureReplace"
                    Layout.columnSpan: 2
                }
                GridLayout {
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing
                    visible: replace.visible
                    Layout.columnSpan: 2
                    Layout.leftMargin: 3 * Ez.Metrics.sectionPadding
                    Ez.FormLabel {
                        text: qsTr("Characters")
                        enabled: replaceCount.enabled
                    }
                    CommandField {
                        id: replaceCount
                        session: root.session
                        fieldId: "replaceCount"
                        name: "pictureReplaceCount"
                    }
                    Ez.FormLabel {
                        text: qsTr("Variable")
                        enabled: replaceVariable.enabled
                    }
                    CommandField {
                        id: replaceVariable
                        session: root.session
                        fieldId: "replaceVariable"
                        name: "pictureReplaceVariable"
                        Layout.fillWidth: true
                    }
                }
                Ez.FormLabel {
                    text: qsTr("Transparent Color")
                }
                RowLayout {
                    spacing: 2 * Ez.Metrics.columnSpacing
                    Repeater {
                        model: [1, 0]
                        CommandRadio {
                            required property int modelData
                            session: root.session
                            fieldId: "transparentColor"
                            value: modelData
                            objectName: "pictureTransparentColor" + modelData
                        }
                    }
                }
            }
            Item {
                Layout.fillHeight: true
            }
        }
    }

    // Move Picture: the number of the picture
    Component {
        id: movePicture
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

    // the Advanced tab: RPG Maker 2003 1.12's spritesheet and layers (the tab has their tag)
    Component {
        id: advancedTab
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Ez.Section {
                title: qsTr("Spritesheet")
                GridLayout {
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing
                    Ez.FormLabel {
                        text: qsTr("Columns")
                        enabled: root.runs112
                    }
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        CommandField {
                            session: root.session
                            fieldId: "columns"
                            name: "pictureSheetColumns"
                        }
                        Ez.FormLabel {
                            text: qsTr("Rows")
                            enabled: root.runs112
                            Layout.leftMargin: Ez.Metrics.columnSpacing
                        }
                        CommandField {
                            session: root.session
                            fieldId: "rows"
                            name: "pictureSheetRows"
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                    }
                    Ez.FormLabel {
                        text: qsTr("Show")
                        enabled: root.runs112
                    }
                    RowLayout {
                        spacing: Ez.Metrics.columnSpacing
                        CommandField {
                            session: root.session
                            fieldId: "frameMode"
                            name: "pictureSheetMode"
                            Layout.fillWidth: false
                            Layout.preferredWidth: 150
                        }
                        CommandField {
                            session: root.session
                            fieldId: "frame"
                            name: "pictureSheetFrame"
                            hides: true
                        }
                        CommandField {
                            session: root.session
                            fieldId: "frameVariable"
                            name: "pictureSheetFrameVariable"
                            hides: true
                            Layout.preferredWidth: 180
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                    }
                    Item {
                        implicitWidth: 1
                    }
                    CommandField {
                        session: root.session
                        fieldId: "once"
                        name: "pictureSheetOnce"
                        tagShown: false
                    }
                }
            }
            Ez.Section {
                title: qsTr("Layer")
                GridLayout {
                    columns: 2
                    rowSpacing: Ez.Metrics.rowSpacing
                    columnSpacing: Ez.Metrics.columnSpacing
                    Ez.FormLabel {
                        text: qsTr("On the map")
                        enabled: root.runs112
                    }
                    CommandField {
                        session: root.session
                        fieldId: "mapLayer"
                        name: "pictureMapLayer"
                        Layout.fillWidth: true
                    }
                    Ez.FormLabel {
                        text: qsTr("In battle")
                        enabled: root.runs112
                    }
                    CommandField {
                        session: root.session
                        fieldId: "battleLayer"
                        name: "pictureBattleLayer"
                        Layout.fillWidth: true
                    }
                    Ez.FormLabel {
                        text: qsTr("Erase on")
                        enabled: root.runs112
                    }
                    RowLayout {
                        spacing: 2 * Ez.Metrics.columnSpacing
                        CommandField {
                            session: root.session
                            fieldId: "eraseOnMapChange"
                            name: "pictureEraseOnMapChange"
                            checkText: qsTr("Map change")
                            tagShown: false
                        }
                        CommandField {
                            session: root.session
                            fieldId: "eraseOnBattleEnd"
                            name: "pictureEraseOnBattleEnd"
                            checkText: qsTr("Battle end")
                            tagShown: false
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                    }
                    Ez.FormLabel {
                        text: qsTr("Affected by")
                        enabled: root.runs112
                    }
                    RowLayout {
                        spacing: 2 * Ez.Metrics.columnSpacing
                        CommandField {
                            session: root.session
                            fieldId: "tint"
                            name: "pictureAffectedByTint"
                            checkText: qsTr("Tint")
                            tagShown: false
                        }
                        CommandField {
                            session: root.session
                            fieldId: "flash"
                            name: "pictureAffectedByFlash"
                            checkText: qsTr("Flash")
                            tagShown: false
                        }
                        CommandField {
                            session: root.session
                            fieldId: "shake"
                            name: "pictureAffectedByShake"
                            checkText: qsTr("Shake")
                            tagShown: false
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                    }
                }
            }
            Item {
                Layout.fillHeight: true
            }
        }
    }
}
