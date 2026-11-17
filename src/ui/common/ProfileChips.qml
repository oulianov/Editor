// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The profile of a game as small chips (GameProfile::chipsOf): its engine,
 * then each add-on it turns on with its tag (Ez.Origins: M, E, P). The
 * tooltip of a chip says what it is and what runs the game. When the chips
 * are wider than the room they have, the add-ons show their tag alone,
 * their name in their tooltip.
 *
 * For the scripts, each chip is named "chip" and its key (chipEngine,
 * chipManiac, chipEasyrpg, chipDynRPG...), its text the name it shows in
 * full; text: all of them, joined by commas.
 */
Row {
    id: root

    /** The chips: {key, text, tag, tip, page} */
    property var chips: []
    /** The width they may take, beyond which the add-ons show their tag alone (0: any) */
    property real room: 0
    /** A click on a chip asks for where it is changed (clicked: a page of Project Settings, or "database-system") */
    property bool clickable: false
    /** On the highlighted line of a list: in its colors */
    property bool highlighted: false

    readonly property string text: chips.map(chip => chip.text).join(", ")
    /** The width of the chips with their names */
    readonly property real fullWidth: {
        let width = Math.max(0, chips.length - 1) * spacing
        for (const chip of chips) {
            width += Math.ceil(textMetrics.advanceWidth(chip.text)) + 2 * chipPadding
            if (chip.tag !== "") {
                width += tagWidth(chip.tag) + spacing
            }
        }
        return width
    }
    readonly property bool compact: room > 0 && fullWidth > room

    readonly property int chipPadding: 4
    readonly property color textColor: highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
    // the origin whose color each tag has
    readonly property var tagOrigins: ({ M: "maniac", E: "easyrpg", P: "dynrpg" })

    signal clicked(string page)

    function tagWidth(tag) {
        return Math.max(Math.ceil(tagMetrics.height) + 1, Math.ceil(tagMetrics.advanceWidth(tag)) + 6)
    }

    spacing: 3

    FontMetrics {
        id: textMetrics
        font.family: Ez.Metrics.font.family
        font.pointSize: Ez.Metrics.font.pointSize * 0.9
    }
    FontMetrics {
        id: tagMetrics
        font.family: Ez.Metrics.font.family
        font.pointSize: Ez.Metrics.font.pointSize * 0.85
        font.bold: true
    }

    Repeater {
        model: root.chips
        delegate: Rectangle {
            id: chip

            required property var modelData
            readonly property string text: modelData.text
            /** Its tag alone: an add-on, the chips compact */
            readonly property bool tagOnly: root.compact && modelData.tag !== ""

            objectName: "chip" + modelData.key.charAt(0).toUpperCase() + modelData.key.slice(1)
            implicitWidth: content.implicitWidth + (tagOnly ? 0 : 2 * root.chipPadding)
            implicitHeight: Math.ceil(textMetrics.height) + 2
            radius: 3
            color: "transparent"
            border.width: tagOnly ? 0 : 1
            border.color: root.highlighted ? root.textColor : Ez.Metrics.sectionBorderColor

            Row {
                id: content
                anchors.centerIn: parent
                spacing: root.spacing

                Text {
                    visible: !chip.tagOnly
                    anchors.verticalCenter: parent.verticalCenter
                    text: chip.modelData.text
                    font: textMetrics.font
                    color: root.textColor
                }
                Rectangle {
                    visible: chip.modelData.tag !== ""
                    anchors.verticalCenter: parent.verticalCenter
                    width: root.tagWidth(chip.modelData.tag)
                    height: Math.ceil(tagMetrics.height) + 1
                    radius: 3
                    color: Ez.Origins.color(root.tagOrigins[chip.modelData.tag] ?? "")

                    Text {
                        anchors.centerIn: parent
                        text: chip.modelData.tag
                        font: tagMetrics.font
                        color: Ez.Origins.dark ? "#1b1b1b" : "#ffffff"
                    }
                }
            }

            HoverHandler {
                id: hover
                cursorShape: root.clickable ? Qt.PointingHandCursor : Qt.ArrowCursor
            }
            TapHandler {
                enabled: root.clickable
                onTapped: root.clicked(chip.modelData.page)
            }
            Controls.ToolTip.visible: hover.hovered
            Controls.ToolTip.text: chip.modelData.tip
            Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
    }
}
