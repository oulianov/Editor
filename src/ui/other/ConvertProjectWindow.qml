// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ConvertProjectDialog"

import QtQuick
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * RPG Maker 2003's question when an RPG Maker 2000 project is converted:
 * what the conversion changes, where the backup goes, and the choices of
 * the EasyRPG Player. The conversion itself is the caller's
 * (ProjectConvert::to2003), with the options of the window once accepted.
 */
Ez.DialogWindow {
    id: root

    /** The maps of the project, rewritten */
    property int maps: 0
    /** The folder the files of RPG Maker 2000 are copied to */
    property string backupPath
    readonly property alias keepBattle: keepBattle.checked
    readonly property alias keepExperience: keepExperience.checked

    objectName: "ConvertProjectDialog"
    title: qsTr("Convert to RPG Maker 2003")
    preferredWidth: 520
    standardButtons: Controls.DialogButtonBox.Cancel
    buttons: [
        Ez.Button {
            objectName: "buttonConvert"
            text: qsTr("Convert")
            Controls.DialogButtonBox.buttonRole: Controls.DialogButtonBox.AcceptRole
        }
    ]

    // RPG Maker 2003's own question
    Ez.Note {
        objectName: "labelQuestion"
        text: qsTr("Game data will be converted from RPG Maker 2000 format to RPG Maker 2003 format. "
            + "This cannot be reversed. Are you sure you want to proceed?")
        font.bold: true
    }
    Ez.Note {
        objectName: "labelChanges"
        textFormat: Text.RichText
        text: "<ul style=\"margin-left: 0px; -qt-list-indent: 1;\"><li>%1</li><li>%2</li><li>%3</li><li>%4</li></ul>"
            .arg(qsTr("The database gets the data of RPG Maker 2003: the battle commands of RPG Maker 2000 "
                + "(Attack, Skill, Defend, Item, Escape) for every hero, the 32 battler animations of "
                + "RPG Maker 2003 (graphics of its RTP), a System2 graphic, the menu of RPG Maker 2000 "
                + "and the terms that RPG Maker 2000 does not have (Status, Row, Order...)."))
            .arg(qsTr("The game uses the battles of RPG Maker 2003 (side view, active time), its experience "
                + "curve and its level limit (99)."))
            .arg(qsTr("The database, the map tree and the maps (%1) are written in the format of RPG Maker 2003: "
                + "RPG Maker 2000 cannot open the project anymore.").arg(root.maps))
            .arg(qsTr("RPG_RT.exe, the runtime of RPG Maker 2000, cannot run the game anymore: it is moved to the "
                + "backup. Copy the RPG_RT.exe of RPG Maker 2003 into the game folder to play the game with it; "
                + "the EasyRPG Player plays it without."))
    }
    Ez.Section {
        title: qsTr("Backup")
        Ez.Note {
            objectName: "labelBackup"
            text: qsTr("Before the conversion, RPG_RT.ldb, RPG_RT.lmt and the maps are copied to a "
                + "new folder of the game:\n%1").arg(root.backupPath)
        }
    }
    Ez.Section {
        title: qsTr("EasyRPG Player")
        Ez.Note {
            text: qsTr("Settings of the EasyRPG Player only: RPG_RT and RPG Maker 2003 ignore them.")
        }
        Ez.CheckBox {
            id: keepBattle
            objectName: "checkKeepBattle"
            text: qsTr("Keep the battles of RPG Maker 2000")
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.text: qsTr("Front view battles, with the Fight, Auto and Escape commands of the party")
        }
        Ez.CheckBox {
            id: keepExperience
            objectName: "checkKeepExperience"
            text: qsTr("Keep the experience curve of RPG Maker 2000")
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.text: qsTr("The heroes need the experience of RPG Maker 2000 to reach each level")
        }
    }
}
