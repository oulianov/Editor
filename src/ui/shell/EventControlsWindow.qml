// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

// The battle pages of the project's first troop in their QML editor
// (BattlePageEditor: the trigger, its window, the command list), for the
// scripts (tests/scripts/battle_pages.txt: "qml-open EventControlsWindow");
// what they change stays in memory

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

Ez.DialogWindow {
    id: root

    readonly property var database: Ez.ProjectData.database()

    objectName: "qmlEventControls"
    title: "Ez event controls"
    preferredWidth: 520
    height: 420

    Controls.TabBar {
        id: tabs
        objectName: "tabPages"
        Layout.fillWidth: true
        font: Ez.Metrics.font

        Repeater {
            model: root.database ? root.database.size("troops/0/pages") : 0
            Controls.TabButton {
                required property int index
                text: String(index + 1)
                width: Math.max(implicitWidth, 60)
                font: Ez.Metrics.font
            }
        }
    }

    BattlePageEditor {
        troopIndex: 0
        pageIndex: tabs.currentIndex
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: 300
    }
}
