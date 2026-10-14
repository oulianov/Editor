// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The small button beside a field that chooses its value ("..."): as wide
 * as its text needs, at least square, not the width of the dialog buttons.
 *
 * With a menu it is a drop-down button, a small arrow after its text says
 * so: a click (Space, Return or Down) shows the menu under it, a click on
 * it while the menu is shown closes it, as a drop-down list does. Escape
 * and a click elsewhere close it too.
 */
Ez.Button {
    id: root

    /** Choices shown under the button when it is clicked (clicked() is still emitted) */
    property Controls.Menu menu: null

    /** Shows the menu under the button, or closes it when it was shown at the press of the click */
    function toggleMenu() {
        if (!menu) {
            return
        }
        if (d.shownAtPress) {
            d.shownAtPress = false
            menu.close()
        } else {
            menu.popup(root, 0, root.height)
        }
    }

    text: "..."
    // the text keeps its padding on both sides; the arrow is past it
    rightPadding: leftPadding + (menu ? arrow.width + spacing : 0)
    implicitWidth: Math.max(implicitHeight, Math.ceil(implicitContentWidth) + leftPadding + rightPadding)

    QtObject {
        id: d
        /** The menu was shown when the button was pressed: its click closes it */
        property bool shownAtPress: false
        /** The menu is being closed, by the event being handled */
        property bool closing: false
    }
    // a press on the button leaves the menu to its click (which closes it):
    // closed by the press, the click would show it again. A menu in a window
    // of its own is closed by any press outside it: the press on the button
    // comes as it closes, and is remembered as made on the menu shown.
    onPressed: d.shownAtPress = menu !== null && (menu.visible || d.closing)
    Connections {
        target: root.menu
        function onAboutToHide() {
            d.closing = true
            Qt.callLater(() => { d.closing = false })
        }
    }
    Binding {
        target: root.menu
        property: "closePolicy"
        value: Controls.Popup.CloseOnEscape | Controls.Popup.CloseOnPressOutsideParent
        when: root.menu !== null
    }

    Kirigami.Icon {
        id: arrow
        visible: root.menu !== null
        source: "arrow-down"
        width: Kirigami.Units.iconSizes.small
        height: width
        anchors.right: parent.right
        anchors.rightMargin: root.leftPadding
        anchors.verticalCenter: parent.verticalCenter
    }

    // the keys of a drop-down list
    Keys.onPressed: (event) => {
        if (menu && (event.key === Qt.Key_Down || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                && (event.modifiers & ~Qt.KeypadModifier) === Qt.NoModifier) {
            event.accepted = true
            if (!menu.visible) {
                menu.popup(root, 0, root.height)
            }
        }
    }

    onClicked: toggleMenu()
}
