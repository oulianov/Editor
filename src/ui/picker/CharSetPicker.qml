// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * The picker of a CharSet: the 8 characters of the sheet, the one chosen
 * framed (preview CharSet); or every pose of the sheet, a click choosing the
 * character, its facing and its frame, also chosen below it (CharSetPose).
 */
PickerBase {
    id: root

    /** The poses of the sheet, a pose chosen (else a character) */
    readonly property bool poseMode: picker.preview === Ez.FilePickerModel.CharSetPose
    /** The option Transparent (shown in the preview) */
    property bool showTransparency: false

    zoom: poseMode ? 1 : 2

    previewContent: Ez.CharSetPaintedItem {
        id: charsetPainted
        pickerMode: true
        poseMode: root.poseMode
        path: root.picker.currentPath
        transparent: root.pickerData.transparent
        width: implicitWidth * root.zoom
        height: implicitHeight * root.zoom
        onCellIndexChanged: root.pickerData.index = charsetPainted.cellIndex
        onFacingChanged: {
            if (root.poseMode) {
                root.pickerData.direction = charsetPainted.facing
            }
        }
        onFrameChanged: {
            if (root.poseMode) {
                root.pickerData.pattern = charsetPainted.frame
            }
        }
        Binding on cellIndex {
            value: Math.max(0, Math.min(7, root.pickerData.index))
        }
        Binding on facing {
            when: root.poseMode
            value: root.pickerData.direction
        }
        Binding on frame {
            when: root.poseMode
            value: root.pickerData.pattern
        }
    }

    Ez.Section {
        visible: root.showTransparency
        title: qsTranslate("ActorWidget", "Options")
        Ez.CheckBox {
            objectName: "checkTransparent"
            text: qsTranslate("ActorWidget", "Translucent")
            checked: root.pickerData.transparent
            onToggled: root.pickerData.transparent = checked
        }
    }

    // the facing and the frame of a pose (also clicked in the sheet)
    Ez.Section {
        visible: root.poseMode
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
                    onToggled: root.pickerData.direction = index
                    Binding {
                        target: facing
                        property: "checked"
                        value: root.pickerData.direction === facing.index
                    }
                }
            }
        }
    }
    Ez.Section {
        visible: root.poseMode
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
                    onToggled: root.pickerData.pattern = index
                    Binding {
                        target: frameOption
                        property: "checked"
                        value: root.pickerData.pattern === frameOption.index
                    }
                }
            }
        }
    }
}
