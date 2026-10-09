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

class EventCommandList;
class QWidget;

namespace EventBranchHelpers {

/** Rebuilds only branch markers, retaining each surviving branch's commands.
 * Nothing is changed if deletion is declined or the existing block is invalid. */
bool apply(EventCommandList& commands, const lcf::rpg::EventCommand& edited,
	lcf::rpg::EventCommand::Code closing_code,
	const std::vector<lcf::rpg::EventCommand>& markers, QWidget* parent,
	bool initial_branch, bool match_first_parameter = false);

}
