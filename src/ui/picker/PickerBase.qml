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
 * The resource picker (Pickers::pickFile, Ez.Pickers.pickFile): the files of
 * a folder of the game and of the RTP on the left, the file selected on the
 * right: its preview (previewContent, zoomed), what it is, and what is
 * chosen with it (formContent: the character or face clicked, a pose, a
 * hue, the settings of a sound, which can be heard). A file is always
 * selected: the one of the data, else the first. OK and Apply give the
 * choice (pickerData); Apply keeps the picker open.
 *
 * The pickers of each kind of file are made on it (CharSetPicker,
 * FaceSetPicker, ImagePicker, MonsterPicker, SoundPicker, MusicPicker);
 * alone, it shows no preview (movies).
 */
Ez.DialogWindow {
    id: root

    /** The files and the player (made by Pickers::pickFile) */
    required property Ez.FilePickerModel picker
    /** The title asked for (empty: "Select from <folder>") */
    property string customTitle

    /**
     * What is picked: the file selected and what is chosen with it, edited by
     * the picker. The picker's model follows it (what the player plays, what
     * OK and Apply give).
     */
    readonly property Ez.PickerData pickerData: Ez.PickerData {}
    readonly property string filename: picker.currentName
    readonly property string directory: picker.folder

    /** The controls under the preview and the information of the file (sections) */
    default property alias formContent: column.data
    /** The preview of the file (one item, as large as its zoom), scrolled when zoomed */
    property alias previewContent: previewHolder.data
    readonly property Item previewItem: previewHolder.children.length > 0 ? previewHolder.children[0] : null

    /** The zoom of the preview; the buttons go to whole pixels (x1, x2, x4...) */
    property real zoom: 1
    readonly property real minimumZoom: 1
    readonly property real maximumZoom: 16

    function zoomIn() {
        zoom = Math.min(maximumZoom, Math.pow(2, Math.floor(Math.log2(zoom) + 1e-6) + 1))
    }
    function zoomOut() {
        zoom = Math.max(minimumZoom, Math.pow(2, Math.ceil(Math.log2(zoom) - 1e-6) - 1))
    }

    /** A double-click or Return on a file is OK (false: the audio pickers play it) */
    property bool acceptOnActivate: true
    /** A file was double-clicked, or Return pressed on it */
    signal fileActivated()

    /** The choice is in pickerData: the model follows it from then on */
    property bool ready: false

    objectName: "PickerDialog"
    title: customTitle !== "" ? customTitle : qsTr("Select from %1").arg(picker.folder)
    standardButtons: Controls.DialogButtonBox.Ok | Controls.DialogButtonBox.Cancel
                     | Controls.DialogButtonBox.Apply | Controls.DialogButtonBox.Help
    preferredWidth: 720
    height: Math.max(minimumHeight, 540)

    Component.onCompleted: {
        pickerData.fromModel(picker)
        ready = true
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

    // the model follows the choice
    Binding {
        target: root.pickerData
        property: "filename"
        value: root.picker.currentName
    }
    Binding {
        when: root.ready
        target: root.picker
        property: "spriteIndex"
        value: root.pickerData.index
    }
    Binding {
        when: root.ready
        target: root.picker
        property: "direction"
        value: root.pickerData.direction
    }
    Binding {
        when: root.ready
        target: root.picker
        property: "pattern"
        value: root.pickerData.pattern
    }
    Binding {
        when: root.ready
        target: root.picker
        property: "hue"
        value: root.pickerData.hue
    }
    Binding {
        when: root.ready
        target: root.picker
        property: "fadeIn"
        value: root.pickerData.fadein
    }
    Binding {
        when: root.ready
        target: root.picker
        property: "volume"
        value: root.pickerData.volume
    }
    Binding {
        when: root.ready
        target: root.picker
        property: "tempo"
        value: root.pickerData.tempo
    }
    Binding {
        when: root.ready
        target: root.picker
        property: "balance"
        value: root.pickerData.balance
    }

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
            findRow: (text, from) => root.picker.rowStartingWith(text, from)
            Controls.SplitView.fillWidth: true
            Controls.SplitView.minimumWidth: 160
            onPicked: row => root.picker.currentRow = row
            onActivated: row => {
                if (row < 0) {
                    return
                }
                root.fileActivated()
                if (root.acceptOnActivate) {
                    root.accept()
                }
            }
        }

        Item {
            id: side
            implicitWidth: column.implicitWidth
            implicitHeight: column.implicitHeight
            Controls.SplitView.preferredWidth: Math.max(implicitWidth, 330)
            Controls.SplitView.minimumWidth: implicitWidth

            // the preview fills the height, else the controls are at the top
            ColumnLayout {
                id: column
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: previewFrame.visible ? parent.height : implicitHeight
                spacing: Ez.Metrics.sectionSpacing

                // the preview, scrolled when zoomed
                Rectangle {
                    id: previewFrame
                    objectName: "graphicsView"
                    visible: root.previewItem !== null
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
                        contentWidth: Math.max(width, previewHolder.width)
                        contentHeight: Math.max(height, previewHolder.height)
                        // shown while the picture is larger than its frame
                        Controls.ScrollBar.vertical: Controls.ScrollBar {
                            policy: Controls.ScrollBar.AlwaysOn
                        }
                        Controls.ScrollBar.horizontal: Controls.ScrollBar {
                            policy: Controls.ScrollBar.AlwaysOn
                        }

                        Item {
                            id: previewHolder
                            // centered while smaller than the frame, on whole pixels (drawn sharp)
                            x: Math.max(0, Math.round((flick.width - width) / 2))
                            y: Math.max(0, Math.round((flick.height - height) / 2))
                            width: root.previewItem ? root.previewItem.width : 0
                            height: root.previewItem ? root.previewItem.height : 0
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

                // what the file is (game or RTP, size), the zoom beside it
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
                        visible: previewFrame.visible
                        text: qsTranslate("PreviewZoom", "Zoom Out")
                        enabled: root.zoom > root.minimumZoom
                        Layout.alignment: Qt.AlignTop
                        onClicked: root.zoomOut()
                    }
                    Ez.Button {
                        objectName: "pushZoomIn"
                        visible: previewFrame.visible
                        text: qsTranslate("PreviewZoom", "Zoom In")
                        enabled: root.zoom < root.maximumZoom
                        Layout.alignment: Qt.AlignTop
                        onClicked: root.zoomIn()
                    }
                }
            }
        }
    }
}
