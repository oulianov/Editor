/*
 * This file is part of EasyRPG Editor.
 *
 * EasyRPG Editor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Editor is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Editor. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QColor>
#include <cstdint>

class QPalette;

/**
 * Colors of the lines of the event command list, by kind of command.
 *
 * RPG Maker 2003 1.08 (RPG2003.EXE, TScriptListBox line drawer at 0x4B5728)
 * only draws comments in green (0x008000) and every other line in black.
 * The other colors follow the later RPG Maker 2003 editor (orange actor
 * commands, red variables, blue flow...), extended to every command by kind.
 */
namespace CommandColors {

enum class Kind : std::uint8_t {
	Text,       ///< messages: the normal text color
	End,        ///< the empty "<>" line closing a block or the page
	Progress,   ///< switches, variables, timers
	Flow,       ///< branches, loops, labels, calls
	Block,      ///< the lines of a block: else, end, choice cases...
	Party,      ///< money, items, party, heroes, monsters
	Movement,   ///< teleports, events, sprites, move routes
	Screen,     ///< screen effects, pictures, wait
	Audio,      ///< music, sounds, movies
	Scene,      ///< shops, menus, system settings, map settings
	Comment,
	Extension,  ///< EasyRPG and Maniac Patch commands
	Count
};

/** The kind of a command code (Text for unknown codes); O(1) */
Kind kind(int code);

/**
 * The color of a kind for this palette, from a light or a dark palette with
 * a WCAG AA contrast. Text and End return the palette's text color.
 */
QColor color(Kind kind, const QPalette& palette);

/** The background of every other line: the base tinted blue-gray, as RPG Maker 2003 */
QColor stripeColor(const QPalette& palette);

}

/** The color of the line of a command for this palette */
QColor commandColor(int code, const QPalette& palette);
