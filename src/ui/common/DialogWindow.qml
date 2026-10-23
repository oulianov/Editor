// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQuick.Templates as T
import org.easyrpg.editor as Ez

/**
 * A dialog window of the Editor (opened by QmlHost::exec): its content in a
 * column, then its buttons (OK and Cancel unless told otherwise). It is as
 * large as its content needs, at least.
 *
 * result says how it closed (QmlHost::Result): 1 accepted (OK, Return),
 * 0 rejected (Cancel, Escape, the close button of the window). accept() can
 * be refused by onAccepting: set accepting.ok to false; reject() (and
 * closing the window) by onRejecting the same way, while something runs.
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

        /** The tool tip the items share (Ez.ToolTip) */
        readonly property T.ToolTip toolTip: Controls.ToolTip.toolTip

        ColumnLayout {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Ez.Metrics.sectionSpacing
        }
        Ez.ButtonBox {
            id: box
            Layout.fillWidth: true
            // a window with buttons of its own elsewhere has none here; those
            // added to the box (MessageWindow's named answers) count too
            visible: root.standardButtons !== Controls.DialogButtonBox.NoButton || root.buttons.length > 0 || count > 0
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

    /**
     * A pop-up is open over the window (the search of a list, a menu, a
     * dialog drawn in it): it takes Escape and Return first. The tool tip
     * the items share takes no keys: it does not count (as for Qt's
     * shortcuts).
     */
    function popupOpen(): bool {
        const overlay = Controls.Overlay.overlay
        if (!overlay) {
            return false
        }
        const toolTip = layout.toolTip
        const toolTipItem = toolTip && toolTip.contentItem ? toolTip.contentItem.parent : null
        for (let i = 0; i < overlay.children.length; ++i) {
            const child = overlay.children[i]
            if (child.visible && child !== toolTipItem) {
                return true
            }
        }
        return false
    }

    Shortcut {
        sequences: [StandardKey.Cancel]
        // only in the window in front (not under the dialog it opened), and
        // a pop-up open takes Escape first
        enabled: root.active && root.activeFocusItem !== null && !root.popupOpen()
        onActivated: root.reject()
    }

    // Return and Enter that the control in focus leaves (a field once it
    // took its text, a check box; not a text of several lines, a list
    // opening its lines with them, an open drop-down) press OK
    // (Ez.ButtonBox.pressDefault): they come up from the control to the
    // window's own item, as to a desktop dialog. A pop-up open takes them
    // first.
    Connections {
        target: {
            let item = root.contentItem
            while (item && item.parent) {
                item = item.parent
            }
            return item ? item.Keys : null
        }
        function onReturnPressed(event) {
            root.pressDefault(event)
        }
        function onEnterPressed(event) {
            root.pressDefault(event)
        }
    }
    function pressDefault(event) {
        if (root.closed || root.popupOpen()) {
            event.accepted = false
            return
        }
        box.pressDefault(event)
    }
}
