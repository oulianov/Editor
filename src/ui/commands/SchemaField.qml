// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The control of a field of a command's schema (SchemaForm): a spin box
 * for a number, a list of the database for an ID, a file and its picker...
 * bound to the value of the field in the session. The control is named
 * after the field (objectName: its id).
 *
 * bound: the rules of the field show it; else it shows its default value
 * and writes nothing (a mode not chosen: greyed, as RPG Maker).
 *
 * A value with a mode of the Maniac Patch (a hero, or the one in a
 * variable; a name, or a string variable) has the box of its mode before
 * it (objectName: the mode's id), in the games of the patch or while the
 * mode is not RPG Maker's; the field of the mode chosen replaces it.
 */
Loader {
    id: root

    required property Ez.CommandEditSession session
    required property var field
    property bool bound: true
    /** The objectName of its control (the hand-written editors give their own, which scripts use) */
    property string name: field.id
    /** The field of its mode (Maniac Patch), null for none */
    readonly property var modeField: field.mode ? session.schema.field(field.mode) : null
    readonly property int modeValue: modeField && bound && session.revision >= 0 ? session.read(modeField.id) : 0
    readonly property bool modeShown: modeField !== null && (session.maniac || modeValue !== 0)
    /** The field shown: this one, or the field of another mode in its place */
    readonly property var shown: {
        if (!modeField || !bound || session.revision < 0) {
            return field
        }
        const others = session.schema.fields.filter(one => one.replaces === field.id)
        return others.find(one => session.holdsExcept(one.id, "")) ?? field
    }
    /** Its pointer in the session: its value, or its default when not bound */
    readonly property string key: bound ? shown.path : "/defaults/" + field.id
    /** The range of a number, the Maniac Patch's in its games */
    readonly property int minimum: (session.maniac ? shown.maniacMinimum : shown.minimum) ?? 0
    readonly property int maximum: (session.maniac ? shown.maniacMaximum : shown.maximum) ?? 0
    /** The map whose events an event field lists: 0 the map of the event; the
     *  map of its link (Clone Map Event's source map, 0: the current one); -1
     *  when a variable gives that map (an ID then) */
    readonly property int eventsMap: {
        if (shown.kind !== "Character" || !shown.link || session.revision < 0) {
            return 0
        }
        return Number(session.read(shown.link + "Mode")) === 0 ? Number(session.read(shown.link)) : -1
    }
    /** Its control takes the width of its row (the lists, the texts) */
    readonly property bool wide: (shown.list !== "" || ["File", "String", "Map", "Character", "Choice", "Tile", "Expression", "Location"].includes(shown.kind))
                                 && !(shown.preview && shown.kind === "Tile") || (shown.kind === "Sample" && shown.link === "tint")

    /** A text of the schema in the language of the interface */
    function tr(text) {
        if (!text || !text.text) {
            return ""
        }
        const shown = text.context === "" ? text.text : qsTranslate(text.context, text.text)
        return text.number >= 0 ? shown.arg(text.number) : shown
    }

    Layout.fillWidth: wide

    sourceComponent: modeShown ? withMode : control
    /** The control of the field shown */
    readonly property Component control: {
        switch (shown.kind) {
        case "Number":
            return shown.slider ? slider : number
        case "Check":
            return check
        case "Choice":
        case "CharacterSet":
            return choice
        case "Time":
            return time
        case "String":
            return text
        case "Note":
            return note
        case "Sample":
            return shown.link === "tint" ? tintSample : shown.link === "flash" ? flashSample : fadeButtons
        case "File":
            return shown.preview ? picture : file
        case "Map":
            return map
        case "MapX":
        case "MapY":
            return number
        case "Location":
            return location
        case "Character":
            return eventsMap < 0 ? number : character
        case "Tile":
            return shown.preview ? tilePreview : tile
        case "Expression":
            return expression
        default:
            // the IDs of the database, the string variables (StringId)
            return shown.list === "" ? number : shown.preview && shown.kind === "Animation" ? animation : entry
        }
    }

    Component {
        id: withMode
        // the mode, then the control of its value
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.ComboBox {
                objectName: root.field.mode
                jsonData: root.session
                key: root.bound ? root.modeField.path : "/defaults/" + root.modeField.id
                enabled: root.session.revision < 0 ? false : root.session.isEnabled(root.modeField.id)
                model: root.modeField.items.map(item => ({ value: item.value, text: root.tr(item.text) }))
            }
            Loader {
                sourceComponent: root.control
                Layout.fillWidth: root.wide
            }
        }
    }

    Ez.JsonField {
        id: current
        source: root.session
        path: root.key
    }

    /** The entries of a combo box, with the value of the command when it is none of them */
    function withUnknown(entries, unknownText) {
        const shown = Number(current.value)
        if (!current.valid || entries.some(entry => entry.value === shown)) {
            return entries
        }
        return entries.concat([{ value: shown, text: unknownText.arg(shown) }])
    }

    Component {
        id: number
        Ez.SpinBox {
            objectName: root.name
            jsonData: root.session
            key: root.key
            from: Math.min(root.minimum, Number(current.value ?? 0))
            to: Math.max(root.maximum, Number(current.value ?? 0))
            stepSize: root.shown.step
            suffix: root.tr(root.shown.suffix)
        }
    }

    Component {
        id: slider
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Ez.Slider {
                objectName: root.name + "Slider"
                jsonData: root.session
                key: root.key
                from: root.minimum
                to: root.maximum
                // a tick mark at each step, as the Widgets editor's (Shake Screen)
                tickInterval: root.shown.ticks ? Math.max(1, root.shown.step) : 0
                Layout.fillWidth: true
                Layout.minimumWidth: 100
            }
            Ez.SpinBox {
                objectName: root.name
                jsonData: root.session
                key: root.key
                from: root.minimum
                to: root.maximum
                stepSize: root.shown.step
                suffix: root.tr(root.shown.suffix)
            }
        }
    }

    Component {
        id: check
        Ez.CheckBox {
            objectName: root.name
            text: root.tr(root.shown.label)
            // inverted: checked for 0
            checked: current.valid && (Number(current.value) !== 0) !== root.shown.inverted
            onToggled: {
                if (current.valid) {
                    current.value = checked !== root.shown.inverted ? 1 : 0
                }
            }
        }
    }

    Component {
        id: choice
        Ez.ComboBox {
            objectName: root.name
            jsonData: root.session
            key: root.key
            model: {
                // the items of another engine or kept only while the command has them
                const shown = Number(current.value)
                const items = root.shown.kind === "CharacterSet" ? root.session.characterSets : root.shown.items
                return items.filter(item => item.value === shown
                                    || (!item.kept && (!item.rpg2003 || root.session.rpg2k3) && (!item.maniac || root.session.maniac)))
                            .map(item => ({ value: item.value, text: root.tr(item.text) }))
            }
        }
    }

    Component {
        id: time
        RowLayout {
            id: timeRow
            spacing: Ez.Metrics.columnSpacing
            // in seconds: shown as minutes and seconds
            readonly property int total: Number(current.value ?? 0)
            Ez.SpinBox {
                objectName: root.name + "Minutes"
                from: 0
                to: 99
                value: Math.floor(timeRow.total / 60)
                suffix: " " + qsTranslate("TimerOperandWidget", "Minutes")
                onValueModified: current.value = value * 60 + timeRow.total % 60
            }
            Ez.SpinBox {
                objectName: root.name + "Seconds"
                from: 0
                to: 59
                value: timeRow.total % 60
                suffix: " " + qsTranslate("TimerOperandWidget", "Seconds")
                onValueModified: current.value = Math.floor(timeRow.total / 60) * 60 + value
            }
        }
    }

    Component {
        id: text
        Ez.TextField {
            objectName: root.name
            jsonData: root.session
            key: root.key
            limit: root.shown.limit
        }
    }

    Component {
        id: note
        Ez.Note {
            text: root.tr(root.shown.label)
        }
    }

    Component {
        id: file
        RowLayout {
            id: fileRow
            spacing: Ez.Metrics.columnSpacing

            readonly property string folder: root.shown.folder
            readonly property bool audio: folder === "Music" || folder === "Sound"

            /** Opens the picker on the file; a music or a sound brings its settings */
            function pick() {
                const request = { folder: folder, name: current.value ?? "",
                                  preview: folder === "Music" ? "Music" : folder === "Sound" ? "Sound"
                                         : folder === "Movie" ? "None" : "Image" }
                // not those in variables (Maniac Patch)
                const settings = ["volume", "tempo", "balance", "fadeIn"].filter(id => root.session.path(id) !== ""
                    && (root.session.path(id + "Mode") === "" || root.session.read(id + "Mode") === 0))
                if (audio) {
                    for (const id of settings) {
                        request[id] = root.session.read(id)
                    }
                }
                const choice = Ez.Pickers.pickFile(request, fileRow)
                if (!choice) {
                    return
                }
                current.value = choice.name
                if (audio) {
                    // the settings moved in the picker
                    for (const id of settings) {
                        if (request[id] !== undefined && choice[id] !== request[id]) {
                            root.session.write(id, choice[id])
                        }
                    }
                }
            }

            Ez.TextField {
                objectName: root.name
                jsonData: root.session
                key: root.key
                Layout.fillWidth: true
                Layout.minimumWidth: 120 + buttonsWidth
                buttons: [
                    Ez.FieldButton {
                        objectName: root.name + "Picker"
                        fused: true
                        onClicked: fileRow.pick()
                    },
                    Ez.FieldButton {
                        // no music, no sound: like the first entry of RPG Maker's list
                        objectName: root.name + "Off"
                        fused: true
                        visible: root.shown.off
                        text: qsTranslate("MediaAudioWidget", "(OFF)")
                        onClicked: current.value = "(OFF)"
                    }
                ]
            }
        }
    }

    Component {
        id: entry
        Ez.RefComboBox {
            id: refBox
            objectName: root.name
            database: root.session.database
            listKey: root.shown.list
            // Change Equipment: only what a hero equips
            ids: root.shown.equipment ? root.session.equipmentIds() : undefined
            noneText: root.shown.none ? root.tr(root.shown.none.text) : ""
            noneValue: root.shown.none ? root.shown.none.value : 0
            jsonData: root.session
            key: root.key
            pickable: root.bound
            // the session's picker: its database is read again after it
            pickHandler: function() {
                const id = root.session.pickEntry(root.shown.list, Number(current.value), !!root.shown.none, refBox)
                if (id !== null && id !== undefined) {
                    current.value = id
                }
                return id
            }
        }
    }

    Component {
        id: map
        Ez.ComboBox {
            objectName: root.name
            jsonData: root.session
            key: root.key
            // the items of the field first (Clone Map Event's current map)
            model: root.withUnknown((root.shown.items ?? []).map(item => ({ value: item.value, text: root.tr(item.text) }))
                                    .concat(root.session.maps), qsTranslate("LocationCommandWidget", "Unknown Map (%1)"))
        }
    }

    Component {
        id: character
        // searched by number or name, as Move Event's target
        Ez.RefComboBox {
            objectName: root.name
            jsonData: root.session
            key: root.key
            // the items kept only while the command has them
            choices: root.withUnknown(root.shown.items.filter(item => !item.kept || item.value === Number(current.value))
                                    .map(item => ({ value: item.value, text: root.tr(item.text) }))
                                    .concat(root.eventsMap === 0 ? root.session.events : root.session.eventsOf(root.eventsMap)),
                                    qsTranslate("LocationCommandWidget", "Unknown Event (%1)"))
        }
    }

    Component {
        id: location
        // RPG Maker 2003's location box: the map and the cell, a click (or
        // its "...") picks them on the map view
        Ez.PathField {
            id: locationBox

            readonly property bool fixed: root.shown.link === ""
            // the fields of the location (hidden: this box shows them)
            readonly property var xField: root.session.schema.fields.find(one => one.kind === "MapX" && one.link === root.shown.link)
            readonly property var yField: root.session.schema.fields.find(one => one.kind === "MapY" && one.link === root.shown.link)
            readonly property string xId: xField ? xField.id : "x"
            readonly property string yId: yField ? yField.id : "y"
            readonly property int mapId: root.session.revision < 0 ? 0
                : fixed ? root.session.eventMap : root.session.read(root.shown.link)
            readonly property bool pickable: root.bound && root.session.canPickLocation && (!fixed || root.session.eventMap > 0)

            /** The name of the map of the location, "" for none */
            function mapName() {
                const map = root.session.maps.find(one => one.value === mapId)
                return map ? map.text.replace(/^\d+: /, "")
                    : mapId > 0 ? qsTranslate("LocationCommandWidget", "Unknown Map (%1)").arg(mapId) : ""
            }
            function pick() {
                if (!pickable) {
                    return
                }
                const facing = root.session.path("facing") !== "" && root.session.isEnabled("facing")
                               ? root.session.read("facing") : -1
                const chosen = root.session.pickLocation(fixed ? 0 : mapId, root.session.read(xId), root.session.read(yId),
                                                         facing, locationBox)
                if (!chosen) {
                    return
                }
                if (!fixed) {
                    root.session.write(root.shown.link, chosen.map)
                }
                root.session.write(xId, chosen.x)
                root.session.write(yId, chosen.y)
                if (facing >= 0 && chosen.direction >= 0) {
                    root.session.write("facing", chosen.direction)
                }
            }

            fieldName: root.name
            buttonName: "locationPicker"
            buttonToolTip: qsTranslate("LocationCommandWidget", "Select on Map...")
            field.readOnly: true
            field.activeFocusOnPress: false
            // empty while another mode is chosen (its values are the variables)
            text: root.session.revision < 0 || !root.bound ? ""
                : qsTranslate("LocationCommandWidget", "%1 (%2, %3)").arg(mapName())
                      .arg(root.session.read(xId)).arg(root.session.read(yId))
            button.enabled: pickable
            onBrowse: pick()
            Layout.fillWidth: true

            TapHandler {
                parent: locationBox.field
                onTapped: locationBox.pick()
            }
        }
    }

    Component {
        id: expression
        ColumnLayout {
            id: expressionColumn
            spacing: Ez.Metrics.rowSpacing
            readonly property var read: root.session.revision < 0 ? null : root.session.expression(root.shown.id)
            /** The offset of the error of the text typed, -1 for none */
            property int error: -1

            /** A new expression: S[1] (an empty one is no expression) */
            function start() {
                if (root.bound && read.readable && read.text === "") {
                    root.session.setExpression(root.shown.id, "S[1]")
                }
            }
            Component.onCompleted: start()
            Connections {
                target: root
                function onBoundChanged() {
                    expressionColumn.start()
                }
            }

            Ez.TextField {
                id: expressionText
                objectName: root.name
                Layout.fillWidth: true
                Layout.minimumWidth: 240
                readOnly: !expressionColumn.read.readable || !root.bound
                font: Ez.Metrics.monospaceFont
                hoverEnabled: true
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: 600
                Controls.ToolTip.text: qsTranslate("ManiacCommandWidget", "Variables V[1], switches S[1], pointers V[V[1]], ranges V[1..5], arrays [1, 2][V[1]], "
                    + "C operators (+ - * / % & | ^ << >> == != < <= > >= && || ! ~ ?:), assignments (V[1] = 2, +=...) "
                    + "and functions: rand, item, event, actor, party, enemy, misc, pow, sqrt, sin, cos, atan2, min, max, "
                    + "abs, clamp, muldiv, divmul, between, lerp, arraysum, arraymin, arraymax.")
                // the text of the command, but not while typing (it is written as typed)
                Binding on text {
                    value: expressionColumn.read.text ?? ""
                    when: !expressionText.activeFocus
                    restoreMode: Binding.RestoreNone
                }
                onTextEdited: expressionColumn.error = root.session.setExpression(root.shown.id, text)
            }
            Ez.Note {
                visible: !expressionColumn.read.readable || expressionColumn.error >= 0
                text: !expressionColumn.read.readable
                      ? qsTranslate("ManiacCommandWidget", "This expression cannot be read: it is kept as it is, use Edit raw to change it.")
                      : qsTranslate("ManiacCommandWidget", "Not a valid expression (at character %1): it is not saved.").arg(expressionColumn.error + 1)
                Layout.fillWidth: true
            }
        }
    }

    Component {
        id: tile
        RowLayout {
            id: tileRow
            spacing: Ez.Metrics.columnSpacing
            Ez.SpinBox {
                objectName: root.name
                jsonData: root.session
                key: root.key
                from: Math.min(root.shown.minimum, Number(current.value ?? 0))
                to: Math.max(root.shown.maximum, Number(current.value ?? 0))
                Layout.fillWidth: true
            }
            Ez.FieldButton {
                objectName: root.name + "Picker"
                text: qsTranslate("LocationCommandWidget", "Set")
                visible: root.session.canPickTile
                // the chipset of the map of the event
                enabled: root.bound && root.session.eventMap > 0
                onClicked: {
                    const layer = root.shown.link !== "" ? root.session.read(root.shown.link) : 0
                    const chosen = root.session.pickTile(layer, Number(current.value), tileRow)
                    if (chosen !== null && chosen !== undefined) {
                        current.value = chosen
                    }
                }
            }
        }
    }

    /** The value of another field of the command, 0 when it has none */
    function valueOf(id) {
        return root.session.revision < 0 || root.session.path(id) === "" ? 0 : root.session.read(id)
    }

    Component {
        id: tintSample
        // RPG Maker's sample of Tint Screen: colours and greys, tinted
        Rectangle {
            objectName: "sample"
            implicitWidth: 364
            implicitHeight: 22
            color: "transparent"
            border.color: Ez.Metrics.sectionBorderColor
            Layout.fillWidth: true
            Canvas {
                id: tintCanvas
                anchors.fill: parent
                anchors.margins: 1
                readonly property var tone: [root.valueOf("red"), root.valueOf("green"), root.valueOf("blue"), root.valueOf("saturation")]
                onToneChanged: requestPaint()
                onWidthChanged: requestPaint()
                onPaint: {
                    const context = getContext("2d")
                    const half = Math.floor(height / 2)
                    for (let x = 0; x < width; ++x) {
                        const hue = Qt.hsva(x / width, 1, 1, 1)
                        const grey = x / Math.max(1, width - 1)
                        const lines = [[hue.r, hue.g, hue.b, 0, half], [grey, grey, grey, half, height - half]]
                        for (const line of lines) {
                            // the saturation, then each channel (100: unchanged), as the Player
                            const mean = (line[0] + line[1] + line[2]) / 3
                            const channel = (value, index) => Math.max(0, Math.min(1,
                                (mean + (value - mean) * tone[3] / 100) * tone[index] / 100))
                            context.fillStyle = Qt.rgba(channel(line[0], 0), channel(line[1], 1), channel(line[2], 2), 1)
                            context.fillRect(x, line[3], 1, line[4])
                        }
                    }
                }
            }
        }
    }

    Component {
        id: flashSample
        // RPG Maker's sample of a flash: its colour at its strength (0-31)
        ColumnLayout {
            spacing: Ez.Metrics.rowSpacing
            Ez.FormLabel {
                text: qsTranslate("FlashScreenWidget", "Sample")
            }
            Rectangle {
                objectName: "sample"
                implicitWidth: 49
                implicitHeight: 78
                border.color: Ez.Metrics.sectionBorderColor
                gradient: Gradient {
                    GradientStop { position: 0; color: "#7fa7d8" }
                    GradientStop { position: 0.6; color: "#d8d0b8" }
                    GradientStop { position: 1; color: "#6f8f4f" }
                }
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    color: Qt.rgba(root.valueOf("red") / 31, root.valueOf("green") / 31, root.valueOf("blue") / 31, 1)
                    opacity: root.enabled ? root.valueOf("strength") / 31 : 0
                }
            }
        }
    }

    Component {
        id: fadeButtons
        // RPG Maker's buttons beside the time: the music of the map, then its fade-out
        RowLayout {
            id: fade
            spacing: Ez.Metrics.columnSpacing
            readonly property var music: root.session.revision < 0 ? ({}) : root.session.mapMusic()
            /** The volume while fading out, from the music's to 0 */
            property real level: 0
            Ez.AudioPlayer {
                id: player
                volume: Math.round(fade.level)
            }
            Timer {
                // a moment at its volume, then the fade-out
                id: fadeStart
                interval: 1000
                onTriggered: fadeOut.start()
            }
            NumberAnimation {
                id: fadeOut
                target: fade
                property: "level"
                to: 0
                duration: Math.max(1, root.valueOf("time"))
                onFinished: player.stop()
            }
            Ez.FieldButton {
                objectName: "fadePlay"
                text: ""
                icon.name: "media-playback-start"
                enabled: !!fade.music.path
                Controls.ToolTip.visible: hovered
                Controls.ToolTip.delay: 600
                Controls.ToolTip.text: enabled ? qsTranslate("FadeOutBGMWidget", "Play the music of the map and fade it out")
                                               : qsTranslate("FadeOutBGMWidget", "The map plays no music")
                onClicked: {
                    fadeOut.stop()
                    fade.level = fade.music.volume
                    player.tempo = fade.music.tempo
                    player.balance = fade.music.balance
                    player.play(fade.music.path)
                    fadeStart.restart()
                }
            }
            Ez.FieldButton {
                objectName: "fadeStop"
                text: ""
                icon.name: "media-playback-stop"
                enabled: player.state !== Ez.AudioPlayer.Stopped
                onClicked: {
                    fadeStart.stop()
                    fadeOut.stop()
                    player.stop()
                }
            }
            Component.onDestruction: player.stop()
        }
    }

    Component {
        id: animation
        // RPG Maker's animated preview above the list
        ColumnLayout {
            spacing: Ez.Metrics.rowSpacing
            Rectangle {
                id: animationBox
                readonly property int animationId: Number(current.value ?? 0)
                readonly property var animations: root.session.database ? root.session.database.list("animations") : null
                readonly property bool animated: animationId >= 1 && !!animations && animationId <= animations.count
                objectName: root.name + "Preview"
                color: "black"
                border.color: Ez.Metrics.sectionBorderColor
                // the middle of the battle screen (320 x 240) at half its size
                implicitWidth: 176
                implicitHeight: 101
                Layout.fillWidth: true
                // inside the frame
                Item {
                    id: screen
                    anchors.fill: parent
                    anchors.margins: 1
                    clip: true
                    Ez.AnimationCanvas {
                        id: animationCanvas
                        animationId: animationBox.animated ? animationBox.animationId : 0
                        target: -1
                        showOrigin: false
                        // a preview: the cels are not edited here
                        enabled: false
                        visible: animationBox.animated
                        width: 320
                        height: 240
                        transformOrigin: Item.TopLeft
                        scale: 0.5
                        x: (screen.width - 320 * scale) / 2
                        y: (screen.height - 240 * scale) / 2
                        onAnimationIdChanged: restart.restart()
                        // played to its end, it starts again a moment later
                        onPlayingChanged: {
                            if (!playing && animationBox.animated) {
                                restart.restart()
                            }
                        }
                        Timer {
                            id: restart
                            interval: 400
                            onTriggered: {
                                if (animationBox.animated && animationBox.visible && !animationCanvas.playing) {
                                    animationCanvas.togglePlay()
                                }
                            }
                        }
                    }
                }
            }
            Loader {
                sourceComponent: entry
                Layout.fillWidth: true
            }
        }
    }

    Component {
        id: picture
        // RPG Maker's picture of the file, its Set button under it
        ColumnLayout {
            id: pictureColumn
            spacing: Ez.Metrics.rowSpacing
            readonly property string name: current.value ?? ""
            function pick() {
                const choice = Ez.Pickers.pickFile({ folder: root.shown.folder, name: name, preview: "Image" }, pictureColumn)
                if (choice) {
                    current.value = choice.name
                }
            }
            Rectangle {
                objectName: root.name + "Preview"
                implicitWidth: 164
                implicitHeight: 124
                color: "transparent"
                border.color: Ez.Metrics.sectionBorderColor
                clip: true
                Image {
                    anchors.fill: parent
                    anchors.margins: 1
                    visible: pictureColumn.name !== ""
                    source: visible ? "image://project/" + root.shown.folder + "/" + pictureColumn.name : ""
                    fillMode: Image.PreserveAspectFit
                    horizontalAlignment: Image.AlignLeft
                    verticalAlignment: Image.AlignTop
                    smooth: false
                    asynchronous: true
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: root.bound
                    cursorShape: Qt.PointingHandCursor
                    onClicked: pictureColumn.pick()
                }
            }
            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                Layout.preferredWidth: 164
                Ez.TextField {
                    objectName: root.name
                    jsonData: root.session
                    key: root.key
                    readOnly: true
                    Layout.fillWidth: true
                }
                Ez.FieldButton {
                    objectName: root.name + "Picker"
                    text: qsTranslate("LocationCommandWidget", "Set")
                    onClicked: pictureColumn.pick()
                }
            }
        }
    }

    Component {
        id: tilePreview
        // RPG Maker's tile beside its Set button (the chipset of the map of the event)
        RowLayout {
            id: tileView
            spacing: Ez.Metrics.columnSpacing
            readonly property int tileLayer: root.session.revision < 0 || root.shown.link === "" ? 0 : root.session.read(root.shown.link)
            readonly property var graphic: root.session.revision < 0 ? ({}) : root.session.tileGraphic(tileLayer, Number(current.value ?? 0))
            readonly property bool drawn: graphic.chipset !== undefined
            function pick() {
                const chosen = root.session.pickTile(tileLayer, Number(current.value), tileView)
                if (chosen !== null && chosen !== undefined) {
                    current.value = chosen
                }
            }
            Rectangle {
                objectName: root.name + "Preview"
                visible: tileView.drawn
                implicitWidth: 36
                implicitHeight: 36
                color: "transparent"
                border.color: Ez.Metrics.sectionBorderColor
                Ez.MapLayerView {
                    x: 2
                    y: 2
                    width: 32
                    height: 32
                    chipsetId: tileView.graphic.chipset ?? -1
                    gridTiles: tileView.drawn ? [tileView.graphic.tile] : []
                    gridColumns: 1
                    zoom: 32 / Math.max(1, tileSize)
                }
            }
            // without the map: its number
            Ez.SpinBox {
                objectName: root.name
                visible: !tileView.drawn
                jsonData: root.session
                key: root.key
                from: Math.min(root.shown.minimum, Number(current.value ?? 0))
                to: Math.max(root.shown.maximum, Number(current.value ?? 0))
            }
            Ez.FieldButton {
                objectName: root.name + "Picker"
                text: qsTranslate("LocationCommandWidget", "Set")
                visible: root.session.canPickTile
                enabled: root.bound && root.session.eventMap > 0
                onClicked: tileView.pick()
            }
        }
    }
}
