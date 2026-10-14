// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "MainWindow"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The actions of the main window, one Controls.Action per action of the
 * catalog (ActionCatalog), named as it names them (actionMapSave...): their
 * texts, icons, keys, and whether they are enabled and checked, as the
 * catalog says. Triggered, they go to the shell (QmlShell::trigger); a
 * checkable one says its new state.
 *
 * The keys of the map tree (Ctrl+C, Del...) are the tree's own: those
 * actions have none here (MapTree). The menus, the toolbars and the tree
 * find them by name (action()).
 */
Item {
    id: root

    required property Ez.Shell shell
    readonly property Ez.ActionCatalog catalog: shell ? shell.actions : null
    /** The Editor's icons (resources/icons) for the colors shown, light or dark */
    property string iconFolder: "qrc:/bright/"
    /** Changes with the states and the language of the catalog: the
     *  bindings that read it follow */
    property int revision: 0
    /** The hint of the action hovered, for the status bar */
    property string hint

    /** The actions by name, once they are all made */
    property var byName: ({})

    /** The action of a name, null for none */
    function action(name) {
        return byName[name] ?? null
    }

    visible: false
    Component.onCompleted: {
        const actions = {}
        for (const child of data) {
            if (child instanceof Controls.Action) {
                actions[child.objectName] = child
            }
        }
        byName = actions
    }

    Connections {
        target: root.catalog
        function onStateChanged() {
            root.revision++
        }
        function onRetranslated() {
            root.revision++
        }
    }

    /** An action of the catalog, by its name */
    component CatalogAction: Controls.Action {
        id: action

        required property string name
        // revision >= 0: read again when the catalog changes
        readonly property string toolTip: root.revision >= 0 ? root.catalog.toolTip(action.name) : ""
        readonly property string statusTip: root.revision >= 0 ? root.catalog.statusTip(action.name) : ""
        readonly property bool shown: root.catalog.isVisible(action.name)
        readonly property var keys: root.catalog.shortcutTexts(action.name)
        /** Its keys work in the whole window, not only in the map tree */
        readonly property bool windowKeys: !root.catalog.isTreeShortcut(action.name)
        /** Where macOS puts it (Ez.ActionCatalog.Quit, About, Preferences; None: its menu) */
        readonly property int menuRole: root.catalog.menuRole(action.name)
        readonly property string iconName: root.catalog.iconName(action.name)

        objectName: action.name
        text: root.revision >= 0 ? root.catalog.text(action.name) : ""
        icon.source: action.iconName === "" ? "" : root.iconFolder + action.iconName
        checkable: root.catalog.isCheckable(action.name)
        checked: root.revision >= 0 && root.catalog.isChecked(action.name)
        enabled: root.revision >= 0 && root.catalog.isEnabled(action.name)
        // only while the window has the keyboard (not under a window of its own)
        shortcut: action.windowKeys && action.keys.length > 0 && root.shell.windowFocused ? action.keys[0] : ""
        onTriggered: {
            root.shell.trigger(action.name, action.checked)
            // the catalog's state again (a click on the layer checked unchecked it)
            root.revision++
        }
    }

    // the keys of an action beyond its first one
    Instantiator {
        model: {
            const extra = []
            for (const child of Object.values(root.byName)) {
                if (child.windowKeys) {
                    for (let i = 1; i < child.keys.length; i++) {
                        extra.push({ action: child, key: child.keys[i] })
                    }
                }
            }
            return extra
        }
        delegate: Shortcut {
            required property var modelData
            sequence: modelData.key
            enabled: modelData.action.enabled && root.shell.windowFocused
            onActivated: modelData.action.trigger()
        }
    }

    // In the order of the catalog (the order of the menus)
    CatalogAction { name: "actionProjectOpen" }
    CatalogAction { name: "actionProjectNew" }
    CatalogAction { name: "actionProjectClose" }
    CatalogAction { name: "actionGameDiskCreate" }
    CatalogAction { name: "actionQuit" }
    CatalogAction { name: "actionMapSave" }
    CatalogAction { name: "actionMapRevert" }
    CatalogAction { name: "actionLayerLower" }
    CatalogAction { name: "actionLayerUpper" }
    CatalogAction { name: "actionLayerEvents" }
    CatalogAction { name: "actionZoom100" }
    CatalogAction { name: "actionZoomIn" }
    CatalogAction { name: "actionZoomOut" }
    CatalogAction { name: "actionDatabase" }
    CatalogAction { name: "actionResourceManager" }
    CatalogAction { name: "actionJukebox" }
    CatalogAction { name: "actionSearch" }
    CatalogAction { name: "actionPlayTest" }
    CatalogAction { name: "actionFullScreenToggle" }
    CatalogAction { name: "actionTitleBackgroundToggle" }
    CatalogAction { name: "actionContents" }
    CatalogAction { name: "actionAbout" }
    CatalogAction { name: "actionScriptEditor" }
    CatalogAction { name: "actionPaletteToggle" }
    CatalogAction { name: "actionMapTreeToggle" }
    CatalogAction { name: "actionUndo" }
    CatalogAction { name: "actionRedo" }
    CatalogAction { name: "actionMapSaveAll" }
    CatalogAction { name: "actionProjectSettings" }
    CatalogAction { name: "actionProjectConvert2003" }
    CatalogAction { name: "actionSettings" }
    CatalogAction { name: "actionDrawSelect" }
    CatalogAction { name: "actionZoom" }
    CatalogAction { name: "actionDrawPen" }
    CatalogAction { name: "actionDrawRectangle" }
    CatalogAction { name: "actionDrawCircle" }
    CatalogAction { name: "actionDrawFill" }
    CatalogAction { name: "actionProjectImport" }
    CatalogAction { name: "actionMapProperties" }
    CatalogAction { name: "actionMapNew" }
    CatalogAction { name: "actionMapCopy" }
    CatalogAction { name: "actionMapPaste" }
    CatalogAction { name: "actionMapDelete" }
    CatalogAction { name: "actionAreaNew" }
    CatalogAction { name: "actionAreaProperties" }
    CatalogAction { name: "actionAreaDelete" }
    CatalogAction { name: "actionMapGenerateDungeon" }
    CatalogAction { name: "actionMapShift" }
    CatalogAction { name: "actionPlayTestOptions" }
    CatalogAction { name: "actionZoom1_1" }
    CatalogAction { name: "actionZoom1_2" }
    CatalogAction { name: "actionZoom1_4" }
    CatalogAction { name: "actionZoom1_8" }
}
