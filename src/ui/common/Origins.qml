// SPDX-FileCopyrightText: EasyRPG Editor Authors
// SPDX-License-Identifier: GPL-3.0-or-later

pragma Singleton

import QtQuick
import org.kde.kirigami as Kirigami
import org.easyrpg.editor as Ez

/**
 * Where a command, an option or a field comes from when it is not one of the
 * game's own engine, whether the game runs it, and how the windows show it:
 * a small tag (Ez.OriginTag: a letter in a color), the control greyed when
 * the game does not run it, a tooltip saying what it needs and where to turn
 * it on (Ez.OriginTip). Read from Ez.Game: a binding that calls these
 * functions follows the game's setup as it changes.
 *
 * The origins:
 * - "rpg2003": an option of RPG Maker 2003's commands. Part of a 2003 game
 *   (no tag); in a 2000 game, EasyRPG Player runs it with its RPG Maker 2003
 *   commands (tag E).
 * - "maniac": the Maniac Patch (tag M).
 * - "easyrpg": EasyRPG Player's extensions (tag E): its own commands, true
 *   color images...
 * - "player": what EasyRPG Player reads and RPG_RT.exe ignores, without the
 *   extensions (tag E, always available): the EasyRPG options of the
 *   database...
 * - "dynrpg", "keypatch", "powermode": the patches of RPG_RT.exe that
 *   EasyRPG Player also runs, DynRPG, the Ineluki Key Patch, Power Mode 2003
 *   (tag P).
 * - "engine2003": RPG Maker 2003's engine itself (its battles, its
 *   pictures...): not in a 2000 game, even with RPG Maker 2003's commands,
 *   until it is converted. No tag: greyed, with its tooltip.
 *
 * A tag is relative to the game: what its engine runs has none.
 */
QtObject {
    id: root

    /** The origins, in the order of the tags */
    readonly property var all: ["rpg2003", "maniac", "easyrpg", "player", "dynrpg", "keypatch", "powermode"]

    /** The dark colors of the theme */
    readonly property bool dark: Kirigami.Theme.backgroundColor.hslLightness < 0.5

    // the key of each patch in Ez.Game.patches
    readonly property var patchKeys: ({ dynrpg: "DynRPG", keypatch: "KeyPatch", powermode: "PowerMode2003" })

    /** The badges of the origins are shown (Editor Settings): else only those
     *  of what the game has and does not run (OriginTag's warning) */
    readonly property bool badges: Ez.Editor.originBadges
    /** What the game does not run is shown greyed (Editor Settings): else
     *  hidden (shown()), but what the game already has */
    readonly property bool unavailableShown: Ez.Editor.unavailableShown

    /** The game open runs what comes from origin ("" or an unknown origin: yes) */
    function available(origin) {
        switch (origin) {
        case "rpg2003": return Ez.Game.commands2k3
        case "engine2003": return Ez.Game.rpg2k3
        case "maniac": return Ez.Game.maniac
        case "easyrpg": return Ez.Game.easyrpg
        case "dynrpg":
        case "keypatch":
        case "powermode": return (Ez.Game.patches[patchKeys[origin]] || 0) > 0
        }
        return true
    }

    /** What comes from origin shows a tag in the game open (not what its
     *  engine runs), while the badges are shown */
    function tagged(origin) {
        if (!badges)
            return false
        if (origin === "rpg2003")
            return !Ez.Game.rpg2k3
        return all.indexOf(origin) >= 0
    }

    /**
     * What comes from origin is shown: the game runs it, or what it does not
     * run is shown greyed (Editor Settings). A command or an option the game
     * already has is shown whatever this says:
     *
     *     visible: Ez.Origins.shown("maniac") || root.isSet("maniacMode")
     */
    function shown(origin) {
        return unavailableShown || available(origin)
    }

    /** The letter of the tag: M (Maniac Patch), E (EasyRPG Player), P (a patch) */
    function letter(origin) {
        switch (origin) {
        case "maniac": return "M"
        case "rpg2003":
        case "easyrpg":
        case "player": return "E"
        case "dynrpg":
        case "keypatch":
        case "powermode": return "P"
        }
        return ""
    }

    /** The color of the tag, as the letter: purple, green, orange (lighter in dark themes) */
    function color(origin) {
        switch (letter(origin)) {
        case "M": return dark ? "#c39bd3" : "#8e44ad"
        case "E": return dark ? "#7dcea0" : "#1e8449"
        case "P": return dark ? "#f5b041" : "#b9770e"
        }
        return Kirigami.Theme.textColor
    }

    /** The name of what the game needs to run it */
    function name(origin) {
        switch (origin) {
        case "rpg2003": return qsTr("RPG Maker 2003 commands")
        case "engine2003": return qsTr("RPG Maker 2003")
        case "maniac": return qsTr("Maniac Patch")
        case "easyrpg": return qsTr("EasyRPG Extensions")
        case "player": return qsTr("EasyRPG Player")
        case "dynrpg": return qsTr("DynRPG")
        case "keypatch": return qsTr("Ineluki Key Patch")
        case "powermode": return qsTr("Power Mode 2003")
        }
        return ""
    }

    /** Where it is turned on: Project Settings › tab › option */
    function place(origin) {
        switch (origin) {
        case "rpg2003": return qsTr("Project Settings › EasyRPG Player › RPG Maker 2003 commands")
        case "engine2003": return qsTr("Project Settings › Engine › Convert to RPG Maker 2003")
        case "maniac": return qsTr("Project Settings › Engine › Maniac Patch")
        case "easyrpg": return qsTr("Project Settings › EasyRPG Player › EasyRPG Extensions")
        case "dynrpg":
        case "keypatch":
        case "powermode": return qsTr("Project Settings › EasyRPG Player › Patches")
        }
        return ""
    }

    /** What it is and what runs it, when the game runs it */
    function about(origin) {
        switch (origin) {
        case "rpg2003": return qsTr("An option of RPG Maker 2003: in this RPG Maker 2000 game, only EasyRPG Player runs it")
        case "maniac": return qsTr("Maniac Patch: runs with its RPG_RT.exe and with EasyRPG Player")
        case "easyrpg": return qsTr("EasyRPG Extensions: runs with EasyRPG Player only")
        case "player": return qsTr("EasyRPG Player only: RPG_RT.exe ignores it")
        case "dynrpg": return qsTr("DynRPG: EasyRPG Player runs only some of its plugins")
        case "keypatch":
        case "powermode": return qsTr("%1: runs with its RPG_RT.exe and with EasyRPG Player").arg(name(origin))
        }
        return ""
    }

    /** What the game needs to run it, and where to turn it on */
    function need(origin) {
        switch (origin) {
        case "rpg2003": return qsTr("Needs RPG Maker 2003's commands in this RPG Maker 2000 game: %1").arg(place(origin))
        case "engine2003": return qsTr("Needs an RPG Maker 2003 game: %1").arg(place(origin))
        case "maniac": return qsTr("Needs the Maniac Patch: %1").arg(place(origin))
        case "easyrpg": return qsTr("Needs the EasyRPG Extensions: %1").arg(place(origin))
        case "dynrpg":
        case "keypatch":
        case "powermode": return qsTr("Needs %1: %2").arg(name(origin)).arg(place(origin))
        }
        return ""
    }

    /** What the game does with what is already there (a command, a value set)
     *  from an origin it does not run */
    function warning(origin) {
        switch (origin) {
        case "rpg2003": return qsTr("Not run in this RPG Maker 2000 game without RPG Maker 2003's commands: %1").arg(place(origin))
        case "engine2003": return qsTr("Not run in an RPG Maker 2000 game: %1").arg(place(origin))
        case "maniac": return qsTr("Not run without the Maniac Patch: %1").arg(place(origin))
        case "easyrpg": return qsTr("Not run without the EasyRPG Extensions: %1").arg(place(origin))
        case "dynrpg":
        case "keypatch":
        case "powermode": return qsTr("Not run without %1: %2").arg(name(origin)).arg(place(origin))
        }
        return ""
    }

    /** The tooltip of a tag or of what it marks: about when the game runs it, else need */
    function tip(origin) {
        return available(origin) ? about(origin) : need(origin)
    }
}
