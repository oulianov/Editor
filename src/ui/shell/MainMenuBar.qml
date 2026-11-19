// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MainWindow"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The menu bar of the main window, drawn in the window (Windows, Linux, and
 * macOS without its native menu bar: offscreen), its menus as the catalog
 * lists them (Game Project, Maps, View, Tools, Game, Help), each named for
 * the scripts (menuGame...). Hovering an item shows its
 * hint in the status bar.
 */
Controls.MenuBar {
    id: root

    /** The actions of the window (Actions.qml) */
    required property Actions actions
    readonly property Ez.ActionCatalog catalog: actions.shell.actions
    /** The items of the actions, as made: {name, menu (its objectName), item} */
    property var entries: []

    /** The item of an action in a menu, null for none */
    function entry(name) {
        for (const found of entries) {
            if (found.name === name) {
                return found
            }
        }
        return null
    }

    function build() {
        const added = []
        const menus = {}
        const list = catalog.menuList()
        // the menus first: a menu holds a submenu made before it is filled
        for (const info of list) {
            if (!info.name.startsWith("menuTree")) {
                menus[info.name] = menuComponent.createObject(root, { name: info.name })
            }
        }
        for (const info of list) {
            const menu = menus[info.name]
            if (!menu) {
                continue
            }
            for (const item of info.items) {
                if (item === "") {
                    menu.addItem(separatorComponent.createObject(menu.contentItem))
                } else if (item.startsWith("menu:")) {
                    menu.addMenu(menus[item.substring(5)])
                } else {
                    const action = actions.action(item)
                    if (action && action.shown) {
                        const made = itemComponent.createObject(menu.contentItem, { catalogAction: action })
                        menu.addItem(made)
                        added.push({ name: item, menu: info.name, item: made })
                    }
                }
            }
            if (info.menuBar) {
                addMenu(menu)
            }
        }
        entries = added
    }

    objectName: "menuBar"
    // once the actions are all made
    Component.onCompleted: Qt.callLater(build)

    Component {
        id: menuComponent
        Controls.Menu {
            required property string name
            objectName: name
            title: root.actions.revision >= 0 ? root.catalog.menuTitle(name) : ""
        }
    }
    Component {
        id: separatorComponent
        Controls.MenuSeparator {}
    }
    Component {
        id: itemComponent
        Controls.MenuItem {
            id: menuItem
            required property var catalogAction
            action: catalogAction
            // the style draws the keys right of the text without making room
            // for them: the menu is as wide as its text, its keys and a gap
            implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                    implicitContentWidth + leftPadding + rightPadding
                                    + (keys.text === "" ? 0 : Kirigami.Units.gridUnit + keys.advanceWidth),
                                    implicitIndicatorWidth + leftPadding + rightPadding)
            onHoveredChanged: root.actions.hint = hovered ? catalogAction.statusTip : ""
            TextMetrics {
                id: keys
                font: menuItem.font
                // its first keys, shown or not (the width does not change when the window gets the keyboard)
                text: menuItem.catalogAction.keys.length > 0 ? menuItem.catalogAction.keys[0] : ""
            }
        }
    }
}
