// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A number asked in a dialog drawn over the window, as RPG Maker's Array
 * Size and Multiple Copy: a section with its spin box (selected, Return
 * accepts), then OK and Cancel. chosen(value) once accepted.
 */
Ez.Dialog {
    id: root

    /** The title of the section around the number */
    property alias label: section.title
    property alias value: spin.value
    property alias from: spin.from
    property alias to: spin.to
    /** The objectName of the spin box (scripts) */
    property string spinName

    signal chosen(int value)

    anchors.centerIn: Controls.Overlay.overlay
    standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
    font: Ez.Metrics.font
    padding: Ez.Metrics.dialogMargin

    onOpened: {
        spin.forceActiveFocus()
        // the number typed replaces it
        const field = spin.contentItem as TextInput
        if (field) {
            field.selectAll()
        }
    }
    onAccepted: chosen(spin.value)

    contentItem: Ez.Section {
        id: section

        Ez.SpinBox {
            id: spin
            objectName: root.spinName
            from: 1
            Layout.fillWidth: true
            // the dialog as wide as its buttons need
            Layout.preferredWidth: 190
        }
    }
}
