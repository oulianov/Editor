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

#include "event_command_list.h"
#include <cassert>
#include "common/dbstring.h"

EventCommandList::EventCommandList(std::vector<lcf::rpg::EventCommand>& commands, int current_index) :
	m_commands(&commands), m_current_index(current_index)
{
	// no-op
}

std::vector<lcf::rpg::EventCommand>& EventCommandList::commands() {
	assert(m_commands);
	return *m_commands;
}

const std::vector<lcf::rpg::EventCommand>& EventCommandList::commands() const {
	assert(m_commands);
	return *m_commands;
}

lcf::rpg::EventCommand& EventCommandList::command() {
	assert(m_commands);
	return (*m_commands)[m_current_index];
}

const lcf::rpg::EventCommand &EventCommandList::command() const {
	assert(m_commands);
	return (*m_commands)[m_current_index];
}

size_t EventCommandList::size() const {
	assert(m_commands);
	return m_commands->size();
}

int EventCommandList::index() const {
	return m_current_index;
}

void EventCommandList::setLines(lcf::rpg::EventCommand::Code next_code, const QStringList& lines) {
	assert(m_commands);
	auto& cmds = *m_commands;
	auto& first = cmds[m_current_index];
	first.string = lcf::DBString(ToDBString(lines.value(0)));

	// remove the old continuation lines, then add the new ones
	size_t end = m_current_index + 1;
	while (end < cmds.size() && cmds[end].code == static_cast<int32_t>(next_code)) {
		++end;
	}
	cmds.erase(cmds.begin() + m_current_index + 1, cmds.begin() + end);

	std::vector<lcf::rpg::EventCommand> next;
	for (int i = 1; i < lines.size(); ++i) {
		lcf::rpg::EventCommand cmd;
		cmd.code = static_cast<int32_t>(next_code);
		cmd.indent = first.indent;
		cmd.string = lcf::DBString(ToDBString(lines[i]));
		next.push_back(cmd);
	}
	cmds.insert(cmds.begin() + m_current_index + 1, next.begin(), next.end());
}
