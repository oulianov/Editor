// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A number. With jsonData and key it shows a number of the project's data
 * and writes the one chosen. It takes the mouse wheel only once clicked: a
 * swipe over a form scrolls the form.
 *
 * Its buttons are scrubbed, as RPG Maker 2003's and Photoshop's numbers: a
 * click steps (held: again and again), a press dragged right or up raises
 * the number, left or down lowers it (the axis moved most), a step every
 * scrubPixels, faster the further from where the drag started (twice as
 * fast every scrubDoubling pixels: the whole range of 999999 is reached).
 * Shift: 10 steps at a time. Ctrl (Cmd on macOS) and a drag on the number
 * scrub it too; without it the drag selects the text. Escape during the
 * drag puts back the number it started with. Each number goes the way of
 * a typed one (valueModified, the data), within from and to.
 */
Controls.SpinBox {
    id: root

    property var jsonData: null
    property string key: ""
    property string prefix: ""
    property string suffix: ""

    /** Pixels dragged for a step, near where the drag started */
    property real scrubPixels: 4
    /** Pixels dragged further for steps twice as large */
    property real scrubDoubling: 40
    /** Whether the number is being scrubbed (a button or the number dragged) */
    readonly property bool scrubbing: buttonScrub.active || textScrub.active

    editable: true
    from: -999999
    to: 999999
    font: Ez.Metrics.font
    wheelEnabled: activeFocus

    // disabled: greyed (Ez.Metrics.disabledOpacity), on the background of an
    // enabled field (Breeze darkens it)
    opacity: enabled ? 1 : Ez.Metrics.disabledOpacity
    Binding {
        target: root.background
        property: "color"
        value: Ez.Metrics.palette.base
        when: !root.enabled && root.background !== null
    }
    // as wide as the longest number of its range, whatever it shows: a
    // column of them lines up and a new value moves nothing
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            Math.ceil(widest.advanceWidth) + 2 * spacing + 2 + leftPadding + rightPadding)

    TextMetrics {
        id: widest
        font: root.font
        text: {
            const first = root.textFromValue(root.from, root.locale)
            const last = root.textFromValue(root.to, root.locale)
            return first.length > last.length ? first : last
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
    onValueModified: {
        if (field.valid) {
            field.value = value
        }
    }

    // the digits without group separators, as RPG Maker
    textFromValue: function(value, locale) {
        return prefix + Number(value).toLocaleString(locale, 'f', 0).split(locale.groupSeparator).join("") + suffix
    }
    // a new prefix or suffix: the text shown again (the spin box writes it only for a new
    // value), the value of the data put back by its Binding
    function refreshText() {
        if (field.valid && from < to) {
            const shown = value
            value = shown < to ? shown + 1 : shown - 1
            value = shown
        }
    }
    onPrefixChanged: refreshText()
    onSuffixChanged: refreshText()
    valueFromText: function(text, locale) {
        if (prefix !== "" && text.startsWith(prefix)) {
            text = text.substring(prefix.length)
        }
        if (suffix !== "" && text.endsWith(suffix)) {
            text = text.substring(0, text.length - suffix.length)
        }
        return Number.fromLocaleString(locale, text)
    }

    // the scrubbing: where the drag started, the number then, the steps
    // done (fractions of a step too, within from and to)
    QtObject {
        id: scrub

        property point origin
        property int start: 0
        property real steps: 0
        property real distance: 0
        property bool cancelled: false

        // the steps of a drag of distance pixels from the origin
        function curve(distance) {
            const doubling = Math.max(1, root.scrubDoubling)
            const size = Math.abs(distance)
            return Math.sign(distance) * doubling / Math.LN2 / Math.max(0.1, root.scrubPixels)
                * (Math.pow(2, size / doubling) - 1)
        }

        function begin(point) {
            origin = point.scenePosition
            start = root.value
            steps = 0
            distance = 0
            cancelled = false
            // the keys (Escape) come to it
            if (!root.activeFocus) {
                root.forceActiveFocus(Qt.MouseFocusReason)
            }
        }

        function move(point) {
            if (cancelled) {
                return
            }
            const right = point.scenePosition.x - origin.x
            const up = origin.y - point.scenePosition.y
            const now = Math.abs(right) >= Math.abs(up) ? right : up
            const factor = (point.modifiers & Qt.ShiftModifier) ? 10 : 1
            const step = Math.max(1, Math.abs(root.stepSize))
            const low = Math.min(root.from, root.to)
            const high = Math.max(root.from, root.to)
            steps += (curve(now) - curve(distance)) * factor
            distance = now
            // no further than from and to: back the other way at once
            steps = Math.max((low - start) / step, Math.min((high - start) / step, steps))
            set(start + Math.trunc(steps) * step)
        }

        function cancel() {
            cancelled = true
            set(start)
        }

        // as a typed number
        function set(number) {
            const low = Math.min(root.from, root.to)
            const high = Math.max(root.from, root.to)
            number = Math.max(low, Math.min(high, number))
            if (number !== root.value) {
                root.value = number
                root.valueModified()
            }
        }
    }

    // Return and Enter take the number typed, then go on to the window (its
    // OK sees the number): Qt's spin box takes it only once they are released
    Keys.onReturnPressed: (event) => root.takeTyped(event)
    Keys.onEnterPressed: (event) => root.takeTyped(event)
    function takeTyped(event) {
        event.accepted = false
        const input = contentItem as TextInput
        if (!editable || !input) {
            return
        }
        const typed = valueFromText(input.text, locale)
        if (!isNaN(typed)) {
            scrub.set(Math.trunc(typed))
        }
    }

    // Escape before the shortcuts of the window (its Cancel)
    Keys.onShortcutOverride: (event) => {
        event.accepted = root.scrubbing && !scrub.cancelled && event.key === Qt.Key_Escape
    }
    Keys.onEscapePressed: (event) => {
        if (root.scrubbing && !scrub.cancelled) {
            scrub.cancel()
        } else {
            event.accepted = false
        }
    }

    // A press on a button dragged scrubs: the handler takes the press from
    // the spin box, which makes no step; a click steps as before. It is on
    // the spin box itself: in an item over the buttons it would keep the
    // press from it.
    DragHandler {
        id: buttonScrub
        target: null
        cursorShape: Qt.SizeHorCursor
        // pressed on a button; anywhere else (the number): never dragged far
        // enough (32767: the largest threshold)
        property bool onButton: false
        dragThreshold: onButton ? Application.styleHints.startDragDistance : 32767
        onGrabChanged: (transition, point) => {
            if (transition === PointerDevice.GrabPassive) {
                const at = root.mapFromItem(null, point.scenePosition.x, point.scenePosition.y)
                onButton = root.over(root.down.indicator, at) || root.over(root.up.indicator, at)
            }
        }
        onActiveChanged: if (active) scrub.begin(centroid)
        onCentroidChanged: if (active) scrub.move(centroid)
    }
    // Over the number: with Ctrl (Cmd on macOS), as Photoshop; once started,
    // Shift can be added and Ctrl let go. Without Ctrl it does not take the
    // press: the text is selected.
    Item {
        z: 1
        x: root.contentItem ? root.contentItem.x : 0
        y: root.contentItem ? root.contentItem.y : 0
        width: root.contentItem ? root.contentItem.width : 0
        height: root.contentItem ? root.contentItem.height : 0
        DragHandler {
            id: textScrub
            target: null
            acceptedModifiers: active ? Qt.KeyboardModifierMask : Qt.ControlModifier
            cursorShape: Qt.SizeHorCursor
            onActiveChanged: if (active) scrub.begin(centroid)
            onCentroidChanged: if (active) scrub.move(centroid)
        }
    }
    function over(item, point) {
        return item !== null && item.visible && point.x >= item.x && point.x < item.x + item.width
            && point.y >= item.y && point.y < item.y + item.height
    }
    // the buttons show they can be dragged
    HoverHandler {
        id: hover
        cursorShape: {
            const at = hover.point.position
            return root.over(root.down.indicator, at) || root.over(root.up.indicator, at) ? Qt.SizeHorCursor
                : root.over(root.contentItem, at) && root.editable ? Qt.IBeamCursor : Qt.ArrowCursor
        }
    }
}
