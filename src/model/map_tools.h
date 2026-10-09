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

#include <lcf/rpg/map.h>

namespace MapTools {

/** An empty 20x15 map, as RPG Maker creates it (no template file needed) */
lcf::rpg::Map newMap();

/**
 * Moves the tiles and the events of the map by dx, dy cells. What leaves
 * one side comes back on the other side (RPG Maker's "Shift Map").
 */
void shiftMap(lcf::rpg::Map& map, int dx, int dy);

/** x modulo size, always in 0..size-1 */
int wrap(int x, int size);

}
