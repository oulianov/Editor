// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as Controls
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * The tag of where a command, an option or a field comes from (Ez.Origins):
 * its letter in its color, beside its text. Shown only when the game tags
 * that origin (none for what its engine runs: no size then). Its tooltip
 * says what runs it, or what the game needs and where to turn it on; it
 * shows even when the tag is in a greyed control. Greyed with that control,
 * and for what the game does not run.
 *
 * warning: what is already there and that the game does not run (a command
 * of the Maniac Patch in a game without it): an exclamation mark after the
 * letter, the tooltip saying the game does not run it.
 */
Rectangle {
    id: root

    /** An origin of Ez.Origins ("maniac"...) */
    property string origin
    /** What is there is not run: the tag says so */
    property bool warning: false
    /** The tooltip; by default from Ez.Origins */
    property string toolTip: warning ? Ez.Origins.warning(origin) : Ez.Origins.tip(origin)

    /** Shown: the game tags the origin, or does not run what is there (a
     *  warning of RPG Maker 2003's engine in a 2000 game: "!" alone) */
    readonly property bool shown: Ez.Origins.tagged(origin) || (warning && origin !== "" && !Ez.Origins.available(origin))
    /** What it shows, for the scripts (expect NAME M!): its letter, "!" for a warning; empty while not shown */
    readonly property string text: shown ? letter.text : ""
    /**
     * Greyed as what it tags: in a greyed control, or for what the game
     * does not run (the control beside it greyed); a warning stays bright
     */
    readonly property bool greyed: !warning && (!enabled || (origin !== "" && !Ez.Origins.available(origin)))
    // the opacity of the items it is in (a greyed check box, an entry that
    // cannot be chosen): greyed once, not twice
    readonly property real inheritedOpacity: {
        let opacity = 1
        for (let item = parent; item; item = item.parent) {
            opacity *= item.opacity
        }
        return opacity
    }

    visible: shown
    opacity: greyed ? Math.min(1, Ez.Metrics.disabledOpacity / Math.max(inheritedOpacity, Ez.Metrics.disabledOpacity)) : 1
    implicitWidth: shown ? Math.max(implicitHeight, letter.implicitWidth + 6) : 0
    implicitHeight: shown ? letter.implicitHeight + 1 : 0
    radius: 3
    color: Ez.Origins.color(origin)
    border.width: warning ? 1 : 0
    border.color: Kirigami.Theme.negativeTextColor

    Text {
        id: letter
        anchors.centerIn: parent
        text: Ez.Origins.letter(root.origin) + (root.warning ? "!" : "")
        font.family: Ez.Metrics.font.family
        font.pointSize: Ez.Metrics.font.pointSize * 0.85
        font.bold: true
        color: Ez.Origins.dark ? "#1b1b1b" : "#ffffff"
    }

    HoverHandler {
        id: hover
    }

    Controls.ToolTip.visible: hover.hovered && root.toolTip !== ""
    Controls.ToolTip.text: root.toolTip
    Controls.ToolTip.delay: Kirigami.Units.toolTipDelay
}
