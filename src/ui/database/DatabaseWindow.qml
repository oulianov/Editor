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
 * list: one list alone ("switches", "variables"), as the windows of the
 * "..." buttons.
 */
Ez.DialogWindow {
    id: root

    /** One list instead of the pages ("": the pages) */
    property alias list: view.list
    /** The page and the entry shown at first (DatabaseView.start) */
    property alias start: view.start
    readonly property alias view: view

    /** Shows a page and an entry of it (DatabaseView.showEntry) */
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
    title: list === "switches" ? qsTr("Switches") : list === "variables" ? qsTr("Variables") : qsTr("Database")
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowMinMaxButtonsHint
    standardButtons: Controls.DialogButtonBox.Ok | Controls.DialogButtonBox.Cancel
                     | Controls.DialogButtonBox.Apply | Controls.DialogButtonBox.Help
    // as wide as its widest pages (Hero Status, Terrain; in French too):
    // no page scrolls sideways; one list alone: a smaller one
    preferredWidth: list === "" ? 1040 : 480
    height: Math.max(minimumHeight, list === "" ? 635 : 420)

    onAccepting: (check) => {
        check.ok = applyChanges()
    }
    onClosing: view.saveViewState()

    Component.onCompleted: view.session.window = root
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

    DatabaseView {
        id: view
        Layout.fillWidth: true
        Layout.fillHeight: true
    }

    Ez.MessageDialog {
        id: saveFailed
        title: qsTr("Database")
    }
}
