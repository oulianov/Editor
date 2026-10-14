// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * One bit of a bit array of the project's data (the conditions an item
 * cures...): key is the array, bit the index. A bit beyond the end of the
 * array reads as missing, and checking it grows the array with missing bits.
 */
Controls.CheckBox {
    id: root

    property var jsonData: null
    property string key: ""
    property int bit: 0
    property bool missing: false

    font: Ez.Metrics.font
    verticalPadding: Ez.Metrics.optionPadding

    // disabled: greyed, its box as an enabled one's (as Ez.CheckBox)
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
        value: {
            const bits = field.value
            return bits && root.bit < bits.length ? !!bits[root.bit] : root.missing
        }
        when: field.valid
        restoreMode: Binding.RestoreNone
    }
    onToggled: {
        if (field.valid) {
            root.jsonData.setBit(root.key, root.bit, checked, root.missing)
        }
    }
}
