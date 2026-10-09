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

#include "command_colors.h"
#include <QPalette>
#include <algorithm>
#include <array>
#include <initializer_list>
#include <utility>

namespace CommandColors {

namespace {

// Above every command code (the last is 23311, Branch End in battle)
constexpr int kMaxCode = 24000;

using Table = std::array<Kind, kMaxCode>;

void set(Table& table, Kind kind, std::initializer_list<int> codes) {
	for (int code : codes) {
		table[code] = kind;
	}
}

// The commands from first to last: the codes of RPG Maker go by 10
void setRange(Table& table, Kind kind, int first, int last, int step = 10) {
	for (int code = first; code <= last; code += step) {
		table[code] = kind;
	}
}

// Built once: a line is drawn by indexing it, thousands of lines per paint
const Table& table() {
	static const Table kinds = [] {
		Table t;
		t.fill(Kind::Text);
		set(t, Kind::End, {10});
		set(t, Kind::Comment, {12410, 22410});
		set(t, Kind::Progress, {10210, 10220, 10230, 10910, 10920, 11610});
		set(t, Kind::Flow, {12010, 13310, 12110, 12120, 12210, 12220, 12310, 12320, 12330, 1005});
		set(t, Kind::Block, {22010, 22011, 23310, 23311, 22210, 20140, 20141,
			20710, 20711, 20712, 20713, 20720, 20721, 20722, 20730, 20731, 20732});
		set(t, Kind::Party, {10310, 10320, 10330, 1008, 1009, 13110, 13120, 13130, 13150});
		setRange(t, Kind::Party, 10410, 10500);
		setRange(t, Kind::Party, 10610, 10650);
		setRange(t, Kind::Movement, 10810, 10870);
		set(t, Kind::Movement, {11210, 11310, 11320, 11330, 11340, 11350, 13260});
		setRange(t, Kind::Screen, 11010, 11070);
		setRange(t, Kind::Screen, 11110, 11130);
		set(t, Kind::Screen, {11410, 13210});
		setRange(t, Kind::Audio, 11510, 11560);
		setRange(t, Kind::Scene, 10660, 10690);
		setRange(t, Kind::Scene, 10710, 10740);
		setRange(t, Kind::Scene, 11710, 11750);
		setRange(t, Kind::Scene, 11810, 11840);
		setRange(t, Kind::Scene, 11910, 11960);
		set(t, Kind::Scene, {12420, 12510, 13410, 1006, 1007});
		setRange(t, Kind::Scene, 5001, 5005, 1);
		// EasyRPG and Maniac Patch; the Maniac variables and strings are game progress
		setRange(t, Kind::Extension, 2000, 2099, 1);
		setRange(t, Kind::Extension, 3000, 3099, 1);
		set(t, Kind::Progress, {3013, 3020});
		return t;
	}();
	return kinds;
}

bool isDark(const QPalette& palette) {
	return palette.color(QPalette::Base).lightnessF() < 0.5f;
}

}

Kind kind(int code) {
	if (code < 0 || code >= kMaxCode) {
		return Kind::Text;
	}
	return table()[code];
}

QColor color(Kind kind, const QPalette& palette) {
	// Two palettes, GitHub Primer's syntax colors for light and dark themes:
	// every color has a contrast of at least 4.5:1 (WCAG AA) on the list and
	// on its stripes. The kinds keep RPG Maker 2003's hues.
	auto pick = [&palette](QRgb light, QRgb dark) { return QColor(isDark(palette) ? dark : light); };
	switch (kind) {
		case Kind::Progress: return pick(0xcf222e, 0xff7b72);   // red
		case Kind::Flow: return pick(0x0550ae, 0x79c0ff);       // blue
		case Kind::Block: return pick(0x0a3069, 0xa5d6ff);      // dark blue
		case Kind::Party: return pick(0x953800, 0xffa657);      // orange
		case Kind::Movement: return pick(0x6639ba, 0xd2a8ff);   // purple
		case Kind::Screen: return pick(0x99286e, 0xff9bce);     // pink
		case Kind::Audio: return pick(0x0f6e74, 0x56d4dd);      // teal
		case Kind::Scene: return pick(0x7d4e00, 0xe3b341);      // olive
		// RPG2003.EXE colors its comments green
		case Kind::Comment: return pick(0x116329, 0x7ee787);
		case Kind::Extension: return pick(0x57606a, 0x8b949e);  // gray
		case Kind::Text:
		case Kind::End:
		case Kind::Count:
			break;
	}
	return palette.color(QPalette::Text);
}

QColor stripeColor(const QPalette& palette) {
	// RPG Maker 2003's light blue-gray: the base mixed with a steel blue,
	// a little more on a dark base where the difference shows less
	const QColor base = palette.color(QPalette::Base);
	const QColor tint(70, 110, 170);
	const float amount = isDark(palette) ? 0.10f : 0.08f;
	auto mix = [amount](int from, int to) {
		return static_cast<int>(from + (to - from) * amount + 0.5f);
	};
	return QColor(mix(base.red(), tint.red()), mix(base.green(), tint.green()), mix(base.blue(), tint.blue()));
}

}

QColor commandColor(int code, const QPalette& palette) {
	return CommandColors::color(CommandColors::kind(code), palette);
}
