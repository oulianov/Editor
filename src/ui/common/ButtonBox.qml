// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The buttons of a window (OK, Cancel, Help...), laid out in the window (no
 * band of their own) as the platform's dialogs place them: by their
 * Controls.DialogButtonBox.buttonRole, in the order of the platform's
 * buttonLayout, some at the left edge and the rest at the right edge
 * (macOS: Help, Apply and the window's own actions at the left, Cancel and
 * OK at the right; Windows: all at the right, Reset alone at the left). They
 * show their text only, as RPG Maker's dialogs, at the size of
 * Ez.DialogButton, whatever buttons the window gives (Ez.Buttons too).
 */
Controls.DialogButtonBox {
    id: root

    objectName: "buttonBox"
    padding: 0
    spacing: Ez.Metrics.dialogButtonSpacing
    font: Ez.Metrics.font
    background: null
    // the row fills the box (whatever its alignment); the buttons are placed in it
    contentWidth: availableWidth
    contentHeight: row.implicitHeight
    // as wide as its buttons need (longer texts in other languages): the
    // window is never narrower
    implicitWidth: row.implicitWidth + leftPadding + rightPadding
    implicitHeight: row.implicitHeight + topPadding + bottomPadding

    /**
     * The roles placed at the left edge, by buttonLayout (the ones before
     * the stretch of QDialogButtonBox's layouts)
     */
    readonly property var leftRoles: {
        switch (buttonLayout) {
        case Controls.DialogButtonBox.MacLayout:
            return [Controls.DialogButtonBox.HelpRole, Controls.DialogButtonBox.ResetRole,
                    Controls.DialogButtonBox.ApplyRole, Controls.DialogButtonBox.ActionRole]
        case Controls.DialogButtonBox.KdeLayout:
        case Controls.DialogButtonBox.GnomeLayout:
            return [Controls.DialogButtonBox.HelpRole, Controls.DialogButtonBox.ResetRole]
        case Controls.DialogButtonBox.AndroidLayout:
            return [Controls.DialogButtonBox.HelpRole, Controls.DialogButtonBox.ResetRole,
                    Controls.DialogButtonBox.ActionRole]
        default:
            return [Controls.DialogButtonBox.ResetRole]
        }
    }

    /**
     * Return or Enter that the control in focus left (a field once it took
     * its text, a spin box once it took its number, a check box): OK is
     * pressed, as the default button of a desktop dialog. A button shown in
     * focus (Tab) is pressed instead; OK greyed takes the key and does
     * nothing. A box without OK leaves the key (event.accepted false), and
     * so do Return with Ctrl, Alt, Shift or Cmd and a key held down.
     * Ez.DialogWindow and Ez.Dialog call it for the keys that come up to
     * them.
     */
    function pressDefault(event: var) {
        const ok = standardButton(Controls.DialogButtonBox.Ok)
        if (!ok || !ok.visible || (event.modifiers & ~Qt.KeypadModifier) !== Qt.NoModifier || event.isAutoRepeat) {
            event.accepted = false
            return
        }
        event.accepted = true
        const focused = Window.activeFocusItem as T.Button
        if (focused && focused.visualFocus) {
            if (focused.enabled) {
                focused.click()
            }
        } else if (ok.enabled) {
            // as a click on it, without taking the focus from the control
            ok.clicked()
        }
    }

    /** The size of a button of the box: Ez.DialogButton's, from its text */
    function buttonSize(button: Item): size {
        // the width of its text (the box's own padding, not the button's)
        const control = button as T.Control
        const content = control ? control.implicitContentWidth : button.implicitWidth
        return Qt.size(Math.max(Ez.Metrics.dialogButtonMinimumWidth, Math.ceil(content) + 2 * Ez.Metrics.dialogButtonPadding),
                       Math.max(Ez.Metrics.dialogButtonHeight, Math.ceil(button.implicitHeight)))
    }

    delegate: Ez.DialogButton {
        Kirigami.MnemonicData.controlType: Kirigami.MnemonicData.DialogButton
    }

    contentItem: Item {
        id: row

        /**
         * The buttons shown, in the box's order (the platform's), with their
         * sizes: those at the left edge, those at the right edge, and the
         * width they need
         */
        readonly property var buttons: {
            const left = []
            const right = []
            let height = 0
            // in the box's order, again when it sorts a button added
            for (const button of root.contentChildren) {
                if (!button.visible) {
                    continue
                }
                const role = button.Controls.DialogButtonBox.buttonRole
                const entry = { button: button, size: root.buttonSize(button) }
                height = Math.max(height, entry.size.height)
                if (root.leftRoles.includes(role)) {
                    left.push(entry)
                } else {
                    right.push(entry)
                }
            }
            const leftWidth = groupWidth(left)
            const rightWidth = groupWidth(right)
            const gap = left.length > 0 && right.length > 0 ? Ez.Metrics.dialogButtonGroupSpacing : 0
            return { left: left, right: right, leftWidth: leftWidth, rightWidth: rightWidth,
                     width: leftWidth + gap + rightWidth, height: height }
        }
        function groupWidth(group: var): real {
            let width = 0
            for (const entry of group) {
                width += entry.size.width
            }
            return width + Math.max(0, group.length - 1) * root.spacing
        }

        implicitWidth: buttons.width
        implicitHeight: buttons.height

        // the left group from the left edge, the right one to the right
        // edge; narrower than they need (a message over a small window):
        // all narrowed alike
        function place() {
            const scale = width > 0 && width < buttons.width ? width / buttons.width : 1
            const placeGroup = (group, start) => {
                let x = start
                for (const entry of group) {
                    const button = entry.button
                    button.width = Math.floor(entry.size.width * scale)
                    button.height = entry.size.height
                    button.x = Math.round(x)
                    button.y = Math.round((height - entry.size.height) / 2)
                    x += button.width + root.spacing * scale
                }
            }
            placeGroup(buttons.left, 0)
            placeGroup(buttons.right, Math.max(width - buttons.rightWidth * scale, (buttons.width - buttons.rightWidth) * scale))
        }
        onButtonsChanged: place()
        onWidthChanged: place()
        onHeightChanged: place()

        Repeater {
            model: root.contentModel
        }
    }
}
