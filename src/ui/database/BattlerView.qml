// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "StateWidget"

import QtQuick
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * A battler animation of the database (RPG Maker 2003's battle graphics of
 * a hero or a class) in a 48x48 frame: one pose still (pose: its first
 * frame, as the Hero Status page shows it), or its poses in turn, each
 * walking (pose -1). A pose drawn with a
 * battle animation says so.
 *
 *   BattlerView { database: page.database; battler: page.entry.num("battler_animation") }
 */
Rectangle {
    id: root

    /** The database edited (a JsonView) */
    property var database: null
    /** The ID of the battler animation (0: none) */
    property int battler: 0
    /** The pose shown still (0 the first), -1: the poses in turn */
    property int pose: -1
    /** The frame drawn bigger */
    property real zoom: 1

    readonly property var poses: database && battler > 0 && battler <= battlers.count
                                 ? database.list("battleranimations/" + (battler - 1) + "/poses") : null
    readonly property var battlers: database ? database.list("battleranimations") : null
    /** The pose drawn: the one chosen, else the one of the demo */
    readonly property int shown: pose >= 0 ? pose : demoPose
    // read again when the poses change
    property int revision: 0
    readonly property bool hasPose: !!poses && revision >= 0 && shown >= 0 && shown < poses.count
    readonly property bool battleAnimation: hasPose && poses.num(shown + "/animation_type") === 1

    // the demo: the 12 poses of RPG Maker, a walk of 4 frames each, 4 times
    property int demoPose: 0
    property int step: 0
    readonly property int frame: pose >= 0 ? 0 : [1, 0, 1, 2][step % 4]

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    color: Kirigami.Theme.backgroundColor
    border.color: Ez.Metrics.sectionBorderColor
    radius: 2
    implicitWidth: 48 * zoom + 4
    implicitHeight: 48 * zoom + 4

    Connections {
        target: root.poses
        ignoreUnknownSignals: true
        function onDataChanged() { root.revision++ }
        function onCountChanged() { root.revision++ }
    }
    Timer {
        running: root.pose < 0 && !!root.poses && root.visible
        interval: 200
        repeat: true
        onTriggered: {
            root.step = (root.step + 1) % 16
            if (root.step === 0) {
                root.demoPose = (root.demoPose + 1) % 12
            }
        }
    }
    onBattlerChanged: {
        demoPose = 0
        step = 0
    }

    Ez.SpritePaintedItem {
        id: sprite
        anchors.centerIn: parent
        width: 48 * root.zoom
        height: 48 * root.zoom
        visible: root.hasPose && !root.battleAnimation
        projectData: Ez.ProjectData
        directory: "BattleCharSet"
        filename: visible && root.revision >= 0 ? root.poses.str(root.shown + "/battler_name") : ""
        cellWidth: 48
        cellHeight: 48
        gridColumns: 3
        cellIndex: visible && root.revision >= 0 ? root.poses.num(root.shown + "/battler_index") * 3 + root.frame : 0
    }
    // a file missing from the game and the RTP (an RTP of other file
    // names): its name, not an empty box
    Ez.Label {
        anchors.fill: parent
        anchors.margins: 2
        visible: sprite.visible && sprite.filename !== "" && !sprite.loaded
        text: sprite.filename
        color: Ez.Metrics.mutedTextColor
        wrapMode: Text.Wrap
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font: Ez.Metrics.font
    }
    Ez.Label {
        anchors.fill: parent
        anchors.margins: 2
        visible: root.battleAnimation
        text: qsTr("Battle animation")
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        font: Ez.Metrics.font
    }
}
