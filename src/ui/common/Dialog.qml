// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A dialog drawn over a window (Array Size, a warning...), modal: its
 * buttons (standardButtons) are those of the windows, Ez.ButtonBox, in line
 * with its content. Return and Enter press OK, as in the windows.
 */
Controls.Dialog {
    id: root

    modal: true
    font: Ez.Metrics.font

    footer: Ez.ButtonBox {
        id: box
        leftPadding: root.leftPadding
        rightPadding: root.rightPadding
        bottomPadding: root.bottomPadding
    }

    // Return and Enter that the control in focus leaves (a field once it
    // took its text) press OK (Ez.ButtonBox.pressDefault): they come up from
    // the control to the dialog's own item (the parent of its content and
    // buttons), which takes every key. Closing (a field that accepts it on
    // Return), the dialog no longer has the focus: nothing more. Held apart
    // from the content, which the dialogs give their own.
    readonly property Connections returnKeys: Connections {
        target: box.parent ? box.parent.Keys : null
        function onReturnPressed(event) {
            root.pressDefault(event)
        }
        function onEnterPressed(event) {
            root.pressDefault(event)
        }
    }
    function pressDefault(event) {
        if (!root.activeFocus) {
            event.accepted = false
            return
        }
        box.pressDefault(event)
    }
}
