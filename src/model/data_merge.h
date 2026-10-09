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

#include <QStringList>
#include <lcf/rpg/database.h>
#include <lcf/rpg/map.h>
#include <lcf/rpg/treemap.h>

/**
 * Three-way merges of the data the Editor and another program (rpgsync, a
 * second editor...) both changed since the Editor read it: base is the
 * version read, mine the Editor's, theirs the file now. Each entry (a
 * database entry, a map event, a map of the tree) takes the side that
 * changed it; changed differently on both sides, it is a conflict, and the
 * result keeps mine for it.
 */
namespace DataMerge {
	struct Result {
		/** What both sides changed differently ("Common Event 22"...) */
		QStringList conflicts;
		bool ok() const { return conflicts.isEmpty(); }
	};

	Result database(const lcf::rpg::Database& base, const lcf::rpg::Database& mine,
		const lcf::rpg::Database& theirs, lcf::rpg::Database& out);
	Result map(const lcf::rpg::Map& base, const lcf::rpg::Map& mine,
		const lcf::rpg::Map& theirs, lcf::rpg::Map& out);
	Result treeMap(const lcf::rpg::TreeMap& base, const lcf::rpg::TreeMap& mine,
		const lcf::rpg::TreeMap& theirs, lcf::rpg::TreeMap& out);
}
