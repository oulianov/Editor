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

#include "map_tools.h"

namespace MapTools {

int wrap(int x, int size) {
	if (size <= 0) {
		return 0;
	}
	x %= size;
	return x < 0 ? x + size : x;
}

lcf::rpg::Map newMap() {
	lcf::rpg::Map map;
	const size_t cells = static_cast<size_t>(map.width * map.height);
	// lower tile 0 is water A, upper tile 10000 is the transparent chip
	map.lower_layer.assign(cells, 0);
	map.upper_layer.assign(cells, 10000);
	map.top_level = true;
	// the dungeon generator settings RPG Maker writes into a new map
	map.generator_height = 2;
	map.generator_x.assign(9, 0);
	map.generator_y.assign(9, 0);
	map.generator_tile_ids = {49, 49, 49, 49, 49, 49,
		10000, 10001, 10006, 10007,
		10000, 10001, 10006, 10007,
		10000, 10001, 10006, 10007};
	return map;
}

void shiftMap(lcf::rpg::Map& map, int dx, int dy) {
	const int w = map.width;
	const int h = map.height;
	if (w <= 0 || h <= 0) {
		return;
	}

	auto rotate = [&](std::vector<int16_t>& layer) {
		if (layer.size() != static_cast<size_t>(w * h)) {
			return;
		}
		std::vector<int16_t> shifted(layer.size());
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				shifted[wrap(x + dx, w) + wrap(y + dy, h) * w] = layer[x + y * w];
			}
		}
		layer = std::move(shifted);
	};
	rotate(map.lower_layer);
	rotate(map.upper_layer);

	// Events stay on the scenery they were placed on
	for (auto& event : map.events) {
		event.x = wrap(event.x + dx, w);
		event.y = wrap(event.y + dy, h);
	}
}

}
