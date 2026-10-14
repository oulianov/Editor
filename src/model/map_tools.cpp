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
#include "autotile.h"
#include <algorithm>

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

void resizeMap(lcf::rpg::Map& map, int width, int height) {
	width = std::max(width, 1);
	height = std::max(height, 1);
	const int old_width = map.width;
	const int old_height = map.height;
	auto resize = [&](std::vector<int16_t>& layer, int16_t empty) {
		std::vector<int16_t> resized(static_cast<size_t>(width * height), empty);
		for (int y = 0; y < std::min(height, old_height); ++y) {
			for (int x = 0; x < std::min(width, old_width); ++x) {
				const size_t from = static_cast<size_t>(y * old_width + x);
				if (from < layer.size()) {
					resized[static_cast<size_t>(y * width + x)] = layer[from];
				}
			}
		}
		layer = std::move(resized);
	};
	resize(map.lower_layer, 0);
	resize(map.upper_layer, 10000);
	map.width = width;
	map.height = height;
	std::erase_if(map.events, [&](const lcf::rpg::Event& event) {
		return event.x >= width || event.y >= height;
	});
}

std::vector<int16_t> generatorTiles(const lcf::rpg::Map& map) {
	if (map.generator_tile_ids.size() >= 18) {
		return map.generator_tile_ids;
	}
	return newMap().generator_tile_ids;
}

QRect generatorCells(const std::vector<int16_t>& tiles, int box) {
	if (box < 0 || box >= GeneratorBoxes || tiles.size() < 18) {
		return QRect();
	}
	if (box < ObstacleA) {
		const int terrain = std::clamp(Autotile::translate(static_cast<short>(tiles[static_cast<size_t>(box)])), 0, 161);
		return QRect(terrain % 6, terrain / 6, 1, 1);
	}
	// an obstacle of 1 x 1, 2 x 1 or 1 x 2 tiles leaves the other cells at 10000
	const size_t first = 6 + static_cast<size_t>(box - ObstacleA) * 4;
	const int tile = std::clamp(Autotile::translate(static_cast<short>(tiles[first])) - 162, 0, 143);
	const bool wide = tiles[first + 1] != 10000 || tiles[first + 3] != 10000;
	const bool tall = tiles[first + 2] != 10000 || tiles[first + 3] != 10000;
	return QRect(tile % 6, tile / 6, wide ? 2 : 1, tall ? 2 : 1);
}

void setGeneratorCells(std::vector<int16_t>& tiles, std::vector<uint32_t>& x, std::vector<uint32_t>& y,
		int box, const QRect& cells) {
	if (box < 0 || box >= GeneratorBoxes || cells.isEmpty()) {
		return;
	}
	if (tiles.size() < 18) {
		tiles = newMap().generator_tile_ids;
	}
	x.resize(GeneratorBoxes, 0);
	y.resize(GeneratorBoxes, 0);
	x[static_cast<size_t>(box)] = static_cast<uint32_t>(cells.x());
	y[static_cast<size_t>(box)] = static_cast<uint32_t>(cells.y());
	if (box < ObstacleA) {
		const int terrain = cells.y() * 6 + cells.x();
		// As RPG Maker stores them: autotiles with the pattern 49, which the
		// generator replaces by the borders of their neighbours
		int16_t tile;
		if (terrain < 3) {
			tile = static_cast<int16_t>(terrain * 1000 + 49);
		} else if (terrain < 6) {
			tile = static_cast<int16_t>(3000 + (terrain - 3) * 50);
		} else if (terrain < 18) {
			tile = static_cast<int16_t>(4000 + (terrain - 6) * 50 + 49);
		} else {
			tile = static_cast<int16_t>(5000 + terrain - 18);
		}
		tiles[static_cast<size_t>(box)] = tile;
		return;
	}
	const size_t first = 6 + static_cast<size_t>(box - ObstacleA) * 4;
	for (int i = 0; i < 4; i++) {
		const int dx = i % 2;
		const int dy = i / 2;
		const bool used = dx < std::min(cells.width(), 2) && dy < std::min(cells.height(), 2);
		tiles[first + static_cast<size_t>(i)] = static_cast<int16_t>(
			used ? 10000 + (cells.y() + dy) * 6 + cells.x() + dx : 10000);
	}
}

QList<int> paletteTiles(bool lower) {
	QList<int> ids;
	const int count = lower ? 162 : 144;
	ids.reserve(count);
	for (int i = 0; i < count; i++) {
		// the autotiles with the borders TilePainter::paletteTiles draws;
		// deep water amid shallow water, as the palette shows it
		ids.append(lower ? (i == 2 ? Autotile::translate(2, 0, 240) : Autotile::translate(i, 15))
			: Autotile::translate(i + 162));
	}
	return ids;
}

QList<int> generatorBoxTiles(const std::vector<int16_t>& tiles, int box) {
	const QRect cells = generatorCells(tiles, box);
	if (cells.isEmpty()) {
		return {};
	}
	const bool lower = box < ObstacleA;
	const QList<int> palette = paletteTiles(lower);
	auto at = [&](int x, int y) {
		const qsizetype i = static_cast<qsizetype>(y) * 6 + x;
		return x < 6 && i < palette.size() ? palette[i] : -1;
	};
	if (lower) {
		return {at(cells.x(), cells.y())};
	}
	QList<int> ids;
	for (int dy = 0; dy < 2; dy++) {
		for (int dx = 0; dx < 2; dx++) {
			ids.append(dx < cells.width() && dy < cells.height() ? at(cells.x() + dx, cells.y() + dy) : -1);
		}
	}
	return ids;
}

}
