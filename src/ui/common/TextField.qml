// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A one-line text field. With jsonData and key it shows a text of the
 * project's data and writes what is typed (upstream's API), following the
 * data when something else changes it.
 *
 * limit: the length RPG Maker lets type in a text the game shows in a
 * window of its own size: the text is in a fixed-width font, with a line
 * after the last character (longer text can still be typed).
 *
 * buttons: the buttons choosing its value ("...": Ez.FieldButton with
 * fused), drawn in its frame at its right, a thin line before each, as the
 * "..." of Ez.RefComboBox: one control, not buttons beside it. The first
 * one shown is the field's picker: Alt+Down presses it, and so do Space
 * and a click on the text of a read-only field (a value shown, only
 * chosen).
 *
 *   Ez.TextField {
 *       readOnly: true
 *       buttons: Ez.FieldButton { fused: true; onClicked: pick() }
 *   }
 */
Controls.TextField {
    id: root

    /** The data (a JsonView, a Json or a JsonListView) and the pointer of the text in it */
    property var jsonData: null
    property string key: ""
    /** Characters RPG Maker lets type (0: no limit shown) */
    property int limit: 0
    /** The buttons in the frame, at the right (Ez.FieldButton { fused: true }) */
    property alias buttons: buttonRow.children
    /** The width they take in the frame (0: none shown) */
    readonly property real buttonsWidth: buttonRow.visibleChildren.length > 0 ? buttonRow.width + 1 : 0
    /** The first button shown, pressed by the keys (null: none) */
    readonly property var picker: {
        const shown = buttonRow.visibleChildren
        return shown.length > 0 ? shown[0] : null
    }

    /** Presses the field's picker, as a click on it does */
    function pick() {
        if (picker && picker.enabled) {
            picker.clicked()
        }
    }

    selectByMouse: true

    // disabled: greyed (Ez.Metrics.disabledOpacity), on the background of an
    // enabled field (Breeze darkens it)
    opacity: enabled ? 1 : Ez.Metrics.disabledOpacity
    Binding {
        target: root.background
        property: "color"
        value: Ez.Metrics.palette.base
        when: !root.enabled && root.background !== null
    }
    font: limit > 0 ? Ez.Metrics.monospaceFont : Ez.Metrics.font
    topPadding: Ez.Metrics.fieldPadding
    bottomPadding: Ez.Metrics.fieldPadding
    // as wide as the other fields, whatever its text: a long path does not
    // widen the window (nor Breeze's 200); its buttons besides
    implicitWidth: Ez.Metrics.fieldWidth + buttonsWidth
    // as high as the other fields in the fixed-width font too
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             Math.ceil(Math.max(contentHeight, formMetrics.height)) + topPadding + bottomPadding)

    // the text stops before the buttons
    Binding {
        target: root
        property: "rightPadding"
        value: root.leftPadding + root.buttonsWidth
        when: root.buttonsWidth > 0
    }
    Row {
        id: buttonRow
        anchors.right: parent.right
        anchors.rightMargin: 1
        y: 1
        height: root.height - 2
    }
    // Alt+Down (the key of a drop-down list) presses the picker; Space too in a
    // read-only field. Not F4: a text field does not take it from the window
    // (the previous entry of a database page)
    Keys.onPressed: (event) => {
        if (!picker || event.accepted) {
            return
        }
        const modifiers = event.modifiers & (Qt.ControlModifier | Qt.MetaModifier | Qt.AltModifier | Qt.ShiftModifier)
        if (event.key === Qt.Key_Down && modifiers === Qt.AltModifier || readOnly && event.key === Qt.Key_Space && modifiers === 0) {
            event.accepted = true
            root.pick()
        }
    }
    // a value only chosen: a click on it chooses it
    TapHandler {
        enabled: root.readOnly && root.picker !== null
        onTapped: (eventPoint) => {
            if (eventPoint.position.x < buttonRow.x) {
                root.pick()
            }
        }
    }

    Ez.JsonField {
        id: field
        source: root.jsonData
        path: root.key
    }
    Binding {
        target: root
        property: "text"
        value: field.value ?? ""
        when: field.valid
        restoreMode: Binding.RestoreNone
    }
    onTextEdited: {
        if (field.valid) {
            field.value = text
        }
    }

    FontMetrics {
        id: formMetrics
        font: Ez.Metrics.font
    }
    // the limit: after its last character, scrolled with the text
    FontMetrics {
        id: limitMetrics
        font: root.font
    }
    Rectangle {
        // read again once the text is laid out (contentWidth): before, the positions are 0
        readonly property real end: (root.contentWidth >= 0 ? root.positionToRectangle(root.length).x : 0)
            + Math.max(0, root.limit - root.length) * limitMetrics.averageCharacterWidth
        visible: root.limit > 0 && root.limit >= root.length && end > root.leftPadding && end < root.width - root.rightPadding
        x: Math.round(end)
        y: root.topPadding
        width: 1
        height: root.height - root.topPadding - root.bottomPadding
        color: root.placeholderTextColor
    }
}
