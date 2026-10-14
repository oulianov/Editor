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

#include <array>
#include <cstdint>
#include <vector>
#include <lcf/rpg/chipset.h>

/**
 * RPG Maker's terrains and autotiles, the same for every chipset. A terrain
 * is a cell of the palette: 0-2 the water (A, B and deep), 3-5 the animated
 * tiles, 6-17 the ground autotiles (D block), 18-161 the other lower tiles
 * (E block), 162-305 the upper tiles (F block). A tile id, as maps store it,
 * is a terrain with the borders drawn against its neighbours.
 */
namespace Autotile {

/** Past the edge of the map, in bind(): no border there */
inline constexpr int Outside = -2;

bool isWater(int terrain_id);
bool isABWater(int terrain_id);
bool isDWater(int terrain_id);
bool isAnimation(int terrain_id);
bool isDblock(int terrain_id);
bool isEblock(int terrain_id);
bool isFblock(int terrain_id);

/**
 * The tile id of a terrain with borders: code is a sum of the binding
 * constants of defines.h (UP, DOWNLEFT..., or SAMPLE for the palette's),
 * scode the corners where deep and shallow water meet. 0x7FFF for no
 * terrain.
 */
short translate(int terrain_id, int code = 0, int scode = 0);
/** The terrain of a tile id, -1 for none */
int translate(short tile_id);

/** Water and animated tiles (lower tiles below 3150) change with the frame */
bool isAnimatedTile(short tile_id);
/**
 * The frames shown at a tick of the animation (a tenth of a second), as the
 * Player (TilemapLayer::Draw): the animated tiles have 4 frames (0-3); the
 * water 3 (0-2), changing every 12 (fast) or 24 (slow) frames of the
 * Player, 1-2-3-2 or 1-2-3 as the chipset says.
 */
int animatedTileFrame(unsigned tick);
int waterFrame(const lcf::rpg::Chipset& chipset, unsigned tick);

/**
 * The tile of a cell of the lower layer: its terrain with the borders
 * RPG Maker draws. A D block joins its own terrain, water joins water and
 * the animated tiles, the edge of the map is no border. around: the
 * terrains of the cell and of its 8 neighbours, row by row from the upper
 * left one (around[4] is the cell), Outside past the edge of the map.
 */
short bind(const std::array<int, 9>& around);
/** bind() for the cell x, y of a lower layer of width x height tiles */
short bind(const std::vector<int16_t>& lower_layer, int width, int height, int x, int y);

}
