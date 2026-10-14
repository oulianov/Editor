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

#include "event_branches.h"
#include "controller/dialog_service.h"
#include "model/event_command_defaults.h"
#include <QCoreApplication>
#include <algorithm>

namespace {
// the texts in the context QObject, as the command editors translate them
QString branchText(const char* text) {
	return QCoreApplication::translate("QObject", text);
}
}

bool EventBranches::apply(std::vector<Command>& list, int index, const Command& edited, Code closing_code,
	const std::vector<Command>& markers, DialogService& dialogs, bool initial_branch, bool match_first_parameter)
{
	using Cmd = Code;
	const auto [start, end] = EventCommandDefaults::range(list, index);
	const int indent = list[start].indent;
	const bool has_closing = end > start + 1 && list[end - 1].code == static_cast<int>(closing_code)
		&& list[end - 1].indent == indent;
	if (initial_branch && !has_closing) {
		dialogs.warning(branchText(QT_TRANSLATE_NOOP("QObject", "Cannot edit command branches")),
			branchText(QT_TRANSLATE_NOOP("QObject", "The command block has no matching closing line. Its branches were left unchanged.")));
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
		dialogs.warning(branchText(QT_TRANSLATE_NOOP("QObject", "Cannot edit command branches")),
			branchText(QT_TRANSLATE_NOOP("QObject", "The command block contains commands outside its branch markers. Its branches were left unchanged.")));
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
	if (deleting_commands && dialogs.question(branchText(QT_TRANSLATE_NOOP("QObject", "Delete branch commands?")),
		branchText(QT_TRANSLATE_NOOP("QObject", "The branches being removed contain event commands. Delete those commands?")))
			!= DialogService::Answer::Yes) {
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

std::vector<EventBranches::Code> EventBranches::handlers(const Command& cmd) {
	using Cmd = Code;
	switch (static_cast<Cmd>(cmd.code)) {
		case Cmd::EnemyEncounter: {
			const bool escape = cmd.parameters.size() > 3 && cmd.parameters[3] == 2;
			const bool defeat = cmd.parameters.size() > 4 && cmd.parameters[4] == 1;
			std::vector<Cmd> result;
			if (escape || defeat) {
				result.push_back(Cmd::VictoryHandler);
			}
			if (escape) {
				result.push_back(Cmd::EscapeHandler);
			}
			if (defeat) {
				result.push_back(Cmd::DefeatHandler);
			}
			return result;
		}
		case Cmd::OpenShop:
			if (cmd.parameters.size() > 2 && cmd.parameters[2] != 0) {
				return {Cmd::Transaction, Cmd::NoTransaction};
			}
			return {};
		case Cmd::ShowInn:
			if (cmd.parameters.size() > 2 && cmd.parameters[2] != 0) {
				return {Cmd::Stay, Cmd::NoStay};
			}
			return {};
		default:
			return {};
	}
}

EventBranches::Code EventBranches::closingOf(Code code) {
	switch (code) {
		case Code::EnemyEncounter:
			return Code::EndBattle;
		case Code::OpenShop:
			return Code::EndShop;
		case Code::ShowInn:
			return Code::EndInn;
		default:
			return Code::END;
	}
}
