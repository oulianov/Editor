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

#include <cstdint>
#include <string>
#include <vector>
#include <lcf/rpg/map.h>

/**
 * RPG Maker 2003's Random Dungeon Generator: fills the layers of a map with
 * a dungeon built from the generator settings of the map (Map Properties).
 */
namespace DungeonGenerator {

/**
 * The tiles in Map::generator_tile_ids. An obstacle has 4 tiles (top left,
 * top right, bottom left, bottom right); 10000 marks an unused part.
 */
enum TileSlot {
	Ceiling = 0,
	LowerWall = 1,
	UpperWall = 2,
	FloorA = 3,
	FloorB = 4,
	FloorC = 5,
	ObstacleA = 6,
	ObstacleB = 10,
	ObstacleC = 14,
	TileSlotCount = 18
};

struct Point {
	int x = 0;
	int y = 0;
};

/**
 * Replaces the lower and upper layers of the map (map.width x map.height)
 * by a new dungeon. The cells in keep stay walkable and reachable (an
 * entrance, events); those on the border are moved inside it. The same map,
 * seed and keep always give the same dungeon.
 * Binds the autotiles with core().translate(): no chipset is needed.
 */
void generate(lcf::rpg::Map& map, uint32_t seed, const std::vector<Point>& keep = {});

/**
 * What is wrong with a generated map: floor cells that cannot be reached
 * from the others, a keep cell that is not walkable, a hole in the border.
 * Empty if nothing.
 */
std::string check(const lcf::rpg::Map& map, const std::vector<Point>& keep = {});

/** The map as text, a line per row: # ceiling, W w walls, . b c floors, o obstacle */
std::string toText(const lcf::rpg::Map& map);

}
