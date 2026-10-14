// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "BattlePageWidget"

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A battle page of a troop, as RPG Maker shows it under the troop: its
 * trigger as one line ("Switch [0001:xxx] ON, Turns [3x+1]"), "..." to
 * define it (BattleConditionDialog.qml), then its commands (the battle
 * commands). The page is found by the index of its troop and its own, in
 * a database window's copy (session) or in the project.
 *
 *   BattlePageEditor { session: view.session; troopIndex: 0; pageIndex: 1 }
 */
ColumnLayout {
    id: root

    /** The copy of a database window (Ez.DatabaseSession), null: the project */
    property var session: null
    property int troopIndex: -1
    property int pageIndex: -1
    /** The command list (its keys, its line selected) */
    readonly property alias commandList: commands
    readonly property alias commandEditor: editor
    /** The trigger of the page */
    readonly property alias battlePage: page

    /** Reads the page again: the troop changed (its monsters, its pages) */
    function refresh() {
        page.refresh()
        editor.showTroopPage(troopIndex, pageIndex)
    }

    spacing: Ez.Metrics.rowSpacing

    Ez.BattlePage {
        id: page
        projectData: Ez.ProjectData
        session: root.session
        troop: root.troopIndex
        page: root.pageIndex
    }
    Ez.CommandListEditor {
        id: editor
        projectData: Ez.ProjectData
        session: root.session
        battle: true
    }
    onTroopIndexChanged: Qt.callLater(refresh)
    onPageIndexChanged: Qt.callLater(refresh)
    onSessionChanged: Qt.callLater(refresh)
    Component.onCompleted: editor.showTroopPage(troopIndex, pageIndex)

    RowLayout {
        spacing: Ez.Metrics.columnSpacing
        Layout.fillWidth: true

        Ez.Label {
            objectName: "labelTrigger"
            text: qsTr("Trigger")
            font: Ez.Metrics.font
        }
        Ez.TextField {
            objectName: "lineTrigger"
            readOnly: true
            text: page.triggerText
            // the whole text when the line is too short for it
            Layout.fillWidth: true
            Controls.ToolTip.visible: hovered && text !== ""
            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
            Controls.ToolTip.text: text
            onTextChanged: cursorPosition = 0
        }
        Ez.FieldButton {
            objectName: "pushTrigger"
            enabled: page.valid
            Controls.ToolTip.visible: hovered
            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
            Controls.ToolTip.text: qsTr("Define Battle Event Triggers")
            onClicked: page.editTrigger(this)
        }
    }

    CommandList {
        id: commands
        editor: editor
        Layout.fillWidth: true
        Layout.fillHeight: true
    }
}
