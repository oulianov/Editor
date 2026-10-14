// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Translator: "ClassWidget"
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as Controls
import org.easyrpg.editor as Ez

/**
 * The six parameter curves of a hero or a class as small graphs, two per
 * line, each under its title (names: the objectNames of the graphs). As in
 * RPG Maker, a double-click on a graph opens the Parameter Progression
 * window on it (edit()), where they are changed.
 */
GridLayout {
    id: root

    /** The hero or class (a JsonView) */
    property var entry: null
    /** The highest level and Max HP of the game (99 and 9999, or 50 and 999 in RPG Maker 2000) */
    property int maxLevel: 99
    property int maxHp: 9999
    /** The objectNames of the graphs, the titles above them */
    property var names: []
    property var titles: []
    /** How high a graph is at least */
    property int graphHeight: 60

    /** Opens the Parameter Progression window on a curve (0 Max HP ... 5 Agility) */
    function edit(curve) {
        if (!entry) {
            return
        }
        Ez.Editor.exec("CurveDialog", { parameters: entry.subtree("parameters"), maxLevel: maxLevel, maxHp: maxHp,
                                         curve: curve }, root)
    }

    readonly property var keys: ["maxhp", "maxsp", "attack", "defense", "spirit", "agility"]

    columns: 2
    rowSpacing: Ez.Metrics.rowSpacing
    columnSpacing: Ez.Metrics.columnSpacing

    Repeater {
        model: 6

        ColumnLayout {
            id: graph

            required property int index

            spacing: 2
            // the two columns as wide, whatever their titles
            Layout.preferredWidth: 60
            Layout.fillWidth: true
            Layout.fillHeight: true

            // the title above its graph
            Ez.Label {
                text: root.titles[graph.index] ?? ""
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                font: Ez.Metrics.font
                Layout.fillWidth: true
                background: Rectangle {
                    color: "transparent"
                    border.color: Ez.Metrics.sectionBorderColor
                }
            }
            Ez.CurveCanvas {
                objectName: root.names[graph.index] ?? ""
                values: field.valid && field.value ? field.value : []
                curve: graph.index
                levels: root.maxLevel
                maximum: graph.index === 0 ? root.maxHp : 999
                implicitWidth: 60
                implicitHeight: root.graphHeight
                Layout.fillWidth: true
                Layout.fillHeight: true

                Ez.JsonField {
                    id: field
                    source: root.entry
                    path: "parameters/" + root.keys[graph.index]
                }
                HoverHandler {
                    id: hover
                }
                Controls.ToolTip.visible: hover.hovered
                Controls.ToolTip.delay: 700
                Controls.ToolTip.text: qsTr("Double-click to edit the curve")
                TapHandler {
                    onDoubleTapped: root.edit(graph.index)
                }
            }
        }
    }
}
