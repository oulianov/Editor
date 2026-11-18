// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "NewProjectDialog"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import QtQuick.Dialogs as Dialogs
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Game Project > New Project: where the game goes (a new folder in the
 * folder of the games), its title, its format (the engine: RPG Maker 2003
 * or 2000, the template of EasyRPG's XML format being RPG Maker 2000's) and
 * its add-ons, each with its tag: the Maniac Patch, EasyRPG Player's
 * extensions, RPG Maker 2003's commands in a game of RPG Maker 2000
 * (EasyRPG Player only), written in its EasyRPG.ini as Project Settings
 * writes them. When this computer has no RTP folder set up, or no EasyRPG
 * Player, it offers to download them (on by default). The project is made by
 * the caller, from what the window says once accepted.
 */
Ez.DialogWindow {
    id: root

    /** The folder of the games (ending with "/") */
    property string defDir
    /** What each add-on does, by its key in [Patch] (GamePatches): the tooltips of their boxes */
    property var addOnTexts: ({})
    readonly property alias gameFolder: folder.text
    readonly property alias gameTitle: gameTitle.text
    /** Project::Format: RPG Maker 2003, RPG Maker 2000 or EasyRPG */
    readonly property alias format: format.currentIndex
    readonly property int tileSize: tileSize.currentValue ?? 16
    /** The add-ons chosen: their keys in [Patch] of EasyRPG.ini ("Maniac", "EasyRPG", "RPG2k3Commands") */
    readonly property list<string> patches: {
        const keys = []
        for (const box of [maniac, easyrpg, commands2k3]) {
            if (box.enabled && box.checked) {
                keys.push(box.patch)
            }
        }
        return keys
    }

    /** No RTP folder set up / no EasyRPG Player found: offered for download (the caller says) */
    property bool rtpMissing: false
    property bool playerMissing: false
    /** Download them once the project is made (on unless unchecked) */
    readonly property bool downloadRtp: rtpMissing && downloadRtpBox.checked
    readonly property bool downloadPlayer: playerMissing && downloadPlayerBox.checked

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

    Controls.TabBar {
        id: tabs
        objectName: "tabs"
        font: Ez.Metrics.font
        Layout.fillWidth: true
        Controls.TabButton {
            text: qsTr("General")
            width: implicitWidth
        }
        Controls.TabButton {
            text: qsTr("Advanced")
            width: implicitWidth
        }
    }

    StackLayout {
        currentIndex: tabs.currentIndex
        Layout.fillWidth: true

        // General: where the game goes, its title, what this computer lacks to run it
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
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
            // what this computer lacks to run the game: downloaded once it is made
            Ez.Section {
                objectName: "groupDownloads"
                title: qsTr("Downloads")
                visible: root.rtpMissing || root.playerMissing
                Ez.CheckBox {
                    id: downloadRtpBox
                    objectName: "checkDownloadRtp"
                    text: qsTr("Download the EasyRPG RTP")
                    checked: true
                    visible: root.rtpMissing
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    Controls.ToolTip.text: qsTr("No RTP folder is set up: EasyRPG's free RTP, for the games of RPG Maker 2000 and 2003, is downloaded and added to the RTP folders of the Editor Settings.")
                }
                Ez.CheckBox {
                    id: downloadPlayerBox
                    objectName: "checkDownloadPlayer"
                    text: qsTr("Download EasyRPG Player")
                    checked: true
                    visible: root.playerMissing
                    Controls.ToolTip.visible: hovered
                    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
                    Controls.ToolTip.text: qsTr("No EasyRPG Player is found: the one of easyrpg.org is downloaded for test play.")
                }
            }
        }

        // Advanced: its engine, add-ons, database and tiles
        ColumnLayout {
            spacing: Ez.Metrics.sectionSpacing
            Ez.Section {
                objectName: "groupEngine"
                title: qsTr("Engine")
                Ez.ComboBox {
                    id: format
                    objectName: "comboFormat"
                    model: [{ text: qsTr("RPG Maker 2003"), value: 0 }, { text: qsTr("RPG Maker 2000"), value: 1 },
                            { text: qsTr("EasyRPG (XML)"), value: 2 }]
                    Layout.fillWidth: true
                }
            }
            // what the game runs besides its engine: [Patch] of its EasyRPG.ini
            Ez.Section {
                objectName: "groupAddOns"
                title: qsTr("Add-ons")
                AddOn {
                    id: maniac
                    name: "checkManiac"
                    patch: "Maniac"
                    origin: "maniac"
                }
                AddOn {
                    id: easyrpg
                    name: "checkEasyRpg"
                    patch: "EasyRPG"
                    origin: "easyrpg"
                }
                // RPG Maker 2003's are part of its games
                AddOn {
                    id: commands2k3
                    name: "checkRpg2k3Commands"
                    patch: "RPG2k3Commands"
                    origin: "rpg2003"
                    tagOrigin: "player"
                    enabled: root.format !== 0
                    Ez.OriginTip {
                        toolTip: commands2k3.enabled ? "" : qsTr("Part of RPG Maker 2003: for a game of RPG Maker 2000")
                    }
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
            Ez.Section {
                title: qsTr("Tile Size")
                Ez.ComboBox {
                    id: tileSize
                    objectName: "comboTileSize"
                    // only 16 pixels: RPG_RT.exe and EasyRPG Player draw no other size
                    // (the Player's TILE_SIZE is fixed when it is built)
                    enabled: false
                    model: [{ text: "16", value: 16 }, { text: "32", value: 32 }, { text: "64", value: 64 }]
                    Layout.fillWidth: true
                    Ez.OriginTip {
                        toolTip: qsTr("RPG Maker and EasyRPG Player only run tiles of 16 pixels")
                    }
                }
            }
        }
    }

    /**
     * An add-on: its box, named as the tags name it, what it does in its
     * tooltip, then its tag (what runs it). Its tag is the new game's, not
     * the one of the game open.
     */
    component AddOn: RowLayout {
        id: addOn
        /** Its key in [Patch] */
        required property string patch
        /** Its origin (Ez.Origins): its name */
        required property string origin
        /** The origin of its tag, when another one says better what runs it */
        property string tagOrigin: origin
        /** The objectName of its box (checkManiac...) */
        property string name
        property alias checked: box.checked
        default property alias content: box.data
        spacing: Ez.Metrics.columnSpacing
        Ez.CheckBox {
            id: box
            objectName: addOn.name
            text: Ez.Origins.name(addOn.origin)
            Controls.ToolTip.visible: hovered && Controls.ToolTip.text !== ""
            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
            Controls.ToolTip.text: root.addOnTexts[addOn.patch] ?? ""
        }
        Ez.OriginTag {
            origin: addOn.tagOrigin
            toolTip: Ez.Origins.about(addOn.tagOrigin)
        }
    }

    Dialogs.FolderDialog {
        id: folderDialog
        title: qsTr("Select destination folder")
        currentFolder: Ez.Editor.url(root.defDir)
        onAccepted: root.chooseFolder(Ez.Editor.localPath(selectedFolder))
    }
}
