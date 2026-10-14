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

#include <QString>
#include <string_view>
#include <vector>

/**
 * The Vocabulary page of the database: the terms of the game grouped as in
 * RPG Maker, one row per term, with the engine that has it and the length
 * RPG Maker 2003 lets type. VocabularyPage.qml shows it. The texts are the
 * source texts of the context "VocabularyWidget" (tr() translates them);
 * the terms are the names of the fields of lcf::rpg::Terms.
 */
namespace VocabularyFields {

enum class Engine { Both, Rpg2k, Rpg2k3 };

/** A term; hint shows what the game puts before it, or between it and second */
struct Row {
	Engine engine;
	const char* label;
	const char* term;
	const char* hint = nullptr;
	const char* second = nullptr;
};

struct Group {
	const char* title;
	/** 0: the left column of the page, 1: the right one */
	int column;
	/** Label and field pairs on each line */
	int pairs;
	std::vector<Row> rows;
};

struct Page {
	const char* title;
	std::vector<Group> groups;
};

/** The pages of RPG Maker, the last one split in two to fit small screens */
const std::vector<Page>& pages();

/** The game's engine has the term (RPG Maker 2003 or 2000) */
bool shown(const Row& row, bool rpg2k3);

/** The MaxLength of a term in RPG Maker 2003, 0 for the terms of RPG Maker 2000 only */
int limit(std::string_view term);

/** A text of the table in the language of the Editor (empty for none) */
QString tr(const char* text);

}
