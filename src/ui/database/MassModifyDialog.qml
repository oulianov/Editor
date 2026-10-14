// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "AnimationTools"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * Mass Modify of the Battle Animation page: the values checked given to
 * the chosen cels of a range of frames. Patterns go from 1 to the patterns
 * of the graphic (any without it). OK changes the animation of the canvas.
 *
 *   Ez.Editor.exec("MassModifyDialog", { canvas: preview }, item)
 */
Ez.DialogWindow {
    id: root

    /** The animation's preview (an Ez.AnimationCanvas): its frames are changed */
    property var canvas: null
    readonly property int current: canvas ? Math.max(0, canvas.currentFrame) : 0
    // RPG Maker has 8; more when frames of the animation have more cels
    readonly property int cellCount: Math.max(8, canvas ? canvas.maxCells() : 1)

    objectName: "dialogMassModify"
    title: qsTr("Mass Modify")

    onAccepting: (check) => {
        if (!canvas) {
            return
        }
        const chosen = []
        for (let i = 0; i < cellChecks.count; ++i) {
            chosen.push((cellChecks.itemAt(i) as Ez.CheckBox).checked)
        }
        const values = {}
        if (usePattern.checked) {
            values.pattern = pattern.value - 1
        }
        if (useZoom.checked) {
            values.zoom = zoom.value
        }
        if (useTransparency.checked) {
            values.transparency = transparency.value
        }
        if (useTone.checked) {
            values.tone = [red.value, green.value, blue.value, saturation.value]
        }
        canvas.massModify(frames.first, frames.last, chosen, values)
    }

    FrameRange {
        id: frames
        title: qsTr("Affected Frame")
        name: "spinFrame"
        maximum: root.canvas ? root.canvas.frameCount : 1
        from: root.current + 1
        to: root.canvas ? root.canvas.frameCount : 1
    }

    Ez.Section {
        title: qsTr("Affected Cels")
        GridLayout {
            columns: 8
            rowSpacing: Ez.Metrics.rowSpacing
            columnSpacing: Ez.Metrics.columnSpacing
            Repeater {
                id: cellChecks
                model: root.cellCount
                Ez.CheckBox {
                    required property int index
                    objectName: "checkCel" + (index + 1)
                    text: String(index + 1)
                    checked: true
                }
            }
        }
    }

    // each value is given only when checked
    Ez.Section {
        title: qsTr("Cel Properties")
        GridLayout {
            columns: 2
            rowSpacing: Ez.Metrics.rowSpacing
            columnSpacing: Ez.Metrics.columnSpacing

            Ez.CheckBox {
                id: usePattern
                objectName: "checkImageSection"
                text: qsTr("Image Section")
            }
            Ez.SpinBox {
                id: pattern
                objectName: "spinImageSection"
                from: 1
                to: Math.max(1, root.canvas ? root.canvas.patternLimit : 1)
                value: 1
                enabled: usePattern.checked
            }
            Ez.CheckBox {
                id: useZoom
                objectName: "checkMagnification"
                text: qsTr("Magnification")
            }
            Ez.SpinBox {
                id: zoom
                objectName: "spinMagnification"
                from: 20
                to: 800
                value: 100
                suffix: "%"
                enabled: useZoom.checked
            }
            Ez.CheckBox {
                id: useTransparency
                objectName: "checkTransparency"
                text: qsTr("Transparency")
            }
            Ez.SpinBox {
                id: transparency
                objectName: "spinTransparency"
                from: 0
                to: 100
                value: 0
                suffix: "%"
                enabled: useTransparency.checked
            }
            Ez.CheckBox {
                id: useTone
                objectName: "checkColorCast"
                text: qsTr("Color Cast")
            }
            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                enabled: useTone.checked
                component Tone: Ez.SpinBox {
                    property string tip
                    from: 0
                    to: 200
                    value: 100
                    suffix: "%"
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: 600
                    Controls.ToolTip.text: tip
                }
                Tone {
                    id: red
                    objectName: "spinRed"
                    tip: qsTr("Red")
                }
                Tone {
                    id: green
                    objectName: "spinGreen"
                    tip: qsTr("Green")
                }
                Tone {
                    id: blue
                    objectName: "spinBlue"
                    tip: qsTr("Blue")
                }
                Tone {
                    id: saturation
                    objectName: "spinSaturation"
                    tip: qsTr("Saturation")
                }
            }
        }
    }
}
