// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ResourceManagerDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Tools > Resource Manager, as in RPG Maker: the files of each folder of the
 * game and of the RTP, with a preview, Import (images get their transparent
 * color chosen and their size checked), Export, Rename and Delete (the
 * game's files only), and the folder in the file manager. The files and
 * their operations are an Ez.ResourceLibraryModel's.
 */
Ez.DialogWindow {
    id: root

    /** The folder of the game open, and the RTP */
    property string gamePath
    property string rtpPath
    readonly property alias library: library

    readonly property bool image: library.kind === Ez.ResourceLibraryModel.Image
    readonly property bool audio: library.kind === Ez.ResourceLibraryModel.Music
        || library.kind === Ez.ResourceLibraryModel.Sound

    /** Copies files into the folder shown (the file window's choice; scripts give it too) */
    function importFiles(paths) {
        if (!library.canImport || paths.length === 0) {
            return
        }
        dialogScope.run(() => {
            for (const result of library.importFiles(paths)) {
                dialogs.report(qsTr("Import"), result)
            }
        })
    }
    /** Writes a copy of the file shown */
    function exportTo(path) {
        dialogs.report(qsTr("Export"), library.exportCurrent(path))
    }
    function renameTo(name) {
        player.stop()
        dialogs.report(qsTr("Rename"), library.renameCurrent(name))
    }
    function deleteFile() {
        if (!library.canDelete) {
            return
        }
        dialogScope.run(() => dialogs.report(qsTr("Delete"), library.deleteCurrent()))
    }
    function play() {
        if (library.currentPath !== "") {
            player.play(library.currentPath)
        }
    }

    objectName: "ResourceManagerDialog"
    title: qsTr("Resource Manager")
    standardButtons: Controls.DialogButtonBox.Close | Controls.DialogButtonBox.Help

    Component.onCompleted: library.refresh()
    onClosing: player.stop()

    Ez.ResourceLibraryModel {
        id: library
        gamePath: root.gamePath
        rtpPath: root.rtpPath
        onFolderChanged: player.stop()
        onAboutToRemoveFile: player.stop()
    }
    Ez.ResourceDialogs {
        id: dialogs
        library: library
        window: root
    }
    Ez.DialogScope {
        id: dialogScope
        window: root
    }
    Ez.AudioPlayer {
        id: player
    }
    Connections {
        target: root.buttonBox
        function onHelpRequested() {
            Ez.Editor.help("resource-manager", root.contentItem)
        }
    }

    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true
        Layout.fillHeight: true

        // the folders, as wide as their longest name (italic: the game has none yet)
        Ez.ColumnList {
            id: folders
            listName: "listFolders"
            model: library.folders
            currentRow: library.folder
            visibleLines: 19
            columns: [{
                role: "name",
                icon: "folder",
                sample: root.longestFolder(),
                italic: line => line.missing
            }]
            Layout.fillHeight: true
            onPicked: row => library.folder = row
        }

        Controls.SplitView {
            id: split
            orientation: Qt.Horizontal
            Layout.fillWidth: true
            Layout.fillHeight: true

            // the files of the folder
            ColumnLayout {
                spacing: Ez.Metrics.rowSpacing
                // 2/5 for the files at first, the preview gets the rest
                Controls.SplitView.preferredWidth: split.width * 2 / 5
                Controls.SplitView.minimumWidth: 160

                Ez.TextField {
                    objectName: "filterFiles"
                    placeholderText: qsTr("Filter")
                    Layout.fillWidth: true
                    onTextChanged: library.filter = text
                }
                Ez.ColumnList {
                    id: files
                    listName: "listFiles"
                    model: library
                    currentRow: library.currentRow
                    columns: [
                        { title: qsTr("File"), role: "name", dimmed: line => line.origin === Ez.ResourceLibraryModel.Rtp },
                        { title: qsTranslate("JukeboxDialog", "Format"), role: "format", sample: "MIDI" },
                        { title: qsTranslate("JukeboxDialog", "Source"), sample: qsTranslate("JukeboxDialog", "Game"),
                          text: line => line.origin === Ez.ResourceLibraryModel.Rtp ? qsTranslate("JukeboxDialog", "RTP")
                              : qsTranslate("JukeboxDialog", "Game"),
                          dimmed: line => line.origin === Ez.ResourceLibraryModel.Rtp },
                    ]
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    onPicked: row => library.currentRow = row
                    onActivated: row => {
                        if (root.audio) {
                            root.play()
                        }
                    }
                    onDeletePressed: root.deleteFile()
                }
                Ez.Label {
                    objectName: "labelCount"
                    text: library.countText
                    font: Ez.Metrics.font
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            // what can be done, and the file shown
            ColumnLayout {
                spacing: Ez.Metrics.rowSpacing
                Controls.SplitView.fillWidth: true
                Controls.SplitView.minimumWidth: actions.implicitWidth

                RowLayout {
                    id: actions
                    spacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true

                    ColumnLayout {
                        spacing: Ez.Metrics.rowSpacing
                        Layout.alignment: Qt.AlignTop

                        Ez.Button {
                            objectName: "pushImport"
                            text: qsTr("Import...")
                            enabled: library.canImport
                            Layout.fillWidth: true
                            onClicked: importDialog.open()
                        }
                        Ez.Button {
                            objectName: "pushExport"
                            text: qsTr("Export...")
                            enabled: library.canExport
                            Layout.fillWidth: true
                            onClicked: exportDialog.open()
                        }
                        Ez.Button {
                            objectName: "pushRename"
                            text: qsTr("Rename...")
                            enabled: library.canDelete
                            Layout.fillWidth: true
                            onClicked: renameDialog.open()
                        }
                        Ez.Button {
                            objectName: "pushDelete"
                            text: qsTr("Delete")
                            enabled: library.canDelete
                            Layout.fillWidth: true
                            Controls.ToolTip.visible: hovered && library.canExport && library.currentIsRtp
                            Controls.ToolTip.text: qsTr("Files of the RTP cannot be deleted.")
                            onClicked: root.deleteFile()
                        }
                    }
                    ColumnLayout {
                        spacing: Ez.Metrics.rowSpacing
                        Layout.alignment: Qt.AlignTop

                        Ez.Section {
                            objectName: "groupImportAs"
                            title: qsTr("Import as")
                            enabled: library.canImport && root.image
                            Layout.fillWidth: false

                            Ez.RadioButton {
                                objectName: "radioImportPng"
                                text: qsTr("PNG Image")
                                checked: !library.importXyz
                                onToggled: library.importXyz = !checked
                            }
                            Ez.RadioButton {
                                objectName: "radioImportXyz"
                                text: qsTr("XYZ Image")
                                checked: library.importXyz
                                onToggled: library.importXyz = checked
                            }
                        }
                        Ez.Button {
                            objectName: "pushOpenFolder"
                            text: qsTr("Open Folder")
                            enabled: root.gamePath !== "" || root.rtpPath !== ""
                            Layout.fillWidth: true
                            Controls.ToolTip.visible: hovered
                            Controls.ToolTip.text: qsTr("Show the folder of the file in the file manager")
                            onClicked: library.openFolder()
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }

                // images in a view, to zoom on them as in the pickers; the
                // others say what they are
                Ez.ZoomView {
                    id: preview
                    objectName: "previewView"
                    visible: root.image
                    image: library.previewImage
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                Rectangle {
                    objectName: "preview"
                    visible: !root.image
                    Kirigami.Theme.colorSet: Kirigami.Theme.View
                    Kirigami.Theme.inherit: false
                    color: Kirigami.Theme.backgroundColor
                    border.color: Ez.Metrics.sectionBorderColor
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Ez.Label {
                        objectName: "previewImage"
                        anchors.fill: parent
                        anchors.margins: Ez.Metrics.sectionPadding
                        text: library.previewNote
                        font: Ez.Metrics.font
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                // what the file is, the zoom buttons on its right (as in the pickers)
                RowLayout {
                    spacing: Ez.Metrics.columnSpacing
                    Layout.fillWidth: true

                    Ez.Label {
                        objectName: "previewInfo"
                        text: library.info.join("\n")
                        font: Ez.Metrics.font
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        // the width comes from the window
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignTop
                    }
                    Ez.ZoomButtons {
                        view: preview
                        visible: root.image
                        Layout.alignment: Qt.AlignTop
                    }
                }
                RowLayout {
                    visible: root.audio
                    spacing: Ez.Metrics.columnSpacing

                    Ez.Button {
                        objectName: "pushPlay"
                        text: qsTr("Play")
                        enabled: library.canExport
                        onClicked: root.play()
                    }
                    Ez.Button {
                        objectName: "pushStop"
                        text: qsTr("Stop")
                        enabled: player.state !== Ez.AudioPlayer.Stopped
                        onClicked: player.stop()
                    }
                    Ez.Label {
                        objectName: "labelPlayer"
                        text: player.errorString
                        font: Ez.Metrics.font
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }

    /** The widest name of the folders (for the width of their list) */
    function longestFolder() {
        const folders = library.folders
        let longest = ""
        for (let row = 0; row < folders.rowCount(); ++row) {
            const name = folders.data(folders.index(row, 0))
            if (name.length > longest.length) {
                longest = name
            }
        }
        // italic is a bit wider
        return longest + " "
    }

    Dialogs.FileDialog {
        id: importDialog
        title: qsTr("Import into %1").arg(library.folderName)
        fileMode: Dialogs.FileDialog.OpenFiles
        nameFilters: library.importFilters()
        currentFolder: Ez.Editor.url(library.importDirectory())
        onAccepted: root.importFiles(selectedFiles.map(file => Ez.Editor.localPath(file)))
    }
    Dialogs.FileDialog {
        id: exportDialog
        readonly property string fileName: library.currentPath.substring(library.currentPath.lastIndexOf("/") + 1)
        title: qsTr("Export %1").arg(fileName)
        fileMode: Dialogs.FileDialog.SaveFile
        currentFolder: Ez.Editor.url(library.exportDirectory())
        selectedFile: Ez.Editor.url(library.exportDirectory() + "/" + fileName)
        onAccepted: root.exportTo(Ez.Editor.localPath(selectedFile))
    }

    // the new name of the game's file shown, its extension kept
    Ez.Dialog {
        id: renameDialog
        objectName: "renameDialog"
        title: qsTr("Rename")
        modal: true
        anchors.centerIn: Controls.Overlay.overlay
        standardButtons: Controls.Dialog.Ok | Controls.Dialog.Cancel
        font: Ez.Metrics.font
        onAboutToShow: {
            newName.text = library.currentName
            newName.selectAll()
            newName.forceActiveFocus()
        }
        onAccepted: root.renameTo(newName.text)

        Ez.TextField {
            id: newName
            objectName: "lineNewName"
            implicitWidth: 240
            onAccepted: renameDialog.accept()
        }
    }
}
