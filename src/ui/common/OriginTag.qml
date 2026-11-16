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
 * shows even when the tag is in a greyed control.
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

    visible: shown
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
