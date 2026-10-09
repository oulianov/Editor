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


#include "data_merge.h"
#include <QCoreApplication>
#include <algorithm>
#include <functional>
#include <map>
#include <optional>

namespace {
QString tr(const char* text) {
	return QCoreApplication::translate("DataMerge", text);
}

/** One value: the side that changed it, else a conflict (mine kept) */
template<class T>
bool pick(const T& base, const T& mine, const T& theirs, T& out) {
	if (mine == base) {
		out = theirs;
		return true;
	}
	out = mine;
	return theirs == base || theirs == mine;
}

template<class T>
void value(const T& base, const T& mine, const T& theirs, T& out, const QString& name, DataMerge::Result& result) {
	if (!pick(base, mine, theirs, out)) {
		result.conflicts << name;
	}
}

/** A database list: entry N is at index N-1, so entries merge by index. An
 *  entry missing on one side was removed by Array Size there. */
template<class T>
void list(const std::vector<T>& base, const std::vector<T>& mine, const std::vector<T>& theirs,
		std::vector<T>& out, const char* kind, DataMerge::Result& result) {
	auto at = [](const std::vector<T>& list, size_t i) {
		return i < list.size() ? std::optional<T>(list[i]) : std::nullopt;
	};
	const size_t count = std::max({base.size(), mine.size(), theirs.size()});
	std::vector<T> merged;
	bool ended = false;
	for (size_t i = 0; i < count; ++i) {
		std::optional<T> entry;
		const QString name = tr(kind).arg(i + 1);
		if (!pick(at(base, i), at(mine, i), at(theirs, i), entry)) {
			result.conflicts << name;
		}
		if (!entry) {
			ended = true;
		} else if (ended) {
			// a list cannot skip an entry: one side shortened it, the other
			// changed what lies after
			result.conflicts << name;
		} else {
			merged.push_back(*entry);
		}
	}
	out = std::move(merged);
}

/** Entries known by their ID (map events, the maps of the tree), in ID order */
template<class T>
void byId(const std::vector<T>& base, const std::vector<T>& mine, const std::vector<T>& theirs,
		std::vector<T>& out, const std::function<QString(const T&)>& name, DataMerge::Result& result) {
	std::map<int, std::optional<T>> b, m, t;
	std::map<int, bool> ids;
	for (const auto& [source, target] : {std::pair{&base, &b}, {&mine, &m}, {&theirs, &t}}) {
		for (const T& entry : *source) {
			(*target)[entry.ID] = entry;
			ids[entry.ID] = true;
		}
	}
	std::vector<T> merged;
	for (const auto& [id, unused] : ids) {
		std::optional<T> entry;
		if (!pick(b[id], m[id], t[id], entry)) {
			// named as both sides knew it
			const T& any = b[id] ? *b[id] : (m[id] ? *m[id] : *t[id]);
			result.conflicts << name(any);
		}
		if (entry) {
			merged.push_back(*entry);
		}
	}
	out = std::move(merged);
}

/** A layer: tile by tile when the size did not change */
void layer(const std::vector<int16_t>& base, const std::vector<int16_t>& mine, const std::vector<int16_t>& theirs,
		std::vector<int16_t>& out, const QString& name, DataMerge::Result& result) {
	if (base.size() != mine.size() || base.size() != theirs.size()) {
		value(base, mine, theirs, out, name, result);
		return;
	}
	out = mine;
	bool conflict = false;
	for (size_t i = 0; i < base.size(); ++i) {
		if (mine[i] == base[i]) {
			out[i] = theirs[i];
		} else if (theirs[i] != base[i] && theirs[i] != mine[i]) {
			conflict = true;
		}
	}
	if (conflict) {
		result.conflicts << name;
	}
}
}

DataMerge::Result DataMerge::database(const lcf::rpg::Database& base, const lcf::rpg::Database& mine,
		const lcf::rpg::Database& theirs, lcf::rpg::Database& out) {
	Result result;
	out = mine;
	list(base.actors, mine.actors, theirs.actors, out.actors, QT_TRANSLATE_NOOP("DataMerge", "Hero %1"), result);
	list(base.skills, mine.skills, theirs.skills, out.skills, QT_TRANSLATE_NOOP("DataMerge", "Skill %1"), result);
	list(base.items, mine.items, theirs.items, out.items, QT_TRANSLATE_NOOP("DataMerge", "Item %1"), result);
	list(base.enemies, mine.enemies, theirs.enemies, out.enemies, QT_TRANSLATE_NOOP("DataMerge", "Monster %1"), result);
	list(base.troops, mine.troops, theirs.troops, out.troops, QT_TRANSLATE_NOOP("DataMerge", "Monster Group %1"), result);
	list(base.terrains, mine.terrains, theirs.terrains, out.terrains, QT_TRANSLATE_NOOP("DataMerge", "Terrain %1"), result);
	list(base.attributes, mine.attributes, theirs.attributes, out.attributes, QT_TRANSLATE_NOOP("DataMerge", "Attribute %1"), result);
	list(base.states, mine.states, theirs.states, out.states, QT_TRANSLATE_NOOP("DataMerge", "Condition %1"), result);
	list(base.animations, mine.animations, theirs.animations, out.animations, QT_TRANSLATE_NOOP("DataMerge", "Battle Animation %1"), result);
	list(base.chipsets, mine.chipsets, theirs.chipsets, out.chipsets, QT_TRANSLATE_NOOP("DataMerge", "Chipset %1"), result);
	list(base.switches, mine.switches, theirs.switches, out.switches, QT_TRANSLATE_NOOP("DataMerge", "Switch %1"), result);
	list(base.variables, mine.variables, theirs.variables, out.variables, QT_TRANSLATE_NOOP("DataMerge", "Variable %1"), result);
	list(base.commonevents, mine.commonevents, theirs.commonevents, out.commonevents, QT_TRANSLATE_NOOP("DataMerge", "Common Event %1"), result);
	list(base.classes, mine.classes, theirs.classes, out.classes, QT_TRANSLATE_NOOP("DataMerge", "Class %1"), result);
	list(base.battleranimations, mine.battleranimations, theirs.battleranimations, out.battleranimations,
		QT_TRANSLATE_NOOP("DataMerge", "Battler Animation %1"), result);
	list(base.maniac_string_variables, mine.maniac_string_variables, theirs.maniac_string_variables, out.maniac_string_variables,
		QT_TRANSLATE_NOOP("DataMerge", "String Variable %1"), result);
	value(base.terms, mine.terms, theirs.terms, out.terms, tr("Vocabulary"), result);
	value(base.battlecommands, mine.battlecommands, theirs.battlecommands, out.battlecommands, tr("Battle Commands"), result);
	// every save counts in the system: compared without its count, it keeps
	// the highest
	auto counted = [](lcf::rpg::System system) {
		system.save_count = 0;
		return system;
	};
	value(counted(base.system), counted(mine.system), counted(theirs.system), out.system, tr("System"), result);
	out.system.save_count = std::max(mine.system.save_count, theirs.system.save_count);
	return result;
}

DataMerge::Result DataMerge::map(const lcf::rpg::Map& base, const lcf::rpg::Map& mine,
		const lcf::rpg::Map& theirs, lcf::rpg::Map& out) {
	Result result;
	// the properties of the map, without its layers, events and save counts
	auto properties = [](lcf::rpg::Map map) {
		map.lower_layer.clear();
		map.upper_layer.clear();
		map.events.clear();
		map.save_count = 0;
		map.save_count_2k3e = 0;
		return map;
	};
	lcf::rpg::Map merged;
	value(properties(base), properties(mine), properties(theirs), merged, tr("Map Properties"), result);
	layer(base.lower_layer, mine.lower_layer, theirs.lower_layer, merged.lower_layer, tr("Lower Layer"), result);
	layer(base.upper_layer, mine.upper_layer, theirs.upper_layer, merged.upper_layer, tr("Upper Layer"), result);
	byId<lcf::rpg::Event>(base.events, mine.events, theirs.events, merged.events, [](const lcf::rpg::Event& event) {
		return tr("Event %1 (%2)").arg(event.ID).arg(QString::fromStdString(std::string(event.name)));
	}, result);
	merged.save_count = std::max(mine.save_count, theirs.save_count);
	merged.save_count_2k3e = std::max(mine.save_count_2k3e, theirs.save_count_2k3e);
	out = std::move(merged);
	return result;
}

DataMerge::Result DataMerge::treeMap(const lcf::rpg::TreeMap& base, const lcf::rpg::TreeMap& mine,
		const lcf::rpg::TreeMap& theirs, lcf::rpg::TreeMap& out) {
	Result result;
	out = mine;
	byId<lcf::rpg::MapInfo>(base.maps, mine.maps, theirs.maps, out.maps, [](const lcf::rpg::MapInfo& info) {
		return tr("Map %1 (%2)").arg(info.ID).arg(QString::fromStdString(std::string(info.name)));
	}, result);
	value(base.tree_order, mine.tree_order, theirs.tree_order, out.tree_order, tr("Map Order"), result);
	value(base.start, mine.start, theirs.start, out.start, tr("Start Positions"), result);
	// the last map shown is the Editor's
	out.active_node = mine.active_node;
	return result;
}
