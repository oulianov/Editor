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


// The three-way merges of DataMerge: what the Editor and another program
// (rpgsync) changed since the Editor read the data

#include "model/data_merge.h"
#include <QCoreApplication>
#include <cstdio>

namespace {
int failures = 0;

void check(bool condition, const char* what) {
	if (!condition) {
		std::printf("FAILED: %s\n", what);
		++failures;
	}
}

lcf::rpg::Database database() {
	lcf::rpg::Database db;
	for (int id = 1; id <= 5; ++id) {
		lcf::rpg::CommonEvent event;
		event.ID = id;
		event.name = lcf::DBString("Event " + std::to_string(id));
		db.commonevents.push_back(event);
		lcf::rpg::Actor actor;
		actor.ID = id;
		actor.name = lcf::DBString("Hero " + std::to_string(id));
		db.actors.push_back(actor);
	}
	return db;
}

lcf::rpg::EventCommand wait(int tenths) {
	lcf::rpg::EventCommand command;
	command.code = static_cast<int>(lcf::rpg::EventCommand::Code::Wait);
	command.parameters = lcf::DBArray<int32_t>({tenths});
	return command;
}

void databases() {
	const auto base = database();
	lcf::rpg::Database out;

	// different entries: both kept, no conflict
	auto mine = base;
	auto theirs = base;
	mine.commonevents[0].event_commands.push_back(wait(1));
	theirs.commonevents[3].event_commands.push_back(wait(2));
	auto result = DataMerge::database(base, mine, theirs, out);
	check(result.ok(), "different common events merge");
	check(out.commonevents[0] == mine.commonevents[0], "mine kept");
	check(out.commonevents[3] == theirs.commonevents[3], "theirs kept");

	// different lists: a hero here, a common event there
	mine = base;
	theirs = base;
	mine.actors[1].name = "Renamed";
	theirs.commonevents[1].event_commands.push_back(wait(3));
	result = DataMerge::database(base, mine, theirs, out);
	check(result.ok() && out.actors[1].name == "Renamed" && out.commonevents[1] == theirs.commonevents[1],
		"different lists merge");

	// the same entry, changed differently: a conflict named after it
	mine = base;
	theirs = base;
	mine.commonevents[2].event_commands.push_back(wait(1));
	theirs.commonevents[2].event_commands.push_back(wait(2));
	result = DataMerge::database(base, mine, theirs, out);
	check(result.conflicts == QStringList{QStringLiteral("Common Event 3")}, "same common event conflicts");

	// the same entry, changed the same way: no conflict
	mine = base;
	theirs = base;
	mine.actors[0].name = theirs.actors[0].name = "Same";
	check(DataMerge::database(base, mine, theirs, out).ok(), "same change on both sides merges");

	// Array Size on one side, an entry after the end changed on the other
	mine = base;
	theirs = base;
	mine.commonevents.resize(3);
	theirs.commonevents[4].name = "Changed";
	check(!DataMerge::database(base, mine, theirs, out).ok(), "shortened list against a change after its end conflicts");

	// entries added on one side, another entry changed on the other
	mine = base;
	theirs = base;
	lcf::rpg::CommonEvent added;
	added.ID = 6;
	mine.commonevents.push_back(added);
	theirs.commonevents[0].name = "Changed";
	result = DataMerge::database(base, mine, theirs, out);
	check(result.ok() && out.commonevents.size() == 6 && out.commonevents[0].name == "Changed", "added entry merges");

	// each save counts in the system: no conflict, the highest count
	mine = base;
	theirs = base;
	mine.system.save_count = 3;
	theirs.system.save_count = 5;
	result = DataMerge::database(base, mine, theirs, out);
	check(result.ok() && out.system.save_count == 5, "save counts merge");
}

lcf::rpg::Map map() {
	lcf::rpg::Map map;
	map.width = 4;
	map.height = 3;
	map.lower_layer.assign(12, 0);
	map.upper_layer.assign(12, 10000);
	for (int id = 1; id <= 3; ++id) {
		lcf::rpg::Event event;
		event.ID = id;
		event.name = lcf::DBString("EV" + std::to_string(id));
		event.pages.emplace_back();
		map.events.push_back(event);
	}
	return map;
}

void maps() {
	const auto base = map();
	lcf::rpg::Map out;

	// different events, different tiles: all kept
	auto mine = base;
	auto theirs = base;
	mine.events[0].pages[0].event_commands.push_back(wait(1));
	theirs.events[1].pages[0].event_commands.push_back(wait(2));
	mine.lower_layer[0] = 5;
	theirs.lower_layer[11] = 7;
	auto result = DataMerge::map(base, mine, theirs, out);
	check(result.ok(), "different events and tiles merge");
	check(out.events[0] == mine.events[0] && out.events[1] == theirs.events[1], "both events kept");
	check(out.lower_layer[0] == 5 && out.lower_layer[11] == 7, "both tiles kept");

	// an event added on each side, with different IDs
	mine = base;
	theirs = base;
	lcf::rpg::Event added;
	added.ID = 4;
	mine.events.push_back(added);
	added.ID = 5;
	theirs.events.push_back(added);
	result = DataMerge::map(base, mine, theirs, out);
	check(result.ok() && out.events.size() == 5, "events added on both sides merge");

	// an event deleted here, another changed there
	mine = base;
	theirs = base;
	mine.events.erase(mine.events.begin() + 2);
	theirs.events[0].name = "Changed";
	result = DataMerge::map(base, mine, theirs, out);
	check(result.ok() && out.events.size() == 2 && out.events[0].name == "Changed", "deleted event merges");

	// the same event changed on both sides
	mine = base;
	theirs = base;
	mine.events[1].x = 2;
	theirs.events[1].y = 2;
	result = DataMerge::map(base, mine, theirs, out);
	check(result.conflicts == QStringList{QStringLiteral("Event 2 (EV2)")}, "same event conflicts");

	// the same tile, two different tiles
	mine = base;
	theirs = base;
	mine.upper_layer[3] = 10001;
	theirs.upper_layer[3] = 10002;
	check(DataMerge::map(base, mine, theirs, out).conflicts == QStringList{QStringLiteral("Upper Layer")}, "same tile conflicts");

	// properties here, an event there; save counts
	mine = base;
	theirs = base;
	mine.chipset_id = 2;
	mine.save_count = 4;
	theirs.events[2].name = "Changed";
	theirs.save_count = 6;
	result = DataMerge::map(base, mine, theirs, out);
	check(result.ok() && out.chipset_id == 2 && out.events[2].name == "Changed" && out.save_count == 6, "properties merge");
}

void treeMaps() {
	lcf::rpg::TreeMap base;
	for (int id = 0; id <= 3; ++id) {
		lcf::rpg::MapInfo info;
		info.ID = id;
		info.name = lcf::DBString("Map " + std::to_string(id));
		base.maps.push_back(info);
		base.tree_order.push_back(id);
	}
	lcf::rpg::TreeMap out;

	// a map renamed on each side, the last map shown is mine
	auto mine = base;
	auto theirs = base;
	mine.maps[1].name = "Mine";
	mine.active_node = 1;
	theirs.maps[2].name = "Theirs";
	theirs.active_node = 2;
	auto result = DataMerge::treeMap(base, mine, theirs, out);
	check(result.ok() && out.maps[1].name == "Mine" && out.maps[2].name == "Theirs" && out.active_node == 1, "tree maps merge");

	// the same map renamed differently
	mine = base;
	theirs = base;
	mine.maps[3].name = "Mine";
	theirs.maps[3].name = "Theirs";
	check(DataMerge::treeMap(base, mine, theirs, out).conflicts == QStringList{QStringLiteral("Map 3 (Map 3)")}, "same map conflicts");
}
}

int main(int argc, char* argv[]) {
	QCoreApplication app(argc, argv);
	databases();
	maps();
	treeMaps();
	std::printf("%s\n", failures == 0 ? "data merge: all passed" : "data merge: failures");
	return failures == 0 ? 0 : 1;
}
