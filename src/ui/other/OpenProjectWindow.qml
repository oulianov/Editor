// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "OpenProjectDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQuick.Dialogs as Dialogs
import org.easyrpg.editor as Ez

/**
 * Game Project > Open Project: the projects of the folder of the games
 * (an Ez.ProjectListModel), their settings and their deletion, and Import
 * for a game of RPG Maker. Its result: 1 to open the project selected,
 * 2 for Import.
 */
Ez.DialogWindow {
    id: root

    property Ez.ProjectListModel projects: null

    /**
     * The folder of the games chosen (the folder window's choice, a path
     * typed; scripts give it too): one that is no folder lists nothing
     */
    function chooseFolder(path) {
        path = path.trim()
        if (Qt.platform.os === "windows") {
            path = path.replace(/\\/g, "/")
        }
        projects.folder = path === "" || path.endsWith("/") ? path : path + "/"
        // the field shows it as listed, even when it was already (a "/" typed away)
        pathField.text = Qt.binding(() => root.projects ? root.projects.folder : "")
    }
    /** Closes the window for Import (result 2) */
    function importProject() {
        result = 2
        closed = true
        close()
    }
    function editSettings() {
        const row = projects.currentRow
        const notice = projects.openNotice(row)
        if (notice !== "") {
            message.show(notice)
            return
        }
        const error = projects.editSettings(row)
        if (error !== "") {
            message.show(error)
        }
    }
    function deleteProject() {
        const row = projects.currentRow
        const notice = projects.openNotice(row)
        if (notice !== "") {
            message.show(notice)
            return
        }
        const error = dialogScope.run(() => projects.trash(row))
        if (error) {
            message.show(error)
        }
    }

    objectName: "OpenProjectDialog"
    title: qsTr("Open existing Project")
    preferredWidth: 640
    preferredHeight: 480
    // a project selected, listed
    acceptable: projects !== null && !projects.busy && projects.currentRow >= 0

    buttons: [
        Ez.Button {
            objectName: "pushImport"
            text: qsTr("Import...")
            Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.ActionRole
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.text: qsTr("Copy an RPG Maker 2000/2003 game into a new project")
            onClicked: root.importProject()
        },
        Ez.Button {
            objectName: "pushRename"
            text: qsTr("Settings...")
            enabled: root.acceptable
            Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.ActionRole
            onClicked: root.editSettings()
        },
        Ez.Button {
            objectName: "pushDelete"
            text: qsTr("Delete")
            enabled: root.acceptable
            Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.ActionRole
            onClicked: root.deleteProject()
        }
    ]

    Ez.ColumnList {
        listName: "tableProjects"
        model: root.projects
        currentRow: root.projects ? root.projects.currentRow : -1
        enabled: root.projects !== null && !root.projects.busy
        columns: [
            { title: qsTr("Folder"), role: "folder", icon: "folder" },
            { title: qsTr("Game Title"), role: "title" },
        ]
        Layout.fillWidth: true
        Layout.fillHeight: true
        onPicked: row => root.projects.currentRow = row
        onActivated: root.accept()

        Controls.BusyIndicator {
            anchors.centerIn: parent
            running: root.projects !== null && root.projects.busy
            visible: running
        }
        // a path typed that is no folder: nothing listed
        Ez.Note {
            objectName: "noteNoFolder"
            anchors.centerIn: parent
            width: Math.min(parent.width - 2 * Ez.Metrics.dialogMargin, 360)
            horizontalAlignment: Text.AlignHCenter
            color: Ez.Metrics.mutedTextColor
            visible: root.projects !== null && !root.projects.folderExists
            text: qsTr("This folder doesn't exist.")
        }
    }
    Ez.Section {
        title: qsTr("Path to games folder")
        Ez.PathField {
            id: pathField
            fieldName: "lineProjectPath"
            buttonName: "toolProjectPath"
            text: root.projects ? root.projects.folder : ""
            buttonIcon: "folder"
            onBrowse: folderDialog.open()
            // listed once the typing pauses, and on Return
            onTextEdited: typing.restart()
            onEditingFinished: {
                typing.stop()
                root.chooseFolder(text)
            }
        }
    }

    // the path typed, listed as it is (Return or leaving the field ends it with "/")
    Timer {
        id: typing
        interval: 400
        onTriggered: root.projects.folder = pathField.text
    }
    Ez.DialogScope {
        id: dialogScope
        window: root
    }
    Ez.MessageDialog {
        id: message
        objectName: "messageDialog"
        title: root.title

        function show(text) {
            message.text = text
            open()
        }
    }
    Dialogs.FolderDialog {
        id: folderDialog
        title: qsTr("Select destination folder")
        currentFolder: Ez.Editor.url(root.projects ? root.projects.folder : "")
        onAccepted: root.chooseFolder(Ez.Editor.localPath(selectedFolder))
    }
}
