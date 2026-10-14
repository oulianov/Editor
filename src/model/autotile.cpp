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

#include "autotile.h"
#include "defines.h"

namespace Autotile {

namespace {

// The place of each combination of borders in a block of autotiles
const struct {
	int code;
	short tile;
} borders[] = {
	{UPLEFT, 1},
	{UPRIGHT, 2},
	{UPLEFT+UPRIGHT, 3},
	{DOWNRIGHT, 4},
	{UPLEFT+DOWNRIGHT, 5},
	{UPRIGHT+DOWNRIGHT, 6},
	{UPLEFT+UPRIGHT+DOWNRIGHT, 7},
	{DOWNLEFT, 8},
	{DOWNLEFT+UPLEFT, 9},
	{DOWNLEFT+UPRIGHT, 10},
	{DOWNLEFT+UPLEFT+UPRIGHT, 11},
	{DOWNLEFT+DOWNRIGHT, 12},
	{DOWNLEFT+DOWNRIGHT+UPLEFT, 13},
	{DOWNLEFT+DOWNRIGHT+UPRIGHT, 14},
	{DOWNLEFT+DOWNRIGHT+UPLEFT+UPRIGHT, 15},
	{LEFT, 16},
	{LEFT+UPRIGHT, 17},
	{LEFT+DOWNRIGHT, 18},
	{LEFT+UPRIGHT+DOWNRIGHT, 19},
	{UP, 20},
	{UP+DOWNRIGHT, 21},
	{UP+DOWNLEFT, 22},
	{UP+DOWNRIGHT+DOWNLEFT, 23},
	{RIGHT, 24},
	{RIGHT+DOWNLEFT, 25},
	{RIGHT+UPLEFT, 26},
	{RIGHT+UPLEFT+DOWNLEFT, 27},
	{DOWN, 28},
	{DOWN+UPLEFT, 29},
	{DOWN+UPRIGHT, 30},
	{DOWN+UPLEFT+UPRIGHT, 31},
	{LEFT+RIGHT, 32},
	{UP+DOWN, 33},
	{UP+LEFT, 34},
	{UP+LEFT+DOWNRIGHT, 35},
	{UP+RIGHT, 36},
	{UP+RIGHT+DOWNLEFT, 37},
	{DOWN+RIGHT, 38},
	{DOWN+RIGHT+UPLEFT, 39},
	{DOWN+LEFT, 40},
	{DOWN+LEFT+UPRIGHT, 41},
	{UP+LEFT+RIGHT, 42},
	{UP+LEFT+DOWN, 43},
	{LEFT+RIGHT+DOWN, 44},
	{UP+RIGHT+DOWN, 45},
	{UP+DOWN+LEFT+RIGHT, 46},
	{SAMPLE, 47},
};

// 0 for no borders, and for a combination that is not in a block
short dictionary(int code) {
	static const auto table = [] {
		std::array<short, SAMPLE + 1> tiles{};
		for (const auto& border : borders)
			tiles[static_cast<size_t>(border.code)] = border.tile;
		return tiles;
	}();
	return code >= 0 && code <= SAMPLE ? table[static_cast<size_t>(code)] : 0;
}

}

bool isWater(int terrain_id)
{
	return terrain_id >= 0 && terrain_id <= 2;
}

bool isABWater(int terrain_id)
{
	return terrain_id == 0 || terrain_id == 1;
}

bool isDWater(int terrain_id)
{
	return terrain_id == 2;
}

bool isAnimation(int terrain_id)
{
	return terrain_id >= 3 && terrain_id <= 5;
}

bool isDblock(int terrain_id)
{
	return terrain_id >= 6 && terrain_id <= 17;
}

bool isEblock(int terrain_id)
{
	return terrain_id >= 18 && terrain_id <= 161;
}

bool isFblock(int terrain_id)
{
	return terrain_id >= 162 && terrain_id <= 305;
}

short translate(int terrain_id, int code, int scode)
{
	if (terrain_id < 0)
		return 0x7FFF;
	if (isWater(terrain_id))
		return (static_cast<short>(terrain_id)*1000+dictionary(code)+dictionary(scode)*50);
	// animated tiles have no borders: RPG Maker writes 3000, 3050, 3100
	if (isAnimation(terrain_id))
		return (3000+(static_cast<short>(terrain_id)-3)*50);
	if (isDblock(terrain_id))
		return (4000+(static_cast<short>(terrain_id)-6)*50+dictionary(code));
	if (isEblock(terrain_id))
		return (5000+static_cast<short>(terrain_id)-18);
	if (isFblock(terrain_id))
		return (10000+static_cast<short>(terrain_id)-162);
	return 0x7FFF;
}

int translate(short tile_id)
{
	if (tile_id < 0)
		return -1;
	else if (tile_id < 3000)
		return tile_id/1000;
	else if (tile_id >= 3000 && tile_id < 3150)
		return (tile_id-3000)/50+3;
	else if (tile_id >= 4000 && tile_id < 4600)
		return (tile_id-4000)/50+6;
	else if (tile_id >= 5000 && tile_id < 5144)
		return tile_id-5000+18;
	else if (tile_id >= 10000 && tile_id < 10144)
		// the inverse of translate(int): F block terrains start at 162
		return tile_id-10000+162;
	else
		return -1;
}

bool isAnimatedTile(short tile_id)
{
	return tile_id >= 0 && tile_id < 3150;
}

int animatedTileFrame(unsigned tick)
{
	return static_cast<int>(tick % 4);
}

int waterFrame(const lcf::rpg::Chipset& chipset, unsigned tick)
{
	int water_frame = static_cast<int>(tick / (chipset.animation_speed ? 2 : 4));
	if (chipset.animation_type == lcf::rpg::Chipset::AnimType_cyclic) {
		water_frame %= 3;
	} else {
		water_frame %= 4;
		if (water_frame == 3)
			water_frame = 1;
	}
	return water_frame;
}

short bind(const std::array<int, 9>& around)
{
	// the terrain of the neighbour at dx, dy (-1 to 1)
	auto at = [&](int dx, int dy) { return around[static_cast<size_t>((dy + 1) * 3 + dx + 1)]; };
	const int terrain_id = at(0, 0);
	int code = 0, scode = 0;
	auto sides_and_corners = [&](auto border) {
		const int u = border(0, -1) ? UP : 0;
		const int d = border(0, 1) ? DOWN : 0;
		const int l = border(-1, 0) ? LEFT : 0;
		const int r = border(1, 0) ? RIGHT : 0;
		// a corner only between two sides without a border
		const int ul = u + l == 0 && border(-1, -1) ? UPLEFT : 0;
		const int ur = u + r == 0 && border(1, -1) ? UPRIGHT : 0;
		const int dl = d + l == 0 && border(-1, 1) ? DOWNLEFT : 0;
		const int dr = d + r == 0 && border(1, 1) ? DOWNRIGHT : 0;
		return u + d + l + r + ul + ur + dl + dr;
	};
	if (isDblock(terrain_id))
	{
		code = sides_and_corners([&](int dx, int dy) {
			const int terrain = at(dx, dy);
			return terrain != Outside && terrain != terrain_id;
		});
	}
	else if (isWater(terrain_id) || isAnimation(terrain_id))
	{
		code = sides_and_corners([&](int dx, int dy) {
			const int terrain = at(dx, dy);
			return terrain != Outside && !isWater(terrain) && !isAnimation(terrain);
		});
		// the corners where deep and shallow water meet (Outside is no water)
		if (isDWater(terrain_id))
		{
			auto ab = [&](int dx, int dy) { return isABWater(at(dx, dy)); };
			scode += ab(0, -1) && ab(-1, 0) && ab(-1, -1) ? UPLEFT : 0;
			scode += ab(0, -1) && ab(1, 0) && ab(1, -1) ? UPRIGHT : 0;
			scode += ab(0, 1) && ab(-1, 0) && ab(-1, 1) ? DOWNRIGHT : 0;
			scode += ab(0, 1) && ab(1, 0) && ab(1, 1) ? DOWNLEFT : 0;
		}
		else
		{
			auto deep = [&](int dx, int dy) { return isDWater(at(dx, dy)); };
			auto any = [&](int dx, int dy) { return isWater(at(dx, dy)); };
			scode += deep(0, -1) && deep(-1, 0) && any(-1, -1) ? UPLEFT : 0;
			scode += deep(0, -1) && deep(1, 0) && any(1, -1) ? UPRIGHT : 0;
			scode += deep(0, 1) && deep(-1, 0) && any(-1, 1) ? DOWNRIGHT : 0;
			scode += deep(0, 1) && deep(1, 0) && any(1, 1) ? DOWNLEFT : 0;
		}
	}
	return translate(terrain_id, code, scode);
}

short bind(const std::vector<int16_t>& lower_layer, int width, int height, int x, int y)
{
	std::array<int, 9> around;
	for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++) {
			const int nx = x + dx;
			const int ny = y + dy;
			around[static_cast<size_t>((dy + 1) * 3 + dx + 1)] = nx < 0 || ny < 0 || nx >= width || ny >= height
				? Outside : translate(lower_layer[static_cast<size_t>(nx + ny * width)]);
		}
	// qualified: std::bind would take a std::array
	return Autotile::bind(around);
}

}
