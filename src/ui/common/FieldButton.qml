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
 *
 * fused: it is a part of the field whose value it chooses, drawn in the
 * frame of the field at its right, a thin line before it, as the "..." of
 * Ez.RefComboBox: one of the buttons of an Ez.TextField. The field keeps
 * the focus; its keys press it (see Ez.TextField.buttons).
 *
 *   Ez.TextField { buttons: Ez.FieldButton { fused: true; onClicked: pick() } }
 */
Ez.Button {
    id: root

    /** Choices shown under the button when it is clicked (clicked() is still emitted) */
    property Controls.Menu menu: null
    /** Drawn in the frame of a field (Ez.TextField.buttons), not as a button of its own */
    property bool fused: false

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
    implicitWidth: fused ? Math.max(Math.round(height * 0.9), Math.ceil(implicitContentWidth) + leftPadding + rightPadding)
                         : Math.max(implicitHeight, Math.ceil(implicitContentWidth) + leftPadding + rightPadding)

    // in a field: as high as its frame, the field keeps the focus, the text
    // close to the lines (as the "..." of a list of entries)
    height: fused && parent ? parent.height : implicitHeight
    Binding {
        target: root
        property: "focusPolicy"
        value: Qt.NoFocus
        when: root.fused
    }
    Binding {
        target: root
        property: "leftPadding"
        value: root.spacing
        when: root.fused
    }
    Binding {
        target: root
        property: "verticalPadding"
        value: 0
        when: root.fused
    }
    // the frame is the field's: Breeze's face of a button is not drawn
    Binding {
        target: root.background
        property: "visible"
        value: false
        when: root.fused && root.background !== null
    }
    Item {
        id: segment
        visible: root.fused
        anchors.fill: parent
        z: -1

        /** The last part of the frame: its corners rounded as the field's */
        readonly property bool last: root.parent !== null && root.x + root.width >= root.parent.width - 0.5
        readonly property real radius: last ? Math.max(0, Kirigami.Units.cornerRadius - 1) : 0

        // pointed at, pressed (the menu shown)
        Item {
            anchors.fill: parent
            clip: true
            visible: root.down || root.hovered || (root.menu !== null && root.menu.visible)
            Rectangle {
                x: -segment.radius
                width: parent.width + segment.radius
                height: parent.height
                radius: segment.radius
                color: root.down || (root.menu !== null && root.menu.visible)
                       ? Qt.rgba(Kirigami.Theme.focusColor.r, Kirigami.Theme.focusColor.g, Kirigami.Theme.focusColor.b, 0.3)
                       : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.1)
            }
        }
        // the line between the text of the field (or the part before) and the button
        Rectangle {
            x: 0
            y: Ez.Metrics.fieldPadding - 1
            width: 1
            height: parent.height - 2 * y
            color: Ez.Metrics.sectionBorderColor
        }
    }
    // the pointer of a button, not the text cursor of the field it is in
    HoverHandler {
        enabled: root.fused
        cursorShape: Qt.ArrowCursor
    }

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
