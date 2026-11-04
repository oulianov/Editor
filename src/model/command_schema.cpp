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

#include "command_schema.h"
#include "common/dbstring.h"
#include "defines.h"
#include "core.h"
#include "model/game_patches.h"
#include "model/project.h"
#include <QCoreApplication>
#include <algorithm>
#include <deque>
#include <limits>
#include <string>

using namespace CommandSchema;

namespace {
using Fields = std::vector<Field>;
using Items = std::vector<Item>;

/** Keeps the ids made by the helpers (a prefix and a name) */
const char* name(const char* prefix, const char* suffix) {
	static std::deque<std::string> names;
	return names.emplace_back(std::string(prefix) + suffix).c_str();
}

/** A text in another translation context than the schema's */
Label in(const char* context, const char* text, int number = -1) {
	return Label(context, text, number);
}

/** The name of the command in the palette: the title of the editors without one */
Label palette(const char* text) {
	return Label("EventCommandPalette", text);
}

/** A text not translated */
Label literal(const char* text) {
	return Label("", text);
}

/** A field, with the changes of the table chained */
struct F : Field {
	F& with(Flags more) { flags |= more; return *this; }
	F& copying(int parameter) { copies = parameter; return *this; }
	F& radio() { return with(Flag::Radio); }
	F& slider() { return with(Flag::Slider); }
	F& ticks() { return with(Flag::Slider).with(Flag::Ticks); }
	F& inlined() { return with(Flag::Inline); }
	F& rpg2003() { return with(Flag::Rpg2003); }
	F& maniac() { return with(Flag::Maniac); }
	/** An option of the Maniac Patch in a command of RPG Maker: hidden in the other games */
	F& maniacOnly() { return with(Flag::Maniac).with(Flag::ManiacOnly); }
	F& notManiac() { return with(Flag::NotManiac); }
	F& extension() { return with(Flag::Extension); }
	F& hidden() { return with(Flag::Hidden); }
	F& preview() { return with(Flag::Preview); }
	F& cross() { return with(Flag::Radio).with(Flag::Cross); }
	/** Radio buttons in columns, filled top to bottom (1: one under the other) */
	F& inColumns(int count) { columns = count; return with(Flag::Radio); }
	F& bitsAt(int first, int count) { shift = first; bits = count; return *this; }
	F& bit(int number) { return bitsAt(number, 1); }
	F& withRange(int low, int high) { minimum = low; maximum = high; return *this; }
	F& withDefault(int value) { defaultValue = value; return *this; }
	F& withStep(int value) { step = value; return *this; }
	F& withSuffix(Label text) { suffix = text; return *this; }
	F& withLink(const char* id) { link = id; return *this; }
	F& withLimit(int characters) { limit = characters; return *this; }
	F& withItems(Items more) { items.insert(items.end(), more.begin(), more.end()); return *this; }
	/** The range in the games of the Maniac Patch (by default RPG Maker's,
	 *  the range of the variables widened to int32's) */
	F& maniacRange(int low, int high) { maniacMinimum = low; maniacMaximum = high; return *this; }
	/** Radio buttons in columns in the games of the Maniac Patch (its items shown) */
	F& maniacColumns(int count) { maniacColumnCount = count; return *this; }
	F& when(const char* id, std::vector<int> values) { visible.push_back({id, std::move(values), false}); return *this; }
	F& unless(const char* id, std::vector<int> values) { visible.push_back({id, std::move(values), true}); return *this; }
	F& enabledWhen(const char* id, std::vector<int> values) { enabled.push_back({id, std::move(values), false}); return *this; }
	F& enabledUnless(const char* id, std::vector<int> values) { enabled.push_back({id, std::move(values), true}); return *this; }
};

F field(Kind kind, const char* id, int index, Label label) {
	F result;
	result.kind = kind;
	result.id = id;
	result.index = index;
	result.label = label;
	return result;
}

F section(Label title) {
	return field(Kind::Section, "", StringIndex, title);
}

F note(Label text) {
	return field(Kind::Note, "", StringIndex, text);
}

/** RPG Maker's sample of the fields of the section (Kind::Sample) */
F sample(const char* style) {
	auto result = field(Kind::Sample, "sample", StringIndex, {});
	result.link = style;
	return result;
}

F number(const char* id, int index, Label label, int minimum, int maximum) {
	auto result = field(Kind::Number, id, index, label);
	result.minimum = minimum;
	result.maximum = maximum;
	result.defaultValue = std::clamp(0, minimum, maximum);
	return result;
}

F check(const char* id, int index, Label label) {
	return field(Kind::Check, id, index, label);
}

F choice(const char* id, int index, Label label, Items items) {
	auto result = field(Kind::Choice, id, index, label);
	result.items = std::move(items);
	return result;
}

/** An ID of the database, the first entry by default */
F ref(Kind kind, const char* id, int index, Label label = {}) {
	return field(kind, id, index, label).withDefault(1);
}

F variable(const char* id, int index, Label label = {}) {
	return ref(Kind::Variable, id, index, label);
}

/** The events of the map after the special characters */
F character(const char* id, int index, Label label, Items specials) {
	auto result = field(Kind::Character, id, index, label);
	result.items = std::move(specials);
	result.defaultValue = result.items.empty() ? 1 : result.items.front().value;
	return result;
}

F time(const char* id, int index, Label label = {}) {
	// minutes 0-99, seconds 0-59
	return field(Kind::Time, id, index, label).withRange(0, 99 * 60 + 59);
}

F string(const char* id, Label label = {}) {
	return field(Kind::String, id, StringIndex, label);
}

F file(const char* id, Label label, const char* folder) {
	auto result = field(Kind::File, id, StringIndex, label);
	result.folder = folder;
	return result;
}

F graphic(Kind kind, const char* id, int index, const char* folder) {
	auto result = field(kind, id, index, {});
	result.folder = folder;
	return result;
}

void append(Fields& list, const Field& one) {
	list.push_back(one);
}

void append(Fields& list, const Fields& more) {
	list.insert(list.end(), more.begin(), more.end());
}

/** The fields and groups of fields of a schema, in their order */
template <typename... Parts>
Fields fields(const Parts&... parts) {
	Fields list;
	(append(list, parts), ...);
	return list;
}

/** Fields shown only when the field id has one of values */
Fields onlyWhen(Fields list, const char* id, const std::vector<int>& values) {
	for (auto& one : list) {
		one.visible.push_back({id, values, false});
	}
	return list;
}

/** Fields of one engine */
Fields only(Fields list, Flags engine) {
	for (auto& one : list) {
		one.flags |= engine;
	}
	return list;
}

/** Items "Monster 1".. (text with %1) for the values first.. */
Items numbered(Label text, int count, int first, int firstNumber = 1) {
	Items items;
	for (int i = 0; i < count; ++i) {
		items.push_back({first + i, Label(text.context, text.text, firstNumber + i)});
	}
	return items;
}

Items operator+(Items first, const Items& second) {
	first.insert(first.end(), second.begin(), second.end());
	return first;
}

/*
 * The shared parts of the command editors
 */

// The party, a hero, or the hero in a variable (mode at index, hero at
// index + 1)
Fields partyOperand(Label group, int index, bool pointer = false) {
	Items targets = {
		{0, in("PartyOperandWidget", QT_TRANSLATE_NOOP("PartyOperandWidget", "Entire Party"))},
		{1, in("PartyOperandWidget", QT_TRANSLATE_NOOP("PartyOperandWidget", "Specific"))},
		{2, in("PartyOperandWidget", QT_TRANSLATE_NOOP("PartyOperandWidget", "By Variable"))}};
	if (pointer) {
		// Maniac Patch (Change Condition): 3, the hero in a variable pointer
		targets.push_back({3, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable pointer")), Flag::Maniac});
	}
	Fields list = fields(
		section(group),
		choice("target", index, {}, targets).radio(),
		ref(Kind::Actor, "actor", index + 1).when("target", {1}).inlined(),
		variable("actorVariable", index + 1).when("target", {2}).inlined());
	if (pointer) {
		append(list, variable("actorPointer", index + 1).when("target", {3}).inlined().maniac());
	}
	return list;
}

/** The Maniac Patch's mode of a value in a variable pointer: V[V[x]] */
Item pointerItem() {
	return {2, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable pointer")), Flag::Maniac};
}

// A constant or a variable (mode at index, value at index + 1); with the
// Maniac Patch, a variable pointer
Fields valueOperand(const char* id, Label group, int index, int minimum = -9999999, int maximum = 9999999) {
	const char* operand = name(id, "Operand");
	return fields(
		section(group),
		choice(operand, index, {}, {
			{0, in("ValueOperandWidget", QT_TRANSLATE_NOOP("ValueOperandWidget", "Constant"))},
			{1, in("ValueOperandWidget", QT_TRANSLATE_NOOP("ValueOperandWidget", "By Variable"))},
			pointerItem()}).radio(),
		number(id, index + 1, {}, minimum, maximum).when(operand, {0}).inlined(),
		variable(name(id, "Variable"), index + 1).when(operand, {1}).inlined(),
		variable(name(id, "Pointer"), index + 1).when(operand, {2}).inlined().maniac());
}

// An entry of the database or the one in a variable; with the Maniac Patch,
// a variable pointer (pointer: EasyRPG Player reads it)
Fields entryOperand(Kind kind, const char* id, Label group, int index, bool pointer = true) {
	const char* operand = name(id, "Operand");
	Items items = {
		{0, in("OperandWidget", QT_TRANSLATE_NOOP("OperandWidget", "Specific"))},
		{1, in("OperandWidget", QT_TRANSLATE_NOOP("OperandWidget", "By Variable"))}};
	Fields list = fields(
		section(group),
		choice(operand, index, {}, pointer ? items + Items{pointerItem()} : items).radio(),
		ref(kind, id, index + 1).when(operand, {0}).inlined(),
		variable(name(id, "Variable"), index + 1).when(operand, {1}).inlined());
	if (pointer) {
		append(list, variable(name(id, "Pointer"), index + 1).when(operand, {2}).inlined().maniac());
	}
	return list;
}

// A switch, a range of them, or the one in a variable
// (mode at index, switches from index + 1)
Fields switchOperand(Label group, int index) {
	return fields(
		section(group),
		choice("switchOperand", index, {}, {
			{0, in("OperationOperandWidget", QT_TRANSLATE_NOOP("OperationOperandWidget", "Specific"))},
			{1, in("OperationOperandWidget", QT_TRANSLATE_NOOP("OperationOperandWidget", "Range"))},
			{2, in("OperationOperandWidget", QT_TRANSLATE_NOOP("OperationOperandWidget", "By Variable"))}}).radio(),
		ref(Kind::Switch, "switch", index + 1).when("switchOperand", {0}).inlined(),
		// the first and last switch of the range
		number("first", index + 1, in("OperationOperandWidget", QT_TRANSLATE_NOOP("OperationOperandWidget", "From")), 1, 9999)
			.withDefault(1).when("switchOperand", {1}).inlined(),
		number("last", index + 2, in("OperationOperandWidget", QT_TRANSLATE_NOOP("OperationOperandWidget", "To")), 1, 9999)
			.withDefault(1).when("switchOperand", {1}).inlined(),
		variable("switchVariable", index + 1).when("switchOperand", {2}).inlined());
}

// A time or the seconds in a variable
Fields timerOperand(Label group, int index) {
	return fields(
		section(group),
		choice("timeOperand", index, {}, {
			{0, in("TimerOperandWidget", QT_TRANSLATE_NOOP("TimerOperandWidget", "Constant"))},
			{1, in("TimerOperandWidget", QT_TRANSLATE_NOOP("TimerOperandWidget", "By Variable"))},
			pointerItem()}).radio(),
		time("time", index + 1).when("timeOperand", {0}).inlined(),
		variable("timeVariable", index + 1).when("timeOperand", {1}).inlined(),
		variable("timePointer", index + 1).when("timeOperand", {2}).inlined().maniac());
}

// The file, then the volume, tempo and balance from volume
F audioFile(const char* folder) {
	return file("file", in("MediaAudioWidget", QT_TRANSLATE_NOOP("MediaAudioWidget", "File")), folder).with(Flag::Off);
}

F fadeIn(int index) {
	return number("fadeIn", index, in("MediaAudioWidget", QT_TRANSLATE_NOOP("MediaAudioWidget", "Fade-in Time")), 0, 10000)
		.withSuffix(in("MediaAudioWidget", QT_TRANSLATE_NOOP("MediaAudioWidget", " ms"))).withStep(1000);
}

Fields audio(int volume) {
	return fields(
		number("volume", volume, in("MediaAudioWidget", QT_TRANSLATE_NOOP("MediaAudioWidget", "Volume")), 0, 100)
			.withDefault(100).withSuffix(in("MediaAudioWidget", QT_TRANSLATE_NOOP("MediaAudioWidget", " %"))),
		number("tempo", volume + 1, in("MediaAudioWidget", QT_TRANSLATE_NOOP("MediaAudioWidget", "Tempo")), 50, 150)
			.withDefault(100).withSuffix(in("MediaAudioWidget", QT_TRANSLATE_NOOP("MediaAudioWidget", " %"))),
		number("balance", volume + 2, in("MediaAudioWidget", QT_TRANSLATE_NOOP("MediaAudioWidget", "Balance")), 0, 100)
			.withDefault(50));
}

// A location: a map (map < 0: the map of the event) and
// a cell, given or in variables (mode < 0: given; its bits: the low ones
// of the parameter, modeBits of them). The given one is shown as RPG Maker
// 2003's location box, its map and cell hidden. pointer: with the Maniac
// Patch, the variables can be pointers (2)
Fields location(int mode, int map, int x, int y, int modeBits = 32, bool pointer = false) {
	Fields list = fields(section(in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Location"))));
	if (mode >= 0) {
		// radio buttons under the title, as RPG Maker 2003
		Items items = {
			{0, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Specific Location"))},
			{1, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Location Referenced from Variables"))}};
		if (pointer) {
			items.push_back({2, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Location Referenced from Variable Pointers")),
				Flag::Maniac});
		}
		append(list, choice("locationMode", mode, {}, items).bitsAt(0, modeBits).radio());
	}
	const char* link = map >= 0 ? "map" : nullptr;
	Fields given;
	append(given, field(Kind::Location, "location", StringIndex, {}).withLink(link));
	if (map >= 0) {
		append(given, ref(Kind::Map, "map", map, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Map"))).hidden());
	}
	append(given, field(Kind::MapX, "x", x, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "X Coordinate")))
		.withRange(0, 9999).withLink(link).hidden());
	append(given, field(Kind::MapY, "y", y, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Y Coordinate")))
		.withRange(0, 9999).withLink(link).hidden());
	if (mode < 0) {
		append(list, given);
		return list;
	}
	append(list, onlyWhen(given, "locationMode", {0}));
	// any other mode reads the variables
	Fields variables;
	if (map >= 0) {
		append(variables, variable("mapVariable", map, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Map ID Stored in"))));
	}
	append(variables, variable("xVariable", x, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "X Coordinate Stored in"))));
	append(variables, variable("yVariable", y, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Y Coordinate Stored in"))));
	for (auto& one : variables) {
		one.visible.push_back({"locationMode", {0}, true});
	}
	append(list, variables);
	return list;
}

// The facing after a move: RPG Maker 2003, radio buttons (its Character
// Facing); without a label in a section of its own
F facing(int index, bool labelled = true) {
	return choice("facing", index, labelled ? in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Facing")) : Label(), {
		{0, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Retain Facing"))},
		{1, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Up"))},
		{2, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Right"))},
		{3, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Down"))},
		{4, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Left"))}}).radio().rpg2003();
}

// The special characters a location command can name
Items locationCharacters() {
	return {
		{10005, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "This Event"))},
		{10001, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Hero"))},
		{10002, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Boat"))},
		{10003, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Ship"))},
		{10004, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Airship"))}};
}

// ... and those the picture commands name (texts of context PictureWidget)
Items pictureCharacters() {
	return {
		{10001, in("PictureWidget", QT_TRANSLATE_NOOP("PictureWidget", "Hero"))},
		{10002, in("PictureWidget", QT_TRANSLATE_NOOP("PictureWidget", "Boat"))},
		{10003, in("PictureWidget", QT_TRANSLATE_NOOP("PictureWidget", "Ship"))},
		{10004, in("PictureWidget", QT_TRANSLATE_NOOP("PictureWidget", "Airship"))},
		{10005, in("PictureWidget", QT_TRANSLATE_NOOP("PictureWidget", "This Event"))}};
}

// The screen transitions, the erasing ones or the showing ones
Items transitions(bool show) {
	const auto t = [](const char* text) { return in("ScreenTransitionWidget", text); };
	Items items = show ? Items{
		{0, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Fade In"))},
		{1, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Reconstitute Blocks"))},
		{2, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Unwipe Downward"))},
		{3, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Unwipe Upward"))}}
	: Items{
		{0, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Fade Out"))},
		{1, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Remove Blocks"))},
		{2, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Wipe Downward"))},
		{3, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Wipe Upward"))}};
	items = items + Items{
		{4, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Venetian Blinds"))},
		{5, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Vertical Blinds"))},
		{6, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Horizontal Blinds"))},
		{7, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Receding Square"))},
		{8, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Expanding Square"))},
		{9, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Screen Moves Up"))},
		{10, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Screen Moves Down"))},
		{11, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Screen Moves Left"))},
		{12, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Screen Moves Right"))}};
	items = items + (show ? Items{
		{13, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Vertical Unify"))},
		{14, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Horizontal Unify"))},
		{15, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Unify Quadrants"))},
		{16, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Zoom Out"))}}
	: Items{
		{13, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Vertical Division"))},
		{14, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Horizontal Division"))},
		{15, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Quadrasection"))},
		{16, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Zoom In"))}});
	return items + Items{
		{17, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Mosaic"))},
		{18, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Waver Screen"))},
		{19, t(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Instantaneous"))}};
}

// The monsters of the troop, from 0
F monster(const char* id, int index) {
	return choice(id, index, {}, numbered(in("SceneCommandWidget", QT_TRANSLATE_NOOP("SceneCommandWidget", "Monster %1")), 8, 0));
}

/*
 * Maniac Patch: most values have a mode (a number, a variable, a variable
 * pointer...) packed 4 bits per value in another parameter
 */

struct Mode {
	int mode;
	Label label;
	Kind kind;
};
using Modes = std::vector<Mode>;

Label maniac(const char* text, int number = -1) {
	return in("ManiacCommandWidget", text, number);
}

Modes numberModes() {
	return {{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Constant")), Kind::Number},
		{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable")), Kind::Variable},
		{2, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable pointer")), Kind::Variable}};
}

Modes targetModes() {
	return {{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable")), Kind::Variable},
		{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable pointer")), Kind::Variable}};
}

Modes stringModes() {
	return {{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Text")), Kind::String},
		{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "String")), Kind::StringId},
		{2, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "String pointer")), Kind::Variable}};
}

Modes operator+(Modes first, const Modes& second) {
	first.insert(first.end(), second.begin(), second.end());
	return first;
}

/** The mode of a value: in the nibble slot of the parameter, slot < 0: the whole parameter */
F mode(const char* id, Label label, int index, int slot, const Modes& modes) {
	Items items;
	for (const auto& one : modes) {
		items.push_back({one.mode, one.label});
	}
	auto result = choice(id, index, label, std::move(items));
	if (slot >= 0) {
		result.bitsAt(slot * 4, 4);
	}
	return result;
}

/** The value after its mode: one field per
 *  kind of value, shown for its modes. Without a label: on the row of the mode */
Fields modeValue(const char* id, const char* modeId, int index, int minimum, int maximum, const Modes& modes,
	Label label = {}) {
	std::vector<Kind> kinds;
	for (const auto& one : modes) {
		if (std::find(kinds.begin(), kinds.end(), one.kind) == kinds.end()) {
			kinds.push_back(one.kind);
		}
	}
	Fields list;
	for (Kind kind : kinds) {
		std::vector<int> values;
		for (const auto& one : modes) {
			if (one.kind == kind) {
				values.push_back(one.mode);
			}
		}
		F value;
		switch (kind) {
			case Kind::Number:
				value = number(id, index, {}, minimum, maximum);
				break;
			case Kind::Variable:
				value = variable(name(id, "Variable"), index);
				break;
			case Kind::StringId:
				value = number(name(id, "String"), index, {}, 0, 99999);
				value.kind = Kind::StringId;
				break;
			default:
				// the text is the string of the command
				value = string(name(id, "Text"));
				break;
		}
		if (label.isEmpty()) {
			value.inlined();
		}
		value.label = label;
		if (kinds.size() > 1) {
			value.when(modeId, values);
		}
		list.push_back(value);
	}
	return list;
}

/** A mode and its value on one row */
Fields value(const char* id, Label label, int modeIndex, int slot, int index,
	int minimum = -99999999, int maximum = 99999999, const Modes& modes = numberModes()) {
	const char* modeId = name(id, "Mode");
	return fields(mode(modeId, label, modeIndex, slot, modes), modeValue(id, modeId, index, minimum, maximum, modes));
}

/** X, Y, width and height, their modes in the nibbles first.. of modeIndex */
Fields rectangle(const char* prefix, int modeIndex, int first, int index) {
	return fields(
		value(name(prefix, "X"), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "X")), modeIndex, first, index),
		value(name(prefix, "Y"), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Y")), modeIndex, first + 1, index + 1),
		value(name(prefix, "Width"), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Width")), modeIndex, first + 2, index + 2),
		value(name(prefix, "Height"), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Height")), modeIndex, first + 3, index + 3));
}

Items battleTargets() {
	return {{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Hero"))},
		{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Party member"))},
		{2, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Entire party"))},
		{3, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Enemy"))},
		{4, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "All enemies"))}};
}

Items layers() {
	return {{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Lower layer"))},
		{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Upper layer"))}};
}

/*
 * The Maniac Patch in the commands of RPG Maker: a value can be given by a
 * variable, a variable pointer, a string variable... its mode in a nibble
 * of another parameter (0 in the other games: RPG Maker's value). The
 * editors show the mode before the value in its games, the fields of the
 * other kinds of values in its place
 */

/** value with its modes, in the nibble slot of parameter modeIndex (slot < 0:
 *  the whole parameter). The variables and strings of the other modes are in
 *  parameter index, else the value's (a text: in a parameter of its own) */
Fields modal(F value, int modeIndex, int slot, const Modes& modes, int index = -1) {
	const char* modeId = name(value.id, "Mode");
	value.mode = modeId;
	// shown with the value: its default written with it (the parameter of
	// the mode has other uses where the value is not shown)
	F modeField = mode(modeId, value.label, modeIndex, slot, modes).hidden().maniac().extension();
	modeField.visible = value.visible;
	modeField.enabled = value.enabled;
	Fields list = fields(modeField, value);
	std::vector<Kind> kinds;
	for (const auto& one : modes) {
		if (one.mode != 0 && std::find(kinds.begin(), kinds.end(), one.kind) == kinds.end()) {
			kinds.push_back(one.kind);
		}
	}
	for (Kind kind : kinds) {
		std::vector<int> values;
		for (const auto& one : modes) {
			if (one.mode != 0 && one.kind == kind) {
				values.push_back(one.mode);
			}
		}
		const int at = index >= 0 ? index : value.index;
		F other = kind == Kind::StringId ? field(Kind::StringId, name(value.id, "String"), at, value.label).withRange(0, 99999).withDefault(1)
			: kind == Kind::Switch ? ref(Kind::Switch, name(value.id, "Switch"), at, value.label)
			: variable(name(value.id, "Variable"), at, value.label);
		other.replaces = value.id;
		other.visible = value.visible;
		other.enabled = value.enabled;
		append(list, other.when(modeId, values).hidden().maniac().extension());
	}
	return list;
}

/** A number or an ID of RPG Maker, or the one in a variable or a variable pointer */
Fields modalNumber(F value, int modeIndex, int slot) {
	return modal(std::move(value), modeIndex, slot, numberModes());
}

/** A text or a file of RPG Maker (the string of the command), or a string
 *  variable (T[x], T[V[x]]: in parameter index) */
Fields modalText(F value, int modeIndex, int slot, int index) {
	return modal(std::move(value), modeIndex, slot, stringModes(), index);
}

/** The fields of a music or a sound with their modes, one per field in the
 *  nibbles of parameter modes from the first one (the file's: its string
 *  variable in parameter string) */
Fields withModes(const Fields& list, int modes, int string) {
	Fields result;
	int slot = 0;
	for (const auto& one : list) {
		if (!one.hasValue()) {
			append(result, one);
			continue;
		}
		F value;
		static_cast<Field&>(value) = one;
		append(result, one.usesString() ? modalText(value, modes, slot, string) : modalNumber(value, modes, slot));
		++slot;
	}
	return result;
}

/*
 * The table
 */

class Table {
public:
	/** A schema: the labels without a context get context, the one of its editor */
	void add(Code code, const char* context, Label title, Fields list, Flags flags = {}, bool custom = false) {
		Schema schema;
		schema.code = code;
		schema.title = resolve(title, context);
		schema.fields = std::move(list);
		schema.flags = flags;
		schema.custom = custom;
		// the notes get ids of their own, for their rules
		static const char* const notes[] = {"note1", "note2", "note3", "note4", "note5", "note6", "note7", "note8",
			"note9", "note10", "note11", "note12", "note13", "note14", "note15", "note16"};
		size_t note = 0;
		for (auto& one : schema.fields) {
			if (one.kind == Kind::Note && !*one.id && note < std::size(notes)) {
				one.id = notes[note++];
			}
			one.label = resolve(one.label, context);
			one.suffix = resolve(one.suffix, context);
			for (auto& item : one.items) {
				item.text = resolve(item.text, context);
			}
			if (one.maniacMinimum == 0 && one.maniacMaximum == 0) {
				// the Maniac Patch takes any int32 where RPG Maker takes the
				// values of its variables
				constexpr int variables = 9999999;
				one.maniacMinimum = one.minimum <= -variables ? std::numeric_limits<int32_t>::min() : one.minimum;
				one.maniacMaximum = one.maximum >= variables ? std::numeric_limits<int32_t>::max() : one.maximum;
			}
		}
		m_schemas.push_back(std::move(schema));
	}

	void custom(Code code, const char* context, Label title, Fields list, Flags flags = {}) {
		add(code, context, title, std::move(list), flags, true);
	}

	std::vector<Schema> take() {
		return std::move(m_schemas);
	}

private:
	static Label resolve(Label label, const char* context) {
		if (!label.isEmpty() && !label.context) {
			label.context = context;
		}
		return label;
	}

	std::vector<Schema> m_schemas;
};

// RPG Maker 2003 battle commands
void addBattle2003(Table& table) {
	// Call Common Event
	table.add(Code::CallCommonEvent, "CallCommonEventWidget", QT_TRANSLATE_NOOP("CallCommonEventWidget", "Call Common Event"), fields(
		section(QT_TRANSLATE_NOOP("CallCommonEventWidget", "Event to Call")),
		ref(Kind::CommonEvent, "commonEvent", 0)
	), Flag::Rpg2003);

	// Force Flee
	table.add(Code::ForceFlee, "ForceFleeWidget", QT_TRANSLATE_NOOP("ForceFleeWidget", "Force Flee"), fields(
		section(QT_TRANSLATE_NOOP("ForceFleeWidget", "Target")),
		choice("target", 0, {}, {
			{0, QT_TRANSLATE_NOOP("ForceFleeWidget", "Party Flees")},
			{1, QT_TRANSLATE_NOOP("ForceFleeWidget", "All Monsters Flee")},
			{2, QT_TRANSLATE_NOOP("ForceFleeWidget", "Only this Monster Flees")}}).radio(),
		// Maniac Patch: the monster's mode in p3
		modalNumber(monster("monster", 1).enabledWhen("target", {2}).inlined(), 3, 0),
		section(QT_TRANSLATE_NOOP("ForceFleeWidget", "Option")),
		// checked (1, as RPG Maker 2003 writes it), a surrounded target (the
		// party: in a pincers attack) does not flee
		check("ignoreSurrounded", 2, QT_TRANSLATE_NOOP("ForceFleeWidget", "Ignore if Target is Surrounded"))
	), Flag::Rpg2003);

	// Enable Combo
	table.add(Code::EnableCombo, "EnableComboWidget", QT_TRANSLATE_NOOP("EnableComboWidget", "Enable Combo"), fields(
		section(QT_TRANSLATE_NOOP("EnableComboWidget", "Parameters")),
		// Maniac Patch: their modes in p3
		modalNumber(ref(Kind::Actor, "hero", 0, QT_TRANSLATE_NOOP("EnableComboWidget", "Target Hero")), 3, 0),
		modalNumber(ref(Kind::BattleCommand, "command", 1, QT_TRANSLATE_NOOP("EnableComboWidget", "Target Battle Command")), 3, 1),
		modalNumber(number("repetitions", 2, QT_TRANSLATE_NOOP("EnableComboWidget", "Number of Repetitions"), 1, 99), 3, 2)
	), Flag::Rpg2003);

	// Change Class
	table.add(Code::ChangeClass, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Class"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Class")),
		// Maniac Patch: its mode in nibble 1 of p7
		modalNumber(ref(Kind::Class, "class", 2)
			.withItems({{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "(None)")}}).withDefault(0), 7, 1),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Level")),
		choice("level", 3, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Retain Current Level")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Character Becomes Level 1")}}).inColumns(1),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Learned Skills")),
		choice("skills", 4, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "No Change in Skills")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Add Skills up to Current Level in New Class and Delete Old Skills")},
			{2, QT_TRANSLATE_NOOP("HeroCommandWidget", "Add Skills up to Current Level in New Class to Old Skills")}}).inColumns(1),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Base Statistics")),
		choice("statistics", 5, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "No Change in Statistics")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Halve Statistics")},
			{2, QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Statistics to Level 1 in New Class")},
			{3, QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Statistics to Current Level in New Class")}}).inColumns(1),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Option")),
		check("message", 6, QT_TRANSLATE_NOOP("HeroCommandWidget", "Show Level-up Message if Level Differs After Class Change"))
	), Flag::Rpg2003);

	// Change Battle Commands
	table.add(Code::ChangeBattleCommands, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Battle Commands"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Battle Command")),
		// "All" (0) only means something for Remove
		// Maniac Patch: its mode in nibble 1 of p4
		modalNumber(ref(Kind::BattleCommand, "command", 2, QT_TRANSLATE_NOOP("HeroCommandWidget", "Command:"))
			.withItems({{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "All")}}), 4, 1),
		choice("operation", 3, QT_TRANSLATE_NOOP("HeroCommandWidget", "Operation:"), {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Remove")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Add")}})
	), Flag::Rpg2003);
}


Items maniacOrigins() {
	return {{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Center"))}, {1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Top left"))},
		{2, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Bottom left"))}, {3, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Top right"))},
		{4, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Bottom right"))}, {5, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Top"))},
		{6, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Bottom"))}, {7, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Left"))},
		{8, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Right"))}};
}

// The values of the Maniac Patch are int32s
constexpr int int32Min = std::numeric_limits<int32_t>::min();
constexpr int int32Max = std::numeric_limits<int32_t>::max();

/** A constant or a variable holding it (no pointer) */
Modes constantOrVariable() {
	return {{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Constant")), Kind::Number},
		{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable")), Kind::Variable}};
}

/** A string variable: its number, or the number in a variable */
Modes stringIdModes() {
	return {{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "String")), Kind::StringId},
		{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "String pointer")), Kind::Variable}};
}

/** The string variables of the fields up to the largest int32, from minimum */
Fields anyString(Fields list, int minimum = 0) {
	for (auto& one : list) {
		if (one.kind == Kind::StringId) {
			one.minimum = minimum;
			one.maximum = int32Max;
			one.defaultValue = std::max(one.defaultValue, minimum);
		}
	}
	return list;
}

F stringId(const char* id, int index, Label label = {}) {
	return field(Kind::StringId, id, index, label).withRange(0, int32Max);
}

/** A mode and its value, an int32 by default */
Fields int32Value(const char* id, Label label, int modeIndex, int slot, int index, const Modes& modes = numberModes(),
	int minimum = int32Min, int maximum = int32Max) {
	return anyString(value(id, label, modeIndex, slot, index, minimum, maximum, modes));
}

/** X, Y, width and height (int32s), their modes in the nibbles first.. of modeIndex */
Fields maniacArea(const char* prefix, int modeIndex, int first, int index) {
	return fields(
		int32Value(name(prefix, "X"), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "X")), modeIndex, first, index),
		int32Value(name(prefix, "Y"), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Y")), modeIndex, first + 1, index + 1),
		int32Value(name(prefix, "Width"), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Width")), modeIndex, first + 2, index + 2),
		int32Value(name(prefix, "Height"), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Height")), modeIndex, first + 3, index + 3));
}

/** A picture number given by a mode */
Fields pictureNumber(const char* id, int modeIndex, int slot, int index) {
	return int32Value(id, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture No.")), modeIndex, slot, index, numberModes(), 1, int32Max);
}

/** The constant of a value written when its fields are shown */
Fields startingAt(Fields list, int constant) {
	for (auto& one : list) {
		if (one.kind == Kind::Number) {
			one.defaultValue = constant;
		}
	}
	return list;
}

/** An entry of the database (kind, after the entries specials), or its ID
 *  in a variable or a variable pointer: the mode in the nibble slot of
 *  modeIndex, the value on its row */
Fields entryValue(const char* id, Label label, int modeIndex, int slot, int index, Kind kind, Label entry, Items specials = {}) {
	const char* modeId = name(id, "Mode");
	auto given = ref(kind, id, index).withItems(specials).inlined().when(modeId, {0});
	if (!specials.empty()) {
		given.withDefault(specials.front().value);
	}
	return fields(
		mode(modeId, label, modeIndex, slot, {{0, entry, kind},
			{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable")), Kind::Variable},
			{2, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable pointer")), Kind::Variable}}),
		given,
		variable(name(id, "Variable"), index).inlined().when(modeId, {1, 2}));
}

/** Control Self Variable: the owner of switches, variables and strings (the
 *  game, or an event: its own ones) in the nibble slot of parameter 0, the
 *  modes of its event (index) and map (index + 1) in the next nibbles */
Fields selfOwner(const char* id, Label label, int slot, int index) {
	return fields(
		choice(id, 0, label, {
			{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "The game"))},
			{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common event"))},
			{2, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Map event"))},
			{3, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Current event"))},
			{4, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Calling event"))}}).bitsAt(slot * 4, 4),
		onlyWhen(entryValue(name(id, "CommonEvent"), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common event")), 0, slot + 1, index,
			Kind::CommonEvent, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common event"))), id, {1}),
		onlyWhen(startingAt(int32Value(name(id, "Event"), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Event (10005: this event)")),
			0, slot + 1, index, numberModes(), 1, int32Max), 10005), id, {2}),
		onlyWhen(entryValue(name(id, "Map"), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Map")), 0, slot + 2, index + 1,
			Kind::Map, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Map"))), id, {2}));
}

// Maniac Patch: the layouts EasyRPG Player reads, else those of the EasyRPG
// wiki, of the Maniac Patch's own documents, editor dialogs and TPC; the
// titles of the palette
void addManiac(Table& table) {
	const char* context = "ManiacCommandWidget";

	// Get Save Info: the mode of the slot in nibble 0, 16: the name of the
	// first member to a string variable
	table.add(Code::Maniac_GetSaveInfo, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Get Save Info")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save File")),
		int32Value("slot", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save slot"), 0, 0, 1, constantOrVariable()),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store In")),
		variable("date", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Date (YYMMDD)")),
		variable("time", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Time (HHMMSS)")),
		variable("level", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Level")),
		variable("hp", 5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "HP")),
		check("storeName", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Name of the first member in a string")).bit(4),
		stringId("name", 6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Name")).withDefault(1).enabledUnless("storeName", {0}),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Faces as Pictures (0: none)")),
		// one mode for the four picture numbers
		mode("faceMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture numbers"), 7, -1, constantOrVariable()),
		modeValue("face1", "faceMode", 8, 0, int32Max, constantOrVariable(), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Face %1"), 1)),
		modeValue("face2", "faceMode", 9, 0, int32Max, constantOrVariable(), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Face %1"), 2)),
		modeValue("face3", "faceMode", 10, 0, int32Max, constantOrVariable(), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Face %1"), 3)),
		modeValue("face4", "faceMode", 11, 0, int32Max, constantOrVariable(), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Face %1"), 4))
	), Flag::Maniac);

	// Save Game
	table.add(Code::Maniac_Save, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Save Game")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save File")),
		int32Value("slot", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save slot"), 0, -1, 1, constantOrVariable(), 1, int32Max),
		check("storeResult", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store the result in a variable")),
		variable("result", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Result")).enabledUnless("storeResult", {0})
	), Flag::Maniac);

	// Load Game
	table.add(Code::Maniac_Load, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Load Game")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save File")),
		int32Value("slot", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save slot"), 0, -1, 1, constantOrVariable(), 1, int32Max),
		check("skipCheck", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Do not check that the save exists")),
		check("noBlackout", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Do not black out the screen"))
	), Flag::Maniac);

	// Get Mouse Position: parameter 2 unused
	table.add(Code::Maniac_GetMousePosition, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Get Mouse Position")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store In")),
		variable("x", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "X")),
		variable("y", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Y")),
		check("ignoreRatio", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Ignore the window size ratio"))
	), Flag::Maniac);

	// Set Mouse Position: not run by EasyRPG Player, the layout of the wiki (one mode for both)
	table.add(Code::Maniac_SetMousePosition, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Set Mouse Position")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Position")),
		mode("mode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Given as"), 0, 0, constantOrVariable()),
		modeValue("x", "mode", 1, int32Min, int32Max, constantOrVariable(), QT_TRANSLATE_NOOP("ManiacCommandWidget", "X")),
		modeValue("y", "mode", 2, int32Min, int32Max, constantOrVariable(), QT_TRANSLATE_NOOP("ManiacCommandWidget", "Y")),
		check("ignoreRatio", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Ignore the window size ratio"))
	), Flag::Maniac);

	// Show String Picture: its string holds the text, the system graphic and
	// the font, each after its mode + 1 ("\x01text\x01system\x01font")
	table.custom(Code::Maniac_ShowStringPicture, "ManiacShowStringPictureWidget",
		palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Show String Picture")), fields(
		section(QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Text")),
		string("text", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Text")),
		// the string of the text when its mode (in the string) is not 0
		stringId("textId", 22),
		section(QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Font")),
		value("fontSize", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Size"), 0, 5, 20, 0, int32Max),
		number("letterSpacing", 14, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Letter spacing"), 0, 255).bitsAt(8, 8),
		number("lineSpacing", 14, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Line spacing"), 0, 255).bitsAt(16, 8),
		check("bold", 13, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Bold")).bit(15),
		check("noShadow", 13, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "No shadow")).bit(14),
		check("noGradient", 13, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "No gradient")).bit(13),
		section(QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Window")),
		mode("sizeMode", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Size given as"), 0, 4, numberModes()),
		modeValue("width", "sizeMode", 18, 0, int32Max, numberModes(), QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Width (0: fit the text)")),
		modeValue("height", "sizeMode", 19, 0, int32Max, numberModes(), QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Height (0: fit the text)")),
		choice("background", 13, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Background"), {
			{0, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "None")},
			{1, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Stretched")},
			{2, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Tiled")}}).bitsAt(8, 4),
		check("translucent", 14, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Translucent background")).bit(0),
		check("noFrame", 13, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "No frame")).bit(12),
		check("noPadding", 13, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "No padding")).bit(16),
		section(QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Picture")),
		value("picture", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Picture No."), 0, 0, 1, 1, int32Max),
		mode("positionMode", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Position given as"), 0, 1, numberModes()),
		modeValue("x", "positionMode", 2, int32Min, int32Max, numberModes(), QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "X")),
		modeValue("y", "positionMode", 3, int32Min, int32Max, numberModes(), QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Y")),
		choice("origin", 0, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Origin"), {
			{0, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Center")}, {1, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Top left")},
			{2, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Bottom left")}, {3, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Top right")},
			{4, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Bottom right")}, {5, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Top")},
			{6, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Bottom")}, {7, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Left")},
			{8, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Right")}}).bitsAt(24, 4),
		check("scroll", 13, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Scrolls with the map")).bit(0),
		number("mapLayer", 15, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Map layer"), 0, 10),
		number("battleLayer", 16, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Battle layer"), 0, 10),
		section(QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Magnification and Transparency")),
		value("magnification", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Magnification (%)"), 0, 2, 4, 0, int32Max),
		check("separateHeight", 0, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Height magnified apart")).bit(28),
		// the mode of the height: the 3 highest bits, after this flag
		mode("heightMode", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Height (%)"), 0, -1, numberModes())
			.bitsAt(29, 3).enabledUnless("separateHeight", {0}),
		modeValue("heightMagnification", "heightMode", 23, 0, int32Max, numberModes()),
		value("transparency", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Transparency (%)"), 0, 3, 5, 0, 100),
		section(QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Options")),
		check("eraseOnMapChange", 17, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Erased when the map changes")).bit(0),
		check("eraseOnBattleEnd", 17, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Erased when the battle ends")).bit(1),
		check("tint", 17, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Affected by Tint Screen")).bit(4),
		check("flash", 17, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Affected by Flash Screen")).bit(5),
		check("shake", 17, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Affected by Shake Screen")).bit(6),
		section(QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Color")),
		number("red", 6, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Red (%)"), 0, 200),
		number("green", 7, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Green (%)"), 0, 200),
		number("blue", 8, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Blue (%)"), 0, 200),
		number("saturation", 9, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Saturation (%)"), 0, 200),
		choice("blend", 12, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Blending"), {
			{0, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Normal")},
			{1, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Multiply")},
			{2, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Additive")},
			{3, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Overlay")}}).bitsAt(0, 4),
		check("flipX", 12, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Flip horizontally")).bit(4),
		check("flipY", 12, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Flip vertically")).bit(5),
		section(QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Effect")),
		choice("effect", 10, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Effect"), {
			{0, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "None")},
			{1, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Rotation")},
			{2, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Wave")},
			{3, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Fixed angle")}}),
		number("power", 11, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Power"), int32Min, int32Max).when("effect", {1, 2}),
		onlyWhen(int32Value("angle", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Angle"), 13, 6, 11), "effect", {3}),
		onlyWhen(int32Value("divisor", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Divided by"), 13, 7, 21), "effect", {3})
	), Flag::Maniac);

	// Get Picture Info
	table.add(Code::Maniac_GetPictureInfo, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Get Picture Info")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture")),
		pictureNumber("picture", 0, 0, 3),
		choice("info", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Information"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Position and original size")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Current position and size")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Target position and size")},
			{3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Pixels")}}),
		choice("origin", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Position of"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Center")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Top left")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Edges (left, top, right, bottom)")}}).when("info", {0, 1, 2}),
		check("effects", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "With the effects (tone, flash...)")).bit(0).when("info", {3}),
		check("ignoreAlpha", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Ignore alpha")).bit(1).when("info", {3}),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store In")),
		variable("storeX", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "X")).when("info", {0, 1, 2}),
		variable("storeY", 5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Y")).when("info", {0, 1, 2}),
		variable("storeWidth", 6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Width")).when("info", {0, 1, 2}),
		variable("storeHeight", 7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Height")).when("info", {0, 1, 2}),
		// pixels: an area of the picture, written from a variable on
		onlyWhen(maniacArea("pixel", 0, 1, 4), "info", {3}),
		onlyWhen(value("pixels", QT_TRANSLATE_NOOP("ManiacCommandWidget", "First variable"), 0, 5, 8, 1, int32Max, targetModes()), "info", {3})
	), Flag::Maniac);

	// Control Battle
	table.add(Code::Maniac_ControlBattle, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Control Battle")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Battle Event")),
		choice("hook", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "When"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "ATB gauge increases")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Damage pops up")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Targets are chosen")},
			{3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "A condition changes")},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "A parameter changes")}}),
		int32Value("commonEvent", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common event (0: none)"), 1, -1, 2, numberModes(), 0, int32Max),
		variable("variable", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "First variable"))
	), Flag::Maniac);

	// Control ATB Gauge
	table.add(Code::Maniac_ControlAtbGauge, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Control ATB Gauge")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Target")),
		choice("target", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Target"), battleTargets()),
		onlyWhen(int32Value("targetId", QT_TRANSLATE_NOOP("ManiacCommandWidget", "ID or position"), 1, -1, 2), "target", {0, 1, 3}),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation")),
		choice("operation", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Set")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Increase")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Decrease")}}),
		int32Value("value", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value"), 5, -1, 6),
		choice("unit", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Unit"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Points")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Percent of the gauge")}})
	), Flag::Maniac);

	// Change Battle Commands Ex
	table.add(Code::Maniac_ChangeBattleCommandEx, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Battle Commands Ex")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Hero Commands")),
		check("noRow", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Remove the Row command")),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Party Commands")),
		check("noFight", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Remove Fight")).bit(0),
		check("noAuto", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Remove Auto Battle")).bit(1),
		check("noEscape", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Remove Escape")).bit(2),
		check("win", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Add Win")).bit(3),
		check("lose", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Add Lose")).bit(4)
	), Flag::Maniac);

	// Get Battle Info
	table.add(Code::Maniac_GetBattleInfo, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Get Battle Info")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Target")),
		choice("target", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Target"), battleTargets()),
		onlyWhen(int32Value("targetId", QT_TRANSLATE_NOOP("ManiacCommandWidget", "ID or position"), 2, -1, 3), "target", {0, 1, 3}),
		choice("info", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Information"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Parameter changes")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Conditions")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Attribute resistances")},
			{3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Position and state")}}).when("target", {0, 1, 3}),
		choice("groupInfo", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Information"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Number of members")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Number of members alive")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Number of members able to act")}}).when("target", {2, 4}),
		variable("variable", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "First variable"))
	), Flag::Maniac);

	// Control Variable Array: the modes of A, the length and B in nibbles 0-2
	table.add(Code::Maniac_ControlVarArray, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Control Variable Array")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable Arrays")),
		choice("operation", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Copy A to B")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Swap")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Sort ascending")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Sort descending")},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Shuffle")}, {5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Enumerate")},
			{6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Add B")}, {7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Subtract B")},
			{8, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Multiply by B")}, {9, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Divide by B")},
			{10, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Modulo B")}, {11, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Bitwise OR")},
			{12, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Bitwise AND")}, {13, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Bitwise XOR")},
			{14, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Shift left")}, {15, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Shift right")},
			{16, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Dereference B (A = V[B])")},
			{17, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Sort ascending, B in step")},
			{18, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Sort descending, B in step")},
			{19, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Shuffle, B in step")},
			{20, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Fill with a value")}, {21, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Reverse")},
			{22, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Copy B to A")}}),
		value("arrayA", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Array A from"), 1, 0, 2, 1, int32Max, targetModes()),
		int32Value("length", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Length"), 1, 1, 3),
		// the mode of B is the one of the first value
		onlyWhen(value("arrayB", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Array B from"), 1, 2, 4, 1, int32Max, targetModes()),
			"operation", {0, 1, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 22}),
		onlyWhen(int32Value("start", QT_TRANSLATE_NOOP("ManiacCommandWidget", "First value"), 1, 2, 4), "operation", {5}),
		onlyWhen(int32Value("fill", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value"), 1, 2, 4), "operation", {20}),
		note(QT_TRANSLATE_NOOP("ManiacCommandWidget", "In step: B gets the moves of A (each value of B stays with the one of A at "
			"its place).")).when("operation", {17, 18, 19})
	), Flag::Maniac);

	// Key Input Processing Ex: 0 reads the keys of the list with the keys
	// given to the joypad buttons, 1 without them
	table.add(Code::Maniac_KeyInputProcEx, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Key Input Processing Ex")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Keys")),
		choice("operation", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Keyboard keys (with the joypad assignment)")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Keyboard keys (without the joypad assignment)")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "One key, by its code")},
			{3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Joypad buttons")},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Get the joypad assignment")},
			{5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Set the joypad assignment")}}),
		onlyWhen(value("key", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Virtual key code"), 2, -1, 3, 0, 255), "operation", {2}),
		variable("variable", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store in (from)")).unless("operation", {5}),
		variable("source", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Read from (from)")).when("operation", {5}),
		note(QT_TRANSLATE_NOOP("ManiacCommandWidget", "A key pressed gives 1. The assignment: per key of the list, the joypad button "
			"given to it (-1: none).")).when("operation", {0, 1, 4, 5})
	), Flag::Maniac);

	// Rewrite Map: one tile (a constant, a variable, a pointer), or one per
	// cell from the variables from the tile value (a variable, a pointer)
	table.add(Code::Maniac_RewriteMap, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Rewrite Map")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Tiles")),
		choice("layer", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Layer"), layers()),
		choice("fromVariables", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Tiles"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "The same tile everywhere")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "A tile per cell, from variables")}}),
		onlyWhen(int32Value("tile", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Tile"), 0, 0, 3), "fromVariables", {0}),
		onlyWhen(value("tiles", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Tiles from"), 0, 0, 3, 1, int32Max, targetModes()),
			"fromVariables", {1}),
		check("noAutotile", 8, QT_TRANSLATE_NOOP("ManiacCommandWidget", "No autotile update")),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Area")),
		maniacArea("area", 0, 1, 4)
	), Flag::Maniac);

	// Control Global Save: the first switch or variable of the game and of
	// the global save, constants or variables; the count, also a pointer
	table.add(Code::Maniac_ControlGlobalSave, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Control Global Save")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Global Save")),
		choice("operation", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Open")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Close")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save and close")},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Copy to the game")}, {5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Copy from the game")}}),
		onlyWhen(fields(
			choice("type", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Data"), {
				{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Switches")},
				{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variables")},
				{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Strings")}}),
			int32Value("game", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Game from"), 1, 0, 3, constantOrVariable()),
			int32Value("global", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Global save from"), 1, 1, 4, constantOrVariable()),
			int32Value("count", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Count"), 1, 2, 5)), "operation", {4, 5})
	), Flag::Maniac);

	// Change Picture ID: the modes of the first picture, the count and the
	// target (or distance) in nibbles 0-2
	table.add(Code::Maniac_ChangePictureId, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Picture ID")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Pictures")),
		choice("operation", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Move")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Swap")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Slide")}}),
		int32Value("first", QT_TRANSLATE_NOOP("ManiacCommandWidget", "First picture"), 1, 0, 2),
		int32Value("count", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Count"), 1, 1, 3),
		onlyWhen(int32Value("target", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Target picture"), 1, 2, 4), "operation", {0, 1}),
		onlyWhen(int32Value("distance", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Distance"), 1, 2, 4), "operation", {2}),
		check("ignoreRange", 5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Ignore out of range"))
	), Flag::Maniac);

	// Set Game Option: the mode of the value in nibble 0 of parameter 0, of
	// the height of the faces in nibble 1
	table.add(Code::Maniac_SetGameOption, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Set Game Option")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Option")),
		choice("option", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Option"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Run when inactive")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Game speed")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture limit")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Frame skip")},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Mouse in messages")}, {5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Battle origin")},
			{6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Battle animation limit")}, {7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Face size")}}),
		// the options without a mode write 0 (the parameters of a fast forward: none)
		number("noMode", 0, {}, 0, 0).hidden().when("option", {0, 3, 4, 5}),
		number("fatal", 3, {}, 0, 0).hidden().when("option", {1}),
		choice("inactive", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "When the window is inactive"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Pause")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Keep running")}}).when("option", {0}),
		onlyWhen(startingAt(int32Value("fps", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Frames per second"), 0, 0, 2), 60), "option", {1}),
		onlyWhen(startingAt(int32Value("pictures", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Pictures"), 0, 0, 2), 1000), "option", {2}),
		choice("mouse", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Mouse in messages"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Allowed")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Not allowed")}}).when("option", {4}),
		onlyWhen(int32Value("animations", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Animations"), 0, 0, 2), "option", {6}),
		onlyWhen(startingAt(int32Value("faceWidth", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Width"), 0, 0, 2), 48), "option", {7}),
		onlyWhen(startingAt(int32Value("faceHeight", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Height"), 0, 1, 3), 48), "option", {7}),
		choice("testPlay", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Test play"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Keep")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Off")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "On")}}).bitsAt(0, 2).when("option", {1}),
		check("fastText", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Right Shift skips the messages")).bit(4).when("option", {1}),
		choice("frameSkip", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Frames skipped"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "None")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "1/5")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "1/3")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "1/2")}}).when("option", {3}),
		choice("origin", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Position"), maniacOrigins()).when("option", {5})
	), Flag::Maniac);

	// Call Command: the code, the string of the command called (a text, a
	// string), its parameters: variables from a variable (or a pointer), or
	// written in the command (CallCommandEditor): their values from
	// parameter 5, the modes of four of them per parameter from the one in
	// parameter 2; or expressions from parameter 5
	table.custom(Code::Maniac_CallCommand, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Call Command")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Command")),
		int32Value("code", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Command code"), 0, 0, 1, numberModes(), 0, int32Max),
		// the string of the called command: this command's string, or a string
		mode("textMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Text"), 0, 3, stringModes()),
		anyString(modeValue("text", "textMode", 4, 0, int32Max, stringModes())),
		// in the editor's section of the parameters, with the values and expressions
		section({}),
		mode("firstMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Given as"), 0, 1, {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variables from"), Kind::Variable},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variables from pointer"), Kind::Variable},
			{3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Values"), Kind::Number},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Expressions"), Kind::Expression}}),
		variable("first", 2).when("firstMode", {0, 1}).inlined(),
		onlyWhen(int32Value("count", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Count"), 0, 2, 3, numberModes(), 0, int32Max),
			"firstMode", {0, 1}),
		// written here: their count, the parameter of their first modes
		number("valueCount", 3, {}, 0, int32Max).hidden().when("firstMode", {3}),
		number("modesAt", 2, {}, 0, int32Max).withDefault(5).hidden().when("firstMode", {3}),
		field(Kind::Expression, "expressions", 5, {}).hidden().when("firstMode", {4})
	), Flag::Maniac);

	// Control Strings: the arguments (parameters 4 to 7) depend on the
	// operation; the literal texts of some follow each other in the string
	table.custom(Code::Maniac_ControlStrings, "ManiacControlStringsWidget",
		palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Control Strings")), fields(
		section(QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "String")),
		mode("targetMode", QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Target"), 0, 0, {
			{0, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Single"), Kind::StringId},
			{1, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Range"), Kind::StringId},
			{2, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Pointer"), Kind::Variable},
			{3, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Pointer range"), Kind::Variable}}),
		anyString(modeValue("first", "targetMode", 1, 1, int32Max, {{0, {}, Kind::StringId}, {1, {}, Kind::StringId},
			{2, {}, Kind::Variable}, {3, {}, Kind::Variable}}, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "String")), 1),
		onlyWhen(anyString(modeValue("last", "targetMode", 2, 1, int32Max, {{1, {}, Kind::StringId}, {3, {}, Kind::Variable}},
			QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "To")), 1), "targetMode", {1, 3}),
		section(QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Operation")),
		choice("operation", 3, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Operation"), {
			{0, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Set")}, {1, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Append")},
			{2, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "To number")}, {3, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Get length")},
			{4, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Find")}, {5, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Split")},
			{7, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Save to file")},
			{8, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Remove first line")},
			{9, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Find (regular expression)")},
			{10, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Match (regular expression)")}}).bitsAt(0, 8),
		choice("function", 3, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Value"), {
			{0, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Text")}, {1, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Number")},
			{2, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Switch")}, {3, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Database name")},
			{4, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Database description")},
			{6, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Concatenation")}, {7, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Insertion")},
			{8, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Replacement")}, {9, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Part of a text")},
			{10, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Strings joined")},
			{12, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "File contents")},
			{13, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Text without a part")},
			{14, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Replacement (regular expression)")}}).bitsAt(8, 8).when("operation", {0, 1}),
		check("extract", 3, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Evaluate the control codes (\\V, \\T...)")).bit(18),
		check("hex", 3, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Hexadecimal")).bit(17),
		check("firstOnly", 3, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Replace the first match only")).bit(19)
			.when("operation", {0, 1}).when("function", {14}),
		// arguments of the operation: modes in the nibbles 1-4 of parameter 0
		section(QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "Arguments"))
	), Flag::Maniac);

	// Get Game Info
	table.add(Code::Maniac_GetGameInfo, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Get Game Info")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Information")),
		choice("info", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Information"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Map size")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Tiles")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Window size")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Screen pixels")},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Event processing state")}, {5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Chipset ID")},
			{6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Face graphic")}, {7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Character graphic")},
			{8, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Screen position")}, {9, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Screen shake")},
			{10, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Current BGM")}}),
		choice("layer", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Layer"), layers()).when("info", {1}),
		check("ignoreAlpha", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Ignore alpha")).bit(0).when("info", {3}),
		onlyWhen(maniacArea("area", 0, 0, 3), "info", {1, 3}),
		onlyWhen(int32Value("nest", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Levels up"), 0, 2, 4, numberModes(), 0, int32Max), "info", {4}),
		choice("face", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Of"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Hero")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Message")}}).when("info", {6}),
		choice("character", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Of"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Hero")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Event")}}).when("info", {7}),
		onlyWhen(int32Value("id", QT_TRANSLATE_NOOP("ManiacCommandWidget", "ID"), 0, 0, 5), "info", {6, 7}),
		choice("dynamic", 6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Graphic"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Original")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Current")}}).when("info", {6, 7}),
		variable("variable", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store in")).when("info", {0, 2, 4, 5, 8, 9}),
		variable("first", 7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store from")).when("info", {1, 3}),
		stringId("fileName", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "File name in")).when("info", {6, 7, 10}),
		variable("index", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Index in")).when("info", {6, 7}),
		variable("bgm", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Fade, volume, tempo, balance from")).when("info", {10})
	), Flag::Maniac);

	// Edit Picture
	table.add(Code::Maniac_EditPicture, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Edit Picture")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture")),
		pictureNumber("picture", 0, 0, 1),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Area")),
		maniacArea("area", 0, 1, 2),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Pixels")),
		value("pixels", QT_TRANSLATE_NOOP("ManiacCommandWidget", "From variable"), 0, 5, 6, 1, int32Max, targetModes()),
		check("opaque", 7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Opaque (replace the pixels)")).bit(0),
		check("skipTransparent", 7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Skip transparent pixels")).bit(1)
	), Flag::Maniac);

	// Write Picture
	table.add(Code::Maniac_WritePicture, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Write Picture")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Image")),
		choice("target", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Screen")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture")}}),
		onlyWhen(fields(
			pictureNumber("picture", 0, 0, 2),
			check("dynamic", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "With the effects (tone, flash...)")).bit(0),
			check("opaque", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Opaque")).bit(1)), "target", {1}),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "File (PNG)")),
		mode("fileMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Name given as"), 0, 1, stringModes()),
		anyString(modeValue("file", "fileMode", 3, 0, int32Max, stringModes(), QT_TRANSLATE_NOOP("ManiacCommandWidget", "Name")))
	), Flag::Maniac);

	// Add Move Route: a step added to the route of the Set Move Route just
	// before it (it works there only), its values given by variables. The
	// modes of the action p1 and of the values p2-p4 in nibbles 0-3, of the
	// file name (the string, p5) in nibble 4. An action in a variable takes
	// the values as they are
	{
		auto forAction = [](Fields list, const std::vector<int>& actions) {
			return onlyWhen(onlyWhen(std::move(list), "actionMode", {0}), "action", actions);
		};
		table.add(Code::Maniac_AddMoveRoute, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Add Move Route")), fields(
			section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Step")),
			mode("actionMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Action"), 0, 0, numberModes()),
			choice("action", 1, {}, {
				{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Move")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Face")},
				{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Wait")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Jump")},
				{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Turn on or off")}, {5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Set a value")},
				{6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Switch")}, {7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Change graphic")},
				{8, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Play sound effect")}}).inlined().when("actionMode", {0}),
			variable("actionVariable", 1).inlined().when("actionMode", {1, 2}),
			note(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Works right after a Set Move Route command only: the step is added to its route.")),
			// Move, Face: a direction (the codes of the moves of RPG Maker) or a value
			forAction(fields(
				mode("directionMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Direction"), 0, 1, numberModes()),
				choice("move", 2, {}, {
					{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Up")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Right")},
					{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Down")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Left")},
					{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Up-right")}, {5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Down-right")},
					{6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Down-left")}, {7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Up-left")},
					{8, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Random")}, {9, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Toward the hero")},
					{10, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Away from the hero")}, {11, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Forward")}})
					.inlined().when("action", {0}).when("directionMode", {0}),
				choice("face", 2, {}, {
					{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Up")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Right")},
					{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Down")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Left")},
					{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Turn 90° right")}, {5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Turn 90° left")},
					{6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Turn 180°")}, {7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Turn 90° left or right")},
					{8, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Random")}, {9, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Toward the hero")},
					{10, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Away from the hero")}})
					.inlined().when("action", {1}).when("directionMode", {0}),
				variable("directionVariable", 2).inlined().when("directionMode", {1, 2})), {0, 1}),
			forAction(startingAt(int32Value("steps", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Steps"), 0, 2, 3), 1), {0}),
			forAction(fields(
				int32Value("jumpX", QT_TRANSLATE_NOOP("ManiacCommandWidget", "X"), 0, 1, 2),
				int32Value("jumpY", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Y"), 0, 2, 3)), {3}),
			// Turn on or off: 1 begins the jump, fixes the direction, the phasing
			forAction(fields(
				choice("state", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Setting"), {
					{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Jump (begin, end)")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Fixed direction")},
					{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Phasing")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Stepping animation")}}),
				choice("on", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value"), {
					{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "On")}, {0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Off")}}).radio(),
				note(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Stepping animation: the Maniac Patch's editor writes On to resume it, "
					"its TPC to pause it.")).when("state", {3})), {4}),
			forAction(fields(
				choice("setting", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Setting"), {
					{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Movement speed")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Movement frequency")},
					{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Transparency")}}),
				int32Value("settingValue", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value"), 0, 2, 3)), {5}),
			forAction(fields(
				mode("switchMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Switch"), 0, 1, {
					{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Switch")), Kind::Switch},
					{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Switch pointer")), Kind::Variable}}),
				ref(Kind::Switch, "switch", 2).inlined().when("switchMode", {0}),
				variable("switchVariable", 2).inlined().when("switchMode", {1}),
				int32Value("switchValue", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value"), 0, 2, 3)), {6}),
			// Change graphic, Play sound effect: a file named by the string
			forAction(fields(
				mode("fileMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "File"), 0, 4, stringModes()),
				file("graphicFile", {}, CHARSET).inlined().when("action", {7}).when("fileMode", {0}),
				file("soundFile", {}, SOUND).with(Flag::Off).inlined().when("action", {8}).when("fileMode", {0}),
				stringId("fileString", 5).withDefault(1).inlined().when("fileMode", {1}),
				variable("fileVariable", 5).inlined().when("fileMode", {2})), {7, 8}),
			forAction(int32Value("graphicIndex", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Index in the file"), 0, 1, 2), {7}),
			forAction(fields(
				startingAt(int32Value("volume", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Volume"), 0, 1, 2), 100),
				startingAt(int32Value("tempo", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Tempo"), 0, 2, 3), 100),
				startingAt(int32Value("balance", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Balance"), 0, 3, 4), 50)), {8}),
			// an action in a variable: the values of the actions above, by their place
			onlyWhen(fields(
				int32Value("value1", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value 1"), 0, 1, 2),
				int32Value("value2", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value 2"), 0, 2, 3),
				int32Value("value3", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value 3"), 0, 3, 4),
				anyString(value("name", QT_TRANSLATE_NOOP("ManiacCommandWidget", "File"), 0, 4, 5, 0, int32Max, stringModes()))),
				"actionMode", {1, 2})
		), Flag::Maniac);
	}

	// Edit Tile (Edit Picture (Tile) in the Maniac Patch's editor): tiles of
	// a chipset drawn in a picture, at x, y (pixels), w x h tiles: one tile,
	// or a tile per cell from variables (p8). The modes of the picture, the
	// area, the tile, the chipset and the animation frame in nibbles 0-7
	table.add(Code::Maniac_EditTile, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Edit Tile")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture")),
		pictureNumber("picture", 0, 0, 1),
		int32Value("x", QT_TRANSLATE_NOOP("ManiacCommandWidget", "X (pixels)"), 0, 1, 2),
		int32Value("y", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Y (pixels)"), 0, 2, 3),
		int32Value("width", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Width (tiles)"), 0, 3, 4),
		int32Value("height", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Height (tiles)"), 0, 4, 5),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Tiles")),
		choice("fromVariables", 8, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Tiles"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "The same tile")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "A tile per cell, from variables")}}),
		onlyWhen(int32Value("tile", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Tile"), 0, 5, 6), "fromVariables", {0}),
		onlyWhen(value("tiles", QT_TRANSLATE_NOOP("ManiacCommandWidget", "From variable"), 0, 5, 6, 1, int32Max, targetModes()),
			"fromVariables", {1}),
		choice("layer", 9, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Layer"), layers()),
		entryValue("chipset", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Chipset"), 0, 6, 10, Kind::Chipset,
			maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Chipset")),
			{{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "(The map's)"))}}),
		startingAt(int32Value("pattern", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Animation frame"), 0, 7, 11), -1),
		note(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Animation frame -1: the current one.")),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Options")),
		check("autotile", 7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Autotile")).bit(0).with(Flag::Inverted),
		check("clear", 7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Clear the pixels before drawing")).bit(1)
	), Flag::Maniac);

	// Control Text Processing (Control Message in the Maniac Patch's
	// editor): a common event the messages call (p1: when), its mode in
	// nibble 0 (newer builds: 3-5 by its name, a text or a string), the
	// first variables and strings that get its arguments (0: none)
	table.add(Code::Maniac_ControlTextProcessing, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Control Text Processing")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Called")),
		check("onMarker", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "User event (\\e[] in a message)")).bit(0),
		check("onOpen", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Window created")).bit(1),
		check("onClose", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Window destroyed")).bit(2),
		check("onCharacter", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Each character drawn")).bit(3),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common Event")),
		mode("eventMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common event"), 0, 0, {
			{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common event")), Kind::CommonEvent},
			{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable")), Kind::Variable},
			{2, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable pointer")), Kind::Variable},
			{3, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Name")), Kind::String},
			{4, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Name in a string")), Kind::StringId},
			{5, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Name in a string pointer")), Kind::Variable}}),
		ref(Kind::CommonEvent, "event", 2).withItems({{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "(None)")}}).withDefault(0)
			.inlined().when("eventMode", {0}),
		variable("eventVariable", 2).inlined().when("eventMode", {1, 2, 5}),
		string("eventName").inlined().when("eventMode", {3}),
		stringId("eventString", 2).withDefault(1).inlined().when("eventMode", {4}),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Arguments")),
		variable("systemNumbers", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "System numbers from"))
			.withItems({{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "(None)")}}).withDefault(0),
		stringId("systemTexts", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "System texts from"))
			.withItems({{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "(None)")}}),
		variable("userNumbers", 5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "User numbers from"))
			.withItems({{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "(None)")}}).withDefault(0),
		stringId("userTexts", 6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "User texts from"))
			.withItems({{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "(None)")}}),
		note(QT_TRANSLATE_NOOP("ManiacCommandWidget", "The first system number is the reason of the call (1, 2, 4, 8: in the order "
			"above), then come the window's position and size, which the common event can change. The user values are those of "
			"\\e[] in the message, in their order. 0: none."))
	), Flag::Maniac);

	// Script: JavaScript, its code in the string and in its next lines
	// (Maniac_Script_2), or in a string; the values it returns to the
	// targets from parameter 5, their count in parameter 4: a kind (switch,
	// variable, string, each by a pointer too), a number (ScriptEditor)
	table.custom(Code::Maniac_Script, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Script")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Code")),
		mode("sourceMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Code"), 0, -1, {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Text"), Kind::Lines},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "String"), Kind::StringId},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "String pointer"), Kind::Variable}}),
		field(Kind::Lines, "code", StringIndex, {}).when("sourceMode", {0}),
		stringId("codeString", 1).withDefault(1).when("sourceMode", {1}),
		variable("codeVariable", 1).when("sourceMode", {2}),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Returned Values")),
		number("returns", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Count"), 0, int32Max).hidden()
	), Flag::Maniac);

	// Zoom: not run by EasyRPG Player; the modes of x, y, magnification,
	// duration and layer in nibbles 0-4
	table.add(Code::Maniac_Zoom, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Zoom")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Zoom")),
		int32Value("x", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Center X"), 0, 0, 1),
		int32Value("y", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Center Y"), 0, 1, 2),
		startingAt(int32Value("scale", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Magnification (%, from 100)"), 0, 2, 3), 100),
		int32Value("duration", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Duration (frames)"), 0, 3, 4, numberModes(), 0, int32Max),
		int32Value("layer", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Zoomed up to layer"), 0, 4, 5),
		note(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Layers, as those of pictures: 1 panorama, 2 lower tiles, 3 events below the hero, "
			"4 events at the hero's level, 5 upper tiles, 6 events above the hero, 7 weather, 8 battle animations, 9 windows, "
			"10 timers. 0 ends the zoom.")),
		check("wait", 6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Wait for completion"))
	), Flag::Maniac);

	// Console: the modes in nibbles 2 (the text, the string of the input,
	// the text color) and 3 (the length of the input, the background color).
	// A text of several lines (ConsoleEditor): its lines end with "\r\n"
	table.custom(Code::Maniac_Console, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Console")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Console")),
		choice("operation", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Open")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Close")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Write a text")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Read a text")},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Set the text color")}}),
		// in the editor's section of the text
		section({}),
		onlyWhen(anyString(value("text", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Text"), 0, 2, 3, 0, int32Max, stringModes())),
			"operation", {2}),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Input")),
		onlyWhen(anyString(value("input", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store in"), 0, 2, 3, 0, int32Max, stringIdModes()), 1),
			"operation", {3}),
		onlyWhen(int32Value("length", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Maximum length (0: none)"), 0, 3, 4), "operation", {3}),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Colors")),
		check("changeFront", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Change the text color")).bit(3).when("operation", {4}),
		onlyWhen(int32Value("front", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Text color"), 0, 2, 3), "operation", {4}),
		check("changeBack", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Change the background color")).bit(4).when("operation", {4}),
		onlyWhen(int32Value("back", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Background color"), 0, 3, 4), "operation", {4}),
		note(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Colors: 0 black, 1 blue, 2 green, 3 aqua, 4 red, 5 purple, 6 yellow, 7 white, "
			"8 gray, 9-15 the light ones (15 bright white).")).when("operation", {4}),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Options")),
		check("extract", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Evaluate the control codes (\\V, \\N, \\T...)")).bit(0)
			.when("operation", {2, 3}),
		check("hex", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Hexadecimal")).bit(1).when("operation", {2, 3}),
		check("noLineBreak", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "No line break after the text")).bit(2).when("operation", {2})
	), Flag::Maniac);

	// Control Self Variable (newer builds of the Maniac Patch; the layout
	// read from the commands of games): the switches, variables or strings
	// of an event (its own ones, from 1) or of the game. p0: nibbles 0-3 the
	// target (its owner, the modes of its event p1, its map p2 and the first
	// one p3: 1 a number, 2 in a variable), nibbles 4-7 the value (the owner
	// of the one copied, the modes of its event p4 and map p5, the mode of
	// the value p6); p7: the mode of the count p8 in bits 0-3, the data in
	// bits 4-5, bit 8 an array copied; p9 the operation. A text set has its
	// length in p6 (CommandEditorRegistry)
	table.add(Code::Maniac_ControlSelfVariable, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Control Self Variable")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation")),
		choice("operation", 9, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Set")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Reset (0, OFF, empty)")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Reset all, event by event")}}).radio(),
		choice("data", 7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Data"), {
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Switches")}, {0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variables")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Strings")},
			{3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "All of them (reset only)")}}).bitsAt(4, 2),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Target")),
		selfOwner("owner", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Owner"), 0, 1),
		onlyWhen(fields(
			mode("firstMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "First"), 0, 3, {
				{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Constant")), Kind::Number},
				{2, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable")), Kind::Variable}}).withDefault(1),
			startingAt(modeValue("first", "firstMode", 3, int32Min, int32Max, {
				{1, {}, Kind::Number}, {2, {}, Kind::Variable}}), 1),
			startingAt(int32Value("count", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Count"), 7, 0, 8), 1)), "operation", {0, 1}),
		onlyWhen(fields(
			startingAt(int32Value("events", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Events"), 7, 0, 8), 1),
			note(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Events: the owner and the next ones, this many (0: none; less "
				"than 0: all of them)."))), "operation", {2}),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value")),
		onlyWhen(fields(
			mode("valueMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value"), 0, 7, {
				{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Constant")), Kind::Number},
				{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Copy of")), Kind::Number},
				{2, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Copy of the one in")), Kind::Variable}}),
			number("value", 6, {}, int32Min, int32Max).inlined().when("valueMode", {0}).unless("data", {1}),
			string("text").inlined().when("valueMode", {0}).when("data", {1}),
			number("from", 6, {}, int32Min, int32Max).withDefault(1).inlined().when("valueMode", {1}),
			variable("fromVariable", 6).inlined().when("valueMode", {2}),
			onlyWhen(fields(
				selfOwner("source", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Of"), 4, 4),
				check("array", 7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "An array: a value per item, from this one")).bit(8)),
				"valueMode", {1, 2}),
			note(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Switches: 0 is OFF, another value ON. A copy is of the data chosen above."))),
			"operation", {0})
	), Flag::Maniac);

	// Control Media Option: get a setting of the engine into a variable (a
	// variable or a pointer), or set it (a constant, a variable or a
	// pointer), the mode in nibble 2
	table.add(Code::Maniac_ControlMediaOption, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Control Media Option")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Media Option")),
		choice("operation", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Get")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Set")}}).radio(),
		choice("setting", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Setting"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Screen mode (0: window, 1: full screen)")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Window: zoom (1-4)")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Window: interpolation (0, 1)")},
			{3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Window: keep within the display (0, 1)")},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Full screen: interpolation (0, 1)")},
			{5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Full screen: uneven pixel size (0, 1)")},
			{6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Full screen: stretch, ignoring the aspect ratio (0, 1)")},
			{7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Show the frame rate (0, 1)")},
			{8, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save the settings as the default (0, 1)")},
			{9, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Audio volume (0-100)")}}),
		onlyWhen(value("target", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store in"), 0, 2, 3, 1, int32Max, targetModes()),
			"operation", {0}),
		onlyWhen(int32Value("value", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value"), 0, 2, 3), "operation", {1})
	), Flag::Maniac);
}

// Messages, switches, variables, money, items, party and heroes
void addMessagesAndParty(Table& table) {
	// Show Message: its text over several lines, with the message codes
	table.custom(Code::ShowMessage, "ShowMessageWidget", QT_TRANSLATE_NOOP("ShowMessageWidget", "Show message"), fields(
		field(Kind::Lines, "message", StringIndex, {})
	));

	// Message Options
	table.add(Code::MessageOptions, "MessageOptionsWidget", QT_TRANSLATE_NOOP("MessageOptionsWidget", "Message Options"), fields(
		section(QT_TRANSLATE_NOOP("MessageOptionsWidget", "Window type")),
		choice("type", 0, {}, {
			{0, QT_TRANSLATE_NOOP("MessageOptionsWidget", "Normal")},
			{1, QT_TRANSLATE_NOOP("MessageOptionsWidget", "Transparent")}}).radio(),
		section(QT_TRANSLATE_NOOP("MessageOptionsWidget", "Window position")),
		choice("position", 1, {}, {
			{0, QT_TRANSLATE_NOOP("MessageOptionsWidget", "Top")},
			{1, QT_TRANSLATE_NOOP("MessageOptionsWidget", "Middle")},
			{2, QT_TRANSLATE_NOOP("MessageOptionsWidget", "Bottom")}}).radio(),
		section(QT_TRANSLATE_NOOP("MessageOptionsWidget", "Options")),
		check("autoPosition", 2, QT_TRANSLATE_NOOP("MessageOptionsWidget", "Autoselect window position to prevent hero from being obscure")),
		check("parallel", 3, QT_TRANSLATE_NOOP("MessageOptionsWidget", "Allow other events to continue while message is shown")),
		// Maniac Patch: the size of the window, the font and the spacing, their
		// modes in the nibbles of p4 (the font's name: the string of the
		// command, or T[p7]); 0 keeps a value, -1 restores its default (a
		// font name: empty, "*"), a spacing is written plus 1
		section(maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Window Size"))),
		modalNumber(number("width", 5, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Width:")), -1, 9999999).maniacOnly().extension(), 4, 0),
		modalNumber(number("height", 6, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Height:")), -1, 9999999).maniacOnly().extension(), 4, 1),
		section(maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Font"))),
		modalText(string("fontName", maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Name:"))).maniacOnly(), 4, 2, 7),
		modalNumber(number("fontSize", 8, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Size:")), -1, 9999999).maniacOnly().extension(), 4, 3),
		section(maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Spacing"))),
		modalNumber(number("charSpacing", 9, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Char:")), -9999999, 9999999).maniacOnly().extension(), 4, 4),
		modalNumber(number("lineSpacing", 10, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Line:")), -9999999, 9999999).maniacOnly().extension(), 4, 5),
		note(maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "0 keeps a value, -1 restores its default; a spacing is written plus 1"))).maniacOnly()
	));

	// Select Face Graphic: a picker with the face and its index
	table.custom(Code::ChangeFaceGraphic, "ChangeFaceGraphicWidget", QT_TRANSLATE_NOOP("ChangeFaceGraphicWidget", "Face Graphics"), fields(
		section(QT_TRANSLATE_NOOP("ChangeFaceGraphicWidget", "Face Graphic")),
		graphic(Kind::Face, "face", 0, FACESET),
		section(QT_TRANSLATE_NOOP("ChangeFaceGraphicWidget", "Display Position")),
		// EasyRPG Player: 0 left, any other value right
		choice("position", 1, {}, {
			{0, QT_TRANSLATE_NOOP("ChangeFaceGraphicWidget", "Left")},
			{1, QT_TRANSLATE_NOOP("ChangeFaceGraphicWidget", "Right")}}).radio(),
		section(QT_TRANSLATE_NOOP("ChangeFaceGraphicWidget", "Option")),
		check("flip", 2, QT_TRANSLATE_NOOP("ChangeFaceGraphicWidget", "Flip Image Horizontally")),
		// Maniac Patch: the file from a string variable, the index from a
		// variable (their modes in p3); the face picker edits the same values
		section(in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Maniac Patch"))),
		modalText(string("fileName", QT_TRANSLATE_NOOP("ChangeFaceGraphicWidget", "File")), 3, 0, 4),
		modalNumber(number("fileIndex", 0, QT_TRANSLATE_NOOP("ChangeFaceGraphicWidget", "Index"), 0, 15), 3, 1),
		// and an animation from the index to the end index (p6), its
		// options in the bits of p5, the modes of its values in p3
		section(maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Animation"))),
		check("animated", 5, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Animation"))).bit(1).maniacOnly().extension(),
		modalNumber(number("endIndex", 6, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Animation End Index")), 0, 15)
			.maniacOnly().extension().enabledWhen("animated", {1}), 3, 2),
		modalNumber(number("interval", 7, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Animation Interval")), 0, 9999999)
			.maniacOnly().extension().enabledWhen("animated", {1}), 3, 3),
		modalNumber(number("intervalVariation", 8, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Interval Variation")), 0, 9999999)
			.maniacOnly().extension().enabledWhen("animated", {1}), 3, 4),
		modalNumber(number("framesPerCell", 9, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Frame Per Cell")), 0, 9999999)
			.maniacOnly().extension().enabledWhen("animated", {1}), 3, 5),
		modalNumber(number("framesVariation", 10, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Fpc Variation")), 0, 9999999)
			.maniacOnly().extension().enabledWhen("animated", {1}), 3, 6),
		check("forceUpdate", 5, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Force Update"))).bit(0).maniacOnly().extension(),
		check("turnaround", 5, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Enable Turnaround"))).bit(2).maniacOnly().extension()
			.enabledWhen("animated", {1}),
		check("once", 5, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Once"))).bit(3).maniacOnly().extension().enabledWhen("animated", {1}),
		check("intervalPositive", 5, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Limit Interval Variation to Positive Number")))
			.bit(4).maniacOnly().extension().enabledWhen("animated", {1}),
		check("framesPositive", 5, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Limit Fpc Variation to Positive Number")))
			.bit(5).maniacOnly().extension().enabledWhen("animated", {1}),
		check("turnaroundInterval", 5, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Apply Interval at Turnaround")))
			.bit(6).maniacOnly().extension().enabledWhen("animated", {1})
	));

	// Show Choices: the choices are the texts of its option lines, the
	// branches follow them
	table.custom(Code::ShowChoice, "ShowChoiceWidget", QT_TRANSLATE_NOOP("ShowChoiceWidget", "Show Choices"), fields(
		section(QT_TRANSLATE_NOOP("ShowChoiceWidget", "Choices")),
		section(QT_TRANSLATE_NOOP("ShowChoiceWidget", "Cancellation Behavior")),
		choice("cancel", 0, {}, {
			{0, QT_TRANSLATE_NOOP("ShowChoiceWidget", "Ignore")},
			{1, QT_TRANSLATE_NOOP("ShowChoiceWidget", "Select Choice 1")},
			{2, QT_TRANSLATE_NOOP("ShowChoiceWidget", "Select Choice 2")},
			{3, QT_TRANSLATE_NOOP("ShowChoiceWidget", "Select Choice 3")},
			{4, QT_TRANSLATE_NOOP("ShowChoiceWidget", "Select Choice 4")},
			{5, QT_TRANSLATE_NOOP("ShowChoiceWidget", "Execute Handler")}}).radio()
	));

	// Input Number: 1 to 6 digits in RPG Maker, 9 with Maniac Patch
	table.add(Code::InputNumber, "InputNumberWidget", QT_TRANSLATE_NOOP("InputNumberWidget", "Input Number"), fields(
		number("digits", 0, QT_TRANSLATE_NOOP("InputNumberWidget", "Digits Allowed"), 1, 6).withDefault(1).maniacRange(1, 9),
		variable("variable", 1, QT_TRANSLATE_NOOP("InputNumberWidget", "Store in Variable"))
	));

	// Control Switches
	table.add(Code::ControlSwitches, "SwitchOperationsWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Control Switches")), fields(
		switchOperand(QT_TRANSLATE_NOOP("SwitchOperationsWidget", "Switch"), 0),
		section(QT_TRANSLATE_NOOP("SwitchOperationsWidget", "Operation")),
		choice("operation", 3, {}, {
			{0, QT_TRANSLATE_NOOP("SwitchOperationsWidget", "Turn On")},
			{1, QT_TRANSLATE_NOOP("SwitchOperationsWidget", "Turn Off")},
			{2, QT_TRANSLATE_NOOP("SwitchOperationsWidget", "Toggle between On and Off")}}).radio()
	));

	// the values of a hero, or of a member of the party
	const Items heroFields = {
		{0, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Level")}, {1, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Experience")},
		{2, QT_TRANSLATE_NOOP("VariableOperationsWidget", "HP")}, {3, QT_TRANSLATE_NOOP("VariableOperationsWidget", "MP")},
		{4, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Maximum HP")}, {5, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Maximum MP")},
		{6, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Attack")}, {7, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Defense")},
		{8, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Intelligence")}, {9, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Agility")},
		{10, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Weapon number")}, {11, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Shield number")},
		{12, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Armor number")}, {13, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Helmet number")},
		{14, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Accessory number")},
		{15, QT_TRANSLATE_NOOP("VariableOperationsWidget", "ID"), Flag::Maniac},
		{16, QT_TRANSLATE_NOOP("VariableOperationsWidget", "ATB gauge"), Flag::Maniac},
		{17, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Experience to next level"), Flag::Maniac}};
	// Control Variables: with Maniac Patch, expressions are placed after the
	// parameters of the operand (layoutExpressions), and the arguments of the
	// functions (operands 11-25) have their modes in parameters 5 to 10
	table.custom(Code::ControlVars, "VariableOperationsWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Control Variables")), fields(
		section(QT_TRANSLATE_NOOP("VariableOperationsWidget", "Variable to change")),
		choice("target", 0, {}, {
			{0, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Specific")},
			{1, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Range")},
			{2, QT_TRANSLATE_NOOP("VariableOperationsWidget", "By Variable")},
			{3, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Range by Variables"), Flag::Maniac},
			// the expression is after the parameters of the operand, parameter 1 says where
			{4, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Expression"), Flag::Maniac}}).radio(),
		variable("variable", 1).when("target", {0}).inlined(),
		number("first", 1, {}, 1, 9999).withDefault(1).when("target", {1}).inlined(),
		number("last", 2, {}, 1, 9999).withDefault(1).when("target", {1}).inlined(),
		variable("pointer", 1).when("target", {2}).inlined(),
		variable("firstPointer", 1).when("target", {3}).inlined().maniac(),
		variable("lastPointer", 2).when("target", {3}).inlined().maniac(),
		section(QT_TRANSLATE_NOOP("VariableOperationsWidget", "Operation")),
		choice("operation", 3, {}, {
			{0, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Set e&qual")},
			{1, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Add")},
			{2, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Subtract")},
			{3, QT_TRANSLATE_NOOP("VariableOperationsWidget", "&Multiply")},
			{4, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Divide")},
			{5, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Modulus")},
			{6, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Bitwise OR"), Flag::Maniac},
			{7, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Bitwise AND"), Flag::Maniac},
			{8, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Bitwise XOR"), Flag::Maniac},
			{9, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Shift left"), Flag::Maniac},
			{10, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Shift right"), Flag::Maniac}}).radio(),
		section(QT_TRANSLATE_NOOP("VariableOperationsWidget", "Operand")),
		choice("operand", 4, {}, {
			{0, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Number")},
			{1, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Va&lue of variable")},
			{2, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Value stored in inde&x")},
			{3, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Random number between")},
			{4, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Item")},
			{5, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Hero")},
			{6, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Sprite")},
			{7, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Other")},
			{8, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Monster"), Flag::Rpg2003},
			{9, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Party member"), Flag::Maniac},
			{10, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Switch (0 or 1)"), Flag::Maniac},
			{11, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Power"), Flag::Maniac},
			{12, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Square root"), Flag::Maniac},
			{13, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Sine"), Flag::Maniac},
			{14, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Cosine"), Flag::Maniac},
			{15, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Arc tangent (atan2)"), Flag::Maniac},
			{16, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Minimum"), Flag::Maniac},
			{17, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Maximum"), Flag::Maniac},
			{18, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Absolute value"), Flag::Maniac},
			{19, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Binary operation"), Flag::Maniac},
			{20, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Comparison (ternary)"), Flag::Maniac},
			{21, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Expression"), Flag::Maniac},
			{22, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Linear interpolation"), Flag::Maniac},
			{23, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Sum of variables"), Flag::Maniac},
			{24, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Minimum of variables"), Flag::Maniac},
			{25, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Maximum of variables"), Flag::Maniac}}).radio(),
		number("number", 5, {}, -9999999, 9999999).when("operand", {0}).inlined(),
		variable("operandVariable", 5).when("operand", {1}).inlined(),
		variable("operandPointer", 5).when("operand", {2}).inlined(),
		number("randomFrom", 5, {}, -9999999, 9999999).when("operand", {3}).inlined(),
		number("randomTo", 6, {}, -9999999, 9999999).when("operand", {3}).inlined(),
		ref(Kind::Item, "item", 5).when("operand", {4}).inlined(),
		choice("itemCount", 6, {}, {
			{0, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Number held")},
			{1, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Number equipped")}}).when("operand", {4}).inlined(),
		ref(Kind::Actor, "hero", 5).when("operand", {5}).inlined(),
		choice("heroField", 6, {}, heroFields).when("operand", {5}).inlined(),
		// the same values of a member of the party
		choice("memberField", 6, {}, heroFields).when("operand", {9}).inlined().maniac(),
		character("sprite", 5, {}, {
			{10001, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Hero")},
			{10002, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Boat")},
			{10003, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Ship")},
			{10004, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Airship")},
			{10005, QT_TRANSLATE_NOOP("VariableOperationsWidget", "This Event")}}).when("operand", {6}).inlined(),
		choice("spriteField", 6, {}, {
			{0, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Map ID")}, {1, QT_TRANSLATE_NOOP("VariableOperationsWidget", "X coordinate")},
			{2, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Y coordinate")}, {3, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Facing")},
			{4, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Screen-relative X")}, {5, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Screen-relative Y")},
			{6, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Event ID"), Flag::Maniac}}).when("operand", {6}).inlined(),
		choice("other", 5, {}, {
			{0, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Money")}, {1, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Timer 1 seconds")},
			{2, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Size of party")}, {3, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Number of saves")},
			{4, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Number of battles")}, {5, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Number of victories")},
			{6, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Number of defeats")}, {7, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Number of escapes")},
			{8, QT_TRANSLATE_NOOP("VariableOperationsWidget", "MIDI ticks")}, {9, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Timer 2 seconds")},
			{10, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Date (YYMMDD)"), Flag::Maniac},
			{11, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Time (HHMMSS)"), Flag::Maniac},
			{12, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Frames"), Flag::Maniac},
			{13, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Maniac Patch version"), Flag::Maniac}}).when("operand", {7}).inlined(),
		// the monsters of the battle (troop slots from 0)
		choice("monster", 5, {}, numbered(QT_TRANSLATE_NOOP("VariableOperationsWidget", "Monster %1"), 8, 0)).when("operand", {8}).inlined(),
		choice("monsterField", 6, {}, {
			{0, QT_TRANSLATE_NOOP("VariableOperationsWidget", "HP")}, {1, QT_TRANSLATE_NOOP("VariableOperationsWidget", "MP")},
			{2, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Maximum HP")}, {3, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Maximum MP")},
			{4, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Attack")}, {5, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Defense")},
			{6, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Intelligence")}, {7, QT_TRANSLATE_NOOP("VariableOperationsWidget", "Agility")},
			{8, QT_TRANSLATE_NOOP("VariableOperationsWidget", "ID"), Flag::Maniac},
			{9, QT_TRANSLATE_NOOP("VariableOperationsWidget", "ATB gauge"), Flag::Maniac}}).when("operand", {8}).inlined(),
		choice("member", 5, {}, numbered(QT_TRANSLATE_NOOP("VariableOperationsWidget", "Member %1"), 4, 0)).when("operand", {9}).inlined().maniac(),
		ref(Kind::Switch, "switch", 5).when("operand", {10}).inlined().maniac(),
		// the words of the expression after their count in parameter 5
		field(Kind::Expression, "expression", 6, {}).when("operand", {21}).maniac()
	));

	// Timer Operations
	table.add(Code::TimerOperation, "TimerOperationWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Timer Operations")), fields(
		section(QT_TRANSLATE_NOOP("TimerOperationWidget", "Select Timer")),
		choice("timer", 5, {}, {
			{0, QT_TRANSLATE_NOOP("TimerOperationWidget", "Timer 1")},
			{1, QT_TRANSLATE_NOOP("TimerOperationWidget", "Timer 2")}}).radio(),
		section(QT_TRANSLATE_NOOP("TimerOperationWidget", "Operation")),
		choice("operation", 0, {}, {
			{0, QT_TRANSLATE_NOOP("TimerOperationWidget", "Set Time")},
			{1, QT_TRANSLATE_NOOP("TimerOperationWidget", "Start Timer")},
			{2, QT_TRANSLATE_NOOP("TimerOperationWidget", "Stop Timer")}}).radio(),
		onlyWhen(timerOperand(QT_TRANSLATE_NOOP("TimerOperationWidget", "Operand"), 1), "operation", {0}),
		onlyWhen(fields(
			section(QT_TRANSLATE_NOOP("TimerOperationWidget", "Options")),
			check("showTimer", 3, QT_TRANSLATE_NOOP("TimerOperationWidget", "Show Timer on Screen")),
			check("battleTimer", 4, QT_TRANSLATE_NOOP("TimerOperationWidget", "Continue Timer in Battle"))), "operation", {1})
	));

	// Change Money
	table.add(Code::ChangeGold, "ChangeMoneyWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Money")), fields(
		section(QT_TRANSLATE_NOOP("ChangeMoneyWidget", "Operation")),
		choice("operation", 0, {}, {
			{0, QT_TRANSLATE_NOOP("ChangeMoneyWidget", "Increase Money")},
			{1, QT_TRANSLATE_NOOP("ChangeMoneyWidget", "Decrease Money")}}).radio(),
		valueOperand("amount", QT_TRANSLATE_NOOP("ChangeMoneyWidget", "Amount"), 1)
	));

	// Change Items
	table.add(Code::ChangeItems, "ChangeItemWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Items")), fields(
		section(QT_TRANSLATE_NOOP("ChangeItemWidget", "Operation")),
		choice("operation", 0, {}, {
			{0, QT_TRANSLATE_NOOP("ChangeItemWidget", "Add item")},
			{1, QT_TRANSLATE_NOOP("ChangeItemWidget", "Remo&ve item")}}).radio(),
		entryOperand(Kind::Item, "item", QT_TRANSLATE_NOOP("ChangeItemWidget", "Select item"), 1),
		valueOperand("amount", QT_TRANSLATE_NOOP("ChangeItemWidget", "Amount"), 3)
	));

	// Change Party
	table.add(Code::ChangePartyMembers, "ChangePartyWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Party")), fields(
		section(QT_TRANSLATE_NOOP("ChangePartyWidget", "Operation")),
		choice("operation", 0, {}, {
			{0, QT_TRANSLATE_NOOP("ChangePartyWidget", "Add member")},
			{1, QT_TRANSLATE_NOOP("ChangePartyWidget", "Remo&ve member")}}).radio(),
		entryOperand(Kind::Actor, "hero", QT_TRANSLATE_NOOP("ChangePartyWidget", "Select Actor"), 1)
	));

	// Change Experience
	table.add(Code::ChangeExp, "ChangeExperienceWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Experience")), fields(
		partyOperand(QT_TRANSLATE_NOOP("ChangeExperienceWidget", "Actor"), 0),
		section(QT_TRANSLATE_NOOP("ChangeExperienceWidget", "Operation")),
		choice("operation", 2, {}, {
			{0, QT_TRANSLATE_NOOP("ChangeExperienceWidget", "Increase")},
			{1, QT_TRANSLATE_NOOP("ChangeExperienceWidget", "Decrease")}}).radio(),
		valueOperand("amount", QT_TRANSLATE_NOOP("ChangeExperienceWidget", "Amount"), 3),
		check("message", 5, QT_TRANSLATE_NOOP("ChangeExperienceWidget", "Show Level Up Message")).enabledWhen("operation", {0})
	));

	// Change Level
	table.add(Code::ChangeLevel, "ChangeLevelWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Level")), fields(
		partyOperand(QT_TRANSLATE_NOOP("ChangeLevelWidget", "Actor"), 0),
		section(QT_TRANSLATE_NOOP("ChangeLevelWidget", "Operation")),
		choice("operation", 2, {}, {
			{0, QT_TRANSLATE_NOOP("ChangeLevelWidget", "Increase")},
			{1, QT_TRANSLATE_NOOP("ChangeLevelWidget", "Decrease")}}).radio(),
		valueOperand("amount", QT_TRANSLATE_NOOP("ChangeLevelWidget", "Amount"), 3),
		check("message", 5, QT_TRANSLATE_NOOP("ChangeLevelWidget", "Show Level Up Message")).enabledWhen("operation", {0})
	));

	// Change Parameters (amounts from 0)
	table.add(Code::ChangeParameters, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Base Statistics"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Operation")),
		choice("operation", 2, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Increase Statistic")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Decrease Statistic")}}).radio(),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Statistic")),
		choice("statistic", 3, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Maximum HP")}, {1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Maximum MP")},
			{2, QT_TRANSLATE_NOOP("HeroCommandWidget", "Attack")}, {3, QT_TRANSLATE_NOOP("HeroCommandWidget", "Defense")},
			{4, QT_TRANSLATE_NOOP("HeroCommandWidget", "Intelligence")}, {5, QT_TRANSLATE_NOOP("HeroCommandWidget", "Agility")}}),
		valueOperand("amount", QT_TRANSLATE_NOOP("HeroCommandWidget", "Amount of Points"), 4, 0, 9999999)
	));

	// Change Skills
	table.add(Code::ChangeSkills, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Skills"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Operation")),
		choice("operation", 2, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Add Skill")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Remove Skill")}}).radio(),
		entryOperand(Kind::Skill, "skill", QT_TRANSLATE_NOOP("HeroCommandWidget", "Skill"), 3)
	));

	// Change Equipment: parameter 3 is the item source when equipping, the
	// slot when unequipping
	table.add(Code::ChangeEquipment, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Equipped Items"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Operation")),
		choice("operation", 2, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Equip Item")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Unequip Item")}}).radio(),
		onlyWhen(fields(
			section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Item")),
			choice("source", 3, {}, {
				{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Specific Item")},
				{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Item with ID Stored in")}}).radio(),
			ref(Kind::Item, "item", 4).with(Flag::Equipment).when("source", {0}).inlined(),
			variable("itemVariable", 4).when("source", {1}).inlined()), "operation", {0}),
		onlyWhen(fields(
			section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Slot from which to Unequip Item")),
			choice("slot", 3, {}, {
				{5, QT_TRANSLATE_NOOP("HeroCommandWidget", "All")}, {0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Weapon")},
				{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Shield")}, {2, QT_TRANSLATE_NOOP("HeroCommandWidget", "Armor")},
				{3, QT_TRANSLATE_NOOP("HeroCommandWidget", "Helmet")}, {4, QT_TRANSLATE_NOOP("HeroCommandWidget", "Accessory")}})
				.inColumns(3).withDefault(5)), "operation", {1})
	));

	// Change HP
	table.add(Code::ChangeHP, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Change HP"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Operation")),
		choice("operation", 2, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Increase HP")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Decrease HP")}}).radio(),
		valueOperand("amount", QT_TRANSLATE_NOOP("HeroCommandWidget", "Amount"), 3, 0, 9999999),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Option")),
		// only a decrease can kill
		check("lethal", 5, QT_TRANSLATE_NOOP("HeroCommandWidget", "HP Reduction can Kill Target")).enabledWhen("operation", {1})
	));

	// Change MP
	table.add(Code::ChangeSP, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Change MP"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Operation")),
		choice("operation", 2, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Increase MP")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Decrease MP")}}).radio(),
		valueOperand("amount", QT_TRANSLATE_NOOP("HeroCommandWidget", "Amount"), 3, 0, 9999999)
	));

	// Change Condition
	// Maniac Patch: the hero in a variable pointer, the condition's mode in p4
	table.add(Code::ChangeCondition, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Character Condition"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0, true),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Operation")),
		choice("operation", 2, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Add Condition")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Remove Condition")}}).radio(),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Condition")),
		modalNumber(ref(Kind::State, "condition", 3), 4, -1)
	));

	// Full Recovery
	table.add(Code::FullHeal, "FullHealWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Full Recovery")), fields(
		partyOperand(QT_TRANSLATE_NOOP("FullHealWidget", "Actor"), 0)
	));

	// Damage Hero
	table.add(Code::SimulatedAttack, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Simulated Enemy Attack"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Attack")),
		number("attack", 2, QT_TRANSLATE_NOOP("HeroCommandWidget", "Attack Power:"), 0, 9999),
		number("defense", 3, QT_TRANSLATE_NOOP("HeroCommandWidget", "Defense Effect:"), 0, 100)
			.withSuffix(QT_TRANSLATE_NOOP("HeroCommandWidget", " %")),
		number("intelligence", 4, QT_TRANSLATE_NOOP("HeroCommandWidget", "Intelligence Effect:"), 0, 100)
			.withSuffix(QT_TRANSLATE_NOOP("HeroCommandWidget", " %")),
		number("variance", 5, QT_TRANSLATE_NOOP("HeroCommandWidget", "Variance:"), 0, 10),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Store Amount of Damage Dealt")),
		check("store", 6, QT_TRANSLATE_NOOP("HeroCommandWidget", "Store in Variable")),
		variable("variable", 7, QT_TRANSLATE_NOOP("HeroCommandWidget", "Variable:")).enabledUnless("store", {0})
	));

	// Change Hero Name: a new hero suggests its name, unless one was typed.
	// Maniac Patch: the hero from a variable, the name from a string (modes in p1)
	table.add(Code::ChangeHeroName, "ChangeActorNameWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Hero Name")), fields(
		modalNumber(ref(Kind::Actor, "hero", 0, QT_TRANSLATE_NOOP("ChangeActorNameWidget", "Actor:")), 1, 0),
		// the length of a hero's name in the database
		modalText(string("name", QT_TRANSLATE_NOOP("ChangeActorNameWidget", "New Name:")).withLimit(12), 1, 1, 2)
	));

	// Change Hero Title: the same with the title
	table.add(Code::ChangeHeroTitle, "ChangeActorTitleWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Hero Title")), fields(
		modalNumber(ref(Kind::Actor, "hero", 0, QT_TRANSLATE_NOOP("ChangeActorTitleWidget", "Actor:")), 1, 0),
		modalText(string("title", QT_TRANSLATE_NOOP("ChangeActorTitleWidget", "New Title:")).withLimit(12), 1, 1, 2)
	));

	// Change Hero Graphic: a picker; a new hero shows its graphic. Maniac
	// Patch: the modes of the hero, the file (a string, in p4) and the index in p3
	table.custom(Code::ChangeSpriteAssociation, "ChangeActorGraphicWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Hero Graphic")), fields(
		section(QT_TRANSLATE_NOOP("ChangeActorGraphicWidget", "Actor")),
		modalNumber(ref(Kind::Actor, "hero", 0), 3, 0),
		section(QT_TRANSLATE_NOOP("ChangeActorGraphicWidget", "Character Graphic")),
		graphic(Kind::Charset, "graphic", 1, CHARSET),
		check("translucent", 2, QT_TRANSLATE_NOOP("ChangeActorGraphicWidget", "Translucent")),
		section(in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Maniac Patch"))),
		modalText(string("fileName", QT_TRANSLATE_NOOP("ChangeActorGraphicWidget", "File")), 3, 1, 4),
		modalNumber(number("fileIndex", 1, QT_TRANSLATE_NOOP("ChangeActorGraphicWidget", "Index"), 0, 7), 3, 2)
	));

	// Change Hero Face: a picker; a new hero shows its face. Maniac Patch:
	// the modes of the hero, the file (a string, in p3) and the index in p2
	table.custom(Code::ChangeActorFace, "ChangeActorFaceWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Hero Face")), fields(
		section(QT_TRANSLATE_NOOP("ChangeActorFaceWidget", "Actor")),
		modalNumber(ref(Kind::Actor, "hero", 0), 2, 0),
		section(QT_TRANSLATE_NOOP("ChangeActorFaceWidget", "Faceset")),
		graphic(Kind::Face, "face", 1, FACESET),
		section(in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Maniac Patch"))),
		modalText(string("fileName", QT_TRANSLATE_NOOP("ChangeActorFaceWidget", "File")), 2, 1, 3),
		modalNumber(number("fileIndex", 1, QT_TRANSLATE_NOOP("ChangeActorFaceWidget", "Index"), 0, 15), 2, 2)
	));

	// Change Vehicle Graphic: a picker; a new vehicle shows its graphic
	table.custom(Code::ChangeVehicleGraphic, "ChangeVehicleGraphicWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Vehicle Graphic")), fields(
		section(QT_TRANSLATE_NOOP("ChangeVehicleGraphicWidget", "Vehicle")),
		choice("vehicle", 0, {}, {
			{0, QT_TRANSLATE_NOOP("ChangeVehicleGraphicWidget", "Boat")},
			{1, QT_TRANSLATE_NOOP("ChangeVehicleGraphicWidget", "Ship")},
			{2, QT_TRANSLATE_NOOP("ChangeVehicleGraphicWidget", "Airship")}}).radio(),
		section(QT_TRANSLATE_NOOP("ChangeVehicleGraphicWidget", "Graphic")),
		graphic(Kind::Charset, "graphic", 1, CHARSET)
	));
}

// System settings, battles, shops and inns
void addSystemAndScenes(Table& table) {
	// Change System BGM
	table.add(Code::ChangeSystemBGM, "MediaAudioWidget", in("ChangeSystemBGMWidget", QT_TRANSLATE_NOOP("ChangeSystemBGMWidget", "Change System BGM")), fields(
		section(QT_TRANSLATE_NOOP("MediaAudioWidget", "Event")),
		choice("event", 0, {}, {
			{0, QT_TRANSLATE_NOOP("MediaAudioWidget", "Battle")}, {1, QT_TRANSLATE_NOOP("MediaAudioWidget", "Victory")},
			{2, QT_TRANSLATE_NOOP("MediaAudioWidget", "Inn")}, {3, QT_TRANSLATE_NOOP("MediaAudioWidget", "Skiff")},
			{4, QT_TRANSLATE_NOOP("MediaAudioWidget", "Ship")}, {5, QT_TRANSLATE_NOOP("MediaAudioWidget", "Airship")},
			{6, QT_TRANSLATE_NOOP("MediaAudioWidget", "Game Over")}}),
		section(QT_TRANSLATE_NOOP("MediaAudioWidget", "Associated BGM")),
		// Maniac Patch: modes in p5, the file's string in p6
		withModes(fields(audioFile(MUSIC), fadeIn(1), audio(2)), 5, 6)
	));

	// Change System SE
	table.add(Code::ChangeSystemSFX, "MediaAudioWidget", in("ChangeSystemSFXWidget", QT_TRANSLATE_NOOP("ChangeSystemSFXWidget", "Change System Sound Effects")), fields(
		section(QT_TRANSLATE_NOOP("MediaAudioWidget", "Event")),
		choice("event", 0, {}, {
			{0, QT_TRANSLATE_NOOP("MediaAudioWidget", "Cursor")}, {1, QT_TRANSLATE_NOOP("MediaAudioWidget", "Decision")},
			{2, QT_TRANSLATE_NOOP("MediaAudioWidget", "Cancel")}, {3, QT_TRANSLATE_NOOP("MediaAudioWidget", "Buzzer")},
			{4, QT_TRANSLATE_NOOP("MediaAudioWidget", "Begin Battle")}, {5, QT_TRANSLATE_NOOP("MediaAudioWidget", "Escape")},
			{6, QT_TRANSLATE_NOOP("MediaAudioWidget", "Enemy Attacks")}, {7, QT_TRANSLATE_NOOP("MediaAudioWidget", "Enemy Damage")},
			{8, QT_TRANSLATE_NOOP("MediaAudioWidget", "Ally Damage")}, {9, QT_TRANSLATE_NOOP("MediaAudioWidget", "Evasion")},
			{10, QT_TRANSLATE_NOOP("MediaAudioWidget", "Enemy Defeated")}, {11, QT_TRANSLATE_NOOP("MediaAudioWidget", "Use Item")}}),
		section(QT_TRANSLATE_NOOP("MediaAudioWidget", "Associated Sound")),
		// Maniac Patch: modes in p4, the file's string in p5
		withModes(fields(audioFile(SOUND), audio(1)), 4, 5)
	));

	// Change System Graphics: the Maniac Patch's mode of the file in p2, its string in p3
	table.add(Code::ChangeSystemGraphics, "ChangeSystemGraphicsWidget", QT_TRANSLATE_NOOP("ChangeSystemGraphicsWidget", "Change System Graphics"), fields(
		section(QT_TRANSLATE_NOOP("ChangeSystemGraphicsWidget", "System Graphic")),
		modalText(file("graphic", {}, SYSTEM), 2, 0, 3),
		section(QT_TRANSLATE_NOOP("ChangeSystemGraphicsWidget", "Graphic Pattern")),
		choice("pattern", 0, {}, {
			{0, QT_TRANSLATE_NOOP("ChangeSystemGraphicsWidget", "Stretch to Fit")},
			{1, QT_TRANSLATE_NOOP("ChangeSystemGraphicsWidget", "Tiled Display")}}).radio(),
		section(QT_TRANSLATE_NOOP("ChangeSystemGraphicsWidget", "Text Font")),
		choice("font", 1, {}, {
			{0, QT_TRANSLATE_NOOP("ChangeSystemGraphicsWidget", "MS Gothic")},
			{1, QT_TRANSLATE_NOOP("ChangeSystemGraphicsWidget", "MS Mincho")}}).radio()
	));

	// Change Screen Transitions: the transitions that show the screen for the odd events
	table.add(Code::ChangeScreenTransitions, "ChangeScreenTransitionsWidget",
		QT_TRANSLATE_NOOP("ChangeScreenTransitionsWidget", "Change Screen Transitions"), fields(
		section(QT_TRANSLATE_NOOP("ChangeScreenTransitionsWidget", "Event")),
		choice("event", 0, {}, {
			{0, QT_TRANSLATE_NOOP("ChangeScreenTransitionsWidget", "Teleport/Erase")},
			{1, QT_TRANSLATE_NOOP("ChangeScreenTransitionsWidget", "Teleport/Show")},
			{2, QT_TRANSLATE_NOOP("ChangeScreenTransitionsWidget", "Begin Battle/Erase")},
			{3, QT_TRANSLATE_NOOP("ChangeScreenTransitionsWidget", "Begin Battle/Show")},
			{4, QT_TRANSLATE_NOOP("ChangeScreenTransitionsWidget", "End Battle/Erase")},
			{5, QT_TRANSLATE_NOOP("ChangeScreenTransitionsWidget", "End Battle/Show")}}),
		section(QT_TRANSLATE_NOOP("ChangeScreenTransitionsWidget", "Associated Transition")),
		choice("erase", 1, {}, transitions(false) + Items{
			{20, in("ScreenTransitionWidget", QT_TRANSLATE_NOOP("ScreenTransitionWidget", "No Transition"))}}).unless("event", {1, 3, 5}),
		choice("show", 1, {}, transitions(true) + Items{
			{20, in("ScreenTransitionWidget", QT_TRANSLATE_NOOP("ScreenTransitionWidget", "No Transition"))}}).when("event", {1, 3, 5})
	));

	// Start Combat: the victory, escape and defeat branches follow it when
	// a handler is custom
	table.custom(Code::EnemyEncounter, "EnemyEncounterWidget", QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Enemy Encounter"), fields(
		section(QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Monster Group")),
		choice("troopKind", 0, {}, {
			{0, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Specific Monster Group")},
			{1, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Monster Group with ID Stored in")},
			pointerItem()}).radio(),
		ref(Kind::Troop, "troop", 1).when("troopKind", {0}).inlined(),
		variable("troopVariable", 1).when("troopKind", {1}).inlined(),
		variable("troopPointer", 1).when("troopKind", {2}).inlined().maniac(),
		section(QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Battle Background")),
		choice("background", 2, {}, {
			{0, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Use Background Defined in Map or Terrain Settings")},
			{1, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Specific Background")},
			{2, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Use Background Associated with"), Flag::Rpg2003}}).radio(),
		file("backgroundFile", {}, BACKDROP).enabledWhen("background", {1}).inlined(),
		ref(Kind::Terrain, "terrain", 8).enabledWhen("background", {2}).inlined().rpg2003(),
		// 0 (the terrain's formation) is not offered by RPG Maker
		choice("formation", 7, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Formation"), {
			{1, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Loose Formation")},
			{2, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Tight Formation")}}).withDefault(1).enabledWhen("background", {1}).rpg2003(),
		section(QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Option")),
		check("firstStrike", 5, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Begin Battle with First Strike without Affecting Enemy Disposition")),
		// Maniac Patch: bit 0 of p9
		check("noFlash", 9, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "No Flash Before the Battle"))).bit(0).maniacOnly().extension(),
		section(QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Escape Handler")),
		choice("escape", 3, {}, {
			{0, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Disallow Escape")},
			{1, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "End Event Processing")},
			{2, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Execute Custom Handler")}}).radio(),
		section(QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Defeat Handler")),
		choice("defeat", 4, {}, {
			{0, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Game Over")},
			{1, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Execute Custom Handler")}}).radio(),
		section(QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Special Battle Conditions")),
		choice("condition", 6, {}, {
			{0, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "No Special Conditions")},
			{1, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Initiative")},
			{2, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Back Attack")},
			{3, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Surround Attack")},
			{4, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Pincers Attack")}}).radio().rpg2003()
	));

	// Call Shop: the goods are the parameters after the fourth; the
	// transaction branches follow it when its handlers are custom
	table.custom(Code::OpenShop, "OpenShopWidget", QT_TRANSLATE_NOOP("OpenShopWidget", "Open Shop Window"), fields(
		section(QT_TRANSLATE_NOOP("OpenShopWidget", "Transactions Available")),
		choice("transactions", 0, {}, {
			{0, QT_TRANSLATE_NOOP("OpenShopWidget", "Buy and Sell")},
			{1, QT_TRANSLATE_NOOP("OpenShopWidget", "Buy Only")},
			{2, QT_TRANSLATE_NOOP("OpenShopWidget", "Sell Only")}}).radio(),
		section(QT_TRANSLATE_NOOP("OpenShopWidget", "Shop Message Set")),
		choice("messages", 1, {}, {
			{0, QT_TRANSLATE_NOOP("OpenShopWidget", "A")},
			{1, QT_TRANSLATE_NOOP("OpenShopWidget", "B")},
			{2, QT_TRANSLATE_NOOP("OpenShopWidget", "C")}}).bitsAt(0, 8),
		section(QT_TRANSLATE_NOOP("OpenShopWidget", "Option")),
		check("handlers", 2, QT_TRANSLATE_NOOP("OpenShopWidget", "Define Custom Handlers for \"Transaction Made\" and \"Transaction not Made\"")),
		// a shop that only buys sells nothing
		section(QT_TRANSLATE_NOOP("OpenShopWidget", "Goods for Sale")).enabledUnless("transactions", {2}),
		// Maniac Patch: the item IDs in variables from p3 (or from the one in
		// p3: a pointer), as many as p4 (its mode in nibble 3 of p1)
		choice("goodsMode", 1, {}, {
			{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Item list"))},
			{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Item IDs in variables")), Flag::Maniac},
			{2, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Item IDs in variables from a variable pointer")), Flag::Maniac}})
			.bitsAt(8, 4).maniacOnly().enabledUnless("transactions", {2}),
		field(Kind::ItemList, "goods", 4, {}).when("goodsMode", {0}).enabledUnless("transactions", {2}),
		variable("goodsVariable", 3, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "From"))).when("goodsMode", {1, 2}).maniac()
			.enabledUnless("transactions", {2}),
		modalNumber(number("goodsCount", 4, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Count")), 0, 9999999).withDefault(1)
			.when("goodsMode", {1, 2}).maniac().enabledUnless("transactions", {2}), 1, 3)
	));

	// Call Inn: the stay branches follow it when its handler is custom
	table.add(Code::ShowInn, "ShowInnWidget", QT_TRANSLATE_NOOP("ShowInnWidget", "Show Inn Message"), fields(
		section(QT_TRANSLATE_NOOP("ShowInnWidget", "Inn Message Set")),
		choice("messages", 0, {}, {
			{0, QT_TRANSLATE_NOOP("ShowInnWidget", "A")},
			{1, QT_TRANSLATE_NOOP("ShowInnWidget", "B")}}),
		section(QT_TRANSLATE_NOOP("ShowInnWidget", "Price")),
		number("price", 1, {}, 0, 999999),
		section(QT_TRANSLATE_NOOP("ShowInnWidget", "Option")),
		check("handlers", 2, QT_TRANSLATE_NOOP("ShowInnWidget", "Use Custom Inn Handler"))
	));

	// Enter Hero Name: the first page of the keyboard depends on the encoding.
	// Maniac Patch: the modes of the hero and of the character limit in p3
	table.add(Code::EnterHeroName, "EnterHeroNameWidget", QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Enter Hero Name"), fields(
		section(QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Hero")),
		modalNumber(ref(Kind::Actor, "hero", 0), 3, 0),
		// 1 to 30 characters, 0: RPG Maker's limit
		modalNumber(number("limit", 4, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Character limit (0: default)")), 0, 30)
			.maniacOnly().extension(), 3, 4),
		section(QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Initial Character Set")),
		// the pages of the other encodings: Hiragana, Katakana, Hangul 1...
		field(Kind::CharacterSet, "characterSet", 1, {}).withItems({
			{0, QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Letters")},
			{1, QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Symbols")}}).radio(),
		section(QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Option")),
		check("showDefault", 2, QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Show Default Name in Entry Area"))
	));
}

// Show Picture and Move Picture: the fields of RPG Maker
// 2003 and the Maniac Patch tab. With Maniac Patch a value can be given by a
// variable: its mode is in bits of p17, p20, p21 or the angle modes
Fields pictureFields(bool move) {
	// low byte: bottom transparency; above: Maniac Patch blend mode and flips
	const int flags = move ? 16 : 14;
	// the modes of the effect's angle and its divisor
	const int angleModes = move ? 4 : 16;
	auto pictureLabel = [](const char* text) { return Label("PictureWidget", text); };
	// the mode of a value (Maniac Patch), in bits of a parameter
	auto valueMode = [&](const char* id, int index, int shift, int bits) {
		Items items;
		for (const auto& one : numberModes()) {
			items.push_back({one.mode, one.label});
		}
		return choice(id, index, pictureLabel(QT_TRANSLATE_NOOP("PictureWidget", "Given as")), std::move(items))
			.bitsAt(shift, bits).maniac().with(index >= flags ? Flags(Flag::Extension) : Flags());
	};
	// the variable a value is in, for the modes other than a number
	auto valueVariable = [&](const char* id, int index, const char* modeId, Label label) {
		return variable(id, index, label).unless(modeId, {0}).maniac();
	};
	Fields list = fields(
		section(QT_TRANSLATE_NOOP("PictureWidget", "Picture No.")),
		valueMode("pictureMode", 17, 0, move ? 8 : 4),
		number("picture", 0, QT_TRANSLATE_NOOP("PictureWidget", "Picture No."), 1, 9999).withDefault(1).when("pictureMode", {0}),
		valueVariable("pictureVariable", 0, "pictureMode", pictureLabel(QT_TRANSLATE_NOOP("PictureWidget", "Picture No."))));
	if (!move) {
		append(list, fields(
			section(QT_TRANSLATE_NOOP("PictureWidget", "Image File")),
			file("file", {}, PICTURE)));
	}
	append(list, fields(
		section(move ? QT_TRANSLATE_NOOP("PictureWidget", "Target Coordinates") : QT_TRANSLATE_NOOP("PictureWidget", "Display Coordinates")),
		// the upper bits are the origin of Maniac Patch
		choice("position", 1, {}, {
			{0, QT_TRANSLATE_NOOP("PictureWidget", "Specific Coordinates")},
			{1, QT_TRANSLATE_NOOP("PictureWidget", "Variable Reference")},
			{2, QT_TRANSLATE_NOOP("PictureWidget", "Variable Pointer"), Flag::Maniac}}).bitsAt(0, 8).radio(),
		number("x", 2, QT_TRANSLATE_NOOP("PictureWidget", "X"), -10000, 10000).withDefault(160).unless("position", {1, 2}),
		number("y", 3, QT_TRANSLATE_NOOP("PictureWidget", "Y"), -10000, 10000).withDefault(120).unless("position", {1, 2}),
		variable("xVariable", 2, QT_TRANSLATE_NOOP("PictureWidget", "X")).when("position", {1, 2}),
		variable("yVariable", 3, QT_TRANSLATE_NOOP("PictureWidget", "Y")).when("position", {1, 2})));
	if (!move) {
		append(list, fields(
			check("scroll", 4, QT_TRANSLATE_NOOP("PictureWidget", "Picture Scrolls with Map")),
			section(QT_TRANSLATE_NOOP("PictureWidget", "Transparent Color")),
			choice("transparentColor", 7, {}, {
				{1, QT_TRANSLATE_NOOP("PictureWidget", "Use")},
				{0, QT_TRANSLATE_NOOP("PictureWidget", "None")}}).withDefault(1)));
	}
	append(list, fields(
		section(QT_TRANSLATE_NOOP("PictureWidget", "Magnification")),
		// p20: the width's mode, bit 4 for a separate height, then the height's mode
		valueMode("magnificationMode", 20, 0, 4),
		number("magnification", 5, {}, 0, 2000).withDefault(100).withSuffix(QT_TRANSLATE_NOOP("PictureWidget", "%"))
			.when("magnificationMode", {0}),
		valueVariable("magnificationVariable", 5, "magnificationMode", {}),
		check("separateHeight", 20, QT_TRANSLATE_NOOP("PictureWidget", "Separate height")).bit(4).maniac().extension(),
		valueMode("heightMode", 20, 5, 4).enabledWhen("separateHeight", {1}),
		number("height", move ? 18 : 31, QT_TRANSLATE_NOOP("PictureWidget", "Height"), 0, 2000).withDefault(100)
			.withSuffix(QT_TRANSLATE_NOOP("PictureWidget", "%")).when("heightMode", {0}).enabledWhen("separateHeight", {1})
			.maniac().extension(),
		valueVariable("heightVariable", move ? 18 : 31, "heightMode", pictureLabel(QT_TRANSLATE_NOOP("PictureWidget", "Height")))
			.enabledWhen("separateHeight", {1}).extension(),
		section(QT_TRANSLATE_NOOP("PictureWidget", "Transparency")),
		valueMode("topMode", 21, 0, 32),
		number("top", 6, QT_TRANSLATE_NOOP("PictureWidget", "Top Half"), 0, 100).withSuffix(QT_TRANSLATE_NOOP("PictureWidget", "%"))
			.when("topMode", {0}),
		valueVariable("topVariable", 6, "topMode", pictureLabel(QT_TRANSLATE_NOOP("PictureWidget", "Top Half"))),
		// a missing one is the top's, and gets it when the command grows
		number("bottom", flags, QT_TRANSLATE_NOOP("PictureWidget", "Bottom Half"), 0, 100).bitsAt(0, 8).copying(6)
			.withSuffix(QT_TRANSLATE_NOOP("PictureWidget", "%")).rpg2003().extension()));
	if (move) {
		append(list, fields(
			section(QT_TRANSLATE_NOOP("PictureWidget", "Transition Time")),
			valueMode("durationMode", 17, 8, 4),
			// Maniac Patch: a negative time is in frames
			number("duration", 14, {}, 0, 10000).maniacRange(-10000, 10000)
				.withSuffix(QT_TRANSLATE_NOOP("PictureWidget", " × 0.1 s")).when("durationMode", {0}),
			valueVariable("durationVariable", 14, "durationMode", {}),
			section(QT_TRANSLATE_NOOP("PictureWidget", "Option")),
			// the other bits: the Maniac Patch options below
			check("wait", 15, QT_TRANSLATE_NOOP("PictureWidget", "Wait for completion")).bit(0)));
	}
	append(list, fields(
		section(QT_TRANSLATE_NOOP("PictureWidget", "Color Tint")),
		// one mode for the four values
		valueMode("colorMode", 17, 12, 4)));
	const char* colors[] = {"red", "green", "blue", "saturation"};
	const char* colorVariables[] = {"redVariable", "greenVariable", "blueVariable", "saturationVariable"};
	const char* colorNames[] = {QT_TRANSLATE_NOOP("PictureWidget", "Red"), QT_TRANSLATE_NOOP("PictureWidget", "Green"),
		QT_TRANSLATE_NOOP("PictureWidget", "Blue"), QT_TRANSLATE_NOOP("PictureWidget", "Saturation")};
	for (int i = 0; i < 4; ++i) {
		append(list, fields(
			number(colors[i], 8 + i, pictureLabel(colorNames[i]), 0, 200).withDefault(100)
				.withSuffix(QT_TRANSLATE_NOOP("PictureWidget", "%")).when("colorMode", {0}),
			valueVariable(colorVariables[i], 8 + i, "colorMode", pictureLabel(colorNames[i]))));
	}
	append(list, fields(
		section(QT_TRANSLATE_NOOP("PictureWidget", "Effects")),
		choice("effect", 12, QT_TRANSLATE_NOOP("PictureWidget", "Effect"), {
			{0, QT_TRANSLATE_NOOP("PictureWidget", "None")},
			{1, QT_TRANSLATE_NOOP("PictureWidget", "Rotation")},
			{2, QT_TRANSLATE_NOOP("PictureWidget", "Waver")},
			{3, QT_TRANSLATE_NOOP("PictureWidget", "Fixed Angle"), Flag::Maniac}}),
		valueMode("powerMode", angleModes, 0, 4).enabledUnless("effect", {0}),
		// the name of the power follows the effect
		number("speed", 13, QT_TRANSLATE_NOOP("PictureWidget", "Speed"), 0, 10000).when("effect", {1}).when("powerMode", {0}),
		number("magnitude", 13, QT_TRANSLATE_NOOP("PictureWidget", "Magnitude"), 0, 10000).unless("effect", {1, 3})
			.enabledUnless("effect", {0}).when("powerMode", {0}),
		number("angle", 13, QT_TRANSLATE_NOOP("PictureWidget", "Angle"), -10000, 10000).when("effect", {3}).when("powerMode", {0}).maniac(),
		valueVariable("powerVariable", 13, "powerMode", pictureLabel(QT_TRANSLATE_NOOP("PictureWidget", "Magnitude")))
			.enabledUnless("effect", {0}),
		// Maniac Patch, fixed angle: angle / divisor
		valueMode("divisorMode", angleModes, 4, 4).when("effect", {3}),
		number("divisor", move ? 7 : 15, QT_TRANSLATE_NOOP("PictureWidget", "Divisor"), -10000, 10000)
			.when("effect", {3}).when("divisorMode", {0}).maniac().with(move ? Flags() : Flags(Flag::Extension)),
		valueVariable("divisorVariable", move ? 7 : 15, "divisorMode", pictureLabel(QT_TRANSLATE_NOOP("PictureWidget", "Divisor")))
			.when("effect", {3}).with(move ? Flags() : Flags(Flag::Extension))));

	Fields maniac = fields(
		section(QT_TRANSLATE_NOOP("PictureWidget", "Maniac Patch")),
		section(QT_TRANSLATE_NOOP("PictureWidget", "Display")),
		choice("origin", 1, QT_TRANSLATE_NOOP("PictureWidget", "Origin"), {
			{0, QT_TRANSLATE_NOOP("PictureWidget", "Center")}, {1, QT_TRANSLATE_NOOP("PictureWidget", "Top left")},
			{2, QT_TRANSLATE_NOOP("PictureWidget", "Bottom left")}, {3, QT_TRANSLATE_NOOP("PictureWidget", "Top right")},
			{4, QT_TRANSLATE_NOOP("PictureWidget", "Bottom right")}, {5, QT_TRANSLATE_NOOP("PictureWidget", "Top")},
			{6, QT_TRANSLATE_NOOP("PictureWidget", "Bottom")}, {7, QT_TRANSLATE_NOOP("PictureWidget", "Left")},
			{8, QT_TRANSLATE_NOOP("PictureWidget", "Right")}}).bitsAt(8, 24),
		choice("blend", flags, QT_TRANSLATE_NOOP("PictureWidget", "Blend mode"), {
			{0, QT_TRANSLATE_NOOP("PictureWidget", "Normal")}, {1, QT_TRANSLATE_NOOP("PictureWidget", "Multiply")},
			{2, QT_TRANSLATE_NOOP("PictureWidget", "Additive")}, {3, QT_TRANSLATE_NOOP("PictureWidget", "Overlay")}}).bitsAt(8, 2).extension(),
		check("flipX", flags, QT_TRANSLATE_NOOP("PictureWidget", "Flip horizontally")).bit(12).extension(),
		check("flipY", flags, QT_TRANSLATE_NOOP("PictureWidget", "Flip vertically")).bit(13).extension());
	if (move) {
		// bit 0 is "wait"; EasyRPG Player reads the others but does not use them yet
		append(maniac, fields(
			section(QT_TRANSLATE_NOOP("PictureWidget", "Option")),
			check("relative", 15, QT_TRANSLATE_NOOP("PictureWidget", "Relative to the current position")).bit(1),
			check("keepTint", 15, QT_TRANSLATE_NOOP("PictureWidget", "Keep the color tint")).bit(2),
			check("keepEffect", 15, QT_TRANSLATE_NOOP("PictureWidget", "Keep the effect")).bit(3),
			check("keepBlend", 15, QT_TRANSLATE_NOOP("PictureWidget", "Keep the blend mode")).bit(4),
			check("keepFlips", 15, QT_TRANSLATE_NOOP("PictureWidget", "Keep the flips")).bit(5),
			check("keepTime", 15, QT_TRANSLATE_NOOP("PictureWidget", "Keep the transition time")).bit(6),
			check("magnifiedPosition", 15, QT_TRANSLATE_NOOP("PictureWidget", "Position of the magnified picture")).bit(8)));
	} else {
		// slot 2 of p17: the file name, from this command or from a string (p30)
		append(maniac, fields(
			section(QT_TRANSLATE_NOOP("PictureWidget", "Image File")),
			choice("nameMode", 17, QT_TRANSLATE_NOOP("PictureWidget", "Name given as"), {
				{0, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Text"))},
				{1, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "String"))},
				{2, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "String pointer"))}}).bitsAt(8, 4).extension(),
			number("nameString", 30, QT_TRANSLATE_NOOP("PictureWidget", "Name"), 0, 99999).when("nameMode", {1}).extension(),
			variable("nameVariable", 30, QT_TRANSLATE_NOOP("PictureWidget", "Name")).when("nameMode", {2}).extension()));
		// p24: 2 animates the frames, else the mode of the frame number (from 1)
		append(maniac, only(fields(
			section(QT_TRANSLATE_NOOP("PictureWidget", "Spritesheet")),
			number("columns", 22, QT_TRANSLATE_NOOP("PictureWidget", "Columns (0: none)"), 0, 999),
			number("rows", 23, QT_TRANSLATE_NOOP("PictureWidget", "Rows"), 0, 999),
			choice("frameMode", 24, QT_TRANSLATE_NOOP("PictureWidget", "Show"), {
				{0, QT_TRANSLATE_NOOP("PictureWidget", "Frame")},
				{1, QT_TRANSLATE_NOOP("PictureWidget", "Frame in variable")},
				{2, QT_TRANSLATE_NOOP("PictureWidget", "Animation")}}),
			number("frame", 25, QT_TRANSLATE_NOOP("PictureWidget", "Frame or speed"), 0, 99999).when("frameMode", {0, 2}),
			variable("frameVariable", 25, QT_TRANSLATE_NOOP("PictureWidget", "Frame or speed")).when("frameMode", {1}),
			check("once", 26, QT_TRANSLATE_NOOP("PictureWidget", "Play once")).when("frameMode", {2}),
			section(QT_TRANSLATE_NOOP("PictureWidget", "Layer")),
			number("mapLayer", 27, QT_TRANSLATE_NOOP("PictureWidget", "Map layer"), 0, 10),
			number("battleLayer", 28, QT_TRANSLATE_NOOP("PictureWidget", "Battle layer"), 0, 10),
			// a missing parameter is 1 | 32 | 64 for EasyRPG Player
			check("eraseOnMapChange", 29, QT_TRANSLATE_NOOP("PictureWidget", "Erase on map change")).bit(0).withDefault(1),
			check("eraseOnBattleEnd", 29, QT_TRANSLATE_NOOP("PictureWidget", "Erase when the battle ends")).bit(1),
			check("tint", 29, QT_TRANSLATE_NOOP("PictureWidget", "Affected by the screen tint")).bit(4),
			check("flash", 29, QT_TRANSLATE_NOOP("PictureWidget", "Affected by the screen flash")).bit(5).withDefault(1),
			check("shake", 29, QT_TRANSLATE_NOOP("PictureWidget", "Affected by the screen shake")).bit(6).withDefault(1)), Flag::Extension));
	}
	append(list, only(maniac, Flag::Maniac));
	return list;
}

// Locations, screen effects, pictures and characters
void addMapAndScreen(Table& table) {
	const char* context = "LocationCommandWidget";

	// Teleport
	table.add(Code::Teleport, context, QT_TRANSLATE_NOOP("LocationCommandWidget", "Teleport"), fields(
		location(-1, 0, 1, 2),
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Character Facing")),
		facing(3, false)
	));

	// Memorize Location
	table.add(Code::MemorizeLocation, context, QT_TRANSLATE_NOOP("LocationCommandWidget", "Memorize Position"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Storage Locations")),
		variable("mapVariable", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Store Map ID in")),
		variable("xVariable", 1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Store X Coordinate in")),
		variable("yVariable", 2, QT_TRANSLATE_NOOP("LocationCommandWidget", "Store Y Coordinate in"))
	));

	// Recall to Location
	table.add(Code::RecallToLocation, context, QT_TRANSLATE_NOOP("LocationCommandWidget", "Recall to Memorized Position"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Recall Location")),
		variable("mapVariable", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Map ID Stored in")),
		variable("xVariable", 1, QT_TRANSLATE_NOOP("LocationCommandWidget", "X Coordinate Stored in")),
		variable("yVariable", 2, QT_TRANSLATE_NOOP("LocationCommandWidget", "Y Coordinate Stored in"))
	));

	// Set Vehicle Location
	table.add(Code::SetVehicleLocation, context, QT_TRANSLATE_NOOP("LocationCommandWidget", "Set Vehicle Location"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Vehicle")),
		choice("vehicle", 0, {}, {
			{0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Boat")},
			{1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Ship")},
			{2, QT_TRANSLATE_NOOP("LocationCommandWidget", "Airship")},
			// the party itself: RPG_RT moves it (the Maniac Patch's option)
			{-1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Party"), Flag::Maniac}}).radio(),
		location(1, 2, 3, 4),
		// Maniac Patch: the facing after the move
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Character Facing")),
		facing(5, false).maniacOnly().extension()
	));

	// Set Event Location: the cell on the map of the event. The cell's mode
	// is the low byte of p1; Maniac Patch: the event's mode in its nibble 2
	// (256 a variable, 512 a variable pointer)
	table.add(Code::ChangeEventLocation, context, QT_TRANSLATE_NOOP("LocationCommandWidget", "Change Event Location"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Event")),
		modalNumber(character("event", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Event"), locationCharacters()), 1, 2),
		location(1, -1, 2, 3, 8),
		facing(4).enabledWhen("locationMode", {0})
	));

	// Swap Event Locations: Maniac Patch: the modes of the events in the
	// nibbles of p2 (a variable, a variable pointer)
	table.add(Code::TradeEventLocations, context, QT_TRANSLATE_NOOP("LocationCommandWidget", "Trade Event Locations"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "First Event")),
		modalNumber(character("first", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Event"), locationCharacters()), 2, 0),
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Second Event")),
		modalNumber(character("second", 1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Event"), locationCharacters()), 2, 1)
	));

	// Get Terrain ID
	table.add(Code::StoreTerrainID, context, QT_TRANSLATE_NOOP("LocationCommandWidget", "Store Terrain ID"), fields(
		location(0, -1, 1, 2, 32, true),
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Storage Location")),
		variable("result", 3, QT_TRANSLATE_NOOP("LocationCommandWidget", "Store Terrain ID in Variable"))
	));

	// Get Event ID
	table.add(Code::StoreEventID, context, QT_TRANSLATE_NOOP("LocationCommandWidget", "Store Event ID"), fields(
		location(0, -1, 1, 2, 32, true),
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Storage Location")),
		variable("result", 3, QT_TRANSLATE_NOOP("LocationCommandWidget", "Store Event ID in Variable"))
	));

	// Erase Screen
	table.add(Code::EraseScreen, "ScreenTransitionWidget", in("EraseScreenWidget", QT_TRANSLATE_NOOP("EraseScreenWidget", "Erase Screen")), fields(
		section(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Transition Method")),
		// Maniac Patch: the transition in a variable (its mode in p1)
		modalNumber(choice("transition", 0, {}, Items{{-1, QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Use Default")}} + transitions(false))
			.withDefault(-1), 1, -1)
	));

	// Show Screen
	table.add(Code::ShowScreen, "ScreenTransitionWidget", in("ShowScreenWidget", QT_TRANSLATE_NOOP("ShowScreenWidget", "Show Screen")), fields(
		section(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Transition Method")),
		modalNumber(choice("transition", 0, {}, Items{{-1, QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Use Default")}} + transitions(true))
			.withDefault(-1), 1, -1)
	));

	// Tint Screen
	table.add(Code::TintScreen, "TintScreenWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Tint Screen")), fields(
		section(QT_TRANSLATE_NOOP("TintScreenWidget", "Color Tone")),
		number("red", 0, QT_TRANSLATE_NOOP("TintScreenWidget", "Red:"), 0, 200).slider().withSuffix(QT_TRANSLATE_NOOP("TintScreenWidget", " %")),
		number("green", 1, QT_TRANSLATE_NOOP("TintScreenWidget", "Green:"), 0, 200).slider().withSuffix(QT_TRANSLATE_NOOP("TintScreenWidget", " %")),
		number("blue", 2, QT_TRANSLATE_NOOP("TintScreenWidget", "Blue:"), 0, 200).slider().withSuffix(QT_TRANSLATE_NOOP("TintScreenWidget", " %")),
		number("saturation", 3, QT_TRANSLATE_NOOP("TintScreenWidget", "Saturation:"), 0, 200).slider()
			.withSuffix(QT_TRANSLATE_NOOP("TintScreenWidget", " %")),
		// RPG Maker's sample: colours with the tint
		sample("tint"),
		section(QT_TRANSLATE_NOOP("TintScreenWidget", "Transition Time")),
		number("time", 4, {}, 0, 1000).withSuffix(QT_TRANSLATE_NOOP("TintScreenWidget", "x0.1 sec")),
		section(QT_TRANSLATE_NOOP("TintScreenWidget", "Option")),
		check("wait", 5, QT_TRANSLATE_NOOP("TintScreenWidget", "Wait for Completion"))
	));

	// Flash Screen: colors of 0-31
	table.add(Code::FlashScreen, "FlashScreenWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Flash Screen")), fields(
		section(QT_TRANSLATE_NOOP("FlashScreenWidget", "Options")),
		choice("mode", 6, {}, {
			{0, QT_TRANSLATE_NOOP("FlashScreenWidget", "Flash Once")},
			{1, QT_TRANSLATE_NOOP("FlashScreenWidget", "Begin Flash")},
			{2, QT_TRANSLATE_NOOP("FlashScreenWidget", "End Flash")}}).radio(),
		// only a single flash is waited for
		check("wait", 5, QT_TRANSLATE_NOOP("FlashScreenWidget", "Wait for Completion")).enabledWhen("mode", {0}),
		number("time", 4, QT_TRANSLATE_NOOP("FlashScreenWidget", "Duration:"), 0, 1000)
			.withSuffix(QT_TRANSLATE_NOOP("FlashScreenWidget", "x0.1 sec")).enabledUnless("mode", {2}),
		section(QT_TRANSLATE_NOOP("FlashScreenWidget", "Flash Color")).enabledUnless("mode", {2}),
		sample("flash").enabledUnless("mode", {2}),
		number("red", 0, QT_TRANSLATE_NOOP("FlashScreenWidget", "Red:"), 0, 31).slider().enabledUnless("mode", {2}),
		number("green", 1, QT_TRANSLATE_NOOP("FlashScreenWidget", "Green:"), 0, 31).slider().enabledUnless("mode", {2}),
		number("blue", 2, QT_TRANSLATE_NOOP("FlashScreenWidget", "Blue:"), 0, 31).slider().enabledUnless("mode", {2}),
		number("strength", 3, QT_TRANSLATE_NOOP("FlashScreenWidget", "Strength:"), 0, 31).slider().enabledUnless("mode", {2})
	));

	// Shake Screen: the mode (parameter 4) is RPG Maker 2003's
	table.add(Code::ShakeScreen, "ShakeScreenWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Shake Screen")), fields(
		section(QT_TRANSLATE_NOOP("ShakeScreenWidget", "Options")),
		choice("mode", 4, {}, {
			{0, QT_TRANSLATE_NOOP("ShakeScreenWidget", "Shake Once")},
			{1, QT_TRANSLATE_NOOP("ShakeScreenWidget", "Begin Shake")},
			{2, QT_TRANSLATE_NOOP("ShakeScreenWidget", "End Shake")}}).radio().extension(),
		check("wait", 3, QT_TRANSLATE_NOOP("ShakeScreenWidget", "Wait for Completion")).enabledWhen("mode", {0}),
		number("time", 2, QT_TRANSLATE_NOOP("ShakeScreenWidget", "Duration:"), 0, 1000)
			.withSuffix(QT_TRANSLATE_NOOP("ShakeScreenWidget", "x0.1 sec")).enabledWhen("mode", {0}),
		// from "Weak" to "Strong"
		section(QT_TRANSLATE_NOOP("ShakeScreenWidget", "Strength")).enabledUnless("mode", {2}),
		number("strength", 0, {}, 0, 8).ticks().enabledUnless("mode", {2}),
		section(QT_TRANSLATE_NOOP("ShakeScreenWidget", "Speed")).enabledUnless("mode", {2}),
		number("speed", 1, {}, 0, 8).ticks().enabledUnless("mode", {2})
	));

	// Pan Screen
	table.add(Code::PanScreen, "PanScreenWidget", QT_TRANSLATE_NOOP("PanScreenWidget", "Pan Screen"), fields(
		section(QT_TRANSLATE_NOOP("PanScreenWidget", "Operation")),
		// Maniac Patch: a lock that teleports do not move (9), and the
		// scrolling by pixels: shifted or to a position, at a speed (0.001
		// pixel a frame) or in a number of frames (4 to 7)
		choice("operation", 0, {}, {
			{0, QT_TRANSLATE_NOOP("PanScreenWidget", "Lock Pan State")},
			{9, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Lock Even When Teleporting")), Flag::Maniac},
			{1, QT_TRANSLATE_NOOP("PanScreenWidget", "Unlock Pan State")},
			{2, QT_TRANSLATE_NOOP("PanScreenWidget", "Pan Screen")},
			{3, QT_TRANSLATE_NOOP("PanScreenWidget", "Return to Origin")},
			{4, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Shift by Pixels")), Flag::Maniac},
			{5, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Shift by Pixels in Frames")), Flag::Maniac},
			{6, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Scroll to Pixel Position")), Flag::Maniac},
			{7, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Scroll to Pixel Position in Frames")), Flag::Maniac}})
			.radio().withDefault(2).maniacColumns(3),
		// a cross of radio buttons, as RPG Maker 2003
		section(QT_TRANSLATE_NOOP("PanScreenWidget", "Direction")),
		choice("direction", 1, {}, {
			{0, QT_TRANSLATE_NOOP("PanScreenWidget", "Up")},
			{3, QT_TRANSLATE_NOOP("PanScreenWidget", "Left")},
			{1, QT_TRANSLATE_NOOP("PanScreenWidget", "Right")},
			{2, QT_TRANSLATE_NOOP("PanScreenWidget", "Down")}}).cross().enabledWhen("operation", {2}),
		section(QT_TRANSLATE_NOOP("PanScreenWidget", "Tiles")),
		number("distance", 2, {}, 0, 9999).withDefault(1).enabledWhen("operation", {2}),
		section(QT_TRANSLATE_NOOP("PanScreenWidget", "Transition Speed")),
		choice("speed", 3, {}, {
			{1, QT_TRANSLATE_NOOP("PanScreenWidget", "1: One-Eighth Normal")},
			{2, QT_TRANSLATE_NOOP("PanScreenWidget", "2: One-Fourth Normal")},
			{3, QT_TRANSLATE_NOOP("PanScreenWidget", "3: One-Half Normal")},
			{4, QT_TRANSLATE_NOOP("PanScreenWidget", "4: Normal")},
			{5, QT_TRANSLATE_NOOP("PanScreenWidget", "5: Twice Normal")},
			{6, QT_TRANSLATE_NOOP("PanScreenWidget", "6: Four Times Normal")},
			{0, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "0: Instantly")), Flag::Maniac}})
			.withDefault(4).enabledWhen("operation", {2, 3}),
		check("wait", 4, QT_TRANSLATE_NOOP("PanScreenWidget", "Wait for completion")).bit(0).enabledWhen("operation", {2, 3, 4, 5, 6, 7}),
		// Maniac Patch: the pixels, their modes in the nibbles of p1
		section(in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Pixel Scrolling"))),
		modalNumber(number("pixelX", 2, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "X (pixels)")), -9999999, 9999999)
			.when("operation", {4, 5, 6, 7}).maniacOnly(), 1, 0),
		modalNumber(number("pixelY", 3, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Y (pixels)")), -9999999, 9999999)
			.when("operation", {4, 5, 6, 7}).maniacOnly(), 1, 1),
		// 0.001 pixel a frame, or frames
		modalNumber(number("pixelSpeed", 5, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Speed or frames")), -9999999, 9999999)
			.when("operation", {4, 5, 6, 7}).maniacOnly().extension(), 1, 2),
		check("centered", 4, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Position of the center of the screen")))
			.bit(1).when("operation", {6, 7}).maniacOnly(),
		check("relative", 4, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Relative to the current position")))
			.bit(2).when("operation", {6, 7}).maniacOnly()
	));

	// Weather Effects
	table.add(Code::WeatherEffects, "WeatherEffectsWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Weather Effects")), fields(
		section(QT_TRANSLATE_NOOP("WeatherEffectsWidget", "Type")),
		choice("type", 0, {}, {
			{0, QT_TRANSLATE_NOOP("WeatherEffectsWidget", "No Weather")},
			{1, QT_TRANSLATE_NOOP("WeatherEffectsWidget", "Rain")},
			{2, QT_TRANSLATE_NOOP("WeatherEffectsWidget", "Snow")},
			{3, QT_TRANSLATE_NOOP("WeatherEffectsWidget", "Fog")},
			{4, QT_TRANSLATE_NOOP("WeatherEffectsWidget", "Sandstorm")}}).radio(),
		section(QT_TRANSLATE_NOOP("WeatherEffectsWidget", "Power")),
		choice("power", 1, {}, {
			{0, QT_TRANSLATE_NOOP("WeatherEffectsWidget", "Weak")},
			{1, QT_TRANSLATE_NOOP("WeatherEffectsWidget", "Medium")},
			{2, QT_TRANSLATE_NOOP("WeatherEffectsWidget", "Strong")}}).radio()
	));

	// Show Picture: RPG Maker 2003 1.12 and Maniac Patch add parameters 14-31
	// (a shorter command gets the values EasyRPG Player uses when they grow);
	// with Maniac Patch the values can come from variables (modes in p17, p20, p21)
	table.custom(Code::ShowPicture, "PictureWidget", in("ShowPictureWidget", QT_TRANSLATE_NOOP("ShowPictureWidget", "Show Picture")),
		pictureFields(false), Flag::NoFlag);

	// Move Picture
	table.custom(Code::MovePicture, "PictureWidget", in("MovePictureWidget", QT_TRANSLATE_NOOP("MovePictureWidget", "Move Picture")),
		pictureFields(true), Flag::NoFlag);

	// Erase Picture: the selection of RPG Maker 2003 and Maniac Patch, shown
	// only when the command has it or in Maniac Patch games
	table.custom(Code::ErasePicture, "ErasePictureWidget", QT_TRANSLATE_NOOP("ErasePictureWidget", "Erase Picture"), fields(
		section(QT_TRANSLATE_NOOP("ErasePictureWidget", "Picture No.")),
		choice("selection", 1, QT_TRANSLATE_NOOP("ErasePictureWidget", "Selection"), {
			{0, QT_TRANSLATE_NOOP("ErasePictureWidget", "Specific Picture")},
			{1, QT_TRANSLATE_NOOP("ErasePictureWidget", "Variable Reference")},
			{2, QT_TRANSLATE_NOOP("ErasePictureWidget", "Range")},
			{3, QT_TRANSLATE_NOOP("ErasePictureWidget", "Range from Variables"), Flag::Maniac},
			{4, QT_TRANSLATE_NOOP("ErasePictureWidget", "Indirect Variable Reference"), Flag::Maniac},
			{5, QT_TRANSLATE_NOOP("ErasePictureWidget", "All Pictures"), Flag::Maniac}}).extension().rpg2003(),
		number("picture", 0, QT_TRANSLATE_NOOP("ErasePictureWidget", "Picture No."), 1, 9999).withDefault(1).when("selection", {0, 2}),
		number("last", 2, QT_TRANSLATE_NOOP("ErasePictureWidget", "Through Picture No."), 1, 9999).withDefault(1)
			.when("selection", {2}).extension().rpg2003(),
		variable("variable", 0, QT_TRANSLATE_NOOP("ErasePictureWidget", "Variable")).when("selection", {1, 3, 4}).rpg2003(),
		variable("lastVariable", 2, QT_TRANSLATE_NOOP("ErasePictureWidget", "Through Variable")).when("selection", {3}).extension().rpg2003()
	));

	// Show Battle Animation. Maniac Patch: where it is shown in p3 (an event,
	// on the entire map with "Global Scope", a picture, a position, the one
	// in two variables), the cache of its file in bit 1 of p2, a buffer (p5,
	// several animations at once), the position (p6, p7) and the flip (p8);
	// the modes of the values in the nibbles of p4
	table.add(Code::ShowBattleAnimation, "ShowBattleAnimationWidget", QT_TRANSLATE_NOOP("ShowBattleAnimationWidget", "Show Battle Animation"), fields(
		section(QT_TRANSLATE_NOOP("ShowBattleAnimationWidget", "Battle Animation")),
		modalNumber(ref(Kind::Animation, "animation", 0).preview(), 4, 0),
		section(QT_TRANSLATE_NOOP("ShowBattleAnimationWidget", "Target")),
		choice("placement", 3, {}, {
			{0, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Follow Map Event"))},
			{1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Follow Map Event, on the Entire Map"))},
			{2, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Follow Picture")), Flag::Maniac},
			{3, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Fixed Position")), Flag::Maniac},
			{4, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Binding")), Flag::Maniac}}).maniacOnly(),
		modalNumber(character("target", 1, {}, pictureCharacters()).when("placement", {0, 1}), 4, 1),
		modalNumber(number("picture", 1, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture")), 1, 9999999)
			.withDefault(1).when("placement", {2}).maniacOnly(), 4, 1),
		modalNumber(number("x", 6, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "X")), -9999999, 9999999)
			.when("placement", {3}).maniacOnly().extension(), 4, 3),
		modalNumber(number("y", 7, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Y")), -9999999, 9999999)
			.when("placement", {3}).maniacOnly().extension(), 4, 4),
		modal(variable("bindX", 6, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "X"))).when("placement", {4}).maniacOnly().extension(), 4, 3, targetModes()),
		modal(variable("bindY", 7, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Y"))).when("placement", {4}).maniacOnly().extension(), 4, 4, targetModes()),
		section(QT_TRANSLATE_NOOP("ShowBattleAnimationWidget", "Options")),
		check("wait", 2, QT_TRANSLATE_NOOP("ShowBattleAnimationWidget", "Wait for completion")).bit(0),
		check("global", 3, QT_TRANSLATE_NOOP("ShowBattleAnimationWidget", "Global Scope")).notManiac(),
		check("cache", 2, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Cache Source"))).bit(1).maniacOnly(),
		modalNumber(check("flip", 8, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Flip"))).maniacOnly().extension(), 4, 5),
		modalNumber(number("buffer", 5, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Buffer")), 0, 9999999).maniacOnly().extension(), 4, 2)
	));

	// Hero Transparency
	table.add(Code::PlayerVisibility, "PlayerVisibilityWidget", QT_TRANSLATE_NOOP("PlayerVisibilityWidget", "Player Visibility"), fields(
		section(QT_TRANSLATE_NOOP("PlayerVisibilityWidget", "Transparency State")),
		choice("state", 0, {}, {
			{0, QT_TRANSLATE_NOOP("PlayerVisibilityWidget", "Transparent")},
			{1, QT_TRANSLATE_NOOP("PlayerVisibilityWidget", "Opaque")}}).radio().withDefault(1)
	));

	// Flash Event: colors of 0-31. Maniac Patch: the modes of the event and
	// of the colors in the nibbles of p7
	table.add(Code::FlashSprite, "FlashSpriteWidget", QT_TRANSLATE_NOOP("FlashSpriteWidget", "Flash Sprite"), fields(
		section(QT_TRANSLATE_NOOP("FlashSpriteWidget", "Target")),
		modalNumber(character("target", 0, {}, pictureCharacters()), 7, 0),
		section(QT_TRANSLATE_NOOP("FlashSpriteWidget", "Color")),
		// as Flash Screen: the sample at the left of the sliders
		sample("flash"),
		modalNumber(number("red", 1, QT_TRANSLATE_NOOP("FlashSpriteWidget", "Red"), 0, 31).slider().withDefault(31), 7, 1),
		modalNumber(number("green", 2, QT_TRANSLATE_NOOP("FlashSpriteWidget", "Green"), 0, 31).slider().withDefault(31), 7, 2),
		modalNumber(number("blue", 3, QT_TRANSLATE_NOOP("FlashSpriteWidget", "Blue"), 0, 31).slider().withDefault(31), 7, 3),
		modalNumber(number("strength", 4, QT_TRANSLATE_NOOP("FlashSpriteWidget", "Strength"), 0, 31).slider().withDefault(31), 7, 4),
		section(QT_TRANSLATE_NOOP("FlashSpriteWidget", "Flash Cycle Time")),
		number("time", 5, {}, 0, 10000).withDefault(5).withSuffix(QT_TRANSLATE_NOOP("FlashSpriteWidget", " × 0.1 s")),
		section(QT_TRANSLATE_NOOP("FlashSpriteWidget", "Option")),
		check("wait", 6, QT_TRANSLATE_NOOP("FlashSpriteWidget", "Wait for completion"))
	));

	// Move Event: the moves after the four parameters (MoveRouteCodec)
	table.custom(Code::MoveEvent, "MoveRouteDialog", QT_TRANSLATE_NOOP("MoveRouteDialog", "Move Event"), fields(
		character("target", 0, QT_TRANSLATE_NOOP("MoveRouteDialog", "Target"), {
			{10001, QT_TRANSLATE_NOOP("MoveRouteDialog", "Hero")},
			{10002, QT_TRANSLATE_NOOP("MoveRouteDialog", "Boat")},
			{10003, QT_TRANSLATE_NOOP("MoveRouteDialog", "Ship")},
			{10004, QT_TRANSLATE_NOOP("MoveRouteDialog", "Airship")},
			{10005, QT_TRANSLATE_NOOP("MoveRouteDialog", "This Event")}}).withDefault(10005),
		choice("frequency", 1, QT_TRANSLATE_NOOP("MoveRouteDialog", "Frequency"), {
			{1, literal("1")}, {2, literal("2")}, {3, literal("3")}, {4, literal("4")},
			{5, literal("5")}, {6, literal("6")}, {7, literal("7")}, {8, literal("8")}}).withDefault(6),
		field(Kind::MoveRoute, "route", 4, {}),
		// the other bits of parameter 2 are Maniac Patch flags, kept: bits 8-11
		// the target's mode (a variable, a variable pointer), set by the editor
		check("repeat", 2, QT_TRANSLATE_NOOP("MoveRouteDialog", "Repeat the moves")).bit(0),
		mode("targetMode", QT_TRANSLATE_NOOP("MoveRouteDialog", "Target"), 2, 2, numberModes()).hidden().maniac(),
		check("skippable", 3, QT_TRANSLATE_NOOP("MoveRouteDialog", "Skip moves that cannot be done"))
	));
}

/** Fields among list enabled only when the field id has one of values */
Fields enabledOnlyWhen(Fields list, std::initializer_list<std::string_view> ids, const char* id, const std::vector<int>& values) {
	for (auto& one : list) {
		if (std::find(ids.begin(), ids.end(), one.id) != ids.end()) {
			one.enabled.push_back({id, values, false});
		}
	}
	return list;
}

// Enable Teleport, Escape, Saving and System Menu
void access(Table& table, Code code, const char* title) {
	table.add(code, "EnableDisableWidget", palette(title), fields(
		choice("access", 0, {}, {
			{0, QT_TRANSLATE_NOOP("EnableDisableWidget", "Forbid")},
			{1, QT_TRANSLATE_NOOP("EnableDisableWidget", "Allow")}}).radio()
	));
}

// Wait, music and sounds, keys, and the settings of the map
void addMediaAndMap(Table& table) {
	// Wait: a time, or (RPG Maker 2003) until a key is pressed. Maniac
	// Patch: frames (bit 8 of p1: 256), the time's mode in p2
	table.add(Code::Wait, "WaitWidget", QT_TRANSLATE_NOOP("WaitWidget", "Wait"), fields(
		choice("mode", 1, {}, {
			{0, QT_TRANSLATE_NOOP("WaitWidget", "Wait for")},
			{1, QT_TRANSLATE_NOOP("WaitWidget", "Wait until a key is pressed"), Flag::Rpg2003}}).bitsAt(0, 8).radio(),
		// one of them on the row, in the unit chosen
		modalNumber(number("time", 0, {}, 0, 9999).withSuffix(QT_TRANSLATE_NOOP("WaitWidget", " × 0.1 s"))
			.unless("inFrames", {1}).enabledWhen("mode", {0}).inlined(), 2, -1),
		modalNumber(number("frames", 0, {}, 0, 9999).withSuffix(in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", " frames")))
			.when("inFrames", {1}).enabledWhen("mode", {0}).inlined().maniacOnly(), 2, -1),
		check("inFrames", 1, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "In frames (1/60 s)")))
			.bit(8).enabledWhen("mode", {0}).maniacOnly()
	));

	// Play BGM
	table.add(Code::PlayBGM, "MediaAudioWidget", in("PlayBGMWidget", QT_TRANSLATE_NOOP("PlayBGMWidget", "Play BGM")), fields(
		section(QT_TRANSLATE_NOOP("MediaAudioWidget", "BGM")),
		// Maniac Patch: modes in p4, the file's string in p5
		withModes(fields(audioFile(MUSIC), fadeIn(0), audio(1)), 4, 5)
	));

	// Fade Out BGM
	table.add(Code::FadeOutBGM, "FadeOutBGMWidget", QT_TRANSLATE_NOOP("FadeOutBGMWidget", "Fade Out BGM"), fields(
		section(QT_TRANSLATE_NOOP("FadeOutBGMWidget", "Fade Out Time")),
		// RPG Maker's slider of 0 to 20 seconds, with buttons to hear it.
		// Maniac Patch: the time in a variable (its mode in p1)
		modalNumber(number("time", 0, {}, 0, 20000).ticks()
			.withSuffix(QT_TRANSLATE_NOOP("FadeOutBGMWidget", " ms")).withStep(1000), 1, -1),
		sample("fade").inlined()
	));

	// Play Sound Effect
	table.add(Code::PlaySound, "MediaAudioWidget", in("PlaySoundWidget", QT_TRANSLATE_NOOP("PlaySoundWidget", "Play Sound Effect")), fields(
		section(QT_TRANSLATE_NOOP("MediaAudioWidget", "Sound Effect")),
		// Maniac Patch: modes in p3, the file's string in p4
		withModes(fields(audioFile(SOUND), audio(0)), 3, 4)
	));

	// Play Movie: the position, or the variables holding it
	table.add(Code::PlayMovie, "PlayMovieWidget", QT_TRANSLATE_NOOP("PlayMovieWidget", "Play Movie"), fields(
		section(QT_TRANSLATE_NOOP("PlayMovieWidget", "Movie File")),
		file("movie", {}, MOVIE),
		section(QT_TRANSLATE_NOOP("PlayMovieWidget", "Resolution")),
		number("width", 3, QT_TRANSLATE_NOOP("PlayMovieWidget", "Width"), 1, 9999).withDefault(320),
		number("height", 4, QT_TRANSLATE_NOOP("PlayMovieWidget", "Height"), 1, 9999).withDefault(240),
		section(QT_TRANSLATE_NOOP("PlayMovieWidget", "Coordinates of Upper-Left Pixel")),
		choice("position", 0, {}, {
			{0, QT_TRANSLATE_NOOP("PlayMovieWidget", "Specific Coordinates")},
			{1, QT_TRANSLATE_NOOP("PlayMovieWidget", "Variable Reference")},
			{2, in("PictureWidget", QT_TRANSLATE_NOOP("PictureWidget", "Variable Pointer")), Flag::Maniac}}).radio(),
		number("x", 1, QT_TRANSLATE_NOOP("PlayMovieWidget", "X"), -9999, 9999).when("position", {0}),
		number("y", 2, QT_TRANSLATE_NOOP("PlayMovieWidget", "Y"), -9999, 9999).when("position", {0}),
		variable("xVariable", 1, QT_TRANSLATE_NOOP("PlayMovieWidget", "X")).when("position", {1, 2}),
		variable("yVariable", 2, QT_TRANSLATE_NOOP("PlayMovieWidget", "Y")).when("position", {1, 2})
	));

	// Key Input Processing: the place of each key depends on the engine and
	// on the number of parameters (RPG Maker 2000, 2003 before and after
	// 1.08). The fields are the ten parameters of a new command, read by
	// both engines; the 2003 layouts also have the number and operator keys
	// and the time taken, and pack the decision and cancel keys in bit 0
	table.custom(Code::KeyInputProc, "KeyInputProcWidget", QT_TRANSLATE_NOOP("KeyInputProcWidget", "Key Input Processing"), fields(
		section(QT_TRANSLATE_NOOP("KeyInputProcWidget", "Store Key Code in")),
		variable("variable", 0, QT_TRANSLATE_NOOP("KeyInputProcWidget", "Variable")),
		section(QT_TRANSLATE_NOOP("KeyInputProcWidget", "Option")),
		check("wait", 1, QT_TRANSLATE_NOOP("KeyInputProcWidget", "Wait Until Key Pressed")),
		section(QT_TRANSLATE_NOOP("KeyInputProcWidget", "Keys to Process")),
		check("down", 6, QT_TRANSLATE_NOOP("KeyInputProcWidget", "Down Key (1)")),
		check("left", 7, QT_TRANSLATE_NOOP("KeyInputProcWidget", "Left Key (2)")),
		check("right", 8, QT_TRANSLATE_NOOP("KeyInputProcWidget", "Right Key (3)")),
		check("up", 9, QT_TRANSLATE_NOOP("KeyInputProcWidget", "Up Key (4)")),
		check("decision", 3, QT_TRANSLATE_NOOP("KeyInputProcWidget", "Decision Key (5)")).bit(0),
		check("cancel", 4, QT_TRANSLATE_NOOP("KeyInputProcWidget", "Cancel Key (6)")).bit(0),
		check("shift", 5, QT_TRANSLATE_NOOP("KeyInputProcWidget", "Shift Key (7)")),
		// Maniac Patch: the mouse, in bit 1 of the decision, cancel, Shift,
		// down and up keys of RPG Maker 2003's newer layout
		section(in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Mouse"))),
		check("mouseLeft", 3, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Left Button (1005)"))).bit(1).maniacOnly(),
		check("mouseRight", 4, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Right Button (1006)"))).bit(1).maniacOnly(),
		check("mouseMiddle", 9, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Middle Button (1007)"))).bit(1).maniacOnly(),
		check("wheelDown", 10, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Wheel Down (1001)"))).bit(1).maniacOnly(),
		check("wheelUp", 13, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Wheel Up (1004)"))).bit(1).maniacOnly()
	));

	// Change Chipset: the Maniac Patch's mode of the chipset in p1 (bit 0 alone:
	// a variable, also in other engines)
	table.add(Code::ChangeMapTileset, "LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Change Map Tileset"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "New Tileset")),
		modalNumber(ref(Kind::Chipset, "chipset", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Tileset")), 1, 0)
	));

	// Change Panorama: the Maniac Patch's modes of the name (nibble 0 of
	// p6, its string in p7) and of the speeds (nibbles 4 and 6)
	table.add(Code::ChangePBG, "LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Change Parallax Background"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Graphic")),
		modalText(file("graphic", {}, PANORAMA).preview(), 6, 0, 7),
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Options")),
		check("horizontal", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Horizontal Scrolling")),
		check("horizontalAuto", 2, QT_TRANSLATE_NOOP("LocationCommandWidget", "Horizontal Autoscroll")).enabledUnless("horizontal", {0}),
		modalNumber(number("horizontalSpeed", 3, QT_TRANSLATE_NOOP("LocationCommandWidget", "Horizontal Speed"), -8, 8)
			.enabledUnless("horizontal", {0}).enabledUnless("horizontalAuto", {0}), 6, 4),
		check("vertical", 1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Vertical Scrolling")),
		check("verticalAuto", 4, QT_TRANSLATE_NOOP("LocationCommandWidget", "Vertical Autoscroll")).enabledUnless("vertical", {0}),
		modalNumber(number("verticalSpeed", 5, QT_TRANSLATE_NOOP("LocationCommandWidget", "Vertical Speed"), -8, 8)
			.enabledUnless("vertical", {0}).enabledUnless("verticalAuto", {0}), 6, 6)
	));

	// Change Encounter Rate
	table.add(Code::ChangeEncounterSteps, "LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Change Encounter Rate"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "New Rate")),
		number("steps", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Steps Between Encounters"), 0, 9999)
	));

	// Change Single Tile
	table.add(Code::TileSubstitution, "LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Tile Substitution"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Tile Layer")),
		choice("layer", 0, {}, {
			{0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Lower Layer")},
			{1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Upper Layer")}}).radio(),
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Original Tile")),
		field(Kind::Tile, "original", 1, {}).withRange(0, 143).withLink("layer").preview(),
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Substituted Tile")),
		field(Kind::Tile, "substitute", 2, {}).withRange(0, 143).withLink("layer").preview()
	));

	// Set Teleport Location
	table.add(Code::TeleportTargets, "LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Teleport Target Management"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Operation")),
		choice("operation", 0, {}, {
			{0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Add Teleport Target")},
			{1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Delete Teleport Target")}}).radio(),
		// deleting a target needs only its map
		enabledOnlyWhen(location(-1, 1, 2, 3), {"x", "y"}, "operation", {0}),
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Turn Switch ON After Teleporting")),
		check("switchOn", 4, QT_TRANSLATE_NOOP("LocationCommandWidget", "Turn on")).enabledWhen("operation", {0}),
		ref(Kind::Switch, "switch", 5, QT_TRANSLATE_NOOP("LocationCommandWidget", "Switch"))
			.enabledWhen("operation", {0}).enabledUnless("switchOn", {0})
	));
	// Enable Teleport
	access(table, Code::ChangeTeleportAccess, QT_TRANSLATE_NOOP("EventCommandPalette", "Enable Teleport"));

	// Set Escape Location
	table.add(Code::EscapeTarget, "LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Set Escape Target"), fields(
		location(-1, 0, 1, 2),
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Turn Switch ON After Escaping")),
		check("switchOn", 3, QT_TRANSLATE_NOOP("LocationCommandWidget", "Turn on")),
		ref(Kind::Switch, "switch", 4, QT_TRANSLATE_NOOP("LocationCommandWidget", "Switch")).enabledUnless("switchOn", {0})
	));

	// Enable Escape
	access(table, Code::ChangeEscapeAccess, QT_TRANSLATE_NOOP("EventCommandPalette", "Enable Escape"));

	// Enable Saving
	// Enable Saving. Maniac Patch: Control System Functions (-1), the
	// functions the player cannot open in the bits of p1
	const auto allowed = [](const char* id, int bit, const char* text) {
		return check(id, 1, in("ManiacCommandWidget", text)).bit(bit).with(Flag::Inverted).when("access", {-1}).maniacOnly();
	};
	table.add(Code::ChangeSaveAccess, "EnableDisableWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Enable Saving")), fields(
		choice("access", 0, {}, {
			{0, QT_TRANSLATE_NOOP("EnableDisableWidget", "Forbid")},
			{1, QT_TRANSLATE_NOOP("EnableDisableWidget", "Allow")},
			{-1, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Control System Functions")), Flag::Maniac}}).radio(),
		section(in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Functions Allowed"))),
		allowed("allowSave", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save menu")),
		allowed("allowMenu", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Party menu")),
		allowed("allowScreenMode", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Window and full screen (F4)")),
		allowed("allowOptions", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Media options (F5)")),
		allowed("allowDebug", 5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Debug menu (F9)")),
		allowed("allowReset", 7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Reset (F12)"))
	));

	// Enable System Menu
	access(table, Code::ChangeMainMenuAccess, QT_TRANSLATE_NOOP("EventCommandPalette", "Enable System Menu"));
}

// The conditions of Conditional Branch, map and battle:
// the parameters 1 to 4 of each type, as EasyRPG Player reads them
Fields conditionFields(bool battle) {
	Items types = {
		{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Switch")},
		{1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Variable")}};
	if (battle) {
		types = types + Items{
			{2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Hero is able to act")},
			{3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Monster is able to act")},
			{4, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Monster is the current target"), Flag::Rpg2003},
			{5, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Hero uses the battle command"), Flag::Rpg2003}};
	} else {
		types = types + Items{
			{2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Timer 1")},
			{3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Money")},
			{4, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Item")},
			{5, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Hero")},
			{6, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Sprite direction")},
			{7, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Party is Riding")},
			{8, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Event was Started Using Action Key")},
			{9, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "BGM has Played Through Once")},
			{10, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Timer 2"), Flag::Rpg2003},
			{11, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Other (RPG Maker 2003)"), Flag::Rpg2003},
			{12, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Other (Maniac Patch)"), Flag::Maniac},
			{13, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Switch of Variable"), Flag::Maniac},
			{14, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Variable of Variable"), Flag::Maniac},
			{15, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "String"), Flag::Maniac},
			{16, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Expression"), Flag::Maniac}};
	}
	const Items onOff = {{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "ON")}, {1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "OFF")}};
	const Items comparisons = {
		{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Equal to")},
		{1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Greater than or Equal to")},
		{2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Less than or Equal to")},
		{3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Greater than")},
		{4, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Less than")},
		{5, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Not Equal to")}};

	Fields list = fields(
		section(QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Condition")),
		choice("type", 0, {}, types),
		// Switch
		ref(Kind::Switch, "switch", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Switch")).when("type", {0}),
		choice("switchState", 2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "is"), onOff).when("type", {0}),
		// Variable: with Maniac Patch, a variable pointer too
		variable("variable", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Variable")).when("type", {1}),
		choice("comparison", 4, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Comparison"), comparisons).when("type", {1}),
		choice("operandType", 2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Compare with"), {
			{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Number")},
			{1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Variable Reference")},
			{2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Variable Pointer"), Flag::Maniac}}).when("type", {1}),
		number("operandNumber", 3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Number"), -9999999, 9999999)
			.when("type", {1}).when("operandType", {0}),
		variable("operandVariable", 3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Variable Reference"))
			.when("type", {1}).unless("operandType", {0}));
	if (battle) {
		append(list, fields(
			ref(Kind::Actor, "hero", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Hero is able to act")).when("type", {2}),
			// the monsters of the troop, from 0
			choice("monster", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Monster"),
				numbered(QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Monster %1"), 8, 0)).when("type", {3}),
			choice("target", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Monster is the current target"),
				numbered(QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Monster %1"), 8, 0)).when("type", {4}).rpg2003(),
			ref(Kind::Actor, "commandHero", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Hero")).when("type", {5}).rpg2003(),
			ref(Kind::BattleCommand, "battleCommand", 2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Battle command"))
				.when("type", {5}).rpg2003()));
	} else {
		append(list, fields(
			// Timer 1 and 2: seconds, at least or at most
			time("timer", 1).when("type", {2, 10}),
			choice("timerComparison", 2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Comparison"), {
				{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "or more")},
				{1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "or less")}}).when("type", {2, 10}),
			number("money", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Money"), 0, 9999999).when("type", {3}),
			choice("moneyComparison", 2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Comparison"), {
				{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "at least")},
				{1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "at most")}}).when("type", {3}),
			// Item, Hero, Sprite: with Maniac Patch, the ID can be in a variable
			// (its mode in parameter 3 for an item or a sprite, 4 for a hero)
			mode("itemMode", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Given as"), 3, -1, numberModes()).when("type", {4}).maniac(),
			ref(Kind::Item, "item", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Item")).when("type", {4}).when("itemMode", {0}),
			variable("itemVariable", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Variable")).when("type", {4})
				.unless("itemMode", {0}).maniac(),
			choice("possession", 2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Condition"), {
				{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Party Possesses")},
				{1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Party Does Not Possess")}}).when("type", {4}),
			mode("heroMode", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Given as"), 4, -1, numberModes()).when("type", {5}).maniac(),
			ref(Kind::Actor, "hero", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Hero")).when("type", {5}).when("heroMode", {0}),
			variable("heroVariable", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Variable")).when("type", {5})
				.unless("heroMode", {0}).maniac(),
			choice("heroTest", 2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Condition"), {
				{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "In the Party")},
				{1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Name")},
				{2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Level at least")},
				{3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "HP at least")},
				{4, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Skill learned")},
				{5, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Item equipped")},
				{6, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Condition")}}).when("type", {5}),
			string("heroName", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Name")).when("type", {5}).when("heroTest", {1}),
			number("heroLevel", 3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Level at least"), 1, 99).withDefault(1)
				.when("type", {5}).when("heroTest", {2}),
			number("heroHp", 3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "HP at least"), 0, 9999999).when("type", {5}).when("heroTest", {3}),
			ref(Kind::Skill, "heroSkill", 3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Skill learned")).when("type", {5}).when("heroTest", {4}),
			ref(Kind::Item, "heroItem", 3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Item equipped")).when("type", {5}).when("heroTest", {5}),
			ref(Kind::State, "heroCondition", 3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Condition")).when("type", {5}).when("heroTest", {6}),
			mode("characterMode", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Given as"), 3, -1, numberModes()).when("type", {6}).maniac(),
			character("character", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Sprite"), {
				{10001, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Hero")},
				{10002, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Boat")},
				{10003, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Ship")},
				{10004, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Airship")},
				{10005, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "This Event")}}).when("type", {6}).when("characterMode", {0}),
			variable("characterVariable", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Variable")).when("type", {6})
				.unless("characterMode", {0}).maniac(),
			choice("direction", 2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "is Facing"), {
				{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Up")},
				{1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Right")},
				{2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Down")},
				{3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Left")}}).when("type", {6}).enabledUnless("characterExists", {1}),
			// p4 = 1: only whether the event exists
			check("characterExists", 4, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Only check that the event exists"))
				.when("type", {6}).maniac(),
			choice("vehicle", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Party is Riding"), {
				{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Boat")},
				{1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Ship")},
				{2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Airship")}}).when("type", {7}),
			choice("other", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Condition"), {
				{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "A save file exists")},
				{1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Test Play mode is active")},
				{2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "ATB Wait mode is active")},
				{3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Fullscreen is active")}}).when("type", {11}).rpg2003(),
			// Maniac Patch
			choice("maniacOther", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Condition"), {
				{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "A save was loaded this frame")},
				{1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "A joypad is used")},
				{2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "The window has the focus")},
				{3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Files can be written")}}).when("type", {12}).maniac(),
			variable("switchPointer", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Switch of variable")).when("type", {13}).maniac(),
			choice("switchPointerState", 2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "is"), onOff).when("type", {13}).maniac(),
			variable("variablePointer", 1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Variable of variable")).when("type", {14}).maniac(),
			choice("pointerComparison", 4, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Comparison"), comparisons).when("type", {14}).maniac(),
			only(onlyWhen(fields(
				mode("pointerOperandMode", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Compare with"), 2, -1, numberModes()),
				modeValue("pointerOperand", "pointerOperandMode", 3, -99999999, 99999999, numberModes())), "type", {14}), Flag::Maniac),
			// strings: the left one T[x] or T[V[x]], the right one also a text
			only(onlyWhen(fields(
				mode("leftMode", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "String"), 1, 0, {
					{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "String"), Kind::StringId},
					{1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "String pointer"), Kind::Variable}}),
				modeValue("left", "leftMode", 2, 0, 99999, {{0, {}, Kind::StringId}, {1, {}, Kind::Variable}}),
				choice("stringComparison", 4, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Comparison"), {
					{0, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Equal to")},
					{1, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Not Equal to")},
					{2, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Contains")},
					{3, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Does not contain")}}).bitsAt(0, 2),
				mode("rightMode", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Compare with"), 1, 1, stringModes()),
				modeValue("right", "rightMode", 3, 0, 99999, stringModes()),
				check("ignoreCase", 4, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Ignore case")).bit(8)), "type", {15}), Flag::Maniac),
			// the words of the expression: all the parameters after the "else" flag
			field(Kind::Expression, "expression", 6, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Expression"))
				.when("type", {16}).maniac().extension()));
	}
	append(list, fields(
		section(QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Option")),
		// the else branch is added or removed on OK
		check("else", 5, QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Execute Custom Handler if Condition Not Met"))));
	return list;
}

// Flow: branches, labels, calls and comments
void addFlow(Table& table) {
	// Conditional Branch: a block with its Else and End lines
	table.custom(Code::ConditionalBranch, "ConditionalBranchWidget", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Conditional Branch"),
		conditionFields(false));

	// Label
	table.add(Code::Label, "LabelWidget", QT_TRANSLATE_NOOP("LabelWidget", "Label"), fields(
		number("label", 0, QT_TRANSLATE_NOOP("LabelWidget", "Label"), 1, 9999).withDefault(1)
	));

	// Go to Label
	table.add(Code::JumpToLabel, "LabelWidget", QT_TRANSLATE_NOOP("LabelWidget", "Jump to Label"), fields(
		number("label", 0, QT_TRANSLATE_NOOP("LabelWidget", "Label"), 1, 9999).withDefault(1)
	));

	// Call Event: a common event, an event of the map and its page, or both
	// from variables; Maniac Patch: a common event from a variable or a
	// pointer, or by its name (the string of the command, a string variable
	// or one from a variable), and an offset added to the ID (its mode in
	// p3, the value in p4)
	table.add(Code::CallEvent, "CallEventWidget", QT_TRANSLATE_NOOP("CallEventWidget", "Call Event"), fields(
		choice("kind", 0, {}, {
			{0, QT_TRANSLATE_NOOP("CallEventWidget", "Common Event")},
			{3, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common Event in a Variable")), Flag::Maniac},
			{4, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common Event in a Variable Pointer")), Flag::Maniac},
			{5, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common Event by Name")), Flag::Maniac},
			{6, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common Event Named in a String")), Flag::Maniac},
			{7, in("ManiacCommandWidget", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common Event Named in a String Pointer")), Flag::Maniac},
			{1, QT_TRANSLATE_NOOP("CallEventWidget", "Map Event")},
			{2, QT_TRANSLATE_NOOP("CallEventWidget", "Map Event chosen by variables")}}).radio().maniacColumns(2),
		ref(Kind::CommonEvent, "commonEvent", 1, QT_TRANSLATE_NOOP("CallEventWidget", "Common Event")).when("kind", {0}),
		variable("commonEventVariable", 1, QT_TRANSLATE_NOOP("CallEventWidget", "Common Event")).when("kind", {3, 4}).maniacOnly(),
		string("commonEventName", maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Name"))).when("kind", {5}).maniacOnly(),
		field(Kind::StringId, "commonEventString", 1, QT_TRANSLATE_NOOP("CallEventWidget", "Common Event"))
			.withRange(0, 99999).withDefault(1).when("kind", {6}).maniacOnly(),
		variable("commonEventStringVariable", 1, QT_TRANSLATE_NOOP("CallEventWidget", "Common Event")).when("kind", {7}).maniacOnly(),
		character("event", 1, QT_TRANSLATE_NOOP("CallEventWidget", "Event"), {
			{10005, QT_TRANSLATE_NOOP("CallEventWidget", "This Event")}}).when("kind", {1}),
		number("page", 2, QT_TRANSLATE_NOOP("CallEventWidget", "Page"), 1, 100).withDefault(1).when("kind", {1}),
		variable("eventVariable", 1, QT_TRANSLATE_NOOP("CallEventWidget", "Event ID in")).when("kind", {2}),
		variable("pageVariable", 2, QT_TRANSLATE_NOOP("CallEventWidget", "Page in")).when("kind", {2}),
		modalNumber(number("offset", 4, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Added to the ID")), int32Min, int32Max)
			.maniacOnly().extension(), 3, -1)
	));

	// Comment: its text over several lines
	table.custom(Code::Comment, "CommentWidget", QT_TRANSLATE_NOOP("CommentWidget", "Comment"), fields(
		field(Kind::Lines, "comment", StringIndex, {})
	));

	// The Maniac Patch's settings of commands without any in RPG Maker (their
	// editors only in its games)
	const char* context = "ManiacCommandWidget";
	const Items comparisons = {
		{0, in("ConditionalBranchWidget", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Equal to"))},
		{1, in("ConditionalBranchWidget", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Greater than or Equal to"))},
		{2, in("ConditionalBranchWidget", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Less than or Equal to"))},
		{3, in("ConditionalBranchWidget", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Greater than"))},
		{4, in("ConditionalBranchWidget", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Less than"))},
		{5, in("ConditionalBranchWidget", QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Not Equal to"))}};
	// a switch reads 1 when on
	const Modes values = numberModes() + Modes{
		{3, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Switch")), Kind::Switch},
		{4, maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Switch pointer")), Kind::Variable}};

	// Loop: forever (RPG Maker), a number of times, counting up or down, while
	// a comparison holds (checked before or after each loop), for each value
	// of a list, of an array of variables or for each event of the map in p0;
	// the modes of the two values (p2, p3) in nibbles 0 and 1 of p1, the
	// comparison in nibble 2; the variable of the index (or of the value) in
	// p4 (0: none). A list (6) has its values from p5, their modes in words
	// from p2: kept as they are. Its End Loop has the same parameters
	// (EasyRPG Player reads them there)
	table.add(Code::Loop, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Loop")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Repeat")),
		choice("type", 0, {}, {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Forever")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "A number of times")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Counting up")},
			{3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Counting down")},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "While (checked before each loop)")},
			{5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "While (checked after each loop)")},
			{6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "For each value of a list"), Flag::Kept},
			{7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "For each variable of an array")},
			{8, QT_TRANSLATE_NOOP("ManiacCommandWidget", "For each event of the map")}}).inColumns(2),
		modal(number("times", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Times"), 0, 9999999).withDefault(1).when("type", {1}), 1, 0, values),
		modal(number("from", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "From"), -9999999, 9999999).when("type", {2, 3}), 1, 0, values),
		modal(number("to", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "To"), -9999999, 9999999).when("type", {2, 3}), 1, 1, values),
		modal(number("left", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value"), -9999999, 9999999).when("type", {4, 5}), 1, 0, values),
		choice("comparison", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Comparison"), comparisons).bitsAt(8, 4).when("type", {4, 5}),
		modal(number("right", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Compared with"), -9999999, 9999999).when("type", {4, 5}), 1, 1, values),
		ref(Kind::Variable, "array", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "From")).when("type", {7}),
		modal(number("count", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Count"), 0, 9999999).withDefault(1).when("type", {7}), 1, 1, values),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Index")),
		// from 0, or the value counted
		ref(Kind::Variable, "index", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Stored in"))
			.withItems({{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "(None)")}}).withDefault(0).unless("type", {0})
	), Flag::Maniac);

	// Erase Event: another event (p2: 0 or 10005 this one; its mode in
	// nibble 1 of p0), erased or brought back (p1)
	table.add(Code::EraseEvent, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Erase Event")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Event")),
		modalNumber(character("event", 2, {}, {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "This Event")},
			{10005, QT_TRANSLATE_NOOP("ManiacCommandWidget", "This Event"), Flag::Kept}}), 0, 1),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation")),
		choice("operation", 1, {}, {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Erase")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Bring back")}}).radio()
	), Flag::Maniac);

	// Call System Functions (Open Save Menu): the function (p0) and its option (p1)
	table.add(Code::OpenSaveMenu, context, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Call System Functions"), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Function")),
		choice("function", 0, {}, {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save menu")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Load menu")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Party menu")},
			{3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Window or full screen")},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Media options")},
			{5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Debug menu")},
			{6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "License")},
			{7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Reset (F12)")}}).inColumns(2),
		choice("screenMode", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Screen"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Toggle")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Window")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Full screen")}}).radio().when("function", {3}),
		check("pause", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Pause the game")).when("function", {5})
	), Flag::Maniac);

	// Break Loop: out of the loop or to its next turn (p0), of the loop it is
	// in or of one around it (p1: the loops left before it)
	table.add(Code::BreakLoop, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Break Loop")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation")),
		choice("operation", 0, {}, {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Break")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Continue")}}).radio(),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Level")),
		number("outer", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Outer loops"), 0, 9999999),
		note(QT_TRANSLATE_NOOP("ManiacCommandWidget", "0: the loop it is in; 1: the loop around it..."))
	), Flag::Maniac);

	// End Event Processing: the self values of the event reset with it (p0)
	table.add(Code::EndEventProcessing, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "End Event Processing")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Reset")),
		check("selfVariables", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Self variables")).bit(0),
		check("selfStrings", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Self string variables")).bit(1),
		check("selfSwitches", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Self switches")).bit(2)
	), Flag::Maniac);
}

// Battle event commands
void addBattle(Table& table) {
	// Change Enemy HP: the monster of the troop, from 0 (Maniac Patch: its
	// mode in p5)
	table.add(Code::ChangeMonsterHP, "SceneCommandWidget", in("ChangeMonsterHPWidget", QT_TRANSLATE_NOOP("ChangeMonsterHPWidget", "Change Monster HP")), fields(
		section(QT_TRANSLATE_NOOP("SceneCommandWidget", "Monster")),
		modalNumber(monster("monster", 0), 5, -1),
		section(QT_TRANSLATE_NOOP("SceneCommandWidget", "Operation")),
		choice("operation", 1, {}, {
			{0, QT_TRANSLATE_NOOP("SceneCommandWidget", "Increase HP")},
			{1, QT_TRANSLATE_NOOP("SceneCommandWidget", "Decrease HP")}}).radio(),
		section(QT_TRANSLATE_NOOP("SceneCommandWidget", "Amount")),
		choice("amountKind", 2, {}, {
			{0, QT_TRANSLATE_NOOP("SceneCommandWidget", "Specific Amount")},
			{1, QT_TRANSLATE_NOOP("SceneCommandWidget", "Amount Stored in")},
			{2, QT_TRANSLATE_NOOP("SceneCommandWidget", "Amount Equal to")}}).radio(),
		number("amount", 3, {}, 0, 9999).when("amountKind", {0}).inlined(),
		variable("amountVariable", 3).when("amountKind", {1}).inlined(),
		number("percent", 3, {}, 0, 100).withSuffix(QT_TRANSLATE_NOOP("SceneCommandWidget", " % of Current HP"))
			.when("amountKind", {2}).inlined(),
		section(QT_TRANSLATE_NOOP("SceneCommandWidget", "Option")),
		// only a decrease can kill
		check("lethal", 4, QT_TRANSLATE_NOOP("SceneCommandWidget", "HP Reduction can Kill Target")).enabledUnless("operation", {0})
	));

	// Change Enemy MP (Maniac Patch: the monster's mode in p5)
	table.add(Code::ChangeMonsterMP, "SceneCommandWidget", in("ChangeMonsterMPWidget", QT_TRANSLATE_NOOP("ChangeMonsterMPWidget", "Change Monster MP")), fields(
		section(QT_TRANSLATE_NOOP("SceneCommandWidget", "Monster")),
		modalNumber(monster("monster", 0), 5, -1),
		section(QT_TRANSLATE_NOOP("SceneCommandWidget", "Operation")),
		choice("operation", 1, {}, {
			{0, QT_TRANSLATE_NOOP("SceneCommandWidget", "Increase MP")},
			{1, QT_TRANSLATE_NOOP("SceneCommandWidget", "Decrease MP")}}).radio(),
		section(QT_TRANSLATE_NOOP("SceneCommandWidget", "Amount")),
		choice("amountKind", 2, {}, {
			{0, QT_TRANSLATE_NOOP("SceneCommandWidget", "Specific Amount")},
			{1, QT_TRANSLATE_NOOP("SceneCommandWidget", "Amount Stored in")}}).radio(),
		number("amount", 3, {}, 0, 9999).when("amountKind", {0}).inlined(),
		variable("amountVariable", 3).when("amountKind", {1}).inlined()
	));

	// Change Enemy Condition (Maniac Patch: the modes of the monster and of
	// the condition in p3)
	table.add(Code::ChangeMonsterCondition, "ChangeMonsterConditionWidget",
		QT_TRANSLATE_NOOP("ChangeMonsterConditionWidget", "Change Monster Condition"), fields(
		section(QT_TRANSLATE_NOOP("ChangeMonsterConditionWidget", "Monster")),
		modalNumber(monster("monster", 0), 3, 0),
		section(QT_TRANSLATE_NOOP("ChangeMonsterConditionWidget", "Operation")),
		choice("operation", 1, {}, {
			{0, QT_TRANSLATE_NOOP("ChangeMonsterConditionWidget", "Add Condition")},
			{1, QT_TRANSLATE_NOOP("ChangeMonsterConditionWidget", "Remove Condition")}}).radio(),
		section(QT_TRANSLATE_NOOP("ChangeMonsterConditionWidget", "Condition")),
		modalNumber(ref(Kind::State, "condition", 2), 3, 1)
	));

	// Show Hidden Enemy (Maniac Patch: the monster's mode in p1)
	table.add(Code::ShowHiddenMonster, "ShowHiddenMonsterWidget", QT_TRANSLATE_NOOP("ShowHiddenMonsterWidget", "Show Hidden Monster"), fields(
		section(QT_TRANSLATE_NOOP("ShowHiddenMonsterWidget", "Monster to Show")),
		modalNumber(monster("monster", 0), 1, -1)
	));

	// Change Battle Background
	table.add(Code::ChangeBattleBG, "ChangeBattleBGWidget", QT_TRANSLATE_NOOP("ChangeBattleBGWidget", "Change Battle Background"), fields(
		section(QT_TRANSLATE_NOOP("ChangeBattleBGWidget", "Battle Background")),
		file("background", {}, BACKDROP)
	));

	// Show Battle Animation (battle), as RPG Maker 2003 writes it: one list
	// for the target and its side (parameter 3, the party): a monster from
	// 0, or a hero ID; -1 all of them. Parameter 4 is the line of the target
	// in RPG Maker 2003's list (All Enemies -1, the monsters of the troop,
	// Entire Party, then the heroes), which it reads back for the party;
	// RPG_RT reads the side only from 5 parameters on. Maniac Patch: the
	// modes of the animation and of the target in p5 (a target from a
	// variable: its side still in p3)
	table.custom(Code::ShowBattleAnimation_B, "ShowBattleAnimation_BWidget",
		QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "Show Battle Animation"), fields(
		section(QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "Animation")),
		modalNumber(ref(Kind::Animation, "animation", 0).preview(), 5, 0),
		section(QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "Target")),
		mode("targetMode", {}, 5, 1, numberModes()).hidden().maniac().extension(),
		variable("targetVariable", 1).when("targetMode", {1, 2}).hidden().maniac().extension(),
		check("party", 3, {}).hidden().extension().rpg2003(),
		choice("target", 1, {}, Items{{-1, QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "All Enemies")}}
			+ numbered(QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "Monster %1"), 8, 0)).withDefault(-1).unless("party", {1}),
		ref(Kind::Actor, "hero", 1).withItems({{-1, QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "Entire Party")}})
			.withDefault(-1).when("party", {1}).rpg2003(),
		number("line", 4, {}, -1, 9999).hidden().extension().rpg2003(),
		section(QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "Option")),
		check("wait", 2, QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "Halt Processing Until Animation Ends"))
	));

	// Conditional Branch (battle)
	table.custom(Code::ConditionalBranch_B, "ConditionalBranchWidget",
		QT_TRANSLATE_NOOP("ConditionalBranchWidget", "Conditional Branch (battle)"), conditionFields(true));
}

const std::vector<Schema>& table() {
	static const std::vector<Schema> schemas = [] {
		Table table;
		addBattle2003(table);
		addManiac(table);
		addMessagesAndParty(table);
		addSystemAndScenes(table);
		addMapAndScreen(table);
		addMediaAndMap(table);
		addFlow(table);
		addBattle(table);
		auto list = table.take();
		// by code, for find()
		std::sort(list.begin(), list.end(), [](const Schema& a, const Schema& b) {
			return static_cast<int>(a.code) < static_cast<int>(b.code);
		});
		return list;
	}();
	return schemas;
}

uint32_t maskOf(int shift, int bits) {
	return bits >= 32 ? 0xFFFFFFFFu : (((1u << bits) - 1u) << shift);
}

bool holdsOne(const Schema& schema, const Condition& condition, const lcf::rpg::EventCommand& command) {
	const Field* field = schema.field(condition.field);
	if (!field || !field->isNumber()) {
		return false;
	}
	const int value = CommandSchema::read(*field, command);
	const bool found = std::find(condition.values.begin(), condition.values.end(), value) != condition.values.end();
	return found != condition.negated;
}
}

QString Label::translated() const {
	if (isEmpty()) {
		return {};
	}
	QString result = context && *context ? QCoreApplication::translate(context, text) : QString::fromUtf8(text);
	return number >= 0 ? result.arg(number) : result;
}

uint32_t Field::mask() const {
	return maskOf(shift, bits);
}

bool Field::hasValue() const {
	return kind != Kind::Section && kind != Kind::Note && kind != Kind::Sample && kind != Kind::Location;
}

bool Field::isNumber() const {
	return hasValue() && index != StringIndex && !isTail();
}

bool Field::usesString() const {
	switch (kind) {
		case Kind::String:
		case Kind::Lines:
		case Kind::File:
		case Kind::Face:
		case Kind::Charset:
			return true;
		default:
			return false;
	}
}

bool Field::isTail() const {
	return kind == Kind::Expression || kind == Kind::MoveRoute || kind == Kind::ItemList;
}

const Field* Schema::field(std::string_view id) const {
	for (const auto& one : fields) {
		// a location box, a sample, a note have no value, but their rules
		if ((one.hasValue() || one.kind == Kind::Location || one.kind == Kind::Sample || one.kind == Kind::Note) && id == one.id) {
			return &one;
		}
	}
	return nullptr;
}

bool CommandSchema::hasEditor(Code code) {
	switch (code) {
		case Code::OpenSaveMenu:
		case Code::Loop:
		case Code::BreakLoop:
		case Code::EndEventProcessing:
		case Code::EraseEvent: {
			// settings of the Maniac Patch (read only for them: the other
			// lines do not need the game's patches)
			const auto& project = core().project();
			return hasEditor(code, project && GamePatches::usesManiac(*project));
		}
		default:
			return hasEditor(code, false);
	}
}

bool CommandSchema::hasEditor(Code code, bool maniac) {
	// the commands without settings, and the lines of the structure of a block
	switch (code) {
		case Code::OpenSaveMenu:
		case Code::Loop:
		case Code::BreakLoop:
		case Code::EndEventProcessing:
		case Code::EraseEvent:
			// Call System Functions, the loops, another event (or back), the
			// self values reset: the Maniac Patch's
			return maniac;
		case Code::END:
		case Code::EnterExitVehicle:
		case Code::ProceedWithMovement:
		case Code::HaltAllMovement:
		case Code::MemorizeBGM:
		case Code::PlayMemorizedBGM:
		case Code::OpenMainMenu:
		case Code::GameOver:
		case Code::ReturntoTitleScreen:
		case Code::OpenLoadMenu:
		case Code::ExitGame:
		case Code::ToggleAtbMode:
		case Code::ToggleFullscreen:
		case Code::OpenVideoOptions:
		case Code::TerminateBattle:
		case Code::ShowChoiceOption:
		case Code::ShowChoiceEnd:
		case Code::ElseBranch:
		case Code::EndBranch:
		case Code::EndLoop:
		case Code::EndBattle:
		case Code::EndShop:
		case Code::VictoryHandler:
		case Code::EscapeHandler:
		case Code::DefeatHandler:
		case Code::Transaction:
		case Code::NoTransaction:
		case Code::Stay:
		case Code::NoStay:
		case Code::EndInn:
		case Code::ElseBranch_B:
		case Code::EndBranch_B:
		case Code::Maniac_EndLoadProcess:
			return false;
		default:
			return true;
	}
}

int CommandSchema::minimumOf(const Field& field, bool maniac) {
	return maniac ? field.maniacMinimum : field.minimum;
}

int CommandSchema::maximumOf(const Field& field, bool maniac) {
	return maniac ? field.maniacMaximum : field.maximum;
}

const Schema* CommandSchema::find(Code code) {
	const auto& list = table();
	auto it = std::lower_bound(list.begin(), list.end(), code, [](const Schema& schema, Code value) {
		return static_cast<int>(schema.code) < static_cast<int>(value);
	});
	return it != list.end() && it->code == code ? &*it : nullptr;
}

const std::vector<Schema>& CommandSchema::all() {
	return table();
}

int CommandSchema::defaultOf(const Field& field, const lcf::rpg::EventCommand& command) {
	if (field.copies >= 0 && field.copies < static_cast<int>(command.parameters.size())) {
		const uint32_t value = static_cast<uint32_t>(command.parameters[field.copies]);
		return field.bits >= 32 ? static_cast<int>(value) : static_cast<int>(value & (field.mask() >> field.shift));
	}
	return field.defaultValue;
}

int CommandSchema::read(const Field& field, const lcf::rpg::EventCommand& command) {
	if (field.index < 0 || field.index >= static_cast<int>(command.parameters.size())) {
		return defaultOf(field, command);
	}
	const uint32_t value = static_cast<uint32_t>(command.parameters[field.index]);
	return field.bits >= 32 ? static_cast<int>(value) : static_cast<int>((value & field.mask()) >> field.shift);
}

bool CommandSchema::write(const Schema& schema, const Field& field, lcf::rpg::EventCommand& command, int value) {
	if (!field.isNumber() || CommandSchema::read(field, command) == value) {
		return false;
	}
	if (field.index >= static_cast<int>(command.parameters.size())) {
		// the new parameters get what they read missing: the defaults of
		// their fields (the first of the alternatives on the same bits)
		std::vector<int32_t> parameters(command.parameters.begin(), command.parameters.end());
		for (int index = static_cast<int>(parameters.size()); index <= field.index; ++index) {
			uint32_t parameter = 0;
			uint32_t set = 0;
			for (const auto& one : schema.fields) {
				if (one.isNumber() && one.index == index && !(one.mask() & set)) {
					parameter |= (static_cast<uint32_t>(defaultOf(one, command)) << one.shift) & one.mask();
					set |= one.mask();
				}
			}
			parameters.push_back(static_cast<int32_t>(parameter));
		}
		command.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
	}
	const uint32_t mask = field.mask();
	const uint32_t old = static_cast<uint32_t>(command.parameters[field.index]);
	command.parameters[field.index] = static_cast<int32_t>((old & ~mask) | ((static_cast<uint32_t>(value) << field.shift) & mask));
	return true;
}

QString CommandSchema::readString(const lcf::rpg::EventCommand& command) {
	return ToQString(command.string);
}

void CommandSchema::writeString(lcf::rpg::EventCommand& command, const QString& text) {
	command.string = ToDBString(text);
}

void CommandSchema::syncBlock(std::vector<lcf::rpg::EventCommand>& commands, int index) {
	if (index < 0 || index >= static_cast<int>(commands.size()) || commands[index].code != static_cast<int>(Code::Loop)) {
		return;
	}
	// the first line back at the indent of the loop closes it
	const auto& loop = commands[index];
	for (size_t i = static_cast<size_t>(index) + 1; i < commands.size() && commands[i].indent >= loop.indent; ++i) {
		if (commands[i].indent == loop.indent) {
			if (commands[i].code == static_cast<int>(Code::EndLoop)) {
				commands[i].parameters = loop.parameters;
			}
			return;
		}
	}
}

bool CommandSchema::holds(const Schema& schema, const std::vector<Condition>& conditions, const lcf::rpg::EventCommand& command) {
	return std::all_of(conditions.begin(), conditions.end(), [&](const Condition& condition) {
		return holdsOne(schema, condition, command);
	});
}

bool CommandSchema::isVisible(const Schema& schema, const Field& field, const lcf::rpg::EventCommand& command) {
	return !(field.flags & Flag::Hidden) && holds(schema, field.visible, command);
}

bool CommandSchema::isEnabled(const Schema& schema, const Field& field, const lcf::rpg::EventCommand& command) {
	return holds(schema, field.enabled, command);
}
