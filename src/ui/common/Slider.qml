// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A number on a scale (volume, tempo...); jsonData and key: as Ez.SpinBox.
 *
 * With a tickInterval it moves by steps, as the trackbars of RPG Maker: the
 * handle snaps to them, a tick mark under the groove shows each, the arrow
 * keys and a notch of the wheel go to the next one (Home and End to the
 * ends). A value between two steps (given by the data) stays as it is until
 * the slider is moved.
 */
Controls.Slider {
    id: root

    property var jsonData: null
    property string key: ""

    /** The steps from `from`, each with a tick mark; 0: no steps, no marks */
    property real tickInterval: 0
    /** The value resetToDefault() gives; undefined: none */
    property var defaultValue: undefined
    readonly property bool hasDefault: defaultValue !== undefined && defaultValue !== null
    readonly property bool isDefault: hasDefault && Math.abs(value - Number(defaultValue)) < 1e-6

    readonly property bool stepped: tickInterval > 0
    readonly property int tickCount: stepped && to > from ? Math.min(201, Math.floor((to - from) / tickInterval + 1e-6) + 1) : 0
    readonly property int tickLength: 4

    /** Moves by steps (negative: down), from the step at or past the value; as moved by the user */
    function stepBy(steps) {
        const step = stepped ? tickInterval : stepSize > 0 ? stepSize : 1
        const at = (value - from) / step
        const index = steps > 0 ? Math.floor(at + 1e-6) + steps : Math.ceil(at - 1e-6) + steps
        setByUser(from + index * step)
    }
    /** Gives the default value (defaultValue), as moved by the user */
    function resetToDefault() {
        if (hasDefault) {
            setByUser(Number(defaultValue))
        }
    }
    function setByUser(target) {
        const bounded = Math.max(Math.min(from, to), Math.min(Math.max(from, to), target))
        if (Math.abs(bounded - value) > 1e-9) {
            value = bounded
            moved()
        }
    }

    stepSize: stepped ? tickInterval : 1
    snapMode: Controls.Slider.SnapAlways
    wheelEnabled: activeFocus && !stepped
    // the tick marks under the groove
    bottomPadding: stepped ? tickLength + 2 : padding
    // greyed when disabled
    opacity: enabled ? 1 : Ez.Metrics.disabledOpacity

    Keys.onPressed: event => {
        if (!root.stepped || event.modifiers !== Qt.NoModifier) {
            return
        }
        const forward = root.mirrored ? -1 : 1
        if (event.key === Qt.Key_Left) {
            root.stepBy(-forward)
        } else if (event.key === Qt.Key_Right) {
            root.stepBy(forward)
        } else if (event.key === Qt.Key_Home) {
            root.setByUser(root.from)
        } else if (event.key === Qt.Key_End) {
            root.setByUser(root.to)
        } else {
            return
        }
        event.accepted = true
    }

    // a notch: a step (the touchpad's small moves add up to one); only
    // once the slider has the focus, as the other controls
    WheelHandler {
        property real rest: 0

        target: null
        enabled: root.stepped && root.activeFocus && root.enabled
        onWheel: event => {
            const angle = event.angleDelta
            const delta = angle.y === 0 ? angle.x : event.inverted ? -angle.y : angle.y
            rest += delta
            const steps = rest > 0 ? Math.floor(rest / 120) : Math.ceil(rest / 120)
            rest -= steps * 120
            if (steps !== 0) {
                root.stepBy(steps)
            }
            event.accepted = true
        }
    }

    Repeater {
        model: root.horizontal ? root.tickCount : 0
        delegate: Rectangle {
            required property int index
            readonly property real handleWidth: root.handle ? root.handle.width : 0
            readonly property real position: index * root.tickInterval / (root.to - root.from)
            x: Math.round(root.leftPadding + handleWidth / 2
                          + (root.mirrored ? 1 - position : position) * (root.availableWidth - handleWidth))
            y: root.height - root.tickLength
            width: 1
            height: root.tickLength
            color: Ez.Metrics.mutedTextColor
        }
    }

    Ez.JsonField {
        id: field
        source: root.jsonData
        path: root.key
    }
    Binding {
        target: root
        property: "value"
        value: Number(field.value ?? 0)
        when: field.valid
        restoreMode: Binding.RestoreNone
    }
    onMoved: {
        if (field.valid) {
            field.value = value
        }
    }
}
