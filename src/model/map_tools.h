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

#include <QList>
#include <QRect>
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

/**
 * Map Properties' new size: the tiles keep their cells, new cells are
 * empty (lower tile 0, upper 10000), and the cells and events past the new
 * edges are deleted.
 */
void resizeMap(lcf::rpg::Map& map, int width, int height);

/*
 * The tiles of the Random Dungeon Generator in Map Properties: nine boxes
 * (ceiling, lower wall, upper wall, floors A to C, obstacles A to C), each
 * picked as cells of the palette (6 per row) and stored in
 * Map::generator_tile_ids as RPG Maker stores them
 */

enum GeneratorBox { Ceiling, LowerWall, UpperWall, FloorA, FloorB, FloorC, ObstacleA, ObstacleB, ObstacleC, GeneratorBoxes };

/** The tile ids of a map, RPG Maker's defaults when it has none */
std::vector<int16_t> generatorTiles(const lcf::rpg::Map& map);
/** The palette cells of a box: one lower tile, or up to 2 x 2 upper tiles */
QRect generatorCells(const std::vector<int16_t>& tiles, int box);
/** Sets the tiles of a box from palette cells; x and y are the cells of
 *  the boxes (Map::generator_x, generator_y, GeneratorBoxes long) */
void setGeneratorCells(std::vector<int16_t>& tiles, std::vector<uint32_t>& x, std::vector<uint32_t>& y,
	int box, const QRect& cells);

/** The tile ids of the cells of the palette of a layer (lower: 162, upper:
 *  144, 6 per row), drawn as the palette shows them; deep water surrounded
 *  by water */
QList<int> paletteTiles(bool lower);
/** The tile ids drawn in the cells of a box: one, or 2 x 2 (-1: empty) */
QList<int> generatorBoxTiles(const std::vector<int16_t>& tiles, int box);

}
