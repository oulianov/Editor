// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * One of several choices of a number of the project's data: checked when it
 * is value. As the radio buttons of RPG Maker 2003 (Windows' groups): Tab
 * stops on the one checked of its group (the first one when none is), the
 * arrow keys check the one before or after it in the group.
 */
Controls.RadioButton {
    id: root

    /** The radio buttons of its group: its ButtonGroup's, else those beside it */
    readonly property var groupButtons: {
        const group = Controls.ButtonGroup.group
        if (group) {
            return group.buttons
        }
        if (!autoExclusive || !parent) {
            return [root]
        }
        return Array.from(parent.children).filter(item => item instanceof Controls.RadioButton && item.autoExclusive)
    }
    /** The buttons of the group that can be checked now */
    function usable(): var {
        return groupButtons.filter(button => button.enabled && button.visible)
    }
    /** Checks the button delta places after it in its group (before: negative) */
    function moveInGroup(delta: int) {
        const buttons = usable()
        const at = buttons.indexOf(root)
        const next = at < 0 ? -1 : at + delta
        if (next >= 0 && next < buttons.length) {
            buttons[next].forceActiveFocus(Qt.TabFocusReason)
            if (!buttons[next].checked) {
                // as a click: the data is written
                buttons[next].click()
            }
        }
    }

    activeFocusOnTab: {
        const buttons = groupButtons
        const checked = buttons.find(button => button.checked && button.enabled && button.visible)
        if (checked) {
            return checked === root
        }
        const first = buttons.find(button => button.enabled && button.visible)
        return first === root || first === undefined
    }
    Keys.onUpPressed: moveInGroup(-1)
    Keys.onLeftPressed: moveInGroup(-1)
    Keys.onDownPressed: moveInGroup(1)
    Keys.onRightPressed: moveInGroup(1)

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
