// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "SettingsDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQuick.Dialogs as Dialogs
import org.easyrpg.editor as Ez

/**
 * The options of the Editor (Ez.Editor), written on OK. The controls are
 * given the options once, not bound to them: writing one option tells all
 * the bindings to Ez.Editor to read them again, which would undo the
 * choices not written yet.
 */
Ez.DialogWindow {
    id: root

    objectName: "SettingsDialog"
    title: qsTr("Settings")
    preferredWidth: 380
    // not closed while the Player is downloaded for it
    acceptable: !download.busy

    Component.onCompleted: {
        language.currentIndex = Math.max(0, language.indexOfValue(Ez.Editor.language))
        reloadExternal.checked = Ez.Editor.reloadExternalChanges
        searchCache.checked = Ez.Editor.searchCache
        rtpPath.text = Ez.Editor.rtpPath
        playerPath.text = Ez.Editor.playerPath
        scriptEditor.text = Ez.Editor.scriptEditor
        animateTiles.checked = Ez.Editor.animateTiles
        commandColors.checked = Ez.Editor.commandColors
        commandStripes.checked = Ez.Editor.commandStripes
    }

    onAccepted: {
        // all read before the first is written; the language last, the
        // windows show it at once
        let rtp = rtpPath.text.trim()
        if (rtp !== "" && !rtp.endsWith("/")) {
            rtp += "/"
        }
        const reload = reloadExternal.checked
        const cache = searchCache.checked
        const player = playerPath.text.trim()
        const editor = scriptEditor.text.trim()
        const animate = animateTiles.checked
        const colors = commandColors.checked
        const stripes = commandStripes.checked
        const code = language.currentValue
        Ez.Editor.reloadExternalChanges = reload
        Ez.Editor.searchCache = cache
        Ez.Editor.rtpPath = rtp
        Ez.Editor.playerPath = player
        Ez.Editor.scriptEditor = editor
        Ez.Editor.animateTiles = animate
        Ez.Editor.commandColors = colors
        Ez.Editor.commandStripes = stripes
        Ez.Editor.language = code
    }

    Ez.Section {
        title: qsTr("General")
        Ez.Form {
            Ez.FormLabel {
                text: qsTr("Language")
            }
            Ez.ComboBox {
                id: language
                objectName: "comboLanguage"
                textRole: "name"
                valueRole: "code"
                model: [{ code: "", name: qsTr("System Language") }].concat(Ez.Editor.languages)
            }
        }
    }

    Ez.Section {
        title: qsTr("Project")
        Ez.CheckBox {
            id: reloadExternal
            objectName: "checkReloadExternal"
            text: qsTr("rpgsync mode: reload the files other programs change")
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.text: qsTr("For rpgsync and other programs that edit the game while the Editor is open: "
                + "what they change is shown, and merged with your unsaved changes when you save.")
        }
        Ez.CheckBox {
            id: searchCache
            objectName: "checkSearchCache"
            text: qsTr("Keep search results between searches")
        }
    }

    Ez.Section {
        title: qsTr("Paths")
        Ez.Form {
            Ez.FormLabel {
                text: qsTr("RTP folder")
            }
            Ez.PathField {
                id: rtpPath
                fieldName: "lineRtpPath"
                onBrowse: rtpDialog.open()
            }
            Ez.FormLabel {
                text: qsTr("EasyRPG Player")
            }
            // a Player of this computer, or the one of easyrpg.org
            Ez.PathField {
                id: playerPath
                fieldName: "linePlayerPath"
                placeholderText: qsTr("Searched next to the Editor and in the PATH")
                menu: Controls.Menu {
                    Controls.MenuItem {
                        objectName: "choosePlayer"
                        text: qsTr("Choose...")
                        onTriggered: playerDialog.open()
                    }
                    Controls.MenuItem {
                        objectName: "downloadPlayer"
                        text: qsTranslate("ExportDialog", "Download EasyRPG Player %1").arg(download.version)
                        enabled: !download.busy
                        onTriggered: download.start()
                    }
                }
            }
            // where rpgsync scripts open: a URL or a command, with {file} and {line}
            Ez.FormLabel {
                text: qsTr("Script editor")
            }
            Ez.PathField {
                id: scriptEditor
                fieldName: "lineScriptEditor"
                placeholderText: qsTr("The program of the system, at the start of the file")
                toolTip: qsTr("For games edited with rpgsync: Open in Script Editor opens the script at the line "
                    + "of the command. A URL or a command, with {file} and {line}.")
                buttonName: "scriptEditorPresets"
                menu: Controls.Menu {
                    Controls.MenuItem {
                        text: "Visual Studio Code"
                        onTriggered: scriptEditor.text = Ez.Editor.defaultScriptEditor
                    }
                    Controls.MenuItem {
                        text: "Cursor"
                        onTriggered: scriptEditor.text = "cursor://file/{file}:{line}"
                    }
                    Controls.MenuItem {
                        text: qsTr("The program of the system (no line)")
                        onTriggered: scriptEditor.text = ""
                    }
                }
            }
        }
        Ez.DownloadBar {
            download: download
        }
    }

    Ez.Section {
        title: qsTr("Appearance")
        Ez.CheckBox {
            id: animateTiles
            objectName: "checkAnimateTiles"
            text: qsTr("Animate water and animated tiles in the map")
        }
        Ez.CheckBox {
            id: commandColors
            objectName: "checkCommandColors"
            text: qsTr("Color the event commands by kind, as RPG Maker 2003")
        }
        Ez.CheckBox {
            id: commandStripes
            objectName: "checkCommandStripes"
            text: qsTr("Stripe the lines of the event commands")
        }
    }

    ColumnLayout {
        spacing: Ez.Metrics.rowSpacing
        Layout.fillWidth: true

        Ez.Button {
            objectName: "buttonResetLocalState"
            text: qsTr("Reset local state")
            onClicked: {
                Ez.Editor.resetLocalState()
                resetNote.text = qsTr("Local state reset to defaults.")
            }
        }
        Ez.Note {
            id: resetNote
            text: qsTr("Reset window sizes, tabs, selections and scroll positions to their defaults. Game data and preferences are kept.")
        }
    }

    Item {
        Layout.fillHeight: true
    }

    Ez.PlayerDownload {
        id: download
        window: root
        // written on OK, as the other options
        onFinished: program => {
            if (program !== "") {
                playerPath.text = program
            }
        }
    }

    Dialogs.FolderDialog {
        id: rtpDialog
        title: qsTr("Select the RTP folder")
        currentFolder: Ez.Editor.url(rtpPath.text)
        onAccepted: rtpPath.text = Ez.Editor.localPath(selectedFolder) + "/"
    }
    Dialogs.FileDialog {
        id: playerDialog
        title: qsTranslate("RunGameDialog", "Locate the EasyRPG Player")
        currentFolder: Ez.Editor.playerFolder()
        onAccepted: {
            const program = Ez.Editor.executableOf(selectedFile)
            if (program === "") {
                notProgram.text = qsTranslate("RunGameDialog", "%1 is not a program.").arg(Ez.Editor.localPath(selectedFile))
                notProgram.open()
                return
            }
            playerPath.text = program
        }
    }
    Ez.MessageDialog {
        id: notProgram
        title: qsTranslate("RunGameDialog", "Test Play")
    }
}
