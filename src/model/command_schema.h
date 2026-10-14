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

#include <QFlags>
#include <QObject>
#include <QString>
#include <cstdint>
#include <string_view>
#include <vector>
#include <lcf/rpg/eventcommand.h>

/**
 * What the editor of each event command shows and where each of its values
 * is in the command: the data the QML command editors are generated from.
 * Its texts keep the translation contexts of the command editors
 * ("ShowMessageWidget"...), so their translations apply.
 *
 * A field edits one value: a parameter, some bits of one, or the string of
 * the command. Fields on the same bits are alternatives (the value of a
 * constant or of a variable...): their rules never show two of them at once.
 * Editors too special for a generated form are marked custom: their fields
 * describe them as far as practical, the QML hand-writes them.
 */
namespace CommandSchema {
	Q_NAMESPACE

	using Code = lcf::rpg::EventCommand::Code;

	/** The index of a field on the string of the command */
	constexpr int StringIndex = -1;

	/** A text of the interface: its translation context and its source text */
	struct Label {
		/** nullptr: the context of the schema (set when the table is built);
		 *  "": a text not translated (a number, a sign) */
		const char* context = nullptr;
		const char* text = nullptr;
		/** For the %1 of the text (Monster %1), -1 for none */
		int number = -1;

		Label() = default;
		Label(const char* text) : text(text) {}
		Label(const char* context, const char* text, int number = -1) :
			context(context), text(text), number(number) {}

		bool isEmpty() const { return !text || !*text; }
		/** The text in the language of the interface */
		QString translated() const;
	};

	enum class Kind {
		/** No value: the title of the group of the next fields */
		Section,
		/** No value: a text shown */
		Note,
		/** A spin box: minimum, maximum, suffix, step */
		Number,
		/** A check box: 0 or 1 (or the bit) */
		Check,
		/** One of items: radio buttons (Flag::Radio) or a combo box. A value
		 *  without an item is kept until one is chosen */
		Choice,
		/** A time in seconds, shown as minutes and seconds */
		Time,
		/** The string of the command */
		String,
		/** The string of the command and of its next lines (Show Message,
		 *  Comment: one line per ShowMessage_2 or Comment_2 command) */
		Lines,
		/** A file of the project: the string of the command, in folder */
		File,
		/** A face: the file in the string, its index in the parameter */
		Face,
		/** A character graphic: the file in the string, its index in the parameter */
		Charset,

		// An ID of the database (from 1); items: the entries before the database's
		Switch,
		Variable,
		Item,
		Actor,
		Skill,
		State,
		Troop,
		Terrain,
		Animation,
		Chipset,
		CommonEvent,
		Class,
		BattleCommand,
		/** A string of Maniac Patch (T[x]): a number */
		StringId,
		/** A map of the map tree */
		Map,
		/** A cell: on the map of the field link (a Map), else on the map of
		 *  the event; with a button to click it on the map */
		MapX,
		MapY,
		/** A character: the events of the map after items (This Event,
		 *  Hero, the vehicles: 10001-10005) */
		Character,
		/** A tile of the chipset (0-143), of the layer in the field link */
		Tile,
		/** The keyboard page of Enter Hero Name: the items depend on the
		 *  encoding of the game (Hiragana and Katakana for Japanese...) */
		CharacterSet,

		// From the parameter to the last one
		/** A Maniac Patch expression (its bytecode words) */
		Expression,
		/** The moves of Move Event (MoveRouteCodec) */
		MoveRoute,
		/** Item IDs (the goods of a shop) */
		ItemList,
	};
	Q_ENUM_NS(Kind)

	enum class Flag {
		NoFlag = 0,
		/** Choice: radio buttons */
		Radio = 1 << 0,
		/** Number: with a slider */
		Slider = 1 << 1,
		/** On the row of the field before (a value after its mode) */
		Inline = 1 << 2,
		/** Check: checked for 0 */
		Inverted = 1 << 3,
		/** Not shown: a value of an engine extension the rules read */
		Hidden = 1 << 4,
		/** A parameter after the ones of the new command (EventCommandDefaults):
		 *  of a newer engine or a patch. A missing one reads defaultValue */
		Extension = 1 << 5,
		/** RPG Maker 2003: disabled in RPG Maker 2000 games */
		Rpg2003 = 1 << 6,
		/** Maniac Patch: disabled in other games */
		Maniac = 1 << 7,
		/** Item: only the equipment (weapons, shields, armors, helmets, accessories) */
		Equipment = 1 << 8,
		/** File: with "(OFF)", no music or sound */
		Off = 1 << 9,
		/** An item shown only when the command has its value (of another engine) */
		Kept = 1 << 10,
		/** Slider: a tick mark at each step, snapping to them (the trackbars of RPG Maker) */
		Ticks = 1 << 11,
	};
	Q_DECLARE_FLAGS(Flags, Flag)
	Q_FLAG_NS(Flags)

	struct Item {
		int value = 0;
		Label text;
		/** Rpg2003, Maniac, Kept */
		Flags flags;
	};

	/** The field id has one of values (negated: none of them) */
	struct Condition {
		const char* field = nullptr;
		std::vector<int> values;
		bool negated = false;
	};

	struct Field {
		/** Unique in the schema, for the rules */
		const char* id = "";
		Kind kind = Kind::Number;
		Label label;
		/** The parameter, StringIndex for the string of the command */
		int index = StringIndex;
		/** The bits of the parameter: [shift, shift + bits) */
		int shift = 0;
		int bits = 32;
		int minimum = 0;
		int maximum = 0;
		int step = 1;
		/** The value of a missing parameter, and the one written when a rule
		 *  shows the field (the first entry of a list, 0...) */
		int defaultValue = 0;
		Label suffix;
		std::vector<Item> items;
		/** Shown when all of them hold */
		std::vector<Condition> visible;
		/** Enabled when all of them hold */
		std::vector<Condition> enabled;
		Flags flags;
		/** File, Face, Charset: the folder of the project */
		const char* folder = nullptr;
		/** MapX, MapY: the Map field; Tile: the field of the layer */
		const char* link = nullptr;
		/** String: the characters RPG Maker lets type (0: no limit) */
		int limit = 0;
		/** A missing parameter reads this parameter instead of defaultValue,
		 *  and gets it when the command grows (the bottom transparency of a
		 *  picture: the top's); -1 for none */
		int copies = -1;

		/** The bits of the parameter it uses */
		uint32_t mask() const;
		/** It edits a value: not a Section, a Note */
		bool hasValue() const;
		/** Its value is the number read(): one parameter, or the index of a Face or Charset */
		bool isNumber() const;
		/** It edits the string of the command (String, Lines, File, Face, Charset) */
		bool usesString() const;
		/** It uses all the parameters from index (Expression, MoveRoute, ItemList) */
		bool isTail() const;
	};

	struct Schema {
		Code code = Code::END;
		Label title;
		std::vector<Field> fields;
		/** Rpg2003: the command needs RPG Maker 2003; Maniac: Maniac Patch */
		Flags flags;
		/** The QML hand-writes the editor: the fields describe it */
		bool custom = false;

		/** The field with this id, nullptr for none */
		const Field* field(std::string_view id) const;
	};

	/** The command opens an editor (the others have no settings): its
	 *  schema, else the editor of the raw parameters */
	bool hasEditor(Code code);
	/** The schema of the command, nullptr for none (the raw editor) */
	const Schema* find(Code code);
	/** All the schemas, by code */
	const std::vector<Schema>& all();

	/** What a number field reads missing: defaultValue, or the parameter it copies */
	int defaultOf(const Field& field, const lcf::rpg::EventCommand& command);
	/** The value of a number field (isNumber()); a missing parameter reads defaultOf() */
	int read(const Field& field, const lcf::rpg::EventCommand& command);
	/** Writes it in the bits of the field, the others are kept. The new
	 *  parameters of a grown command get the defaults of the fields of
	 *  schema, so the other fields read the same. False: the field already
	 *  had this value, the command is unchanged (a missing parameter stays missing) */
	bool write(const Schema& schema, const Field& field, lcf::rpg::EventCommand& command, int value);
	/** The string of the command, for the fields using it */
	QString readString(const lcf::rpg::EventCommand& command);
	void writeString(lcf::rpg::EventCommand& command, const QString& text);

	/** All the conditions hold for the values of the command */
	bool holds(const Schema& schema, const std::vector<Condition>& conditions, const lcf::rpg::EventCommand& command);
	bool isVisible(const Schema& schema, const Field& field, const lcf::rpg::EventCommand& command);
	bool isEnabled(const Schema& schema, const Field& field, const lcf::rpg::EventCommand& command);
}

Q_DECLARE_OPERATORS_FOR_FLAGS(CommandSchema::Flags)
