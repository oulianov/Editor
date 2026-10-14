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
#include <QCoreApplication>
#include <algorithm>
#include <deque>
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
	F& extension() { return with(Flag::Extension); }
	F& hidden() { return with(Flag::Hidden); }
	F& bitsAt(int first, int count) { shift = first; bits = count; return *this; }
	F& bit(int number) { return bitsAt(number, 1); }
	F& withRange(int low, int high) { minimum = low; maximum = high; return *this; }
	F& withDefault(int value) { defaultValue = value; return *this; }
	F& withStep(int value) { step = value; return *this; }
	F& withSuffix(Label text) { suffix = text; return *this; }
	F& withLink(const char* id) { link = id; return *this; }
	F& withLimit(int characters) { limit = characters; return *this; }
	F& withItems(Items more) { items.insert(items.end(), more.begin(), more.end()); return *this; }
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
Fields partyOperand(Label group, int index) {
	return fields(
		section(group),
		choice("target", index, {}, {
			{0, in("PartyOperandWidget", QT_TRANSLATE_NOOP("PartyOperandWidget", "Entire Party"))},
			{1, in("PartyOperandWidget", QT_TRANSLATE_NOOP("PartyOperandWidget", "Specific"))},
			{2, in("PartyOperandWidget", QT_TRANSLATE_NOOP("PartyOperandWidget", "By Variable"))}}).radio(),
		ref(Kind::Actor, "actor", index + 1).when("target", {1}).inlined(),
		variable("actorVariable", index + 1).when("target", {2}).inlined());
}

// A constant or a variable (mode at index, value at index + 1)
Fields valueOperand(const char* id, Label group, int index, int minimum = -9999999, int maximum = 9999999) {
	const char* operand = name(id, "Operand");
	return fields(
		section(group),
		choice(operand, index, {}, {
			{0, in("ValueOperandWidget", QT_TRANSLATE_NOOP("ValueOperandWidget", "Constant"))},
			{1, in("ValueOperandWidget", QT_TRANSLATE_NOOP("ValueOperandWidget", "By Variable"))}}).radio(),
		number(id, index + 1, {}, minimum, maximum).when(operand, {0}).inlined(),
		variable(name(id, "Variable"), index + 1).when(operand, {1}).inlined());
}

// An entry of the database or the one in a variable
Fields entryOperand(Kind kind, const char* id, Label group, int index) {
	const char* operand = name(id, "Operand");
	return fields(
		section(group),
		choice(operand, index, {}, {
			{0, in("OperandWidget", QT_TRANSLATE_NOOP("OperandWidget", "Specific"))},
			{1, in("OperandWidget", QT_TRANSLATE_NOOP("OperandWidget", "By Variable"))}}).radio(),
		ref(kind, id, index + 1).when(operand, {0}).inlined(),
		variable(name(id, "Variable"), index + 1).when(operand, {1}).inlined());
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
			{1, in("TimerOperandWidget", QT_TRANSLATE_NOOP("TimerOperandWidget", "By Variable"))}}).radio(),
		time("time", index + 1).when("timeOperand", {0}).inlined(),
		variable("timeVariable", index + 1).when("timeOperand", {1}).inlined());
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
// a cell, given or in variables (mode < 0: given)
Fields location(int mode, int map, int x, int y) {
	Fields list = fields(section(in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Location"))));
	if (mode >= 0) {
		append(list, choice("locationMode", mode, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Location")), {
			{0, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Specific Location"))},
			{1, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Location Referenced from Variables"))}}));
	}
	const char* link = map >= 0 ? "map" : nullptr;
	Fields given;
	if (map >= 0) {
		append(given, ref(Kind::Map, "map", map, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Map"))));
	}
	append(given, field(Kind::MapX, "x", x, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "X Coordinate")))
		.withRange(0, 9999).withLink(link));
	append(given, field(Kind::MapY, "y", y, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Y Coordinate")))
		.withRange(0, 9999).withLink(link));
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

// The facing after a move: RPG Maker 2003
F facing(int index) {
	return choice("facing", index, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Facing")), {
		{0, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Retain Facing"))},
		{1, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Up"))},
		{2, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Right"))},
		{3, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Down"))},
		{4, in("LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Left"))}}).rpg2003();
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
		for (auto& one : schema.fields) {
			one.label = resolve(one.label, context);
			one.suffix = resolve(one.suffix, context);
			for (auto& item : one.items) {
				item.text = resolve(item.text, context);
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
		monster("monster", 1).enabledWhen("target", {2}).inlined(),
		section(QT_TRANSLATE_NOOP("ForceFleeWidget", "Option")),
		// checked, a surrounded target (the party: in a pincers attack) does not flee
		check("ignoreSurrounded", 2, QT_TRANSLATE_NOOP("ForceFleeWidget", "Ignore if Target is Surrounded")).with(Flag::Inverted)
	), Flag::Rpg2003);

	// Enable Combo
	table.add(Code::EnableCombo, "EnableComboWidget", QT_TRANSLATE_NOOP("EnableComboWidget", "Enable Combo"), fields(
		section(QT_TRANSLATE_NOOP("EnableComboWidget", "Parameters")),
		ref(Kind::Actor, "hero", 0, QT_TRANSLATE_NOOP("EnableComboWidget", "Target Hero")),
		ref(Kind::BattleCommand, "command", 1, QT_TRANSLATE_NOOP("EnableComboWidget", "Target Battle Command")),
		number("repetitions", 2, QT_TRANSLATE_NOOP("EnableComboWidget", "Number of Repetitions"), 1, 99)
	), Flag::Rpg2003);

	// Change Class
	table.add(Code::ChangeClass, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Class"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Class")),
		ref(Kind::Class, "class", 2, QT_TRANSLATE_NOOP("HeroCommandWidget", "Class:"))
			.withItems({{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "(None)")}}).withDefault(0),
		choice("level", 3, QT_TRANSLATE_NOOP("HeroCommandWidget", "Level:"), {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Retain Current Level")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Character Becomes Level 1")}}),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Learned Skills")),
		choice("skills", 4, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "No Change in Skills")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Add Skills up to Current Level in New Class and Delete Old Skills")},
			{2, QT_TRANSLATE_NOOP("HeroCommandWidget", "Add Skills up to Current Level in New Class to Old Skills")}}),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Base Statistics")),
		choice("statistics", 5, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "No Change in Statistics")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Halve Statistics")},
			{2, QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Statistics to Level 1 in New Class")},
			{3, QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Statistics to Current Level in New Class")}}),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Option")),
		check("message", 6, QT_TRANSLATE_NOOP("HeroCommandWidget", "Show Level-up Message if Level Differs After Class Change"))
	), Flag::Rpg2003);

	// Change Battle Commands
	table.add(Code::ChangeBattleCommands, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Battle Commands"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Battle Command")),
		// "All" (0) only means something for Remove
		ref(Kind::BattleCommand, "command", 2, QT_TRANSLATE_NOOP("HeroCommandWidget", "Command:"))
			.withItems({{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "All")}}),
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

// Maniac Patch: the layouts
// EasyRPG Player reads, the titles of the palette
void addManiac(Table& table) {
	const char* context = "ManiacCommandWidget";

	// Get Save Info
	table.add(Code::Maniac_GetSaveInfo, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Get Save Info")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save File")),
		// EasyRPG Player reads the whole parameter 0 as the mode
		value("slot", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save slot"), 0, -1, 1, 1, 9999),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store In")),
		variable("date", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Date (YYMMDD)")),
		variable("time", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Time (HHMMSS)")),
		variable("level", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Level")),
		variable("hp", 5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "HP")),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Faces as Pictures (0: none)")),
		// one mode for the four picture numbers
		mode("faceMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture numbers"), 7, -1, numberModes()),
		modeValue("face1", "faceMode", 8, 0, 9999, numberModes(), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Face %1"), 1)),
		modeValue("face2", "faceMode", 9, 0, 9999, numberModes(), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Face %1"), 2)),
		modeValue("face3", "faceMode", 10, 0, 9999, numberModes(), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Face %1"), 3)),
		modeValue("face4", "faceMode", 11, 0, 9999, numberModes(), maniac(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Face %1"), 4))
	), Flag::Maniac);

	// Save Game
	table.add(Code::Maniac_Save, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Save Game")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save File")),
		value("slot", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save slot"), 0, -1, 1, 1, 9999),
		check("storeResult", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store the result in a variable")),
		variable("result", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Result")).enabledUnless("storeResult", {0})
	), Flag::Maniac);

	// Load Game
	table.add(Code::Maniac_Load, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Load Game")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save File")),
		value("slot", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save slot"), 0, -1, 1, 1, 9999),
		check("skipCheck", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Do not check that the save exists"))
	), Flag::Maniac);

	// Get Mouse Position
	table.add(Code::Maniac_GetMousePosition, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Get Mouse Position")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store In")),
		variable("x", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "X")),
		variable("y", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Y"))
	), Flag::Maniac);

	// Set Mouse Position: not run by EasyRPG Player, the layout of rpgsync (one mode for both)
	table.add(Code::Maniac_SetMousePosition, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Set Mouse Position")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Position")),
		mode("mode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Given as"), 0, 0, numberModes()),
		modeValue("x", "mode", 1, -9999, 9999, numberModes(), QT_TRANSLATE_NOOP("ManiacCommandWidget", "X")),
		modeValue("y", "mode", 2, -9999, 9999, numberModes(), QT_TRANSLATE_NOOP("ManiacCommandWidget", "Y"))
	), Flag::Maniac);

	// Show String Picture: its string holds the text, the system graphic and
	// the font, each after its mode + 1 ("\x01text\x01system\x01font")
	table.custom(Code::Maniac_ShowStringPicture, "ManiacShowStringPictureWidget",
		palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Show String Picture")), fields(
		section(QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Text")),
		string("text", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Text")),
		// the string of the text when its mode (in the string) is not 0
		field(Kind::StringId, "textId", 22, {}).withRange(0, 99999),
		section(QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Font")),
		value("fontSize", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Size"), 0, 5, 20, 0, 999),
		number("letterSpacing", 14, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Letter spacing"), 0, 255).bitsAt(8, 8),
		number("lineSpacing", 14, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Line spacing"), 0, 255).bitsAt(16, 8),
		check("bold", 13, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Bold")).bit(15),
		check("noShadow", 13, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "No shadow")).bit(14),
		check("noGradient", 13, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "No gradient")).bit(13),
		section(QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Window")),
		mode("sizeMode", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Size given as"), 0, 4, numberModes()),
		modeValue("width", "sizeMode", 18, 0, 9999, numberModes(), QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Width (0: fit the text)")),
		modeValue("height", "sizeMode", 19, 0, 9999, numberModes(), QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Height (0: fit the text)")),
		choice("background", 13, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Background"), {
			{0, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "None")},
			{1, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Stretched")},
			{2, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Tiled")}}).bitsAt(8, 4),
		check("translucent", 14, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Translucent background")).bit(0),
		check("noFrame", 13, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "No frame")).bit(12),
		check("noPadding", 13, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "No padding")).bit(16),
		section(QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Picture")),
		value("picture", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Picture No."), 0, 0, 1, 1, 9999),
		mode("positionMode", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Position given as"), 0, 1, numberModes()),
		modeValue("x", "positionMode", 2, -9999, 9999, numberModes(), QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "X")),
		modeValue("y", "positionMode", 3, -9999, 9999, numberModes(), QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Y")),
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
		value("magnification", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Magnification (%)"), 0, 2, 4, 0, 2000),
		check("separateHeight", 0, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Height magnified apart")).bit(28),
		// the mode of the height: the 3 highest bits, after this flag
		mode("heightMode", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Height (%)"), 0, -1, numberModes())
			.bitsAt(29, 3).enabledUnless("separateHeight", {0}),
		modeValue("heightMagnification", "heightMode", 23, 0, 2000, numberModes()),
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
		number("power", 11, QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Power"), -9999, 9999).when("effect", {1, 2}),
		onlyWhen(value("angle", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Angle"), 13, 6, 11), "effect", {3}),
		onlyWhen(value("divisor", QT_TRANSLATE_NOOP("ManiacShowStringPictureWidget", "Divided by"), 13, 7, 21), "effect", {3})
	), Flag::Maniac);

	// Get Picture Info
	table.add(Code::Maniac_GetPictureInfo, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Get Picture Info")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture")),
		value("picture", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture No."), 0, 0, 3, 1, 9999),
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
		onlyWhen(rectangle("pixel", 0, 1, 4), "info", {3}),
		onlyWhen(value("pixels", QT_TRANSLATE_NOOP("ManiacCommandWidget", "First variable"), 0, 5, 8, 1, 99999999, targetModes()), "info", {3})
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
		value("commonEvent", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common event"), 1, -1, 2, 0, 9999),
		variable("variable", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "First variable"))
	), Flag::Maniac);

	// Control ATB Gauge
	table.add(Code::Maniac_ControlAtbGauge, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Control ATB Gauge")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Target")),
		choice("target", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Target"), battleTargets()),
		onlyWhen(value("targetId", QT_TRANSLATE_NOOP("ManiacCommandWidget", "ID or position"), 1, -1, 2, 0, 9999), "target", {0, 1, 3}),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation")),
		choice("operation", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Set")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Increase")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Decrease")}}),
		value("value", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value"), 5, -1, 6),
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
		onlyWhen(value("targetId", QT_TRANSLATE_NOOP("ManiacCommandWidget", "ID or position"), 2, -1, 3, 0, 9999), "target", {0, 1, 3}),
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

	// Control Variable Array
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
			{14, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Shift left")}, {15, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Shift right")}}),
		value("arrayA", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Array A from"), 1, 0, 2, 1, 99999999, targetModes()),
		value("length", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Length"), 1, 1, 3, 1),
		// the mode of B is the one of the first value
		onlyWhen(value("arrayB", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Array B from"), 1, 2, 4, 1, 99999999, targetModes()),
			"operation", {0, 1, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}),
		onlyWhen(value("start", QT_TRANSLATE_NOOP("ManiacCommandWidget", "First value"), 1, 2, 4), "operation", {5})
	), Flag::Maniac);

	// Key Input Processing Ex
	table.add(Code::Maniac_KeyInputProcEx, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Key Input Processing Ex")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Keys")),
		choice("operation", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Read"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "All keys (keyboard)")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "All keys (keyboard and joypad)")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "One key")},
			{3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Joypad")}}),
		onlyWhen(value("key", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Virtual key code"), 2, -1, 3, 0, 255), "operation", {2}),
		variable("variable", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store in (from)"))
	), Flag::Maniac);

	// Rewrite Map
	table.add(Code::Maniac_RewriteMap, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Rewrite Map")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Tiles")),
		choice("layer", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Layer"), layers()),
		value("tile", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Tile"), 0, 0, 3, 0),
		check("fromVariables", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "One tile per cell, from the variables from the tile value")),
		check("noAutotile", 8, QT_TRANSLATE_NOOP("ManiacCommandWidget", "No autotile update")),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Area")),
		rectangle("area", 0, 1, 4)
	), Flag::Maniac);

	// Control Global Save
	table.add(Code::Maniac_ControlGlobalSave, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Control Global Save")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Global Save")),
		choice("operation", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Open")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Close")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Save and close")},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Copy to the game")}, {5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Copy from the game")}}),
		onlyWhen(fields(
			choice("type", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Data"), {
				{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Switches")},
				{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variables")}}),
			value("game", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Game from"), 1, 0, 3, 1),
			value("global", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Global save from"), 1, 1, 4, 1),
			value("count", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Count"), 1, 2, 5, 1)), "operation", {4, 5})
	), Flag::Maniac);

	// Change Picture ID: not run by EasyRPG Player, the layout of rpgsync
	table.add(Code::Maniac_ChangePictureId, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Picture ID")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Pictures")),
		choice("operation", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Operation"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Move")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Swap")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Slide")}}),
		value("first", QT_TRANSLATE_NOOP("ManiacCommandWidget", "First picture"), 1, 0, 2, 1, 9999),
		value("count", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Count"), 1, 1, 3, 1, 9999),
		value("target", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Target picture"), 1, 2, 4, 1, 9999),
		check("ignoreRange", 5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Ignore out of range"))
	), Flag::Maniac);

	// Set Game Option: the mode of parameter 0 applies to both values
	table.add(Code::Maniac_SetGameOption, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Set Game Option")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Option")),
		choice("option", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Option"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Run when inactive")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Game speed")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture limit")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Frame skip")},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Mouse in messages")}, {5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Battle origin")},
			{6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Battle animation limit")}, {7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Face size")}}),
		mode("valueMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value given as"), 0, -1, numberModes()).when("option", {0, 1, 2, 4, 6, 7}),
		onlyWhen(modeValue("inactive", "valueMode", 2, -99999999, 99999999, numberModes(),
			QT_TRANSLATE_NOOP("ManiacCommandWidget", "Run when inactive (1: yes)")), "option", {0}),
		onlyWhen(modeValue("fps", "valueMode", 2, -99999999, 99999999, numberModes(),
			QT_TRANSLATE_NOOP("ManiacCommandWidget", "Frames per second")), "option", {1}),
		onlyWhen(modeValue("pictures", "valueMode", 2, -99999999, 99999999, numberModes(),
			QT_TRANSLATE_NOOP("ManiacCommandWidget", "Pictures")), "option", {2}),
		onlyWhen(modeValue("mouse", "valueMode", 2, -99999999, 99999999, numberModes(),
			QT_TRANSLATE_NOOP("ManiacCommandWidget", "No mouse in messages (1: yes)")), "option", {4}),
		onlyWhen(modeValue("animations", "valueMode", 2, -99999999, 99999999, numberModes(),
			QT_TRANSLATE_NOOP("ManiacCommandWidget", "Animations")), "option", {6}),
		onlyWhen(modeValue("faceWidth", "valueMode", 2, -99999999, 99999999, numberModes(),
			QT_TRANSLATE_NOOP("ManiacCommandWidget", "Width")), "option", {7}),
		onlyWhen(modeValue("faceHeight", "valueMode", 3, -99999999, 99999999, numberModes(),
			QT_TRANSLATE_NOOP("ManiacCommandWidget", "Height")), "option", {7}),
		choice("testPlay", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Test play"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Keep")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Off")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "On")}}).bitsAt(0, 2).when("option", {1}),
		check("fastText", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Fast forward text")).bit(4).when("option", {1}),
		choice("frameSkip", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Frames drawn"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Every frame")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "1/5")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "1/3")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "1/2")}}).when("option", {3}),
		choice("origin", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Position"), maniacOrigins()).when("option", {5})
	), Flag::Maniac);

	// Call Command
	table.add(Code::Maniac_CallCommand, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Call Command")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Command")),
		value("code", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Command code"), 0, 0, 1, 0, 99999),
		// the string of the called command: this command's string, or a string
		mode("textMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Text"), 0, 3, stringModes()),
		modeValue("text", "textMode", 4, 0, 99999, stringModes()),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Parameters")),
		// the values of the variables from a variable id (or a pointer); other
		// modes are values written in the command, kept
		mode("firstMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "From"), 0, 1, targetModes()),
		variable("first", 2).when("firstMode", {0, 1}).inlined(),
		onlyWhen(value("count", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Count"), 0, 2, 3, 0, 9999), "firstMode", {0, 1}),
		note(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Values written in the command (inline values or expressions): kept as they are, use Edit raw to change them."))
			.unless("firstMode", {0, 1})
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
		modeValue("first", "targetMode", 1, 1, 99999, {{0, {}, Kind::StringId}, {1, {}, Kind::StringId},
			{2, {}, Kind::Variable}, {3, {}, Kind::Variable}}, QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "String")),
		onlyWhen(modeValue("last", "targetMode", 2, 1, 99999, {{1, {}, Kind::StringId}, {3, {}, Kind::Variable}},
			QT_TRANSLATE_NOOP("ManiacControlStringsWidget", "To")), "targetMode", {1, 3}),
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
		onlyWhen(rectangle("area", 0, 0, 3), "info", {1, 3}),
		onlyWhen(value("nest", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Levels up"), 0, 2, 4, 0, 99), "info", {4}),
		choice("face", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Of"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Hero")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Message")}}).when("info", {6}),
		choice("character", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Of"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Hero")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Event")}}).when("info", {7}),
		onlyWhen(value("id", QT_TRANSLATE_NOOP("ManiacCommandWidget", "ID"), 0, 0, 5, 0, 99999), "info", {6, 7}),
		choice("dynamic", 6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Graphic"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Original")},
			{1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Current")}}).when("info", {6, 7}),
		variable("variable", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store in")).when("info", {0, 2, 4, 5, 8, 9}),
		variable("first", 7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Store from")).when("info", {1, 3}),
		field(Kind::StringId, "fileName", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "File name in")).withRange(0, 99999)
			.when("info", {6, 7, 10}),
		variable("index", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Index in")).when("info", {6, 7}),
		variable("bgm", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Fade, volume, tempo, balance from")).when("info", {10})
	), Flag::Maniac);

	// Edit Picture
	table.add(Code::Maniac_EditPicture, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Edit Picture")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture")),
		value("picture", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture No."), 0, 0, 1, 1, 9999),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Area")),
		rectangle("area", 0, 1, 2),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Pixels")),
		value("pixels", QT_TRANSLATE_NOOP("ManiacCommandWidget", "From variable"), 0, 5, 6, 1, 99999999, targetModes()),
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
			value("picture", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture No."), 0, 0, 2, 1, 9999),
			check("dynamic", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "With the effects (tone, flash...)")).bit(0),
			check("opaque", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Opaque")).bit(1)), "target", {1}),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "File (PNG)")),
		mode("fileMode", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Name given as"), 0, 1, stringModes()),
		modeValue("file", "fileMode", 3, 0, 99999, stringModes(), QT_TRANSLATE_NOOP("ManiacCommandWidget", "Name"))
	), Flag::Maniac);

	// Edit Tile: not run by EasyRPG Player, the layout of rpgsync
	table.add(Code::Maniac_EditTile, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Edit Tile")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Tiles")),
		value("picture", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Picture No."), 0, 0, 1, 1, 9999),
		value("chipset", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Chipset"), 0, 6, 10, 0, 9999),
		choice("layer", 9, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Layer"), layers()),
		value("tile", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Tile"), 0, 5, 6, 0),
		value("pattern", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Pattern"), 0, 7, 11, 0),
		check("fromVariables", 8, QT_TRANSLATE_NOOP("ManiacCommandWidget", "One tile per cell, from the variables from the tile value")),
		check("noAutotile", 7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "No autotile update")).bit(0),
		check("clear", 7, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Clear the area")).bit(1),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Area")),
		rectangle("area", 0, 1, 2)
	), Flag::Maniac);

	// Control Text Processing: not run by EasyRPG Player, the layout of rpgsync
	table.add(Code::Maniac_ControlTextProcessing, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Control Text Processing")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Common Event")),
		choice("source", 0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Given as"), {
			{0, QT_TRANSLATE_NOOP("ManiacCommandWidget", "ID")}, {1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable")},
			{2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Variable pointer")}, {3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Name")},
			{4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "String")}, {5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "String pointer")}}),
		number("event", 2, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Value"), 0, 99999999).when("source", {0, 1, 2, 4, 5}),
		string("name", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Name")).when("source", {3}),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Called")),
		check("onMarker", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "On a marker")).bit(0),
		check("onOpen", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "When the message opens")).bit(1),
		check("onClose", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "When the message closes")).bit(2),
		check("onCharacter", 1, QT_TRANSLATE_NOOP("ManiacCommandWidget", "On each character")).bit(3),
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Data")),
		variable("info", 3, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Information in")),
		field(Kind::StringId, "text", 4, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Text in string")).withRange(0, 99999),
		number("numbers", 5, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Number arguments"), 0, 99999),
		number("strings", 6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "String arguments"), 0, 99999)
	), Flag::Maniac);

	// Zoom: not run by EasyRPG Player, the layout of rpgsync
	table.add(Code::Maniac_Zoom, context, palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Zoom")), fields(
		section(QT_TRANSLATE_NOOP("ManiacCommandWidget", "Zoom")),
		value("x", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Center X"), 0, 0, 1, -99999, 99999),
		value("y", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Center Y"), 0, 1, 2, -99999, 99999),
		value("scale", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Magnification (%)"), 0, 2, 3, 0, 99999),
		value("duration", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Duration (frames)"), 0, 3, 4, 0, 99999),
		value("layer", QT_TRANSLATE_NOOP("ManiacCommandWidget", "Layer"), 0, 4, 5, 0, 99),
		check("wait", 6, QT_TRANSLATE_NOOP("ManiacCommandWidget", "Wait for completion"))
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
		check("parallel", 3, QT_TRANSLATE_NOOP("MessageOptionsWidget", "Allow other events to continue while message is shown"))
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
		check("flip", 2, QT_TRANSLATE_NOOP("ChangeFaceGraphicWidget", "Flip Image Horizontally"))
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
		number("digits", 0, QT_TRANSLATE_NOOP("InputNumberWidget", "Digits Allowed"), 1, 9).withDefault(1),
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
		choice("operation", 2, QT_TRANSLATE_NOOP("HeroCommandWidget", "Operation:"), {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Increase Statistic")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Decrease Statistic")}}),
		choice("statistic", 3, QT_TRANSLATE_NOOP("HeroCommandWidget", "Statistic:"), {
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
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Remove Skill")}}),
		entryOperand(Kind::Skill, "skill", QT_TRANSLATE_NOOP("HeroCommandWidget", "Skill"), 3)
	));

	// Change Equipment: parameter 3 is the item source when equipping, the
	// slot when unequipping
	table.add(Code::ChangeEquipment, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Equipped Items"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Operation")),
		choice("operation", 2, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Equip Item")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Unequip Item")}}),
		onlyWhen(fields(
			section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Item")),
			choice("source", 3, QT_TRANSLATE_NOOP("HeroCommandWidget", "Source:"), {
				{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Specific Item")},
				{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Item with ID Stored in")}}),
			ref(Kind::Item, "item", 4, QT_TRANSLATE_NOOP("HeroCommandWidget", "Specific Item:")).with(Flag::Equipment).when("source", {0}),
			variable("itemVariable", 4, QT_TRANSLATE_NOOP("HeroCommandWidget", "Variable:")).when("source", {1})), "operation", {0}),
		onlyWhen(fields(
			section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Slot from which to Unequip Item")),
			choice("slot", 3, {}, {
				{5, QT_TRANSLATE_NOOP("HeroCommandWidget", "All")}, {0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Weapon")},
				{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Shield")}, {2, QT_TRANSLATE_NOOP("HeroCommandWidget", "Armor")},
				{3, QT_TRANSLATE_NOOP("HeroCommandWidget", "Helmet")}, {4, QT_TRANSLATE_NOOP("HeroCommandWidget", "Accessory")}})
				.withDefault(5)), "operation", {1})
	));

	// Change HP
	table.add(Code::ChangeHP, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Change HP"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Operation")),
		choice("operation", 2, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Increase HP")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Decrease HP")}}),
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
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Decrease MP")}}),
		valueOperand("amount", QT_TRANSLATE_NOOP("HeroCommandWidget", "Amount"), 3, 0, 9999999)
	));

	// Change Condition
	table.add(Code::ChangeCondition, "HeroCommandWidget", QT_TRANSLATE_NOOP("HeroCommandWidget", "Change Character Condition"), fields(
		partyOperand(QT_TRANSLATE_NOOP("HeroCommandWidget", "Target"), 0),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Operation")),
		choice("operation", 2, {}, {
			{0, QT_TRANSLATE_NOOP("HeroCommandWidget", "Add Condition")},
			{1, QT_TRANSLATE_NOOP("HeroCommandWidget", "Remove Condition")}}),
		section(QT_TRANSLATE_NOOP("HeroCommandWidget", "Condition")),
		ref(Kind::State, "condition", 3)
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

	// Change Hero Name: a new hero suggests its name, unless one was typed
	table.add(Code::ChangeHeroName, "ChangeActorNameWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Hero Name")), fields(
		ref(Kind::Actor, "hero", 0, QT_TRANSLATE_NOOP("ChangeActorNameWidget", "Actor:")),
		// the length of a hero's name in the database
		string("name", QT_TRANSLATE_NOOP("ChangeActorNameWidget", "New Name:")).withLimit(12)
	));

	// Change Hero Title: the same with the title
	table.add(Code::ChangeHeroTitle, "ChangeActorTitleWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Hero Title")), fields(
		ref(Kind::Actor, "hero", 0, QT_TRANSLATE_NOOP("ChangeActorTitleWidget", "Actor:")),
		string("title", QT_TRANSLATE_NOOP("ChangeActorTitleWidget", "New Title:")).withLimit(12)
	));

	// Change Hero Graphic: a picker; a new hero shows its graphic
	table.custom(Code::ChangeSpriteAssociation, "ChangeActorGraphicWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Hero Graphic")), fields(
		section(QT_TRANSLATE_NOOP("ChangeActorGraphicWidget", "Actor")),
		ref(Kind::Actor, "hero", 0),
		section(QT_TRANSLATE_NOOP("ChangeActorGraphicWidget", "Character Graphic")),
		graphic(Kind::Charset, "graphic", 1, CHARSET),
		check("translucent", 2, QT_TRANSLATE_NOOP("ChangeActorGraphicWidget", "Translucent"))
	));

	// Change Hero Face: a picker; a new hero shows its face
	table.custom(Code::ChangeActorFace, "ChangeActorFaceWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Change Hero Face")), fields(
		section(QT_TRANSLATE_NOOP("ChangeActorFaceWidget", "Actor")),
		ref(Kind::Actor, "hero", 0),
		section(QT_TRANSLATE_NOOP("ChangeActorFaceWidget", "Faceset")),
		graphic(Kind::Face, "face", 1, FACESET)
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
		audioFile(MUSIC),
		fadeIn(1),
		audio(2)
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
		audioFile(SOUND),
		audio(1)
	));

	// Change System Graphics
	table.add(Code::ChangeSystemGraphics, "ChangeSystemGraphicsWidget", QT_TRANSLATE_NOOP("ChangeSystemGraphicsWidget", "Change System Graphics"), fields(
		section(QT_TRANSLATE_NOOP("ChangeSystemGraphicsWidget", "System Graphic")),
		file("graphic", {}, SYSTEM),
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
			{1, QT_TRANSLATE_NOOP("EnemyEncounterWidget", "Monster Group with ID Stored in")}}).radio(),
		ref(Kind::Troop, "troop", 1).when("troopKind", {0}).inlined(),
		variable("troopVariable", 1).when("troopKind", {1}).inlined(),
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
			{2, QT_TRANSLATE_NOOP("OpenShopWidget", "C")}}),
		section(QT_TRANSLATE_NOOP("OpenShopWidget", "Option")),
		check("handlers", 2, QT_TRANSLATE_NOOP("OpenShopWidget", "Define Custom Handlers for \"Transaction Made\" and \"Transaction not Made\"")),
		// a shop that only buys sells nothing
		section(QT_TRANSLATE_NOOP("OpenShopWidget", "Goods for Sale")).enabledUnless("transactions", {2}),
		field(Kind::ItemList, "goods", 4, {}).enabledUnless("transactions", {2})
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

	// Enter Hero Name: the first page of the keyboard depends on the encoding
	table.add(Code::EnterHeroName, "EnterHeroNameWidget", QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Enter Hero Name"), fields(
		section(QT_TRANSLATE_NOOP("EnterHeroNameWidget", "Hero")),
		ref(Kind::Actor, "hero", 0),
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
			number("duration", 14, {}, 0, 10000).withSuffix(QT_TRANSLATE_NOOP("PictureWidget", " × 0.1 s")).when("durationMode", {0}),
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
		facing(3)
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
		choice("vehicle", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Vehicle"), {
			{0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Boat")},
			{1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Ship")},
			{2, QT_TRANSLATE_NOOP("LocationCommandWidget", "Airship")},
			{-1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Party"), Flag::Kept}}),
		location(1, 2, 3, 4)
	));

	// Set Event Location: the cell on the map of the event
	table.add(Code::ChangeEventLocation, context, QT_TRANSLATE_NOOP("LocationCommandWidget", "Change Event Location"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Event")),
		character("event", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Event"), locationCharacters()),
		location(1, -1, 2, 3),
		facing(4).enabledWhen("locationMode", {0})
	));

	// Swap Event Locations: the bits of parameter 2 take an event from a
	// variable (an engine extension: shown, not set)
	table.add(Code::TradeEventLocations, context, QT_TRANSLATE_NOOP("LocationCommandWidget", "Trade Event Locations"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "First Event")),
		check("firstFromVariable", 2, {}).bit(0).hidden(),
		character("first", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Event"), locationCharacters()).when("firstFromVariable", {0}),
		variable("firstVariable", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Event ID Stored in")).when("firstFromVariable", {1}),
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Second Event")),
		check("secondFromVariable", 2, {}).bit(1).hidden(),
		character("second", 1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Event"), locationCharacters()).when("secondFromVariable", {0}),
		variable("secondVariable", 1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Event ID Stored in")).when("secondFromVariable", {1})
	));

	// Get Terrain ID
	table.add(Code::StoreTerrainID, context, QT_TRANSLATE_NOOP("LocationCommandWidget", "Store Terrain ID"), fields(
		location(0, -1, 1, 2),
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Storage Location")),
		variable("result", 3, QT_TRANSLATE_NOOP("LocationCommandWidget", "Store Terrain ID in Variable"))
	));

	// Get Event ID
	table.add(Code::StoreEventID, context, QT_TRANSLATE_NOOP("LocationCommandWidget", "Store Event ID"), fields(
		location(0, -1, 1, 2),
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Storage Location")),
		variable("result", 3, QT_TRANSLATE_NOOP("LocationCommandWidget", "Store Event ID in Variable"))
	));

	// Erase Screen
	table.add(Code::EraseScreen, "ScreenTransitionWidget", in("EraseScreenWidget", QT_TRANSLATE_NOOP("EraseScreenWidget", "Erase Screen")), fields(
		section(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Transition Method")),
		choice("transition", 0, {}, Items{{-1, QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Use Default")}} + transitions(false))
			.withDefault(-1)
	));

	// Show Screen
	table.add(Code::ShowScreen, "ScreenTransitionWidget", in("ShowScreenWidget", QT_TRANSLATE_NOOP("ShowScreenWidget", "Show Screen")), fields(
		section(QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Transition Method")),
		choice("transition", 0, {}, Items{{-1, QT_TRANSLATE_NOOP("ScreenTransitionWidget", "Use Default")}} + transitions(true))
			.withDefault(-1)
	));

	// Tint Screen
	table.add(Code::TintScreen, "TintScreenWidget", palette(QT_TRANSLATE_NOOP("EventCommandPalette", "Tint Screen")), fields(
		section(QT_TRANSLATE_NOOP("TintScreenWidget", "Color Tone")),
		number("red", 0, QT_TRANSLATE_NOOP("TintScreenWidget", "Red:"), 0, 200).slider().withSuffix(QT_TRANSLATE_NOOP("TintScreenWidget", " %")),
		number("green", 1, QT_TRANSLATE_NOOP("TintScreenWidget", "Green:"), 0, 200).slider().withSuffix(QT_TRANSLATE_NOOP("TintScreenWidget", " %")),
		number("blue", 2, QT_TRANSLATE_NOOP("TintScreenWidget", "Blue:"), 0, 200).slider().withSuffix(QT_TRANSLATE_NOOP("TintScreenWidget", " %")),
		number("saturation", 3, QT_TRANSLATE_NOOP("TintScreenWidget", "Saturation:"), 0, 200).slider()
			.withSuffix(QT_TRANSLATE_NOOP("TintScreenWidget", " %")),
		number("time", 4, QT_TRANSLATE_NOOP("TintScreenWidget", "Time:"), 0, 1000).withSuffix(QT_TRANSLATE_NOOP("TintScreenWidget", "x0.1 sec")),
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
		choice("operation", 0, {}, {
			{0, QT_TRANSLATE_NOOP("PanScreenWidget", "Lock Pan State")},
			{1, QT_TRANSLATE_NOOP("PanScreenWidget", "Unlock Pan State")},
			{2, QT_TRANSLATE_NOOP("PanScreenWidget", "Pan Screen")},
			{3, QT_TRANSLATE_NOOP("PanScreenWidget", "Return to Origin")}}).radio().withDefault(2),
		section(QT_TRANSLATE_NOOP("PanScreenWidget", "Direction and Distance")),
		choice("direction", 1, QT_TRANSLATE_NOOP("PanScreenWidget", "Direction"), {
			{0, QT_TRANSLATE_NOOP("PanScreenWidget", "Up")},
			{3, QT_TRANSLATE_NOOP("PanScreenWidget", "Left")},
			{1, QT_TRANSLATE_NOOP("PanScreenWidget", "Right")},
			{2, QT_TRANSLATE_NOOP("PanScreenWidget", "Down")}}).enabledWhen("operation", {2}),
		number("distance", 2, QT_TRANSLATE_NOOP("PanScreenWidget", "Tiles"), 0, 9999).withDefault(1).enabledWhen("operation", {2}),
		choice("speed", 3, QT_TRANSLATE_NOOP("PanScreenWidget", "Transition Speed"), {
			{1, QT_TRANSLATE_NOOP("PanScreenWidget", "1: One-Eighth Normal")},
			{2, QT_TRANSLATE_NOOP("PanScreenWidget", "2: One-Fourth Normal")},
			{3, QT_TRANSLATE_NOOP("PanScreenWidget", "3: One-Half Normal")},
			{4, QT_TRANSLATE_NOOP("PanScreenWidget", "4: Normal")},
			{5, QT_TRANSLATE_NOOP("PanScreenWidget", "5: Twice Normal")},
			{6, QT_TRANSLATE_NOOP("PanScreenWidget", "6: Four Times Normal")}}).withDefault(4).enabledWhen("operation", {2, 3}),
		check("wait", 4, QT_TRANSLATE_NOOP("PanScreenWidget", "Wait for completion")).bit(0).enabledWhen("operation", {2, 3})
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

	// Show Battle Animation
	table.add(Code::ShowBattleAnimation, "ShowBattleAnimationWidget", QT_TRANSLATE_NOOP("ShowBattleAnimationWidget", "Show Battle Animation"), fields(
		section(QT_TRANSLATE_NOOP("ShowBattleAnimationWidget", "Battle Animation")),
		ref(Kind::Animation, "animation", 0, QT_TRANSLATE_NOOP("ShowBattleAnimationWidget", "Battle Animation")),
		character("target", 1, QT_TRANSLATE_NOOP("ShowBattleAnimationWidget", "Target"), pictureCharacters()),
		section(QT_TRANSLATE_NOOP("ShowBattleAnimationWidget", "Options")),
		check("wait", 2, QT_TRANSLATE_NOOP("ShowBattleAnimationWidget", "Wait for completion")),
		check("global", 3, QT_TRANSLATE_NOOP("ShowBattleAnimationWidget", "Global Scope"))
	));

	// Hero Transparency
	table.add(Code::PlayerVisibility, "PlayerVisibilityWidget", QT_TRANSLATE_NOOP("PlayerVisibilityWidget", "Player Visibility"), fields(
		section(QT_TRANSLATE_NOOP("PlayerVisibilityWidget", "Transparency State")),
		choice("state", 0, {}, {
			{0, QT_TRANSLATE_NOOP("PlayerVisibilityWidget", "Transparent")},
			{1, QT_TRANSLATE_NOOP("PlayerVisibilityWidget", "Opaque")}}).radio().withDefault(1)
	));

	// Flash Event: colors of 0-31
	table.add(Code::FlashSprite, "FlashSpriteWidget", QT_TRANSLATE_NOOP("FlashSpriteWidget", "Flash Sprite"), fields(
		section(QT_TRANSLATE_NOOP("FlashSpriteWidget", "Target")),
		character("target", 0, {}, pictureCharacters()),
		section(QT_TRANSLATE_NOOP("FlashSpriteWidget", "Color")),
		number("red", 1, QT_TRANSLATE_NOOP("FlashSpriteWidget", "Red"), 0, 31).withDefault(31),
		number("green", 2, QT_TRANSLATE_NOOP("FlashSpriteWidget", "Green"), 0, 31).withDefault(31),
		number("blue", 3, QT_TRANSLATE_NOOP("FlashSpriteWidget", "Blue"), 0, 31).withDefault(31),
		number("strength", 4, QT_TRANSLATE_NOOP("FlashSpriteWidget", "Strength"), 0, 31).withDefault(31),
		section(QT_TRANSLATE_NOOP("FlashSpriteWidget", "Flash Cycle Time")),
		number("time", 5, {}, 0, 10000).withDefault(5).withSuffix(QT_TRANSLATE_NOOP("FlashSpriteWidget", " × 0.1 s")),
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
		// the other bits of parameter 2 are Maniac Patch flags, kept
		check("repeat", 2, QT_TRANSLATE_NOOP("MoveRouteDialog", "Repeat the moves")).bit(0),
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
	// Wait: a time, or (RPG Maker 2003) until a key is pressed
	table.add(Code::Wait, "WaitWidget", QT_TRANSLATE_NOOP("WaitWidget", "Wait"), fields(
		choice("mode", 1, {}, {
			{0, QT_TRANSLATE_NOOP("WaitWidget", "Wait for")},
			{1, QT_TRANSLATE_NOOP("WaitWidget", "Wait until a key is pressed"), Flag::Rpg2003}}).radio(),
		number("time", 0, {}, 0, 9999).withSuffix(QT_TRANSLATE_NOOP("WaitWidget", " × 0.1 s")).enabledWhen("mode", {0}).inlined()
	));

	// Play BGM
	table.add(Code::PlayBGM, "MediaAudioWidget", in("PlayBGMWidget", QT_TRANSLATE_NOOP("PlayBGMWidget", "Play BGM")), fields(
		section(QT_TRANSLATE_NOOP("MediaAudioWidget", "BGM")),
		audioFile(MUSIC),
		fadeIn(0),
		audio(1)
	));

	// Fade Out BGM
	table.add(Code::FadeOutBGM, "FadeOutBGMWidget", QT_TRANSLATE_NOOP("FadeOutBGMWidget", "Fade Out BGM"), fields(
		section(QT_TRANSLATE_NOOP("FadeOutBGMWidget", "Fade Out Time")),
		number("time", 0, QT_TRANSLATE_NOOP("FadeOutBGMWidget", "Time"), 0, 20000)
			.withSuffix(QT_TRANSLATE_NOOP("FadeOutBGMWidget", " ms")).withStep(1000)
	));

	// Play Sound Effect
	table.add(Code::PlaySound, "MediaAudioWidget", in("PlaySoundWidget", QT_TRANSLATE_NOOP("PlaySoundWidget", "Play Sound Effect")), fields(
		section(QT_TRANSLATE_NOOP("MediaAudioWidget", "Sound Effect")),
		audioFile(SOUND),
		audio(0)
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
			{1, QT_TRANSLATE_NOOP("PlayMovieWidget", "Variable Reference")}}).radio(),
		number("x", 1, QT_TRANSLATE_NOOP("PlayMovieWidget", "X"), -9999, 9999).when("position", {0}),
		number("y", 2, QT_TRANSLATE_NOOP("PlayMovieWidget", "Y"), -9999, 9999).when("position", {0}),
		variable("xVariable", 1, QT_TRANSLATE_NOOP("PlayMovieWidget", "X")).when("position", {1}),
		variable("yVariable", 2, QT_TRANSLATE_NOOP("PlayMovieWidget", "Y")).when("position", {1})
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
		check("shift", 5, QT_TRANSLATE_NOOP("KeyInputProcWidget", "Shift Key (7)"))
	));

	// Change Chipset: bit 0 of parameter 1 takes the chipset from a variable
	table.add(Code::ChangeMapTileset, "LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Change Map Tileset"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "New Tileset")),
		check("fromVariable", 1, {}).bit(0).hidden().extension(),
		ref(Kind::Chipset, "chipset", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Tileset")).when("fromVariable", {0}),
		variable("chipsetVariable", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Tileset ID Stored in")).when("fromVariable", {1})
	));

	// Change Panorama: bits of parameter 6 take the name and the speeds from variables
	table.add(Code::ChangePBG, "LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Change Parallax Background"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Graphic")),
		check("nameFromVariable", 6, {}).bit(0).hidden().extension(),
		file("graphic", QT_TRANSLATE_NOOP("LocationCommandWidget", "Graphic"), PANORAMA).when("nameFromVariable", {0}),
		variable("graphicVariable", 7, QT_TRANSLATE_NOOP("LocationCommandWidget", "Graphic Name Stored in"))
			.when("nameFromVariable", {1}).extension(),
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Options")),
		check("horizontal", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Horizontal Scrolling")),
		check("horizontalAuto", 2, QT_TRANSLATE_NOOP("LocationCommandWidget", "Horizontal Autoscroll")).enabledUnless("horizontal", {0}),
		check("horizontalFromVariable", 6, {}).bit(4).hidden().extension(),
		number("horizontalSpeed", 3, QT_TRANSLATE_NOOP("LocationCommandWidget", "Horizontal Speed"), -8, 8)
			.when("horizontalFromVariable", {0}).enabledUnless("horizontal", {0}).enabledUnless("horizontalAuto", {0}),
		variable("horizontalSpeedVariable", 3, QT_TRANSLATE_NOOP("LocationCommandWidget", "Horizontal Speed Stored in"))
			.when("horizontalFromVariable", {1}).enabledUnless("horizontal", {0}).enabledUnless("horizontalAuto", {0}),
		check("vertical", 1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Vertical Scrolling")),
		check("verticalAuto", 4, QT_TRANSLATE_NOOP("LocationCommandWidget", "Vertical Autoscroll")).enabledUnless("vertical", {0}),
		check("verticalFromVariable", 6, {}).bit(6).hidden().extension(),
		number("verticalSpeed", 5, QT_TRANSLATE_NOOP("LocationCommandWidget", "Vertical Speed"), -8, 8)
			.when("verticalFromVariable", {0}).enabledUnless("vertical", {0}).enabledUnless("verticalAuto", {0}),
		variable("verticalSpeedVariable", 5, QT_TRANSLATE_NOOP("LocationCommandWidget", "Vertical Speed Stored in"))
			.when("verticalFromVariable", {1}).enabledUnless("vertical", {0}).enabledUnless("verticalAuto", {0})
	));

	// Change Encounter Rate
	table.add(Code::ChangeEncounterSteps, "LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Change Encounter Rate"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "New Rate")),
		number("steps", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Steps Between Encounters"), 0, 9999)
	));

	// Change Single Tile
	table.add(Code::TileSubstitution, "LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Tile Substitution"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Tile Layer")),
		choice("layer", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Tile Layer"), {
			{0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Lower Layer")},
			{1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Upper Layer")}}),
		field(Kind::Tile, "original", 1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Original Tile")).withRange(0, 143).withLink("layer"),
		field(Kind::Tile, "substitute", 2, QT_TRANSLATE_NOOP("LocationCommandWidget", "Substituted Tile")).withRange(0, 143).withLink("layer")
	));

	// Set Teleport Location
	table.add(Code::TeleportTargets, "LocationCommandWidget", QT_TRANSLATE_NOOP("LocationCommandWidget", "Teleport Target Management"), fields(
		section(QT_TRANSLATE_NOOP("LocationCommandWidget", "Operation")),
		choice("operation", 0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Operation"), {
			{0, QT_TRANSLATE_NOOP("LocationCommandWidget", "Add Teleport Target")},
			{1, QT_TRANSLATE_NOOP("LocationCommandWidget", "Delete Teleport Target")}}),
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
	access(table, Code::ChangeSaveAccess, QT_TRANSLATE_NOOP("EventCommandPalette", "Enable Saving"));

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

	// Call Event: a common event, an event of the map and its page, or both from variables
	table.add(Code::CallEvent, "CallEventWidget", QT_TRANSLATE_NOOP("CallEventWidget", "Call Event"), fields(
		choice("kind", 0, {}, {
			{0, QT_TRANSLATE_NOOP("CallEventWidget", "Common Event")},
			{1, QT_TRANSLATE_NOOP("CallEventWidget", "Map Event")},
			{2, QT_TRANSLATE_NOOP("CallEventWidget", "Map Event chosen by variables")}}).radio(),
		ref(Kind::CommonEvent, "commonEvent", 1, QT_TRANSLATE_NOOP("CallEventWidget", "Common Event")).when("kind", {0}),
		character("event", 1, QT_TRANSLATE_NOOP("CallEventWidget", "Event"), {
			{10005, QT_TRANSLATE_NOOP("CallEventWidget", "This Event")}}).when("kind", {1}),
		number("page", 2, QT_TRANSLATE_NOOP("CallEventWidget", "Page"), 1, 100).withDefault(1).when("kind", {1}),
		variable("eventVariable", 1, QT_TRANSLATE_NOOP("CallEventWidget", "Event ID in")).when("kind", {2}),
		variable("pageVariable", 2, QT_TRANSLATE_NOOP("CallEventWidget", "Page in")).when("kind", {2})
	));

	// Comment: its text over several lines
	table.custom(Code::Comment, "CommentWidget", QT_TRANSLATE_NOOP("CommentWidget", "Comment"), fields(
		field(Kind::Lines, "comment", StringIndex, {})
	));
}

// Battle event commands
void addBattle(Table& table) {
	// Change Enemy HP: the monster of the troop, from 0
	table.add(Code::ChangeMonsterHP, "SceneCommandWidget", in("ChangeMonsterHPWidget", QT_TRANSLATE_NOOP("ChangeMonsterHPWidget", "Change Monster HP")), fields(
		section(QT_TRANSLATE_NOOP("SceneCommandWidget", "Monster")),
		monster("monster", 0),
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

	// Change Enemy MP
	table.add(Code::ChangeMonsterMP, "SceneCommandWidget", in("ChangeMonsterMPWidget", QT_TRANSLATE_NOOP("ChangeMonsterMPWidget", "Change Monster MP")), fields(
		section(QT_TRANSLATE_NOOP("SceneCommandWidget", "Monster")),
		monster("monster", 0),
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

	// Change Enemy Condition
	table.add(Code::ChangeMonsterCondition, "ChangeMonsterConditionWidget",
		QT_TRANSLATE_NOOP("ChangeMonsterConditionWidget", "Change Monster Condition"), fields(
		section(QT_TRANSLATE_NOOP("ChangeMonsterConditionWidget", "Monster")),
		monster("monster", 0),
		section(QT_TRANSLATE_NOOP("ChangeMonsterConditionWidget", "Operation")),
		choice("operation", 1, {}, {
			{0, QT_TRANSLATE_NOOP("ChangeMonsterConditionWidget", "Add Condition")},
			{1, QT_TRANSLATE_NOOP("ChangeMonsterConditionWidget", "Remove Condition")}}).radio(),
		section(QT_TRANSLATE_NOOP("ChangeMonsterConditionWidget", "Condition")),
		ref(Kind::State, "condition", 2)
	));

	// Show Hidden Enemy
	table.add(Code::ShowHiddenMonster, "ShowHiddenMonsterWidget", QT_TRANSLATE_NOOP("ShowHiddenMonsterWidget", "Show Hidden Monster"), fields(
		section(QT_TRANSLATE_NOOP("ShowHiddenMonsterWidget", "Monster to Show")),
		monster("monster", 0)
	));

	// Change Battle Background
	table.add(Code::ChangeBattleBG, "ChangeBattleBGWidget", QT_TRANSLATE_NOOP("ChangeBattleBGWidget", "Change Battle Background"), fields(
		section(QT_TRANSLATE_NOOP("ChangeBattleBGWidget", "Battle Background")),
		file("background", {}, BACKDROP)
	));

	// Show Battle Animation (battle): one list for the target and its side
	// (parameter 3, RPG Maker 2003: the party); monsters count from 0, party
	// members from 1
	table.custom(Code::ShowBattleAnimation_B, "ShowBattleAnimation_BWidget",
		QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "Show Battle Animation"), fields(
		section(QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "Animation")),
		ref(Kind::Animation, "animation", 0),
		section(QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "Target")),
		check("party", 3, {}).hidden().extension().rpg2003(),
		choice("target", 1, {}, Items{{-1, QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "All Enemies")}}
			+ numbered(QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "Monster %1"), 8, 0)).withDefault(-1).unless("party", {1}),
		choice("partyTarget", 1, {}, Items{{-1, QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "Entire Party")}}
			+ numbered(QT_TRANSLATE_NOOP("ShowBattleAnimation_BWidget", "Party Member %1"), 4, 1)).withDefault(-1).when("party", {1}).rpg2003(),
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
	return kind != Kind::Section && kind != Kind::Note;
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
		if (one.hasValue() && id == one.id) {
			return &one;
		}
	}
	return nullptr;
}

bool CommandSchema::hasEditor(Code code) {
	// the commands without settings, and the lines of the structure of a block
	switch (code) {
		case Code::END:
		case Code::EnterExitVehicle:
		case Code::ProceedWithMovement:
		case Code::HaltAllMovement:
		case Code::MemorizeBGM:
		case Code::PlayMemorizedBGM:
		case Code::OpenSaveMenu:
		case Code::OpenMainMenu:
		case Code::Loop:
		case Code::BreakLoop:
		case Code::EndEventProcessing:
		case Code::EraseEvent:
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
