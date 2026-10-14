// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A range of frames or cels of the animation tools ("first - last", from
 * 1): its spin boxes are named name + "From" and name + "To". first and
 * last are zero-based and in order, whatever way the range was entered.
 */
Ez.Section {
    id: root

    property string name
    property int maximum: 1
    property alias from: fromSpin.value
    property alias to: toSpin.value
    readonly property int first: Math.min(fromSpin.value, toSpin.value) - 1
    readonly property int last: Math.max(fromSpin.value, toSpin.value) - 1

    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true
        Ez.SpinBox {
            id: fromSpin
            objectName: root.name + "From"
            from: 1
            to: root.maximum
            Layout.fillWidth: true
        }
        Ez.Label {
            text: "-"
            font: Ez.Metrics.font
        }
        Ez.SpinBox {
            id: toSpin
            objectName: root.name + "To"
            from: 1
            to: root.maximum
            Layout.fillWidth: true
        }
    }
}
