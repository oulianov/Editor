// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "NewProjectDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import org.easyrpg.editor as Ez

/**
 * Game Project > New Project: where the game goes (a new folder in the
 * folder of the games), its title and its format. The project is made by
 * the caller, from what the window says once accepted.
 */
Ez.DialogWindow {
    id: root

    /** The folder of the games (ending with "/") */
    property string defDir
    readonly property alias gameFolder: folder.text
    readonly property alias gameTitle: gameTitle.text
    /** Project::Format: RPG Maker 2003, RPG Maker 2000 or EasyRPG */
    readonly property alias format: format.currentIndex
    readonly property int tileSize: tileSize.currentValue ?? 16

    /** The folder of the games chosen (the folder window's choice; scripts give it too) */
    function chooseFolder(path) {
        if (path !== "") {
            defDir = path.endsWith("/") ? path : path + "/"
        }
    }

    objectName: "NewProjectDialog"
    title: qsTr("Create new Project")
    preferredWidth: 400
    // the folder of the game is needed
    acceptable: gameFolder !== ""

    Ez.Section {
        title: qsTr("Path to games folder")
        Ez.PathField {
            fieldName: "lineProjectPath"
            buttonName: "toolProjectPath"
            text: root.defDir
            field.readOnly: true
            buttonIcon: "folder"
            onBrowse: folderDialog.open()
        }
    }
    Ez.Section {
        title: qsTr("Game Folder")
        Ez.TextField {
            id: folder
            objectName: "lineGameFolder"
            Layout.fillWidth: true
        }
    }
    Ez.Section {
        title: qsTr("Game Title")
        Ez.TextField {
            id: gameTitle
            objectName: "lineGameTitle"
            Layout.fillWidth: true
        }
    }
    Ez.Section {
        title: qsTr("Tile Size")
        Ez.ComboBox {
            id: tileSize
            objectName: "comboTileSize"
            // only 16 pixels, as RPG Maker
            enabled: false
            model: [{ text: "16", value: 16 }, { text: "32", value: 32 }, { text: "64", value: 64 }]
            Layout.fillWidth: true
        }
    }
    Ez.Section {
        objectName: "groupFormat"
        title: qsTr("Format")
        Ez.ComboBox {
            id: format
            objectName: "comboFormat"
            model: [{ text: qsTr("RPG Maker 2003"), value: 0 }, { text: qsTr("RPG Maker 2000"), value: 1 },
                    { text: qsTr("EasyRPG (XML)"), value: 2 }]
            Layout.fillWidth: true
        }
    }
    Ez.Section {
        title: qsTr("DB template")
        Ez.ComboBox {
            objectName: "comboDbTemplate"
            model: [{ text: qsTr("Default<EN>"), value: 0 }]
            Layout.fillWidth: true
        }
    }

    Dialogs.FolderDialog {
        id: folderDialog
        title: qsTr("Select destination folder")
        currentFolder: Ez.Editor.url(root.defDir)
        onAccepted: root.chooseFolder(Ez.Editor.localPath(selectedFolder))
    }
}
