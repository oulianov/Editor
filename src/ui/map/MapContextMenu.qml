// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MapScene"

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The right-click menus of the map: the event layer's, on the cell clicked,
 * with the vehicles in a submenu, and the selection tool's (RPG Maker's),
 * on the tiles or the events selected. Their actions have stable names for
 * scripts (actionEventNew...) and are enabled, New or Edit shown, as the
 * MapEditor says when a menu opens (actionStates).
 *
 * The keys shown are the map's: MapCanvas takes them while the menus are
 * closed, the menu's own work while it is open.
 */
Item {
    id: root

    /** The editing the actions apply to */
    property Ez.MapEditor editor
    /** The states of the actions of the last menu opened, by name: {enabled, visible} */
    property var actionStates: ({})

    /** The menus, made the first time they open (a map shown is rarely right-clicked) */
    property Ez.ContextMenu eventMenu: null
    property Ez.ContextMenu areaMenu: null
    readonly property bool opened: (eventMenu?.opened ?? false) || (areaMenu?.opened ?? false)
                                   || (eventMenu?.vehicles.opened ?? false)
    /** The Editor's icons (resources/icons) for the colors shown, light or dark */
    readonly property string icons: colors.window.hslLightness >= 0.5 ? "qrc:/bright/" : "qrc:/dark/"

    /** The menu the editor asks for (MapEditor.EventMenu or AreaMenu) at x, y of item */
    function popup(menu, item, x, y) {
        refresh(menu)
        if (menu === Ez.MapEditor.EventMenu && !eventMenu) {
            eventMenu = eventMenuComponent.createObject(root) as Ez.ContextMenu
        } else if (menu !== Ez.MapEditor.EventMenu && !areaMenu) {
            areaMenu = areaMenuComponent.createObject(root) as Ez.ContextMenu
        }
        const shown = menu === Ez.MapEditor.EventMenu ? eventMenu : areaMenu
        shown.popupAt(item, x, y)
    }
    /** The actions enabled (and shown) as the editor says, for a menu */
    function refresh(menu) {
        actionStates = editor ? editor.actionStates(menu) : {}
    }
    function enabledOf(name) {
        const state = actionStates[name]
        return state === undefined || state.enabled
    }
    function visibleOf(name) {
        const state = actionStates[name]
        return state === undefined || state.visible
    }

    SystemPalette {
        id: colors
    }

    // An action of the menus (menu: this item), its key shown (and working)
    // while a menu is open
    component MapAction: Controls.Action {
        required property MapContextMenu menu
        property var key: ""
        readonly property bool shown: menu.visibleOf(objectName)
        enabled: menu.enabledOf(objectName)
        shortcut: menu.opened ? key : ""
    }
    // Its item, out of the menu while hidden (New or Edit)
    component MapItem: Controls.MenuItem {
        visible: (action as MapAction)?.shown ?? true
        height: visible ? implicitHeight : 0
    }

    // The actions, here for the scripts (action actionEventNew...)
    MapAction {
        id: eventNew
        menu: root
        objectName: "actionEventNew"
        text: qsTr("New Event")
        icon.source: root.icons + "new"
        key: "Return"
        onTriggered: root.editor.newEvent()
    }
    MapAction {
        id: eventEdit
        menu: root
        objectName: "actionEventEdit"
        text: qsTr("Edit Event")
        icon.source: root.icons + "properties"
        key: "Return"
        onTriggered: root.editor.editEvent()
    }
    MapAction {
        id: eventCut
        menu: root
        objectName: "actionEventCut"
        text: qsTr("Cut Event")
        icon.source: root.icons + "edit-cut"
        key: "Ctrl+X"
        onTriggered: root.editor.cutEvent()
    }
    MapAction {
        id: eventCopy
        menu: root
        objectName: "actionEventCopy"
        text: qsTr("Copy Event")
        icon.source: root.icons + "edit-copy"
        key: "Ctrl+C"
        onTriggered: root.editor.copyEvent()
    }
    MapAction {
        id: eventPaste
        menu: root
        objectName: "actionEventPaste"
        text: qsTr("Paste Event")
        icon.source: root.icons + "edit-paste"
        key: "Ctrl+V"
        onTriggered: root.editor.pasteEvent()
    }
    MapAction {
        id: eventDelete
        menu: root
        objectName: "actionEventDelete"
        text: qsTr("Delete Event")
        icon.source: root.icons + "edit-delete"
        key: "Del"
        onTriggered: root.editor.deleteEvent()
    }
    MapAction {
        id: placeTeleport
        menu: root
        objectName: "actionPlaceTeleport"
        text: qsTr("Place Teleport Event")
        icon.source: root.icons + "place-teleport"
        onTriggered: root.editor.placeTeleport()
    }
    // the start positions, by Core::StartMarker
    MapAction {
        id: placeParty
        menu: root
        objectName: "actionPlacePartyStart"
        text: qsTr("Place Party Starting Position")
        icon.source: root.icons + "place-start"
        onTriggered: root.editor.placeStart(0)
    }
    MapAction {
        id: placeBoat
        menu: root
        objectName: "actionPlaceBoatStart"
        text: qsTr("Skiff")
        onTriggered: root.editor.placeStart(1)
    }
    MapAction {
        id: placeShip
        menu: root
        objectName: "actionPlaceShipStart"
        text: qsTr("Ship")
        onTriggered: root.editor.placeStart(2)
    }
    MapAction {
        id: placeAirship
        menu: root
        objectName: "actionPlaceAirshipStart"
        text: qsTr("Airship")
        onTriggered: root.editor.placeStart(3)
    }
    MapAction {
        id: runHere
        menu: root
        objectName: "actionRunHere"
        text: qsTr("Start Game Here")
        icon.source: root.icons + "play"
        onTriggered: root.editor.runHere()
    }
    MapAction {
        id: areaCut
        menu: root
        objectName: "actionAreaCut"
        text: qsTr("Cut")
        icon.source: root.icons + "edit-cut"
        key: "Ctrl+X"
        onTriggered: root.editor.cutArea()
    }
    MapAction {
        id: areaCopy
        menu: root
        objectName: "actionAreaCopy"
        text: qsTr("Copy")
        icon.source: root.icons + "edit-copy"
        key: "Ctrl+C"
        onTriggered: root.editor.copyArea()
    }
    // in the middle of the view (MapEditor.viewCells)
    MapAction {
        id: areaPaste
        menu: root
        objectName: "actionAreaPaste"
        text: qsTr("Paste")
        icon.source: root.icons + "edit-paste"
        key: "Ctrl+V"
        onTriggered: root.editor.pasteArea()
    }
    MapAction {
        id: areaClear
        menu: root
        objectName: "actionAreaClear"
        text: qsTr("Clear")
        icon.source: root.icons + "edit-clear"
        key: "Del"
        onTriggered: root.editor.clearArea()
    }
    MapAction {
        id: areaSelectAll
        menu: root
        objectName: "actionAreaSelectAll"
        text: qsTr("Select All")
        icon.source: root.icons + "select-all"
        key: "Ctrl+A"
        onTriggered: root.editor.selectAll()
    }

    Component {
        id: eventMenuComponent

        Ez.ContextMenu {
            id: eventMenu
            objectName: "eventMenu"

            readonly property Controls.Menu vehicles: vehicleMenu

            // the item of the vehicles' submenu shows no icon of the submenu's own
            Component.onCompleted: {
                for (let i = 0; i < count; ++i) {
                    const item = itemAt(i) as Controls.MenuItem
                    if (item && item.subMenu === vehicleMenu) {
                        item.icon.source = Qt.binding(() => root.icons + "place-vehicle")
                    }
                }
            }

            MapItem { action: eventNew }
            MapItem { action: eventEdit }
            Controls.MenuSeparator {}
            MapItem { action: eventCut }
            MapItem { action: eventCopy }
            MapItem { action: eventPaste }
            MapItem { action: eventDelete }
            Controls.MenuSeparator {}
            MapItem { action: placeTeleport }
            Controls.MenuSeparator {}
            MapItem { action: placeParty }
            Controls.Menu {
                id: vehicleMenu
                objectName: "vehicleMenu"
                title: qsTr("Place Vehicle Starting Position")
                // the boat, the ship and the airship follow the party
                enabled: root.enabledOf("actionPlaceBoatStart")

                MapItem { action: placeBoat }
                MapItem { action: placeShip }
                MapItem { action: placeAirship }
            }
            Controls.MenuSeparator {}
            MapItem { action: runHere }
        }
    }

    // RPG Maker's menu of the selection tool
    Component {
        id: areaMenuComponent

        Ez.ContextMenu {
            objectName: "areaMenu"

            MapItem { action: areaCut }
            MapItem { action: areaCopy }
            MapItem { action: areaPaste }
            MapItem { action: areaClear }
            MapItem { action: areaSelectAll }
        }
    }
}
