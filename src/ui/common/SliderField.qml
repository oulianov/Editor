// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A setting on a scale (the volume, tempo, balance, fade-in of a sound): an
 * Ez.Slider by steps, its value written beside it, a button giving its
 * default value back, and the texts of its scale under it (optional).
 *
 * The value is given by the data (value) and kept as it is, even between two
 * steps, until the user moves the slider: moved() then gives the new one.
 * The value and the button are as wide in every field: fields one under the
 * other keep their sliders aligned.
 */
GridLayout {
    id: root

    /** The end of the names of the controls: "slider" + name, "label" + name, "reset" + name */
    property string name
    property real from: 0
    property real to: 100
    /** The steps (and tick marks) of the slider */
    property real tickInterval: 10
    /** The value of the data */
    property real value: 0
    /** The value the reset button gives; undefined: no button */
    property var defaultValue: undefined
    /** The value as written beside the slider */
    property var valueText: v => String(v)
    /** The texts under the slider, from its start to its end (RPG Maker's scales) */
    property var marks: []
    /** The width of the value written: the widest of its ends and its default, at least 40 */
    property real valueWidth: Math.max(40, valueMetrics.advanceWidth(valueText(from)),
                                       valueMetrics.advanceWidth(valueText(to)),
                                       slider.hasDefault ? valueMetrics.advanceWidth(valueText(Number(defaultValue))) : 0)
    property string toolTip
    /** The narrowest the slider gets (it takes the width it is given) */
    property real sliderWidth: 120

    readonly property alias slider: slider

    /** The user moved the slider (or reset it) to this value */
    signal moved(real value)

    columns: 3
    Layout.fillWidth: true
    rowSpacing: 0
    columnSpacing: Ez.Metrics.columnSpacing

    Ez.Slider {
        id: slider
        objectName: "slider" + root.name
        from: root.from
        to: root.to
        tickInterval: root.tickInterval
        defaultValue: root.defaultValue
        Layout.fillWidth: true
        Layout.minimumWidth: root.sliderWidth
        Layout.preferredWidth: root.sliderWidth
        Controls.ToolTip.visible: hovered && root.toolTip !== ""
        Controls.ToolTip.text: root.toolTip
        onMoved: root.moved(value)
        Binding on value {
            value: root.value
            restoreMode: Binding.RestoreNone
        }
    }
    Ez.Label {
        objectName: "label" + root.name
        text: root.valueText(slider.value)
        font: Ez.Metrics.font
        horizontalAlignment: Text.AlignRight
        // the slider does not move with the value
        Layout.preferredWidth: root.valueWidth
        Layout.alignment: Qt.AlignVCenter

        FontMetrics {
            id: valueMetrics
            font: Ez.Metrics.font
        }
    }
    Ez.Button {
        objectName: "reset" + root.name
        visible: slider.hasDefault
        text: qsTr("Reset")
        icon.name: "edit-reset"
        display: Controls.AbstractButton.IconOnly
        enabled: !slider.isDefault && slider.enabled
        // square, as high as the fields: the icon, padded alike on all sides
        horizontalPadding: verticalPadding
        implicitWidth: Math.max(implicitHeight, implicitContentWidth + leftPadding + rightPadding)
        Layout.alignment: Qt.AlignVCenter
        Controls.ToolTip.visible: hovered
        Controls.ToolTip.text: qsTr("Reset to %1").arg(root.valueText(Number(root.defaultValue)))
        onClicked: slider.resetToDefault()
    }

    // the scale under the slider
    RowLayout {
        visible: root.marks.length > 0
        spacing: 0
        Layout.fillWidth: true

        Repeater {
            model: root.marks
            delegate: Ez.Label {
                required property string modelData
                required property int index
                text: modelData
                font: Ez.Metrics.font
                horizontalAlignment: index === 0 ? Text.AlignLeft
                                   : index === root.marks.length - 1 ? Text.AlignRight : Text.AlignHCenter
                Layout.fillWidth: true
                Layout.preferredWidth: 1
            }
        }
    }
}
