// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * A right-click menu, opened with popupAt() where the pointer is, as the
 * menus of the system: its top-left corner on the point clicked, in a
 * window of its own; where the usable screen ends below or right of the
 * point (the Dock, the menu bar), it opens above or left of it.
 *
 * A menu drawn in the window (Popup.Item) is pushed inside the window near
 * its bottom or right edge (the map tree, at the bottom of the main window),
 * away from the pointer; Qt pushes a menu window inside the screen the same
 * way, and only knows its size once its items are laid out: the menu is
 * placed again when its size changes.
 */
Controls.Menu {
    id: root

    /** The point the menu opens at: an item of no size in the item clicked
     *  (the menu takes its colors from there), hidden (a layout leaves it) */
    component Anchor: Item {
        objectName: "contextMenuAnchor"
        visible: false
        width: 0
        height: 0
    }
    readonly property Anchor anchor: Anchor {}

    /** Opens the menu with its top-left corner at x, y of item (the point clicked) */
    function popupAt(item: Item, x: real, y: real) {
        anchor.parent = item
        anchor.x = x
        anchor.y = y
        root.popup(anchor, 0, 0)
        place()
    }
    /** At the point, or over it where the screen ends */
    function place() {
        if (!root.visible || root.parent !== anchor) {
            return
        }
        const point = anchor.mapToGlobal(0, 0)
        // the screen without the Dock and the menu bar: Qt pushes a menu
        // window inside it, which would move it off the point
        const screen = Ez.Editor.screenArea(anchor)
        const left = point.x + root.width > screen.x + screen.width && point.x - root.width >= screen.x
        const up = point.y + root.height > screen.y + screen.height && point.y - root.height >= screen.y
        // flipped, its last pixel on the point: a menu opened on a press
        // is under the pointer when the button is released, it stays open
        root.x = left ? 1 - root.width : 0
        root.y = up ? 1 - root.height : 0
    }

    popupType: Controls.Popup.Window
    onWidthChanged: place()
    onHeightChanged: place()
}
