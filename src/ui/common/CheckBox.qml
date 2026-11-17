// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/** An option. With jsonData and key it shows a bool (or a number: not 0) of the project's data */
Controls.CheckBox {
    id: root

    property var jsonData: null
    property string key: ""

    font: Ez.Metrics.font
    verticalPadding: Ez.Metrics.optionPadding

    // disabled: greyed (Ez.Metrics.disabledOpacity), its box as an enabled
    // one's (Breeze darkens it: it looked filled)
    opacity: enabled ? 1 : Ez.Metrics.disabledOpacity
    Binding {
        target: root.indicator
        property: "color"
        value: Ez.Metrics.palette.button
        when: !root.enabled && root.indicator !== null
    }
    // the room of its box counted while it is hidden too (Breeze counts it
    // only while the box shows: the window of a page of tabs not shown
    // opened narrower than that page)
    Binding {
        target: root.contentItem
        property: "leftPadding"
        value: (root.contentItem?.horizontalPadding ?? 0) + (root.indicator?.width ?? 0) + (root.contentItem?.spacing ?? 0)
        when: !root.visible && !root.mirrored && root.contentItem !== null && root.indicator !== null && root.indicator.width > 0
    }

    Ez.JsonField {
        id: field
        source: root.jsonData
        path: root.key
    }
    Binding {
        target: root
        property: "checked"
        value: !!field.value
        when: field.valid
        restoreMode: Binding.RestoreNone
    }
    onToggled: {
        if (field.valid) {
            field.value = checked
        }
    }
}
