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
 */
Controls.TextField {
    id: root

    /** The data (a JsonView, a Json or a JsonListView) and the pointer of the text in it */
    property var jsonData: null
    property string key: ""
    /** Characters RPG Maker lets type (0: no limit shown) */
    property int limit: 0

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
    // widen the window (nor Breeze's 200)
    implicitWidth: Ez.Metrics.fieldWidth
    // as high as the other fields in the fixed-width font too
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             Math.ceil(Math.max(contentHeight, formMetrics.height)) + topPadding + bottomPadding)

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
