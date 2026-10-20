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

#include <optional>
#include <vector>
#include <QString>
#include <lcf/rpg/eventcommand.h>
#include <lcf/rpg/moveroute.h>
#include <lcf/rpg/troop.h>
#include <lcf/rpg/trooppagecondition.h>

class ProjectData;
class QWindow;

/**
 * Opens the window of an event command (its editor), the palette of the
 * commands to insert, and the windows of the routes, modal over a window
 * of either interface: the command lists of QML (CommandListEditor) call
 * them.
 *
 * The editors of the commands are the QML ones of CommandEditorRegistry
 * (hand-written, made from the schema, or the raw one).
 */
namespace CommandEditing {
	using Code = lcf::rpg::EventCommand::Code;

	/** Where the command edited is */
	struct Context {
		/** The lines of its list: the editors of a command of several
		 *  lines (a message, a choice, a branch...) change them */
		std::vector<lcf::rpg::EventCommand>* commands = nullptr;
		/** Its line in commands */
		int index = -1;
		/** Inserted: Cancel removes it (the raw editor warns only on old commands) */
		bool isNew = false;
		/** A command of a troop's battle page */
		bool battle = false;
		/** The monsters of the troop of the battle page, -1 for no troop */
		int troopMembers = -1;
	};

	/**
	 * Edits a command in its window, over a window
	 *
	 * @param command The command, changed in place on OK and Apply
	 * @return false when cancelled; true for a command without a window
	 */
	bool edit(QWindow* over, ProjectData& project, lcf::rpg::EventCommand& command, const Context& context = {});

	/** The parameters of a command as numbers (Add raw, Edit raw); false when cancelled */
	bool editRaw(QWindow* over, ProjectData& project, lcf::rpg::EventCommand& command, bool show_warning);

	/**
	 * The kind of command to insert, in the window of the commands
	 * (CommandPalette.qml): those of a map event, or of a battle (a map
	 * event's last entry opens these in a window of their own)
	 *
	 * @return The code chosen, nothing when cancelled
	 */
	std::optional<Code> pickCommand(QWindow* over, ProjectData& project, bool battle);

	/** The custom route of an event page (MoveRouteEditor.qml); false when cancelled */
	bool editRoute(QWindow* over, lcf::rpg::MoveRoute& route);

	/** A Move Event command: its target, frequency and route; false when cancelled */
	bool editMoveEvent(QWindow* over, ProjectData& project, lcf::rpg::EventCommand& command);

	/**
	 * The triggers of a battle page (BattleConditionDialog.qml): the
	 * monsters of its troop, the battle commands of the database
	 *
	 * @param condition Changed on OK
	 * @return false when cancelled
	 */
	bool editBattleTrigger(QWindow* over, ProjectData& project, const lcf::rpg::Troop& troop,
		lcf::rpg::TroopPageCondition& condition);

	/** The window of a command, as its button names it */
	QString title(Code code);
}
