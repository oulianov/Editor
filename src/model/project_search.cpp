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

#include "project_search.h"
#include "defines.h"
#include "stringizer.h"
#include "common/dbstring.h"
#include "common/filefinder.h"
#include "model/move_route_codec.h"
#include "model/project.h"
#include "model/project_data.h"

#include <QCoreApplication>
#include <QList>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <algorithm>

namespace {

using Kind = SearchResult::Kind;
/** What refers to the query in a command or a condition, in the order found */
using Kinds = QList<Kind>;

QString typeName(Kind kind)
{
	switch (kind) {
	case Kind::Variable:
		return QCoreApplication::translate("SearchDialog", "Variable");
	case Kind::Switch:
		return QCoreApplication::translate("SearchDialog", "Switch");
	case Kind::Item:
		return QCoreApplication::translate("SearchDialog", "Item");
	case Kind::Text:
		return QCoreApplication::translate("SearchDialog", "Event text");
	case Kind::Name:
		return QCoreApplication::translate("SearchDialog", "Event");
	}
	return {};
}

QString typeNames(const Kinds& kinds)
{
	QStringList names;
	for (Kind kind : kinds) {
		names << typeName(kind);
	}
	return names.join(QStringLiteral(", "));
}

QVariantMap choice(int value, const QString& name)
{
	return {{QStringLiteral("value"), value}, {QStringLiteral("name"), name}};
}

} // namespace

ProjectSearch::ProjectSearch(ProjectData& project, QObject* parent) :
	QObject(parent), m_project(project), m_results(new SearchResultsModel(this)), m_suggestions(new SearchSuggestions(this))
{
	updateSuggestions();
}

ProjectSearch::~ProjectSearch() = default;

void ProjectSearch::setText(const QString& text)
{
	if (text == m_text) {
		return;
	}
	m_text = text;
	emit textChanged();
}

void ProjectSearch::setType(Type type)
{
	if (type == m_type) {
		return;
	}
	m_type = type;
	updateSuggestions();
	emit typeChanged();
}

void ProjectSearch::setScope(Scope scope)
{
	if (scope == m_scope) {
		return;
	}
	m_scope = scope;
	emit scopeChanged();
}

void ProjectSearch::setCurrentMapId(int id)
{
	if (id == m_currentMapId) {
		return;
	}
	m_currentMapId = id;
	emit currentMapIdChanged();
}

void ProjectSearch::setCacheEnabled(bool enabled)
{
	m_cache.clear();
	if (enabled == m_cacheEnabled) {
		return;
	}
	m_cacheEnabled = enabled;
	emit cacheEnabledChanged();
}

void ProjectSearch::setMapSource(MapSource source)
{
	m_mapSource = std::move(source);
}

void ProjectSearch::setCurrentMapSource(std::function<int()> source)
{
	m_currentMapSource = std::move(source);
}

void ProjectSearch::clearCache()
{
	m_cache.clear();
}

void ProjectSearch::updateSuggestions()
{
	std::vector<SearchSuggestions::Entry> entries;
	auto append = [&](const auto& records, Type type, const QString& label, const QString& icon) {
		if (m_type != AllTypes && m_type != type) {
			return;
		}
		for (const auto& record : records) {
			const QString name = ToQString(record.name);
			entries.push_back({record.ID, type, name, QStringLiteral("%1: %2 — %3").arg(record.ID).arg(label, name), icon});
		}
	};
	const auto& database = m_project.database();
	append(database.variables, Variables, QCoreApplication::translate("SearchDialog", "Variable"), QStringLiteral("variable"));
	append(database.switches, Switches, QCoreApplication::translate("SearchDialog", "Switch"), QStringLiteral("switch"));
	append(database.items, Items, QCoreApplication::translate("SearchDialog", "Item"), QStringLiteral("resources"));
	append(database.commonevents, EventNames, QCoreApplication::translate("SearchDialog", "Common event"), QStringLiteral("code"));
	m_suggestions->setEntries(std::move(entries));
}

void ProjectSearch::refresh()
{
	updateSuggestions();
	clearCache();
}

void ProjectSearch::reset()
{
	setText(QString());
	setType(AllTypes);
	setScope(WholeProject);
	m_suggestions->setQuery(QString());
	m_results->clear();
	clearCache();
}

void ProjectSearch::open(int row)
{
	if (const SearchResult* result = m_results->result(row)) {
		emit openRequested(result->mapId, result->eventId, result->pageId, result->line);
	}
}

QVariantList ProjectSearch::typeChoices() const
{
	return {
		choice(AllTypes, QCoreApplication::translate("SearchDialog", "All types")),
		choice(Variables, QCoreApplication::translate("SearchDialog", "Variables")),
		choice(Switches, QCoreApplication::translate("SearchDialog", "Switches")),
		choice(Items, QCoreApplication::translate("SearchDialog", "Items")),
		choice(EventNames, QCoreApplication::translate("SearchDialog", "Event names")),
		choice(EventText, QCoreApplication::translate("SearchDialog", "Event text")),
	};
}

QVariantList ProjectSearch::scopeChoices() const
{
	return {
		choice(WholeProject, QCoreApplication::translate("SearchDialog", "Whole project")),
		choice(CurrentMap, QCoreApplication::translate("SearchDialog", "Current map")),
		choice(CommonEvents, QCoreApplication::translate("SearchDialog", "Common events")),
	};
}

std::shared_ptr<const lcf::rpg::Map> ProjectSearch::loadMap(int map_id)
{
	// searches include the current contents of open tabs, without saving them
	if (m_mapSource) {
		if (auto map = m_mapSource(map_id)) {
			return map;
		}
	}
	if (m_cacheEnabled) {
		if (const auto cached = m_cache.constFind(map_id); cached != m_cache.cend()) {
			return *cached;
		}
	}
	std::shared_ptr<const lcf::rpg::Map> map = m_project.project().loadMap(map_id);
	if (m_cacheEnabled && map) {
		m_cache.insert(map_id, map);
	}
	return map;
}

QString ProjectSearch::mapPath(int map_id) const
{
	QStringList path;
	QSet<int> visited;
	const auto& maps = m_project.treeMap().maps;
	int parent_id = map_id;
	while (parent_id > 0 && !visited.contains(parent_id)) {
		visited.insert(parent_id);
		const auto info = std::find_if(maps.begin(), maps.end(), [&](const auto& map) { return map.ID == parent_id; });
		if (info == maps.end()) {
			break;
		}
		path.prepend(ToQString(info->name));
		parent_id = info->parent_map;
	}
	return path.join(QStringLiteral(" > "));
}

int ProjectSearch::search()
{
	const QString text = m_text.trimmed();
	if (text.isEmpty()) {
		m_results->clear();
		return 0;
	}
	if (m_currentMapSource) {
		setCurrentMapId(m_currentMapSource());
	}
	const Type type = m_type;
	// IDs match exactly, including a completed "ID: Name" suggestion. Names match partially.
	static const QRegularExpression numbered(QStringLiteral("^(\\d+)(?::\\s.*)?$"));
	const auto number = numbered.match(text);
	const int id = number.hasMatch() ? number.captured(1).toInt() : -1;
	auto nameMatches = [&](int record_id, const auto& name) {
		return number.hasMatch() ? record_id == id : ToQString(name).contains(text, Qt::CaseInsensitive);
	};
	auto matchingIds = [&](const auto& records, Type category) {
		QSet<int> ids;
		if (type == AllTypes || type == category) {
			for (const auto& record : records) {
				if (nameMatches(record.ID, record.name)) {
					ids.insert(record.ID);
				}
			}
			// references to an unnamed or deleted database entry still have a meaningful ID
			if (id > 0) {
				ids.insert(id);
			}
		}
		return ids;
	};
	const auto& database = m_project.database();
	const auto variables = matchingIds(database.variables, Variables);
	const auto switches = matchingIds(database.switches, Switches);
	const auto items = matchingIds(database.items, Items);
	static const QRegularExpression message_variable(QStringLiteral("\\\\[vV]\\[\\s*(\\d+)\\s*\\]"));
	using Cmd = lcf::rpg::EventCommand::Code;
	const std::string encoding = m_project.project().encoding().toStdString();

	// the switches turned on or off by the moves of a route
	auto routeUsesSwitch = [&](const lcf::rpg::MoveRoute& route) {
		using Move = lcf::rpg::MoveCommand::Code;
		return std::any_of(route.move_commands.begin(), route.move_commands.end(), [&](const auto& move) {
			const auto code = static_cast<Move>(move.command_id);
			return (code == Move::switch_on || code == Move::switch_off) && switches.contains(move.parameter_a);
		});
	};
	Kinds matched;
	auto matchesCommand = [&](const lcf::rpg::EventCommand& command) {
		matched.clear();
		auto p = [&](size_t i) { return i < command.parameters.size() ? command.parameters[i] : -1; };
		auto reference = [&](const QSet<int>& ids, int candidate, Kind kind) {
			if (!ids.contains(candidate)) {
				return false;
			}
			if (!matched.contains(kind)) {
				matched.append(kind);
			}
			return true;
		};
		auto variable = [&](size_t i) { return reference(variables, p(i), Kind::Variable); };
		auto range = [&](const QSet<int>& ids, Kind kind) {
			const bool found = std::any_of(ids.begin(), ids.end(), [&](int candidate) {
				return p(0) == 0 ? candidate == p(1) : p(0) == 1 && candidate >= p(1) && candidate <= p(2);
			});
			if (found && !matched.contains(kind)) {
				matched.append(kind);
			}
			return found;
		};
		bool match = false;
		switch (static_cast<Cmd>(command.code)) {
		case Cmd::ShowMessage:
		case Cmd::ShowMessage_2:
		case Cmd::ShowChoiceOption: {
			auto references = message_variable.globalMatch(ToQString(command.string));
			while (references.hasNext()) {
				if (reference(variables, references.next().captured(1).toInt(), Kind::Variable)) {
					match = true;
					break;
				}
			}
			break;
		}
		case Cmd::InputNumber:
			match = variable(1); break;
		case Cmd::ControlSwitches:
			match = range(switches, Kind::Switch) || (p(0) == 2 && variable(1)); break;
		case Cmd::ControlVars:
			match = range(variables, Kind::Variable) || (p(0) == 2 && variable(1)) ||
				((p(4) == 1 || p(4) == 2) && variable(5)) || (p(4) == 4 && reference(items, p(5), Kind::Item)); break;
		case Cmd::TimerOperation:
			match = p(0) == 0 && p(1) == 1 && variable(2); break;
		case Cmd::ChangeGold:
		case Cmd::ChangePartyMembers:
			match = p(1) == 1 && variable(2); break;
		case Cmd::ChangeItems:
			match = (p(1) == 1 && variable(2)) || (p(3) == 1 && variable(4)) ||
				(p(1) == 0 && reference(items, p(2), Kind::Item)); break;
		case Cmd::EnemyEncounter:
			match = p(0) == 1 && variable(1); break;
		case Cmd::ChangeExp:
		case Cmd::ChangeLevel:
		case Cmd::ChangeSkills:
		case Cmd::ChangeEquipment:
		case Cmd::ChangeHP:
		case Cmd::ChangeSP:
			match = (p(0) == 2 && variable(1)) || (p(3) == 1 && variable(4)) ||
				(command.code == static_cast<int>(Cmd::ChangeEquipment) && p(2) == 0 && p(3) == 0 && reference(items, p(4), Kind::Item)); break;
		case Cmd::ChangeParameters:
			match = (p(0) == 2 && variable(1)) || (p(4) == 1 && variable(5)); break;
		case Cmd::ChangeCondition:
		case Cmd::FullHeal:
			match = p(0) == 2 && variable(1); break;
		case Cmd::SimulatedAttack:
			match = (p(0) == 2 && variable(1)) || (p(6) == 1 && variable(7)); break;
		case Cmd::MemorizeLocation:
		case Cmd::RecallToLocation:
			match = variable(0) || variable(1) || variable(2); break;
		case Cmd::SetVehicleLocation:
			match = p(1) == 1 && (variable(2) || variable(3) || variable(4)); break;
		case Cmd::ChangeEventLocation:
		case Cmd::ShowPicture:
		case Cmd::MovePicture:
			match = (p(1) == 1 && (variable(2) || variable(3))); break;
		case Cmd::StoreTerrainID:
		case Cmd::StoreEventID:
			match = variable(3) || (p(0) == 1 && (variable(1) || variable(2))); break;
		case Cmd::KeyInputProc:
			match = variable(0) || (p(8) == 1 && variable(7)); break;
		case Cmd::ConditionalBranch:
			match = (p(0) == 0 && reference(switches, p(1), Kind::Switch)) ||
				(p(0) == 1 && (variable(1) || (p(2) == 1 && variable(3)))) ||
				(p(0) == 4 && reference(items, p(1), Kind::Item)); break;
		case Cmd::ConditionalBranch_B:
			match = (p(0) == 0 && reference(switches, p(1), Kind::Switch)) ||
				(p(0) == 1 && (variable(1) || (p(2) == 1 && variable(3)))); break;
		case Cmd::TeleportTargets:
			// a switch only when a target is added
			match = p(0) == 0 && p(4) != 0 && reference(switches, p(5), Kind::Switch); break;
		case Cmd::EscapeTarget:
			match = p(3) != 0 && reference(switches, p(4), Kind::Switch); break;
		case Cmd::MoveEvent:
			if (!switches.isEmpty() && routeUsesSwitch(MoveRouteCodec::decode(command, encoding).route)) {
				matched.append(Kind::Switch);
				match = true;
			}
			break;
		case Cmd::CallEvent:
			match = p(0) == 2 && (variable(1) || variable(2)); break;
		case Cmd::PlayMovie:
			match = p(0) == 1 && (variable(1) || variable(2)); break;
		default: break;
		}
		if ((type == AllTypes || type == EventText) && !command.string.empty() && ToQString(command.string).contains(text, Qt::CaseInsensitive)) {
			matched.append(Kind::Text);
			match = true;
		}
		return match;
	};

	std::vector<SearchResult> found;
	// read once a search: the path of a map, the file of a CharSet
	QHash<int, QString> paths;
	QHash<QString, QString> charsets;
	auto addResult = [&](int map_id, int event_id, int page_id, int line, const QString& match,
			const QString& types, Kind kind, const lcf::rpg::Event* event = nullptr) {
		SearchResult result;
		result.mapId = map_id;
		result.eventId = event_id;
		result.pageId = page_id;
		result.line = line;
		if (map_id == CommonEventResults) {
			result.location = QCoreApplication::translate("SearchDialog", "Common Events");
		} else if (map_id == TroopResults) {
			result.location = QCoreApplication::translate("SearchDialog", "Monster Groups");
		} else {
			auto path = paths.find(map_id);
			if (path == paths.end()) {
				path = paths.insert(map_id, mapPath(map_id));
			}
			result.location = *path;
		}
		if (event) {
			result.eventName = ToQString(event->name);
			// the graphic of the page found, else of the first page
			const auto page = std::find_if(event->pages.begin(), event->pages.end(), [&](const auto& page) { return page.ID == page_id; });
			const auto* graphic = page != event->pages.end() ? &*page : event->pages.empty() ? nullptr : &event->pages.front();
			if (graphic && !graphic->character_name.empty()) {
				const QString name = ToQString(graphic->character_name);
				auto file = charsets.find(name);
				if (file == charsets.end()) {
					file = charsets.insert(name, m_project.project().findFile(CHARSET, name, FileFinder::FileType::Image));
				}
				result.charsetFile = *file;
				result.charsetName = name;
				result.charsetFrame = QRect((graphic->character_index % 4) * 72 + graphic->character_pattern * 24,
					(graphic->character_index / 4) * 128 + graphic->character_direction * 32, 24, 32);
			}
		} else if (map_id == CommonEventResults) {
			const auto& events = database.commonevents;
			const auto common = std::find_if(events.begin(), events.end(), [&](const auto& common) { return common.ID == event_id; });
			if (common != events.end()) {
				result.eventName = ToQString(common->name);
			}
		} else if (map_id == TroopResults) {
			const auto& troops = database.troops;
			const auto troop = std::find_if(troops.begin(), troops.end(), [&](const auto& troop) { return troop.ID == event_id; });
			if (troop != troops.end()) {
				result.eventName = ToQString(troop->name);
			}
		}
		result.type = types.isEmpty() ? typeName(Kind::Name) : types;
		result.kind = kind;
		result.match = match;
		found.push_back(std::move(result));
	};
	auto searchCommands = [&](int map_id, int event_id, int page_id, const std::vector<lcf::rpg::EventCommand>& commands,
			const lcf::rpg::Event* event = nullptr) {
		for (size_t line = 0; line < commands.size(); ++line) {
			if (matchesCommand(commands[line])) {
				addResult(map_id, event_id, page_id, static_cast<int>(line + 1), Stringizer::stringize(commands[line]),
					typeNames(matched), matched.value(0, Kind::Text), event);
			}
		}
	};
	auto searchMap = [&](int map_id) {
		const auto map = loadMap(map_id);
		if (!map) {
			return;
		}
		for (const auto& event : map->events) {
			if ((type == AllTypes || type == EventNames) && nameMatches(event.ID, event.name)) {
				addResult(map_id, event.ID, 0, 0, ToQString(event.name), typeName(Kind::Name), Kind::Name, &event);
			}
			for (const auto& page : event.pages) {
				const auto& condition = page.condition;
				Kinds conditions;
				if (condition.flags.variable && variables.contains(condition.variable_id)) {
					conditions.append(Kind::Variable);
				}
				if ((condition.flags.switch_a && switches.contains(condition.switch_a_id)) ||
						(condition.flags.switch_b && switches.contains(condition.switch_b_id))) {
					conditions.append(Kind::Switch);
				}
				if (condition.flags.item && items.contains(condition.item_id)) {
					conditions.append(Kind::Item);
				}
				if (!conditions.isEmpty()) {
					addResult(map_id, event.ID, page.ID, 0, QCoreApplication::translate("SearchDialog", "Event page condition"),
						typeNames(conditions), conditions.first(), &event);
				}
				if (routeUsesSwitch(page.move_route)) {
					addResult(map_id, event.ID, page.ID, 0, QCoreApplication::translate("SearchDialog", "Custom movement pattern"),
						typeName(Kind::Switch), Kind::Switch, &event);
				}
				searchCommands(map_id, event.ID, page.ID, page.event_commands, &event);
			}
		}
	};

	if (m_scope == CurrentMap) {
		if (m_currentMapId > 0) {
			searchMap(m_currentMapId);
		}
	} else if (m_scope == WholeProject) {
		for (const auto& map : m_project.treeMap().maps) {
			if (map.ID > 0 && map.type == lcf::rpg::TreeMap::MapType_map) {
				searchMap(map.ID);
			}
		}
	}
	if (m_scope != CurrentMap) {
		for (const auto& event : database.commonevents) {
			if ((type == AllTypes || type == EventNames) && nameMatches(event.ID, event.name)) {
				addResult(CommonEventResults, event.ID, 0, 0, ToQString(event.name),
					QCoreApplication::translate("SearchDialog", "Common event"), Kind::Name);
			}
			if (event.switch_flag && switches.contains(event.switch_id)) {
				addResult(CommonEventResults, event.ID, 0, 0, QCoreApplication::translate("SearchDialog", "Common event condition"),
					typeName(Kind::Switch), Kind::Switch);
			}
			searchCommands(CommonEventResults, event.ID, 0, event.event_commands);
		}
	}
	if (m_scope == WholeProject) {
		// the battle events of the monster groups
		for (const auto& troop : database.troops) {
			for (const auto& page : troop.pages) {
				const auto& condition = page.condition;
				Kinds conditions;
				if (condition.flags.variable && variables.contains(condition.variable_id)) {
					conditions.append(Kind::Variable);
				}
				if ((condition.flags.switch_a && switches.contains(condition.switch_a_id)) ||
						(condition.flags.switch_b && switches.contains(condition.switch_b_id))) {
					conditions.append(Kind::Switch);
				}
				if (!conditions.isEmpty()) {
					addResult(TroopResults, troop.ID, page.ID, 0, QCoreApplication::translate("SearchDialog", "Battle event condition"),
						typeNames(conditions), conditions.first());
				}
				searchCommands(TroopResults, troop.ID, page.ID, page.event_commands);
			}
		}
	}
	m_results->setResults(std::move(found));
	return m_results->count();
}
