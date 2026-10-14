// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/** A text of several lines; jsonData and key: as Ez.TextField */
Controls.TextArea {
    id: root

    property var jsonData: null
    property string key: ""

    selectByMouse: true
    wrapMode: TextEdit.Wrap
    font: Ez.Metrics.font

    // disabled: greyed (Ez.Metrics.disabledOpacity), on the background of an
    // enabled field (Breeze darkens it)
    opacity: enabled ? 1 : Ez.Metrics.disabledOpacity
    Binding {
        target: root.background
        property: "color"
        value: Ez.Metrics.palette.base
        when: !root.enabled && root.background !== null
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
    onTextChanged: {
        if (field.valid && activeFocus && field.value !== text) {
            field.value = text
        }
    }
}
