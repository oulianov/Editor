// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "AnimationTools"

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Autocomplete of the Battle Animation page: the cels of a range present
 * in the first and last frames of a range are placed in the frames
 * between, moving in a straight line; the pattern, zoom and transparency
 * change too when checked. OK changes the animation of the canvas.
 *
 *   Ez.Editor.exec("AutocompleteDialog", { canvas: preview }, item)
 */
Ez.DialogWindow {
    id: root

    /** The animation's preview (an Ez.AnimationCanvas): its frames are changed */
    property var canvas: null
    readonly property int current: canvas ? Math.max(0, canvas.currentFrame) : 0
    readonly property int cells: canvas ? canvas.maxCells() : 1

    objectName: "dialogAutocomplete"
    title: qsTr("Autocomplete")

    onAccepting: (check) => {
        if (canvas) {
            canvas.autocomplete(frames.first, frames.last, cels.first, cels.last,
                                pattern.checked, zoom.checked, transparency.checked)
        }
    }

    FrameRange {
        id: frames
        title: qsTr("Frame Range")
        name: "spinFrame"
        maximum: root.canvas ? root.canvas.frameCount : 1
        from: root.current + 1
        to: root.canvas ? root.canvas.frameCount : 1
    }
    FrameRange {
        id: cels
        title: qsTr("Cel Range")
        name: "spinCel"
        maximum: root.cells
        from: 1
        to: root.cells
    }
    Ez.Section {
        id: carry
        title: qsTr("Carry Through")
        Ez.CheckBox {
            id: pattern
            objectName: "checkImageSection"
            text: qsTr("Image Section")
            checked: true
        }
        Ez.CheckBox {
            id: zoom
            objectName: "checkMagnification"
            text: qsTr("Magnification")
            checked: true
        }
        Ez.CheckBox {
            id: transparency
            objectName: "checkTransparency"
            text: qsTr("Transparency")
            checked: true
        }
        Controls.ToolTip.visible: carryHover.hovered
        Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
        Controls.ToolTip.text: qsTr("Besides the position, what changes from the first frame to the last")
        HoverHandler {
            id: carryHover
        }
    }
}
