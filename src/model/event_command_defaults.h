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

#include <lcf/rpg/eventcommand.h>
#include <vector>

namespace EventCommandDefaults {
	using Code = lcf::rpg::EventCommand::Code;

	/** A new command of this kind, with the lines that close its block
	 *  (branch ends, choice options, loop end...), indented from indent. */
	std::vector<lcf::rpg::EventCommand> make(Code code, int indent = 0);

	/** The most common parameters of a command in RPG Maker games */
	std::vector<int32_t> parameters(Code code);

	/** First and one past last line of the command at index, with the lines
	 *  of its block. first == last when the line cannot be removed alone
	 *  (a branch end, a choice option...). */
	std::pair<int, int> range(const std::vector<lcf::rpg::EventCommand>& commands, int index);

	/** Whether the line only marks the structure of a block */
	bool isStructural(Code code);
}
