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
 *
 * Besides RPG Maker's: the parts the Maniac Patch adds around some
 * messages (Row::maniac), and the terms of EasyRPG Player (Group::extension),
 * some of which the Maniac Patch reads too (liblcf's ids 203 to 216).
 */
namespace VocabularyFields {

enum class Engine { Both, Rpg2k, Rpg2k3 };

/** Who reads the terms of a group besides RPG Maker */
enum class Extension {
	/** RPG Maker's own */
	None,
	/** EasyRPG Player only */
	EasyRpg,
	/** EasyRPG Player and the Maniac Patch */
	EasyRpgManiac,
};

/** A part of a message: a term (its field), else what the game puts there (hint); neither: a new line */
struct Part {
	const char* term = nullptr;
	const char* hint = nullptr;
};

/** A term; hint shows what the game puts before it, or between it and second */
struct Row {
	Engine engine;
	const char* label;
	const char* term;
	const char* hint = nullptr;
	const char* second = nullptr;
	/** With the Maniac Patch: the parts of the message in their order, its own terms among them (empty: as without it) */
	std::vector<Part> maniac = {};
	/**
	 * A term of EasyRPG Player whose default is liblcf's "default_term"
	 * (shown empty): what the game shows instead (its own text, else
	 * fallbackHint: a text of the table)
	 */
	const char* fallback = nullptr;
	bool fallbackHint = false;
};

struct Group {
	const char* title;
	/** 0: the left column of the page, 1: the right one */
	int column;
	/** Label and field pairs on each line */
	int pairs;
	std::vector<Row> rows;
	Extension extension = Extension::None;
};

struct Page {
	const char* title;
	std::vector<Group> groups;
};

/** The pages of RPG Maker, the last one split in two to fit small screens */
const std::vector<Page>& pages();

/** The game's engine has the term (RPG Maker 2003 or 2000) */
bool shown(const Row& row, bool rpg2k3);

/** The MaxLength of a term in RPG Maker 2003, 0 for the terms of RPG Maker 2000 only, the Maniac Patch's and EasyRPG Player's */
int limit(std::string_view term);

/** A text of the table in the language of the Editor (empty for none) */
QString tr(const char* text);

}
