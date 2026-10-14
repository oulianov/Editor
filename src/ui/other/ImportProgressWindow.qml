// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ImportProjectDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The progress of Game Project > Import Project, over the main window
 * while the game is copied (QmlShellDialogs::withImportProgress): what is
 * done, and how far. The copy cannot be stopped: the window stays until it
 * is over, then its caller deletes it.
 */
Ez.DialogWindow {
    id: root

    /** What is done now ("Importing maps...") */
    property string label
    /** The steps of the work, 0 while they are not known */
    property int maximum: 0
    /** The steps done */
    property int value: 0

    objectName: "ImportProgressDialog"
    title: qsTr("Import Project")
    modality: Qt.ApplicationModal
    flags: Qt.Dialog | Qt.CustomizeWindowHint | Qt.WindowTitleHint
    standardButtons: Controls.DialogButtonBox.NoButton
    preferredWidth: 360

    onRejecting: (check) => {
        check.ok = false
    }

    Ez.Label {
        objectName: "labelProgress"
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: root.label
    }
    Ez.ProgressBar {
        objectName: "progressImport"
        Layout.fillWidth: true
        from: 0
        to: Math.max(1, root.maximum)
        value: root.value
        indeterminate: root.maximum <= 0
    }
}
