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

#include "event_branch_helpers.h"
#include "model/event_command_defaults.h"
#include "model/event_command_list.h"
#include <QMessageBox>
#include <QObject>
#include <algorithm>

bool EventBranchHelpers::apply(EventCommandList& commands, const lcf::rpg::EventCommand& edited,
	lcf::rpg::EventCommand::Code closing_code,
	const std::vector<lcf::rpg::EventCommand>& markers, QWidget* parent,
	bool initial_branch, bool match_first_parameter)
{
	using Cmd = lcf::rpg::EventCommand::Code;
	using Command = lcf::rpg::EventCommand;
	auto& list = commands.commands();
	const auto [start, end] = EventCommandDefaults::range(list, commands.index());
	const int indent = list[start].indent;
	const bool has_closing = end > start + 1 && list[end - 1].code == static_cast<int>(closing_code)
		&& list[end - 1].indent == indent;
	if (initial_branch && !has_closing) {
		QMessageBox::warning(parent, QObject::tr("Cannot edit command branches"),
			QObject::tr("The command block has no matching closing line. Its branches were left unchanged."));
		return false;
	}

	struct Branch {
		Command marker;
		int first;
		int last;
		bool retained = false;
	};
	std::vector<Branch> old;
	const int contents_end = has_closing ? end - 1 : end;
	int initial_end = contents_end;
	for (int i = start + 1; i < contents_end; ++i) {
		if (list[i].indent != indent || !EventCommandDefaults::isStructural(static_cast<Cmd>(list[i].code))
			|| list[i].code == static_cast<int>(Cmd::END)) {
			continue;
		}
		if (old.empty()) {
			initial_end = i;
		} else {
			old.back().last = i;
		}
		old.push_back({list[i], i + 1, contents_end, false});
	}
	if (!initial_branch && initial_end > start + 1) {
		QMessageBox::warning(parent, QObject::tr("Cannot edit command branches"),
			QObject::tr("The command block contains commands outside its branch markers. Its branches were left unchanged."));
		return false;
	}

	std::vector<Command> rebuilt{edited};
	if (initial_branch) {
		rebuilt.insert(rebuilt.end(), list.begin() + start + 1, list.begin() + initial_end);
	}
	for (const auto& marker : markers) {
		auto existing = std::find_if(old.begin(), old.end(), [&](const Branch& branch) {
			return !branch.retained && branch.marker.code == marker.code
				&& (!match_first_parameter || (!branch.marker.parameters.empty() && !marker.parameters.empty()
					&& branch.marker.parameters[0] == marker.parameters[0]));
		});
		if (existing != old.end()) {
			existing->retained = true;
			auto retained_marker = existing->marker;
			retained_marker.indent = marker.indent;
			retained_marker.string = marker.string;
			std::vector<int32_t> parameters(retained_marker.parameters.begin(), retained_marker.parameters.end());
			parameters.resize(std::max(parameters.size(), static_cast<size_t>(marker.parameters.size())));
			std::copy(marker.parameters.begin(), marker.parameters.end(), parameters.begin());
			retained_marker.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
			rebuilt.push_back(retained_marker);
			rebuilt.insert(rebuilt.end(), list.begin() + existing->first, list.begin() + existing->last);
		} else {
			rebuilt.push_back(marker);
			const auto empty = EventCommandDefaults::make(Cmd::END, indent + 1);
			rebuilt.insert(rebuilt.end(), empty.begin(), empty.end());
		}
	}

	const bool deleting_commands = std::any_of(old.begin(), old.end(), [&](const Branch& branch) {
		return !branch.retained && std::any_of(list.begin() + branch.first, list.begin() + branch.last,
			[](const Command& command) { return command.code != static_cast<int>(Cmd::END); });
	});
	if (deleting_commands && QMessageBox::question(parent, QObject::tr("Delete branch commands?"),
		QObject::tr("The branches being removed contain event commands. Delete those commands?"),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
		return false;
	}
	if (has_closing && (initial_branch || !markers.empty())) {
		rebuilt.push_back(list[end - 1]);
	} else if (!markers.empty()) {
		const auto closing = EventCommandDefaults::make(closing_code, indent);
		rebuilt.insert(rebuilt.end(), closing.begin(), closing.end());
	}

	// Assemble and confirm first: Cancel must never touch the shared page vector.
	list.erase(list.begin() + start, list.begin() + end);
	list.insert(list.begin() + start, rebuilt.begin(), rebuilt.end());
	return true;
}
