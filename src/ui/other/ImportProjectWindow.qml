// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ImportProjectDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs as Dialogs
import org.easyrpg.editor as Ez

/**
 * Game Project > Import Project: an RPG Maker 2000 or 2003 game copied as
 * a new project, in a new folder of the folder of the games. The copy is
 * made by the caller, from what the window says once accepted.
 */
Ez.DialogWindow {
    id: root

    /** The folder of the games (ending with "/") */
    property string defDir
    /** The game copied (ending with "/") */
    property string sourceFolder
    readonly property alias gameFolder: folder.text
    readonly property alias convertXyz: convertXyz.checked

    /** The game to copy (the folder window's choice; scripts give it too): refused when it is not one */
    function chooseSource(path) {
        if (path === "") {
            return
        }
        if (!projects.isLegacyProject(path)) {
            error.open()
            return
        }
        if (path.endsWith("/")) {
            path = path.substring(0, path.length - 1)
        }
        sourceFolder = path + "/"
        folder.text = path.substring(path.lastIndexOf("/") + 1)
    }
    /** The folder of the games chosen */
    function chooseFolder(path) {
        if (path !== "") {
            defDir = path.endsWith("/") ? path : path + "/"
        }
    }

    objectName: "ImportProjectDialog"
    title: qsTr("Import Project")
    preferredWidth: 400
    acceptable: gameFolder !== ""

    Ez.Section {
        title: qsTr("Path to source project")
        Ez.PathField {
            fieldName: "lineSourcePath"
            buttonName: "toolSourcePath"
            text: root.sourceFolder
            field.readOnly: true
            buttonIcon: "folder"
            onBrowse: sourceDialog.open()
        }
    }
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
    Ez.CheckBox {
        id: convertXyz
        objectName: "checkConvertXYZ"
        text: qsTr("Convert XYZ images to PNG")
    }

    // what a folder is
    Ez.ProjectListModel {
        id: projects
    }
    Ez.MessageDialog {
        id: error
        objectName: "errorDialog"
        title: qsTr("Error")
        text: qsTr("This folder does not contain a valid RPG Maker project")
    }
    Dialogs.FolderDialog {
        id: sourceDialog
        title: qsTr("Select origin folder")
        currentFolder: Ez.Editor.url(root.defDir)
        onAccepted: root.chooseSource(Ez.Editor.localPath(selectedFolder))
    }
    Dialogs.FolderDialog {
        id: folderDialog
        title: qsTr("Select destination folder")
        currentFolder: Ez.Editor.url(root.defDir)
        onAccepted: root.chooseFolder(Ez.Editor.localPath(selectedFolder))
    }
}
