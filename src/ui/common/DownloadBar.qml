// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ExportDialog"

import QtQuick
import QtQuick.Layouts
import org.easyrpg.editor as Ez

/**
 * A download of the EasyRPG Player running (Ez.PlayerDownload): what is
 * downloaded, its progress, and a button to stop it. Shown while it runs.
 */
ColumnLayout {
    id: root

    /** The Ez.PlayerDownload shown */
    property Ez.PlayerDownload download: null
    /** The Cancel button of the bar (a window whose Cancel stops it too: none) */
    property bool cancelButton: true

    visible: download !== null && download.busy
    spacing: Ez.Metrics.rowSpacing
    Layout.fillWidth: true

    Ez.Label {
        objectName: "downloadText"
        text: qsTr("Downloading %1").arg(root.download ? root.download.fileName : "")
        font: Ez.Metrics.font
        elide: Text.ElideMiddle
        Layout.fillWidth: true
        // the width comes from the window
        Layout.preferredWidth: 1
    }
    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true

        Ez.ProgressBar {
            objectName: "downloadProgress"
            from: 0
            to: Math.max(1, root.download ? root.download.progressMaximum : 1)
            value: root.download ? root.download.progress : 0
            // the size is not known before the first bytes
            indeterminate: root.download !== null && root.download.progressMaximum === 0
            Layout.fillWidth: true
        }
        Ez.Button {
            objectName: "cancelDownload"
            text: qsTr("Cancel")
            visible: root.cancelButton
            onClicked: root.download.cancel()
        }
    }
}
