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

#include <lcf/rpg/eventpage.h>

/**
 * The pages of events as RPG Maker 2003's event editor makes them (read in
 * RPG2003.EXE). liblcf's defaults are the file's, for a field missing from
 * it: a page made by the editor differs (it stands still).
 */
namespace EventPageDefaults {

/** A new page: the event stands still; the rest is liblcf's */
inline lcf::rpg::EventPage newPage() {
	lcf::rpg::EventPage page;
	page.move_type = lcf::rpg::EventPage::MoveType_stationary;
	return page;
}

/**
 * New Page: the look and the movement of the page shown (graphic, trigger,
 * layer, animation, speed, move route...), new conditions and no commands
 */
inline lcf::rpg::EventPage nextPage(const lcf::rpg::EventPage& shown) {
	lcf::rpg::EventPage page = newPage();
	page.character_name = shown.character_name;
	page.character_index = shown.character_index;
	page.character_direction = shown.character_direction;
	page.character_pattern = shown.character_pattern;
	page.translucent = shown.translucent;
	page.move_type = shown.move_type;
	page.move_frequency = shown.move_frequency;
	page.trigger = shown.trigger;
	page.layer = shown.layer;
	page.overlap_forbidden = shown.overlap_forbidden;
	page.animation_type = shown.animation_type;
	page.move_speed = shown.move_speed;
	page.move_route = shown.move_route;
	return page;
}

/** The passability flags of a chipset (Chipset passable_data) */
enum PassableFlag { Down = 0x01, Left = 0x02, Right = 0x04, Up = 0x08, AllDirections = 0x0F, Above = 0x10 };

/**
 * The layer of an event shown as a tile of the upper layer, from the
 * passability of the tile in the chipset: a star is above the hero, a
 * blocking tile (x) on the hero's layer, a tile walked on (o) below it
 */
inline int tileLayer(int flags) {
	if (flags & Above) {
		return lcf::rpg::EventPage::Layers_above;
	}
	return (flags & AllDirections) == 0 ? lcf::rpg::EventPage::Layers_same : lcf::rpg::EventPage::Layers_below;
}

/**
 * The graphic of a page changed: between a character and a tile (or none),
 * RPG Maker moves the event between the hero's layer and below it, and
 * stops its animation (Non-Continuous). Above Hero stays. The same kind of
 * graphic changes nothing else
 */
inline void graphicChanged(lcf::rpg::EventPage& page, bool was_character) {
	const bool character = !page.character_name.empty();
	if (character == was_character) {
		return;
	}
	if (character && page.layer == lcf::rpg::EventPage::Layers_below) {
		page.layer = lcf::rpg::EventPage::Layers_same;
	} else if (!character && page.layer == lcf::rpg::EventPage::Layers_same
			&& page.character_index == 0) {
		// no graphic (a tile has the layer of its passability: tileLayer)
		page.layer = lcf::rpg::EventPage::Layers_below;
	}
	page.animation_type = lcf::rpg::EventPage::AnimType_non_continuous;
}

}
