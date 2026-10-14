// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "RunGameDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Test play without an EasyRPG Player (TestPlay::launch): download the one
 * of easyrpg.org for this computer, or locate one (then kept in the
 * settings). Accepted with the program in `player`.
 */
Ez.DialogWindow {
    id: root

    /** The program chosen, once accepted */
    property string player: ""

    objectName: "playerMissing"
    title: qsTr("Test Play")
    preferredWidth: 380
    standardButtons: Controls.DialogButtonBox.Cancel
    buttons: [
        Ez.Button {
            objectName: "downloadPlayer"
            text: qsTranslate("ExportDialog", "Download EasyRPG Player %1").arg(download.version)
            enabled: !download.busy
            Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.ActionRole
            onClicked: download.start()
        },
        Ez.Button {
            objectName: "locatePlayer"
            text: qsTr("Locate...")
            enabled: !download.busy
            Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.ActionRole
            onClicked: playerDialog.open()
        }
    ]

    RowLayout {
        spacing: Ez.Metrics.dialogMargin
        Layout.fillWidth: true

        // as the warnings of MessageWindow
        Kirigami.Icon {
            source: "dialog-warning"
            implicitWidth: Kirigami.Units.iconSizes.medium
            implicitHeight: Kirigami.Units.iconSizes.medium
            Layout.alignment: Qt.AlignTop
        }
        Ez.Label {
            objectName: "messageText"
            text: qsTr("The EasyRPG Player was not found.\n\nPut it next to the Editor or in the PATH, or locate its program.")
            wrapMode: Text.Wrap
            font: Ez.Metrics.font
            Layout.fillWidth: true
            Layout.preferredWidth: 340
        }
    }
    Ez.DownloadBar {
        download: download
        // the window's Cancel stops it
        cancelButton: false
    }

    Ez.PlayerDownload {
        id: download
        window: root
        onFinished: program => {
            if (program !== "") {
                root.player = program
                root.accept()
            }
        }
    }

    Dialogs.FileDialog {
        id: playerDialog
        title: qsTr("Locate the EasyRPG Player")
        currentFolder: Ez.Editor.playerFolder()
        onAccepted: {
            const program = Ez.Editor.executableOf(selectedFile)
            if (program === "") {
                notProgram.text = qsTr("%1 is not a program.").arg(Ez.Editor.localPath(selectedFile))
                notProgram.open()
                return
            }
            // as chosen in the settings
            Ez.Editor.playerPath = program
            root.player = program
            root.accept()
        }
    }
    Ez.MessageDialog {
        id: notProgram
        title: qsTr("Test Play")
    }
}
