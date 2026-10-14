// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "AnimationTools"

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Mass Copy/Clear of the Battle Animation page: the cels of a range of
 * frames copied to the frames from a starting one (frames are added past
 * the end), or removed. OK changes the animation of the canvas.
 *
 *   Ez.Editor.exec("MassCopyClearDialog", { canvas: preview }, item)
 */
Ez.DialogWindow {
    id: root

    /** The animation's preview (an Ez.AnimationCanvas): its frames are changed */
    property var canvas: null
    readonly property int current: canvas ? Math.max(0, canvas.currentFrame) : 0

    objectName: "dialogMassCopyClear"
    title: qsTr("Mass Copy/Clear")

    onAccepting: (check) => {
        if (!canvas) {
            return
        }
        if (radioClear.checked) {
            canvas.massClear(range.first, range.last)
        } else {
            canvas.massCopy(range.first, range.last, start.value - 1)
        }
    }

    FrameRange {
        id: range
        title: qsTr("Frame Range")
        name: "spinFrame"
        maximum: root.canvas ? root.canvas.frameCount : 1
        from: root.current + 1
        to: root.current + 1
    }

    Ez.Section {
        title: qsTr("Action")
        Controls.ButtonGroup {
            id: action
            objectName: "groupAction"
        }
        Ez.RadioButton {
            id: radioCopy
            objectName: "radioCopy"
            text: qsTr("Copy")
            checked: true
            Controls.ButtonGroup.group: action
        }
        Ez.Form {
            Ez.FormLabel {
                text: qsTr("Starting Frame")
            }
            // the copy may go past the last frame: frames are added
            Ez.SpinBox {
                id: start
                objectName: "spinStartFrame"
                from: 1
                to: 999
                value: Math.min(root.current + 2, 999)
                enabled: radioCopy.checked
            }
        }
        Ez.RadioButton {
            id: radioClear
            objectName: "radioClear"
            text: qsTr("Clear")
            Controls.ButtonGroup.group: action
        }
    }
}
