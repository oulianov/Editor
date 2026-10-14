// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A dialog window of the Editor (opened by QmlHost::exec): its content in a
 * column, then its buttons (OK and Cancel unless told otherwise). It is as
 * large as its content needs, at least.
 *
 * result says how it closed (QmlHost::Result): 1 accepted (OK), 0 rejected
 * (Cancel, Escape, the close button of the window). accept() can be
 * refused by onAccepting: set accepting.ok to false; reject() (and closing
 * the window) by onRejecting the same way, while something runs.
 */
Controls.ApplicationWindow {
    id: root

    property int result: 0
    default property alias content: body.data
    readonly property alias body: body
    property alias standardButtons: box.standardButtons
    readonly property alias buttonBox: box
    /** The size it opens at, when it is more than its content needs */
    property int preferredWidth: 0
    property int preferredHeight: 0
    /**
     * Buttons of its own beside the standard ones, in the button box: their
     * Controls.DialogButtonBox.buttonRole places them (ActionRole: on the
     * side, the window stays open)
     */
    property list<Item> buttons
    /** OK can be chosen (false: greyed, while something runs for the window) */
    property bool acceptable: true
    /**
     * The name its size and place are remembered by (QmlHost, in the
     * settings: it opens as it was closed); "": centered at its size
     */
    property string stateKey: ""
    /**
     * It holds an edit lock of its own (the event and database windows):
     * else, modal, it holds an EditLock::Dialog while it is open, and the
     * project is not read again from disk meanwhile (ModalWindowLocks)
     */
    property bool ownEditLock: false

    signal accepting(var accepting)
    signal rejecting(var rejecting)
    signal accepted()
    signal rejected()

    function accept() {
        if (!acceptable) {
            return
        }
        const check = { ok: true }
        accepting(check)
        if (!check.ok) {
            return
        }
        result = 1
        closed = true
        accepted()
        close()
    }
    function reject() {
        const check = { ok: true }
        rejecting(check)
        if (!check.ok) {
            return
        }
        result = 0
        closed = true
        rejected()
        close()
    }
    property bool closed: false
    onClosing: close => {
        if (!closed) {
            const check = { ok: true }
            rejecting(check)
            if (!check.ok) {
                close.accepted = false
                return
            }
            closed = true
            rejected()
        }
    }

    flags: Qt.Dialog
    font: Ez.Metrics.font
    minimumWidth: Math.ceil(layout.implicitWidth) + 2 * Ez.Metrics.dialogMargin
    minimumHeight: Math.ceil(layout.implicitHeight) + 2 * Ez.Metrics.dialogMargin
    width: Math.max(minimumWidth, preferredWidth)
    height: Math.max(minimumHeight, preferredHeight)

    ColumnLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: Ez.Metrics.dialogMargin
        spacing: Ez.Metrics.sectionSpacing

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Ez.Metrics.sectionSpacing
        }
        Ez.ButtonBox {
            id: box
            Layout.fillWidth: true
            // a window with buttons of its own elsewhere has none here
            visible: root.standardButtons !== Controls.DialogButtonBox.NoButton || root.buttons.length > 0
            standardButtons: Controls.DialogButtonBox.Ok | Controls.DialogButtonBox.Cancel
            onAccepted: root.accept()
            onRejected: root.reject()
            Component.onCompleted: {
                for (const button of root.buttons) {
                    addItem(button)
                }
            }
        }
    }

    Binding {
        // again when the standard buttons are made
        target: {
            box.standardButtons
            return box.standardButton(Controls.DialogButtonBox.Ok)
        }
        property: "enabled"
        value: root.acceptable
        when: target !== null
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        // only in the window in front (not under the dialog it opened), and
        // a pop-up open (the search of a list, a menu) takes Escape first
        enabled: {
            if (!root.active || !root.activeFocusItem) {
                return false
            }
            const overlay = Controls.Overlay.overlay
            if (!overlay) {
                return true
            }
            for (let i = 0; i < overlay.children.length; ++i) {
                if (overlay.children[i].visible) {
                    return false
                }
            }
            return true
        }
        onActivated: root.reject()
    }
}
