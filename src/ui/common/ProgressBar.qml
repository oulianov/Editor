// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The progress of a work (an export, a download): Breeze's bar, with a
 * block going to and fro while the end is not known (indeterminate). Its
 * own content: Breeze's indeterminate stripes log errors as they are laid out.
 */
Controls.ProgressBar {
    id: root

    contentItem: Item {
        // as high as Breeze's groove: a third of a line, even
        readonly property int grooveHeight: Math.floor(metrics.height / 3) + Math.floor(metrics.height / 3) % 2

        implicitWidth: 200
        implicitHeight: grooveHeight
        clip: true

        FontMetrics {
            id: metrics
            font: Ez.Metrics.font
        }

        Rectangle {
            visible: !root.indeterminate && width > 0
            width: root.position * parent.width
            height: parent.height
            radius: height / 2
            color: Kirigami.Theme.alternateBackgroundColor
            border.width: 1
            border.color: Kirigami.Theme.focusColor
        }
        Rectangle {
            id: block
            visible: root.indeterminate
            width: parent.width / 4
            height: parent.height
            radius: height / 2
            color: Kirigami.Theme.focusColor

            SequentialAnimation on x {
                running: root.indeterminate && root.visible
                loops: Animation.Infinite
                NumberAnimation {
                    from: 0
                    to: block.parent.width - block.width
                    duration: Kirigami.Units.veryLongDuration * 2
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    from: block.parent.width - block.width
                    to: 0
                    duration: Kirigami.Units.veryLongDuration * 2
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }
}
