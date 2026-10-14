// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "PickerDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The resource picker (Pickers::pickFile, Ez.Pickers.pickFile): the files
 * of a folder of the game and of the RTP on the left, the file selected on
 * the right: its preview, what it is, and what is chosen with it (the
 * character or face clicked, a pose, a hue, the settings of a sound, which
 * can be heard). A file is always selected: the one of the data, else the
 * first. OK and Apply give the choice; Apply keeps the picker open.
 */
Ez.DialogWindow {
    id: root

    required property Ez.FilePickerModel picker
    /** The title asked for (empty: "Select from <folder>") */
    property string customTitle

    readonly property int preview: picker.preview
    readonly property bool audio: preview === Ez.FilePickerModel.Music || preview === Ez.FilePickerModel.Sound
    readonly property bool image: !audio && preview !== Ez.FilePickerModel.None
    /** The zoom of the preview; the buttons go to whole pixels (x1, x2, x4...) */
    property real zoom: preview === Ez.FilePickerModel.CharSet ? 2 : 1
    readonly property real minimumZoom: 1
    readonly property real maximumZoom: 16

    function zoomIn() {
        zoom = Math.min(maximumZoom, Math.pow(2, Math.floor(Math.log2(zoom) + 1e-6) + 1))
    }
    function zoomOut() {
        zoom = Math.max(minimumZoom, Math.pow(2, Math.ceil(Math.log2(zoom) - 1e-6) - 1))
    }

    objectName: "PickerDialog"
    title: customTitle !== "" ? customTitle : qsTr("Select from %1").arg(picker.folder)
    standardButtons: Controls.DialogButtonBox.Ok | Controls.DialogButtonBox.Cancel
                     | Controls.DialogButtonBox.Apply | Controls.DialogButtonBox.Help
    preferredWidth: 720
    height: Math.max(minimumHeight, 540)

    Component.onCompleted: {
        files.show(picker.currentRow)
        // no file (an empty folder): nothing to give
        for (const kind of [Controls.DialogButtonBox.Ok, Controls.DialogButtonBox.Apply]) {
            const button = buttonBox.standardButton(kind)
            if (button) {
                button.enabled = Qt.binding(() => root.picker.currentRow >= 0)
            }
        }
    }
    onClosing: picker.stop()

    Connections {
        target: root.picker
        function onCurrentChanged() {
            if (files.currentIndex !== root.picker.currentRow) {
                files.show(root.picker.currentRow)
            }
        }
    }
    Connections {
        target: root.buttonBox
        function onApplied() {
            root.picker.apply()
        }
        function onHelpRequested() {
            Ez.Editor.help("resource-picker", root.contentItem)
        }
    }

    Controls.SplitView {
        id: split
        objectName: "splitterPicker"
        orientation: Qt.Horizontal
        Layout.fillWidth: true
        Layout.fillHeight: true
        implicitWidth: files.implicitWidth + side.implicitWidth + 8
        implicitHeight: Math.max(files.implicitHeight, side.implicitHeight)

        Ez.PickerList {
            id: files
            listName: "filesystemView"
            model: root.picker
            dimRole: "rtp"
            Controls.SplitView.fillWidth: true
            Controls.SplitView.minimumWidth: 160
            onPicked: row => root.picker.currentRow = row
            onActivated: row => {
                // a double-click plays a sound, picks a picture
                if (root.audio) {
                    root.picker.play()
                } else if (row >= 0) {
                    root.accept()
                }
            }
        }

        ColumnLayout {
            id: side
            spacing: Ez.Metrics.sectionSpacing
            Controls.SplitView.preferredWidth: Math.max(implicitWidth, 330)
            Controls.SplitView.minimumWidth: implicitWidth

            // the preview, scrolled when zoomed
            Rectangle {
                id: previewFrame
                objectName: "graphicsView"
                visible: root.image
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                Kirigami.Theme.inherit: false
                color: Kirigami.Theme.backgroundColor
                border.color: Ez.Metrics.sectionBorderColor
                radius: 2
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 324
                Layout.minimumHeight: 244

                Flickable {
                    id: flick
                    anchors.fill: parent
                    anchors.margins: 1
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    contentWidth: Math.max(width, sprite.width)
                    contentHeight: Math.max(height, sprite.height)
                    // shown while the picture is larger than its frame
                    Controls.ScrollBar.vertical: Controls.ScrollBar {
                        policy: Controls.ScrollBar.AlwaysOn
                    }
                    Controls.ScrollBar.horizontal: Controls.ScrollBar {
                        policy: Controls.ScrollBar.AlwaysOn
                    }

                    Loader {
                        id: sprite
                        // centered while smaller than the frame
                        x: Math.max(0, (flick.width - width) / 2)
                        y: Math.max(0, (flick.height - height) / 2)
                        active: root.image
                        sourceComponent: {
                            switch (root.preview) {
                            case Ez.FilePickerModel.CharSet:
                                return charSet
                            case Ez.FilePickerModel.CharSetPose:
                                return charSetPose
                            case Ez.FilePickerModel.FaceSet:
                                return faceSet
                            case Ez.FilePickerModel.Monster:
                                return monster
                            default:
                                return wholeImage
                            }
                        }
                    }
                }
                // the trackpad pinch zooms freely
                PinchHandler {
                    property real startZoom: 1
                    target: null
                    onActiveChanged: {
                        if (active) {
                            startZoom = root.zoom
                        }
                    }
                    onActiveScaleChanged: {
                        if (active) {
                            root.zoom = Math.max(root.minimumZoom, Math.min(root.maximumZoom, startZoom * activeScale))
                        }
                    }
                }
            }

            // what the file is, the zoom beside it
            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                Layout.fillWidth: true
                Ez.Label {
                    objectName: "labelInfo"
                    text: root.picker.info
                    font: Ez.Metrics.font
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.alignment: Qt.AlignTop
                }
                Ez.Button {
                    objectName: "pushZoomOut"
                    visible: root.image
                    text: qsTranslate("PreviewZoom", "Zoom Out")
                    enabled: root.zoom > root.minimumZoom
                    Layout.alignment: Qt.AlignTop
                    onClicked: root.zoomOut()
                }
                Ez.Button {
                    objectName: "pushZoomIn"
                    visible: root.image
                    text: qsTranslate("PreviewZoom", "Zoom In")
                    enabled: root.zoom < root.maximumZoom
                    Layout.alignment: Qt.AlignTop
                    onClicked: root.zoomIn()
                }
            }

            // the facing and the frame of a pose (also clicked in the sheet)
            Ez.Section {
                visible: root.preview === Ez.FilePickerModel.CharSetPose
                Layout.fillHeight: false
                title: qsTranslate("PickerCharsetWidget", "Facing")
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Repeater {
                        model: [qsTranslate("PickerCharsetWidget", "Up"), qsTranslate("PickerCharsetWidget", "Right"),
                                qsTranslate("PickerCharsetWidget", "Down"), qsTranslate("PickerCharsetWidget", "Left")]
                        delegate: Ez.RadioButton {
                            id: facing
                            required property int index
                            required property string modelData
                            objectName: "radioFacing" + index
                            text: modelData
                            onToggled: root.picker.direction = index
                            Binding {
                                target: facing
                                property: "checked"
                                value: root.picker.direction === facing.index
                            }
                        }
                    }
                }
            }
            Ez.Section {
                visible: root.preview === Ez.FilePickerModel.CharSetPose
                Layout.fillHeight: false
                title: qsTranslate("PickerCharsetWidget", "Frame")
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Repeater {
                        model: [qsTranslate("PickerCharsetWidget", "Left"), qsTranslate("PickerCharsetWidget", "Middle"),
                                qsTranslate("PickerCharsetWidget", "Right")]
                        delegate: Ez.RadioButton {
                            id: frameOption
                            required property int index
                            required property string modelData
                            objectName: "radioFrame" + index
                            text: modelData
                            onToggled: root.picker.pattern = index
                            Binding {
                                target: frameOption
                                property: "checked"
                                value: root.picker.pattern === frameOption.index
                            }
                        }
                    }
                }
            }

            // the color alteration of a monster
            Ez.Section {
                visible: root.preview === Ez.FilePickerModel.Monster
                Layout.fillHeight: false
                title: qsTranslate("EnemyPickerWidget", "Color Alteration")
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Ez.Slider {
                        id: hueSlider
                        objectName: "sliderHue"
                        from: 0
                        to: 359
                        Layout.fillWidth: true
                        onMoved: root.picker.hue = value
                        Binding on value {
                            value: root.picker.hue
                        }
                    }
                    Ez.SpinBox {
                        objectName: "spinHue"
                        from: 0
                        to: 359
                        onValueModified: root.picker.hue = value
                        Binding on value {
                            value: root.picker.hue
                        }
                    }
                }
            }

            // the settings of a music or sound, heard at once: by the steps
            // of RPG Maker's trackbars (11 positions), each with its default
            // (lcf::rpg::Music, lcf::rpg::Sound)
            Ez.Section {
                objectName: "groupFadeIn"
                visible: root.preview === Ez.FilePickerModel.Music
                Layout.fillHeight: false
                title: qsTranslate("PickerAudioWidget", "Fade In Time")
                Ez.SliderField {
                    name: "FadeIn"
                    from: 0
                    to: 10
                    tickInterval: 1
                    defaultValue: 0
                    value: root.picker.fadeIn / 1000
                    valueText: seconds => (Math.round(seconds * 10) / 10) + "s"
                    onMoved: seconds => root.picker.fadeIn = Math.round(seconds * 1000)
                }
            }
            Ez.Section {
                visible: root.audio
                Layout.fillHeight: false
                title: qsTranslate("PickerAudioWidget", "Volume")
                Ez.SliderField {
                    name: "Volume"
                    from: 0
                    to: 100
                    tickInterval: 10
                    defaultValue: 100
                    value: root.picker.volume
                    valueText: volume => volume + "%"
                    onMoved: volume => root.picker.volume = Math.round(volume)
                }
            }
            Ez.Section {
                visible: root.audio
                Layout.fillHeight: false
                title: qsTranslate("PickerAudioWidget", "Tempo")
                Ez.SliderField {
                    name: "Tempo"
                    from: 50
                    to: 150
                    tickInterval: 10
                    defaultValue: 100
                    value: root.picker.tempo
                    valueText: tempo => tempo + "%"
                    onMoved: tempo => root.picker.tempo = Math.round(tempo)
                }
            }
            Ez.Section {
                visible: root.audio
                Layout.fillHeight: false
                title: qsTranslate("PickerAudioWidget", "Balance")
                Ez.SliderField {
                    name: "Balance"
                    from: 0
                    to: 100
                    tickInterval: 10
                    defaultValue: 50
                    value: root.picker.balance
                    valueText: balance => (100 - balance) + ":" + balance
                    onMoved: balance => root.picker.balance = Math.round(balance)
                }
            }
            RowLayout {
                visible: root.audio
                spacing: Ez.Metrics.columnSpacing
                Ez.Button {
                    objectName: "buttonPlay"
                    text: qsTranslate("PickerAudioWidget", "Play")
                    icon.name: "media-playback-start"
                    onClicked: root.picker.play()
                }
                Ez.Button {
                    objectName: "buttonStop"
                    text: qsTranslate("PickerAudioWidget", "Stop")
                    icon.name: "media-playback-stop"
                    enabled: root.picker.playing
                    onClicked: root.picker.stop()
                }
                Item {
                    Layout.fillWidth: true
                }
            }
            Ez.Label {
                objectName: "labelStatus"
                visible: root.audio
                text: root.picker.playStatus
                font: Ez.Metrics.font
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                Layout.preferredWidth: 1
            }
            // the controls at the top, the room left below them
            Item {
                visible: !root.image
                Layout.fillHeight: true
            }
        }
    }

    // the whole picture, as in its file
    Component {
        id: wholeImage
        Image {
            source: root.picker.currentName !== ""
                    ? "image://project/" + root.picker.folder + "/" + encodeURIComponent(root.picker.currentName) + "?transparent=0"
                    : ""
            cache: false
            smooth: false
            width: implicitWidth * root.zoom
            height: implicitHeight * root.zoom
        }
    }
    // the 8 characters of the sheet, the one chosen framed
    Component {
        id: charSet
        Ez.CharSetPaintedItem {
            id: characters
            pickerMode: true
            path: root.picker.currentPath
            width: implicitWidth * root.zoom
            height: implicitHeight * root.zoom
            onCellIndexChanged: root.picker.spriteIndex = characters.cellIndex
            Binding on cellIndex {
                value: Math.max(0, Math.min(7, root.picker.spriteIndex))
            }
        }
    }
    // every pose of the sheet: a click chooses the character, its facing and its frame
    Component {
        id: charSetPose
        Ez.CharSetPaintedItem {
            id: sheet
            pickerMode: true
            poseMode: true
            path: root.picker.currentPath
            width: implicitWidth * root.zoom
            height: implicitHeight * root.zoom
            onCellIndexChanged: root.picker.spriteIndex = sheet.cellIndex
            onFacingChanged: root.picker.direction = sheet.facing
            onFrameChanged: root.picker.pattern = sheet.frame
            Binding on cellIndex {
                value: Math.max(0, Math.min(7, root.picker.spriteIndex))
            }
            Binding on facing {
                value: root.picker.direction
            }
            Binding on frame {
                value: root.picker.pattern
            }
        }
    }
    Component {
        id: faceSet
        Ez.FaceSetPaintedItem {
            id: faces
            pickerMode: true
            path: root.picker.currentPath
            width: implicitWidth * root.zoom
            height: implicitHeight * root.zoom
            onCellIndexChanged: root.picker.spriteIndex = faces.cellIndex
            Binding on cellIndex {
                value: Math.max(0, Math.min(15, root.picker.spriteIndex))
            }
        }
    }
    Component {
        id: monster
        Ez.MonsterPaintedItem {
            pickerMode: true
            path: root.picker.currentPath
            hue: root.picker.hue
            width: implicitWidth * root.zoom
            height: implicitHeight * root.zoom
        }
    }
}
