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

#include <QCoreApplication>
#include <QIcon>
#include <QListWidget>
#include <vector>
#include <lcf/rpg/learning.h>
#include <lcf/rpg/parameters.h>
#include "common/dbstring.h"

class ProjectData;
class QTableWidget;

// Editors for what classes and heroes both have: resistance ranks, skills
// learned and parameter curves
class ClassEditors {
	Q_DECLARE_TR_FUNCTIONS(ClassEditors)

public:
	// Ranks A to E are 0 to 4; RPG Maker reads a missing rank as C (2)
	static constexpr int kDefaultRank = 2;

	static QIcon rankIcon(int rank);

	// One line per attribute or condition, with its rank
	template<class T>
	static void fillRanks(QListWidget* list, const std::vector<T>& entries, const std::vector<uint8_t>& ranks) {
		list->clear();
		for (size_t i = 0; i < entries.size(); ++i) {
			list->addItem(new QListWidgetItem(rankIcon(i < ranks.size() ? ranks[i] : kDefaultRank),
				ToQString(entries[i].name)));
		}
	}

	// Gives a line the next rank (A, B, C, D, E, then A again)
	static void cycleRank(QListWidget* list, std::vector<uint8_t>& ranks, int row);

	// The skills sorted by level, and an empty last line to add one
	static void fillSkills(QTableWidget* table, const std::vector<lcf::rpg::Learning>& skills, ProjectData& project);

	// Edits a skill, or adds one when the row is past the end; true when changed
	static bool editSkill(QWidget* parent, ProjectData& project, std::vector<lcf::rpg::Learning>& skills,
		int row, int max_level);

	static void removeSkill(std::vector<lcf::rpg::Learning>& skills, int row);

	// Values of the six parameters for each level; true when changed
	static bool editParameters(QWidget* parent, lcf::rpg::Parameters& parameters, int max_level, int max_hp);
};
