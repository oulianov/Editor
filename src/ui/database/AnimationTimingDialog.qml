// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "BattleAnimationWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * An effect timing of a battle animation: on a frame, a sound effect, a
 * flash (of the target or the screen, its color over black at its
 * strength) and a shake (RPG Maker 2003). OK puts it in the animation of
 * the canvas (index -1: a new one).
 *
 *   Ez.Editor.exec("AnimationTimingDialog", { canvas: preview, index: 0, rpg2k3: true }, item)
 */
Ez.DialogWindow {
    id: root

    /** The animation's preview (an Ez.AnimationCanvas) */
    property var canvas: null
    /** The timing edited, -1 for a new one */
    property int index: -1
    property bool rpg2k3: true
    /** The timing as the canvas gives it (see AnimationCanvas.timing) */
    property var timing: canvas ? (index >= 0 ? canvas.timing(index) : canvas.newTiming()) : ({})
    /** The sound effect chosen */
    property var se: timing.se ?? ({ name: "", volume: 100, tempo: 100, balance: 50 })

    objectName: "AnimationTimingDialog"
    title: qsTr("Animation Effect Timing")

    onAccepting: (check) => {
        if (!canvas) {
            return
        }
        canvas.putTiming({
            frame: frame.value,
            se: root.se,
            flash_scope: flashScope.value,
            flash_red: red.value,
            flash_green: green.value,
            flash_blue: blue.value,
            flash_power: power.value,
            screen_shake: shake.value,
        }, index)
    }

    Ez.Form {
        Ez.FormLabel {
            text: qsTr("Frame")
        }
        Ez.SpinBox {
            id: frame
            objectName: "spinTimingFrame"
            from: 1
            to: Math.max(1, root.canvas ? root.canvas.frameCount : 1)
            value: root.timing.frame ?? 1
        }
        Ez.FormLabel {
            text: qsTr("Sound Effect")
        }
        RowLayout {
            spacing: Ez.Metrics.columnSpacing
            Layout.fillWidth: true
            Ez.TextField {
                objectName: "lineSound"
                readOnly: true
                text: root.se.name ?? ""
                Layout.fillWidth: true
                Layout.minimumWidth: 140
            }
            Ez.FieldButton {
                objectName: "lineSoundPicker"
                onClicked: {
                    const choice = Ez.Pickers.pickFile({ folder: "Sound", preview: "Sound", name: root.se.name ?? "",
                                                         volume: root.se.volume, tempo: root.se.tempo,
                                                         balance: root.se.balance }, this)
                    if (choice) {
                        root.se = { name: choice.name, volume: choice.volume, tempo: choice.tempo, balance: choice.balance }
                    }
                }
            }
        }
    }

    Ez.Section {
        title: qsTr("Flash")

        Ez.Form {
            Ez.FormLabel {
                text: qsTr("Flash Scope")
            }
            RowLayout {
                id: flashScope
                property int value: root.timing.flash_scope ?? 0
                spacing: 2 * Ez.Metrics.columnSpacing
                Repeater {
                    model: [qsTr("None"), qsTr("Target"), qsTr("Screen")]
                    Ez.RadioButton {
                        required property string modelData
                        required property int index
                        objectName: "radioFlash" + index
                        text: modelData
                        checked: flashScope.value === index
                        onToggled: if (checked) flashScope.value = index
                    }
                }
            }
        }
        // the flash color over black, at its strength
        Ez.Form {
            enabled: flashScope.value !== 0
            component Channel: Ez.SpinBox {
                from: 0
                to: 31
            }
            Ez.FormLabel {
                text: qsTr("Red")
            }
            Channel {
                id: red
                objectName: "spinFlashRed"
                value: root.timing.flash_red ?? 31
            }
            Ez.FormLabel {
                text: qsTr("Green")
            }
            Channel {
                id: green
                objectName: "spinFlashGreen"
                value: root.timing.flash_green ?? 31
            }
            Ez.FormLabel {
                text: qsTr("Blue")
            }
            Channel {
                id: blue
                objectName: "spinFlashBlue"
                value: root.timing.flash_blue ?? 31
            }
            Ez.FormLabel {
                text: qsTr("Strength")
            }
            Channel {
                id: power
                objectName: "spinFlashPower"
                value: root.timing.flash_power ?? 31
            }
            Ez.FormLabel {
                text: qsTr("Sample")
            }
            Rectangle {
                objectName: "labelFlashSample"
                readonly property real strength: power.value / 31
                color: Qt.rgba(red.value / 31 * strength, green.value / 31 * strength, blue.value / 31 * strength, 1)
                border.color: Ez.Metrics.sectionBorderColor
                implicitHeight: 28
                Layout.fillWidth: true
            }
        }
    }

    // shaking is new in RPG Maker 2003
    Ez.Section {
        title: qsTr("Shake Scope")
        enabled: root.rpg2k3
        RowLayout {
            id: shake
            property int value: root.timing.screen_shake ?? 0
            spacing: 2 * Ez.Metrics.columnSpacing
            Repeater {
                model: [qsTr("None"), qsTr("Target"), qsTr("Screen")]
                Ez.RadioButton {
                    required property string modelData
                    required property int index
                    objectName: "radioShake" + index
                    text: modelData
                    checked: shake.value === index
                    onToggled: if (checked) shake.value = index
                }
            }
        }
    }
}
