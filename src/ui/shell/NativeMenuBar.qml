// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MainWindow"
pragma ComponentBehavior: Bound

import QtQuick
import org.easyrpg.editor as Ez

/**
 * The menu bar of the main window on macOS: the native one, its menus as
 * the catalog lists them (MainMenuBar elsewhere). Only Quit, About and
 * Settings go to the application menu (their roles): macOS would guess
 * others from their texts, wrongly in some languages ("Options du test du
 * jeu..." as Preferences). Test Play Options is there too, beside
 * Settings. Its menus and items are the platform's (PlatformMenuBar).
 */
Ez.PlatformMenuBar {
    id: root

    /** The actions of the window (Actions.qml) */
    required property Actions actions
    readonly property Ez.ActionCatalog catalog: actions.shell.actions
    /** The items of the actions, as made: {name, role ("quit", "about",
     *  "preferences", "application", "" for none), menu (its objectName), item} */
    property var entries: []
    property bool built: false

    /** The name of the role of an item, as the entries have it */
    function roleName(role) {
        switch (role) {
        case Ez.PlatformMenuItem.QuitRole:
            return "quit"
        case Ez.PlatformMenuItem.AboutRole:
            return "about"
        case Ez.PlatformMenuItem.PreferencesRole:
            return "preferences"
        case Ez.PlatformMenuItem.ApplicationSpecificRole:
            return "application"
        default:
            return ""
        }
    }
    /** The entry of an action: with that role, else any (role undefined); null for none */
    function entry(name, role) {
        for (const found of entries) {
            if (found.name === name && (role === undefined || found.role === role)) {
                return found
            }
        }
        return null
    }

    function build() {
        // once (a script's check may ask first)
        if (built) {
            return
        }
        built = true
        const added = []
        const menus = {}
        const list = catalog.menuList()
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
                    menu.addItem(separatorComponent.createObject(menu))
                } else if (item.startsWith("menu:")) {
                    menu.addMenu(menus[item.substring(5)])
                } else {
                    const action = actions.action(item)
                    if (action && action.shown) {
                        const made = itemComponent.createObject(menu, { catalogAction: action }) as Ez.PlatformMenuItem
                        menu.addItem(made)
                        added.push({ name: item, role: roleName(made.role), menu: info.name, item: made })
                    }
                }
            }
            if (info.name === "menuTools") {
                const made = itemComponent.createObject(menu, {
                    catalogAction: actions.action("actionPlayTestOptions"),
                    role: Ez.PlatformMenuItem.ApplicationSpecificRole
                })
                menu.addItem(made)
                added.push({ name: "actionPlayTestOptions", role: "application", menu: info.name, item: made })
            }
            if (info.menuBar) {
                addMenu(menu)
            }
        }
        entries = added
    }

    // once the actions are all made
    Component.onCompleted: Qt.callLater(build)

    Component {
        id: menuComponent
        Ez.PlatformMenu {
            required property string name
            objectName: name
            title: root.actions.revision >= 0 ? root.catalog.menuTitle(name) : ""
        }
    }
    Component {
        id: separatorComponent
        Ez.PlatformMenuItem {
            separator: true
        }
    }
    Component {
        id: itemComponent
        Ez.PlatformMenuItem {
            required property var catalogAction
            objectName: catalogAction.objectName
            text: catalogAction.text
            iconSource: catalogAction.icon.source
            enabled: catalogAction.enabled
            checkable: catalogAction.checkable
            checked: catalogAction.checked
            shortcut: catalogAction.shortcut
            role: catalogAction.menuRole === Ez.ActionCatalog.MenuRole.Quit ? Ez.PlatformMenuItem.QuitRole
                : catalogAction.menuRole === Ez.ActionCatalog.MenuRole.About ? Ez.PlatformMenuItem.AboutRole
                : catalogAction.menuRole === Ez.ActionCatalog.MenuRole.Preferences ? Ez.PlatformMenuItem.PreferencesRole
                : Ez.PlatformMenuItem.NoRole
            onTriggered: catalogAction.trigger()
        }
    }
}
