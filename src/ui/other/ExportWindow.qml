// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ExportDialog"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQuick.Dialogs as Dialogs
import org.easyrpg.editor as Ez

/**
 * Game Project > Export Game: writes the game ready to play for each
 * platform checked (Windows, macOS, Linux, the web), like RPG Maker's
 * Create Gamedisk. One page, read from top to bottom: the game, the
 * platforms with their folder and Player, the options, then Export with
 * its log and progress. The settings and the work are an ExportController's;
 * the settings are kept in the project when the window closes.
 */
Ez.DialogWindow {
    id: root

    required property Ez.ExportController controller
    /** Closed once the export or the download stops (asked while it ran) */
    property bool closeWhenDone: false
    /** The macOS platform is checked: its application can be customized */
    property bool macOSChecked: false
    /** The widths of the columns of the platforms, the widest of the rows */
    property real checkWidth: 0
    property real launcherWidth: 0

    objectName: "ExportDialog"
    title: qsTr("Export Game")
    preferredWidth: 760
    // its buttons are in its footer, with Open Folder
    standardButtons: Controls.DialogButtonBox.NoButton

    onRejecting: check => {
        if (root.controller.busy) {
            // closes when the work has stopped
            check.ok = false
            root.closeWhenDone = true
            root.controller.cancel()
            return
        }
        if (root.controller.dirty) {
            root.controller.save()
        }
    }
    Connections {
        target: root.controller
        function onFinished() { root.workDone() }
        function onFailed() { root.workDone() }
        function onDownloadFinished() { root.workDone() }
    }
    function workDone() {
        if (root.closeWhenDone) {
            Qt.callLater(root.reject)
        }
    }
    /** A path as the controller keeps them: relative to the game folder, or absolute */
    function absolute(path) {
        return path.startsWith("/") || /^[A-Za-z]:/.test(path) ? path : root.controller.sourceFolder + "/" + path
    }

    Ez.DialogScope {
        id: dialogs
        window: root
    }

    // 1. The game
    Ez.Section {
        title: qsTr("Game")
        Ez.Form {
            Ez.FormLabel {
                text: qsTr("Title:")
            }
            Ez.TextField {
                objectName: "gameTitle"
                text: root.controller.title
                onTextEdited: root.controller.title = text
                Layout.fillWidth: true
            }
            Ez.FormLabel {
                text: qsTr("Version:")
            }
            Ez.TextField {
                objectName: "gameVersion"
                text: root.controller.version
                placeholderText: "1.0"
                onTextEdited: root.controller.version = text
                Layout.fillWidth: true
            }
            Ez.FormLabel {
                text: qsTr("Icon:")
            }
            RowLayout {
                spacing: Ez.Metrics.columnSpacing
                Layout.fillWidth: true
                Ez.PathField {
                    id: icon
                    fieldName: "iconPath"
                    buttonName: "chooseIcon"
                    text: root.controller.icon
                    placeholderText: qsTr("A PNG image")
                    toolTip: qsTr("The icon of the program (Windows), of the application (macOS), "
                        + "of the desktop entry (Linux) and of the web page")
                    onBrowse: iconDialog.open()
                    onTextEdited: root.controller.icon = icon.text
                }
                // the icon chosen, as high as its field (none: nothing shown)
                Image {
                    objectName: "iconPreview"
                    visible: root.controller.iconPath !== "" && status === Image.Ready
                    source: root.controller.iconPath !== "" ? Ez.Editor.url(root.controller.iconPath) : ""
                    fillMode: Image.PreserveAspectFit
                    sourceSize: Qt.size(height * 2, height * 2)
                    Layout.preferredWidth: icon.field.height
                    Layout.preferredHeight: icon.field.height
                }
            }
        }
    }

    // 2. The platforms: one line each, its folder and its Player
    Ez.Section {
        title: qsTr("Platforms")
        Repeater {
            model: root.controller.platforms
            delegate: RowLayout {
                id: platform
                required property var model
                required property int index
                readonly property string platformId: model.id
                readonly property bool on: model.enabled

                spacing: Ez.Metrics.columnSpacing
                Layout.fillWidth: true

                // the columns as wide as their widest line; later, not while the line is laid out
                function fit() {
                    root.checkWidth = Math.max(root.checkWidth, check.implicitWidth)
                    root.launcherWidth = Math.max(root.launcherWidth, launcher.implicitWidth)
                }

                Binding {
                    target: root
                    property: "macOSChecked"
                    value: platform.on
                    when: platform.platformId === "macos"
                }

                Ez.CheckBox {
                    id: check
                    objectName: "export_" + platform.platformId
                    text: platform.model.name
                    Layout.preferredWidth: root.checkWidth
                    onImplicitWidthChanged: Qt.callLater(platform.fit)
                    onToggled: platform.model.enabled = checked
                    Binding on checked {
                        value: platform.on
                    }
                }
                // an unchecked platform keeps its settings, greyed
                Ez.PathField {
                    fieldName: "path_" + platform.platformId
                    buttonName: "browse_" + platform.platformId
                    enabled: platform.on
                    text: platform.model.path
                    toolTip: qsTr("Emptied and written at each export. A relative path starts from the game folder.")
                    buttonToolTip: qsTr("Choose the folder")
                    onTextEdited: platform.model.path = text
                    onBrowse: {
                        folderDialog.currentFolder = Ez.Editor.url(root.absolute(platform.model.path))
                        folderDialog.apply = path => { platform.model.path = path }
                        folderDialog.open()
                    }
                }
                // the Player used: a pull-down to change it, get it or locate it
                Ez.FieldButton {
                    id: launcher
                    objectName: "launcher_" + platform.platformId
                    text: platform.model.launcherText
                    icon.name: platform.model.launcherReady ? "" : "dialog-warning"
                    enabled: platform.on && !root.controller.busy
                    Layout.preferredWidth: root.launcherWidth
                    onImplicitWidthChanged: Qt.callLater(platform.fit)
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.text: platform.model.launcherToolTip
                    menu: Controls.Menu {
                        id: launchers

                        Instantiator {
                            model: platform.model.launcherChoices
                            delegate: Controls.MenuItem {
                                required property string modelData
                                text: root.controller.describeLauncher(modelData)
                                checkable: true
                                checked: modelData === platform.model.launcher
                                onTriggered: platform.model.launcher = modelData
                            }
                            onObjectAdded: (index, object) => launchers.insertItem(index, object)
                            onObjectRemoved: (index, object) => launchers.removeItem(object)
                        }
                        Controls.MenuSeparator {
                            visible: platform.model.launcherChoices.length > 0
                        }
                        Controls.MenuItem {
                            objectName: "download_" + platform.platformId
                            text: qsTr("Download EasyRPG Player %1").arg(root.controller.playerVersion)
                            enabled: !root.controller.busy
                            onTriggered: dialogs.run(() => root.controller.platforms.download(platform.index))
                        }
                        Controls.MenuItem {
                            objectName: "locate_" + platform.platformId
                            text: qsTr("Locate...")
                            onTriggered: {
                                // an application is a file in the dialogs of macOS
                                const folder = Qt.platform.os === "osx" ? platform.platformId === "web"
                                    : platform.platformId === "web" || platform.platformId === "macos"
                                const title = qsTr("EasyRPG Player for %1").arg(platform.model.name)
                                const apply = path => {
                                    const error = root.controller.platforms.locate(platform.index, path)
                                    if (error !== "") {
                                        problem.text = error
                                        problem.open()
                                    }
                                }
                                const dialog = folder ? launcherFolderDialog : launcherFileDialog
                                dialog.title = title
                                dialog.apply = apply
                                dialog.open()
                            }
                        }
                    }
                }
            }
        }
    }

    // 3. The options, one line each: the details are in their tooltips
    Ez.Section {
        title: qsTr("Options")
        Ez.CheckBox {
            objectName: "includeRtp"
            text: qsTr("Include the RTP files the game uses")
            onToggled: root.controller.includeRtp = checked
            Binding on checked { value: root.controller.includeRtp }
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.text: qsTr("The graphics, music and sounds of the RTP that the game uses are copied with it: "
                + "players do not need the RTP. Needed for the web.")
        }
        Ez.CheckBox {
            objectName: "cleanDatabase"
            text: qsTr("Remove unused database entries")
            onToggled: root.controller.cleanDatabase = checked
            Binding on checked { value: root.controller.cleanDatabase }
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.text: qsTr("Entries nothing uses are emptied; the others keep their numbers. "
                + "Skipped for games with Maniac Patch, EasyRPG or DynRPG commands. Your project does not change.")
        }
        Ez.CheckBox {
            objectName: "protectFiles"
            text: qsTr("Prevent opening the game in editors")
            enabled: root.controller.canProtect
            onToggled: root.controller.protect = checked
            Binding on checked { value: root.controller.protect }
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.text: root.controller.canProtect
                ? qsTr("The map tree is written so that only the EasyRPG Player reads it: "
                    + "RPG Maker and the EasyRPG Editor cannot open the exported game. A deterrent, not encryption.")
                : qsTr("Only for games in the RPG Maker format")
        }
        Ez.CheckBox {
            objectName: "makeZip"
            text: qsTr("Make a zip of each export")
            onToggled: root.controller.zip = checked
            Binding on checked { value: root.controller.zip }
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.text: qsTr("Next to each folder; for the web, the zip to upload to itch.io")
        }
        Ez.CheckBox {
            objectName: "customizeApp"
            text: qsTr("Give the macOS application the game's title and icon")
            enabled: root.macOSChecked
            onToggled: root.controller.customizeApp = checked
            Binding on checked { value: root.controller.customizeApp }
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.text: qsTr("The application is signed again for this computer: other Macs then ask to allow it.")
        }
    }

    // 4. Export: what it does, then the buttons
    Ez.LogView {
        textArea.objectName: "exportLog"
        lines: root.controller.log
        placeholderText: qsTr("The export reports here what it copies and adds.")
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 100
    }
    Ez.ProgressBar {
        objectName: "exportProgress"
        from: 0
        to: Math.max(1, root.controller.progressMaximum)
        value: root.controller.progress
        indeterminate: root.controller.busy && root.controller.progressMaximum === 0
        Layout.fillWidth: true
    }
    RowLayout {
        spacing: Ez.Metrics.dialogButtonSpacing
        Layout.fillWidth: true

        Ez.DialogButton {
            objectName: "openFolder"
            text: qsTr("Open Folder")
            enabled: !root.controller.busy && root.controller.folders.length > 0
            onClicked: root.controller.openFolder()
        }
        Item {
            Layout.fillWidth: true
        }
        Ez.DialogButton {
            objectName: "exportButton"
            text: qsTr("Export")
            enabled: !root.controller.busy
            highlighted: true
            // the questions (folders replaced) over this window
            onClicked: dialogs.run(() => root.controller.start())
        }
        Ez.DialogButton {
            objectName: "cancelExport"
            text: qsTr("Cancel")
            enabled: root.controller.busy
            onClicked: root.controller.cancel()
        }
        Ez.DialogButton {
            objectName: "closeButton"
            text: qsTr("Close")
            onClicked: root.reject()
        }
    }

    Dialogs.FolderDialog {
        id: folderDialog
        property var apply: null
        title: qsTr("Export Folder")
        onAccepted: apply(Ez.Editor.localPath(selectedFolder))
    }
    Dialogs.FolderDialog {
        id: launcherFolderDialog
        property var apply: null
        currentFolder: Ez.Editor.playerFolder()
        onAccepted: apply(Ez.Editor.localPath(selectedFolder))
    }
    Dialogs.FileDialog {
        id: launcherFileDialog
        property var apply: null
        currentFolder: Ez.Editor.playerFolder()
        onAccepted: apply(Ez.Editor.localPath(selectedFile))
    }
    Dialogs.FileDialog {
        id: iconDialog
        title: qsTr("Icon")
        currentFolder: Ez.Editor.url(root.absolute(root.controller.icon !== "" ? root.controller.icon : "."))
        nameFilters: [qsTr("PNG images (*.png)")]
        onAccepted: {
            icon.text = Ez.Editor.localPath(selectedFile)
            root.controller.icon = icon.text
        }
    }
    Ez.MessageDialog {
        id: problem
        title: qsTr("Export Game")
    }
}
