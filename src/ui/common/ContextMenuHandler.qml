// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.easyrpg.editor as Ez

/**
 * The right button of an item, opening its menu on the press as the menus
 * of macOS do, not on a click: a click with two fingers on a trackpad
 * scrolls a little between its press and its release, which cancels a tap
 * (a click of the right button, the menu never opened). Ctrl+click is a
 * right click too.
 *
 * requested(x, y) comes first, at the point pressed in the item the handler
 * is in: there the line under the pointer is selected, as a click of the
 * left button would. Then the menu, if one is set, opens there
 * (Ez.ContextMenu.popupAt): its top-left corner on the pointer, or its last
 * pixel when flipped at the edge of the screen, so the release of the
 * button lands on its border, not on an item.
 *
 *   MouseArea {
 *       acceptedButtons: Qt.LeftButton
 *       Ez.ContextMenuHandler {
 *           contextMenu: lineMenu
 *           onRequested: (x, y) => list.select(list.lineAt(y))
 *       }
 *   }
 */
TapHandler {
    id: root

    /** The menu opened at the point pressed (none: requested() opens one) */
    property Ez.ContextMenu contextMenu: null

    /** The right button pressed at x, y of the item the handler is in */
    signal requested(real x, real y)

    acceptedButtons: Qt.RightButton
    onPressedChanged: {
        if (pressed) {
            const at = point.pressPosition
            requested(at.x, at.y)
            if (contextMenu) {
                contextMenu.popupAt(parent, at.x, at.y)
            }
        }
    }
}
