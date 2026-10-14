// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/** One of several choices of a number of the project's data: checked when it is value */
Controls.RadioButton {
    id: root

    property var jsonData: null
    property string key: ""
    property int value: 0

    font: Ez.Metrics.font
    verticalPadding: Ez.Metrics.optionPadding
    // never narrower than its text in a layout (sections sharing a row by
    // their preferred widths cut it: the French Map Properties)
    Layout.minimumWidth: implicitWidth

    // disabled: greyed (Ez.Metrics.disabledOpacity), its box as an enabled
    // one's (Breeze darkens it: it looked filled)
    opacity: enabled ? 1 : Ez.Metrics.disabledOpacity
    Binding {
        target: root.indicator
        property: "color"
        value: Ez.Metrics.palette.button
        when: !root.enabled && root.indicator !== null
    }

    Ez.JsonField {
        id: field
        source: root.jsonData
        path: root.key
    }
    Binding {
        target: root
        property: "checked"
        value: Number(field.value) === root.value
        when: field.valid
        restoreMode: Binding.RestoreNone
    }
    onToggled: {
        if (checked && field.valid) {
            field.value = root.value
        }
    }
}
