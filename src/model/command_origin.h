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

#include <QString>
#include <QStringList>
#include <vector>
#include <lcf/rpg/eventcommand.h>
#include "model/command_schema.h"

/**
 * Where an event command comes from, and what of it a game must run: the
 * origins of Ez.Origins (src/ui/common/Origins.qml):
 * - "rpg2003": RPG Maker 2003's commands and their options (a 2000 game
 *   runs them with the patch RPG2k3Commands, GameSetup::commands2k3);
 * - "engine2003": RPG Maker 2003's engine itself (its battles...), a 2003
 *   game only;
 * - "maniac": the Maniac Patch;
 * - "easyrpg": EasyRPG Player's extensions: its commands, and the calls of
 *   its own plugin (a comment starting with @easyrpg_, which DynRPG runs too);
 * - "dynrpg", "keypatch": a call of a DynRPG plugin (a comment starting with
 *   @), a script of the Ineluki Key Patch (a sound named *.script).
 * (Power Mode 2003 changes what variables 1 to 8 mean: no command tells it.)
 *
 * From the command schema (CommandSchema: the flags of the command, of its
 * fields and of the items chosen) and from the values of the command: the
 * same command of RPG Maker uses the Maniac Patch only with a Maniac mode
 * chosen. The command list tags its lines with it, the checks of a game list
 * what it uses that its setup does not run.
 */
namespace CommandOrigin {
	/** The origin of flags of the schema (of a command, a field, an item), ""
	 *  for none: Maniac or ManiacOnly "maniac", EasyRpg "easyrpg", Engine2003
	 *  "engine2003", Rpg2003 "rpg2003" (the first that holds). NotManiac has
	 *  none: RPG Maker's control of a value */
	QString ofFlags(CommandSchema::Flags flags);
	/** The origin of the command itself (its code): "" for RPG Maker's commands of 2000 */
	QString ofCode(lcf::rpg::EventCommand::Code code);
	/** The origins the command uses, the one of its code first, each once;
	 *  empty for one RPG Maker 2000 runs as it is. A line of a comment
	 *  alone: its next lines are commands of their own (Comment_2) */
	QStringList uses(const lcf::rpg::EventCommand& command);

	/** The game open runs what comes from origin (GameSetup), as Ez.Origins.available */
	bool available(const QString& origin);
	/** What comes from origin has a tag in the game open, as Ez.Origins.tagged:
	 *  not what its engine runs; none while the Editor Settings hide the badges */
	bool tagged(const QString& origin);
	/** What comes from origin is offered, as Ez.Origins.shown: the game runs
	 *  it, or what it does not run is shown greyed (EditorSettings::
	 *  unavailableShown). What a command already has is shown anyway */
	bool shown(const QString& origin);
	/** The game open runs what command uses from origin: available(origin);
	 *  a call of EasyRPG Player's plugin on the first line of a comment also
	 *  with DynRPG */
	bool runs(const QString& origin, const lcf::rpg::EventCommand& command);
	/** The origins of uses() the game open does not run (runs()): what of
	 *  the command it ignores */
	QStringList notRun(const lcf::rpg::EventCommand& command);
}
