// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A choice in a list. With jsonData and key it shows the entry whose value
 * is a number of the project's data (a JsonListView: its "value" is the ID;
 * an unknown one shows the fallback entry, "(None)"), and writes the one
 * chosen. It takes the mouse wheel only once clicked.
 */
Controls.ComboBox {
    id: root

    property var jsonData: null
    property string key: ""
    /**
     * The value of the entry shown for a value missing from a list of
     * JavaScript entries ("(None)" for an item the hero cannot equip);
     * undefined: none shown. Nothing is written. A JsonListView has its own
     * (fallbackString, fallbackValue).
     */
    property var fallbackValue: undefined
    /** The role telling whether an entry can be chosen (empty: they all can), as textRole */
    property string enabledRole: ""

    /** Whether the entry at index can be chosen */
    function optionEnabled(index) {
        if (enabledRole === "" || index < 0 || index >= count) {
            return index >= 0 && index < count
        }
        // a list of entries (a JavaScript array, or a list C++ gives)
        const entries = model
        const entry = entries && entries.length !== undefined ? entries[index] : undefined
        return !entry || entry[enabledRole] !== false
    }

    // in a JsonListView "0001: Alex" and the ID
    textRole: "text"
    valueRole: "value"
    font: Ez.Metrics.font
    wheelEnabled: activeFocus
    // as wide as its text (Ez.Metrics.comboWidth at least), not Breeze's 200
    implicitWidth: Math.max(Ez.Metrics.comboWidth, Math.ceil(implicitContentWidth) + leftPadding + rightPadding)

    // disabled: greyed (Ez.Metrics.disabledOpacity), on the face of an
    // enabled one (Breeze darkens it: it looked pressed)
    opacity: enabled ? 1 : Ez.Metrics.disabledOpacity
    Binding {
        target: root.background
        property: "color"
        value: root.editable ? Ez.Metrics.palette.base : Ez.Metrics.palette.button
        when: !root.enabled && !root.flat && root.background !== null
    }

    // an entry that cannot be chosen is greyed (Breeze's delegate otherwise)
    delegate: Controls.MenuItem {
        required property var model
        required property int index

        implicitWidth: leftPadding + implicitContentWidth + rightPadding
        width: parent ? parent.width : implicitWidth
        // a list of texts has no role: textRole ""
        text: root.textRole !== "" ? model[root.textRole] : model.modelData
        font: root.font
        highlighted: root.highlightedIndex === index
        hoverEnabled: root.hoverEnabled
        enabled: root.enabledRole === "" || model[root.enabledRole] !== false
    }

    // the text inside: as high as a field's
    Binding {
        target: root.contentItem
        property: "topPadding"
        value: Ez.Metrics.fieldPadding
        when: root.contentItem instanceof TextInput
    }
    Binding {
        target: root.contentItem
        property: "bottomPadding"
        value: Ez.Metrics.fieldPadding
        when: root.contentItem instanceof TextInput
    }

    Ez.JsonField {
        id: field
        source: root.jsonData
        path: root.key
    }
    Binding {
        target: root
        property: "currentIndex"
        value: {
            // again once the entries come (or change, even as many): indexOfValue does not say
            if (root.count === 0 || !root.model) {
                return -1
            }
            const index = root.indexOfValue(field.value)
            if (index >= 0 || !root.model) {
                return index
            }
            const list = root.model.sourceModel ?? root.model
            if (list && list.fallbackString) {
                return root.indexOfValue(list.fallbackValue)
            }
            return root.fallbackValue !== undefined ? root.indexOfValue(root.fallbackValue) : -1
        }
        when: field.valid
        restoreMode: Binding.RestoreNone
    }
    onActivated: {
        if (field.valid) {
            field.value = currentValue
        }
    }

    // Return and Enter on the closed box go on to the items around it (the
    // window's OK), as a desktop combo box leaves them: Qt's takes them even
    // closed. Open, they choose the entry marked.
    Keys.onReturnPressed: (event) => root.passReturn(event)
    Keys.onEnterPressed: (event) => root.passReturn(event)
    function passReturn(event) {
        if (popup && popup.visible) {
            event.accepted = false
            return
        }
        Ez.Editor.passKey(root, event)
        event.accepted = true
    }
}
