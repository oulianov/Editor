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
#include <functional>
#include <lcf/rpg/eventcommand.h>
#include <vector>

class DialogService;

/**
 * The branches of a block (the choices of Show Choices, the else of a
 * Conditional Branch, the handlers of a battle, a shop, an inn) rebuilt
 * after its command was edited: the branch markers change, the commands of
 * each branch kept stay with it.
 */
namespace EventBranches {
	using Command = lcf::rpg::EventCommand;
	using Code = Command::Code;

	/**
	 * Rebuilds only the branch markers of the block of commands[index],
	 * retaining each surviving branch's commands. Nothing is changed if
	 * the deletion of commands is declined or the existing block is invalid
	 * (the dialogs tell why).
	 *
	 * @param edited The command of the block as edited
	 * @param closing The line closing the block
	 * @param markers The branch markers wanted, in order
	 * @param initial_branch The commands right after the command are a
	 *  branch of their own (a Conditional Branch's "then")
	 * @param match_first_parameter A marker is kept only for one with the
	 *  same first parameter (the choices of Show Choices)
	 * @return Whether the block was rebuilt
	 */
	bool apply(std::vector<Command>& commands, int index, const Command& edited, Code closing,
		const std::vector<Command>& markers, DialogService& dialogs, bool initial_branch,
		bool match_first_parameter = false);

	/** The handlers of the branches a battle, a shop or an inn command asks for */
	std::vector<Code> handlers(const Command& command);
	/** The line closing the block of a command with handlers (END for others) */
	Code closingOf(Code code);
}
