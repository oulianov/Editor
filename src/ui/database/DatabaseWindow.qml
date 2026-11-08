// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "DatabaseDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The database window (Game > Database, DatabaseWindows): a copy of the project's database on its pages. OK and Apply put it
 * into the project and its file; when another program changed the file
 * meanwhile, what it changed is merged, and what both sides changed is
 * asked (DatabaseSession.apply, in windows over this one). Cancel writes
 * nothing. While it is open, the project does not read its database again.
 *
 * list: one list alone ("switches", "variables", "maniac_string_variables"),
 * as the windows of the "..." buttons.
 */
Ez.DialogWindow {
    id: root

    /** One list instead of the pages ("": the pages) */
    property alias list: view.list
    /** The page and the entry shown at first (DatabasePage.start) */
    property alias start: view.start
    readonly property alias view: view
    /**
     * The part of the screen it opens on that windows can use (DatabaseWindows
     * gives the main window's): its first size depends on it
     */
    property rect screenArea: Ez.Editor.screenArea(contentItem)

    /**
     * The size it opens at the first time (then where and as large as it
     * was closed, QmlHost): all the tabs side by side (the widest pages,
     * Hero Status and System, need less, in French too) and the Heroes page
     * whole, higher on a large screen for the lists, command lists and
     * canvases; within the screen (on a laptop's, the pages scroll), with
     * room for its title bar, which is not in its size, above it once
     * centred. One list alone: a smaller one.
     */
    function firstSize() {
        const titleRoom = 32
        let width = list === "" ? Math.max(1040, view.tabsWidth + 2 * Ez.Metrics.dialogMargin) : 480
        let height = list === "" ? Math.max(850, screenArea.height * 0.7) : 420
        if (screenArea.width > 0 && screenArea.height > 0) {
            width = Math.min(width, screenArea.width - 2 * titleRoom)
            height = Math.min(height, screenArea.height - 2 * titleRoom)
        }
        return Qt.size(Math.floor(width), Math.floor(height))
    }

    /** Shows a page and an entry of it (DatabasePage.showEntry) */
    function showEntry(start) {
        view.showEntry(start)
    }

    /** OK, Apply: true once the database is in the project and written */
    function applyChanges() {
        switch (view.apply()) {
        case Ez.DatabaseSession.Applied:
            return true
        case Ez.DatabaseSession.SaveFailed:
            // in the project, not on disk: said, the window stays
            saveFailed.text = view.session.message
            saveFailed.open()
            return false
        case Ez.DatabaseSession.Reload:
            // the changes discarded for the file: closed, then the project reads it
            root.reject()
            return false
        default:
            // changed on disk and cancelled: nothing changed
            return false
        }
    }

    objectName: "DatabaseDialog"
    // its session holds the lock while it is open (EditLock::Database)
    ownEditLock: true
    // where and as large as it was closed
    stateKey: list === "" ? "DatabaseWindow" : "DatabaseWindow/" + list
    title: list === "switches" ? qsTr("Switches") : list === "variables" ? qsTr("Variables")
         : list === "maniac_string_variables" ? qsTr("String Variables") : qsTr("Database")
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowMinMaxButtonsHint
    standardButtons: Controls.DialogButtonBox.Ok | Controls.DialogButtonBox.Cancel
                     | Controls.DialogButtonBox.Apply | Controls.DialogButtonBox.Help

    onAccepting: (check) => {
        check.ok = applyChanges()
    }
    onClosing: view.saveViewState()

    Component.onCompleted: {
        view.session.window = root
        // set once, before QmlHost centres it at that size: a page made
        // later does not resize it
        const size = firstSize()
        preferredWidth = size.width
        preferredHeight = size.height
    }
    Binding {
        target: view.session
        property: "editing"
        value: root.visible
    }

    Connections {
        target: root.buttonBox
        function onApplied() {
            root.applyChanges()
        }
        function onHelpRequested() {
            Ez.Editor.help(view.topic, root.contentItem)
        }
    }

    DatabasePage {
        id: view
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    Ez.MessageDialog {
        id: saveFailed
        title: qsTr("Database")
    }
}
