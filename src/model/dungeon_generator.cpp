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

#include "dungeon_generator.h"
#include "core.h"
#include "defines.h"
#include "model/map_tools.h"
#include <algorithm>
#include <cstdlib>
#include <random>
#include <set>

namespace DungeonGenerator {

namespace {

constexpr int16_t no_upper_tile = 10000;

enum class Cell : uint8_t { Solid, UpperWall, LowerWall, Floor, Obstacle };

struct Rect {
	int x0, y0, x1, y1; // inclusive
};

/** The 18 tiles of the settings; RPG Maker writes them into every map */
std::vector<int16_t> tileIds(const lcf::rpg::Map& map) {
	if (map.generator_tile_ids.size() >= TileSlotCount)
		return map.generator_tile_ids;
	return MapTools::newMap().generator_tile_ids;
}

int wallHeight(const lcf::rpg::Map& map) {
	// "Use" upper wall: walls are 2 tiles high, else 1
	return map.generator_upper_wall ? 2 : 1;
}

/** Where the dungeon goes: inside the ceiling border and below the top walls */
Rect innerArea(const lcf::rpg::Map& map) {
	const int side = map.generator_surround ? 1 : 0;
	Rect r{side, wallHeight(map) + side, map.width - 1 - side, map.height - 1 - side};
	if (r.x1 < r.x0 || r.y1 < r.y0)
		r = {0, 0, std::max(map.width - 1, 0), std::max(map.height - 1, 0)};
	return r;
}

Point clampInside(const lcf::rpg::Map& map, Point p) {
	const Rect r = innerArea(map);
	return {std::clamp(p.x, r.x0, r.x1), std::clamp(p.y, r.y0, r.y1)};
}

class Builder {
public:
	Builder(const lcf::rpg::Map& map, uint32_t seed) :
		m_map(map), m_rng(seed), w(map.width), h(map.height),
		grain(map.generator_tiles == lcf::rpg::Map::GeneratorTiles_two ? 2 : 1),
		wall(wallHeight(map)), inner(innerArea(map)),
		cells(static_cast<size_t>(w * h), Cell::Solid)
	{
		// The passages run on a lattice: a node is grain x grain, a ceiling
		// column apart, and walls plus a ceiling row apart vertically
		px = grain + 1;
		py = grain + wall + 1;
		const int iw = inner.x1 - inner.x0 + 1;
		const int ih = inner.y1 - inner.y0 + 1;
		nx = std::max(1, (iw - grain) / px + 1);
		ny = std::max(1, (ih - grain) / py + 1);
		// centred: what is left over goes to both sides
		ox = inner.x0 + std::max(0, iw - ((nx - 1) * px + grain)) / 2;
		oy = inner.y0 + std::max(0, ih - ((ny - 1) * py + grain)) / 2;
	}

	void build(const std::vector<Point>& keep) {
		switch (m_map.generator_mode) {
		case lcf::rpg::Map::GeneratorMode_linked_rooms:
			rooms();
			break;
		case lcf::rpg::Map::GeneratorMode_maze_passage:
			passages(nx * ny, 0);
			break;
		case lcf::rpg::Map::GeneratorMode_open_room:
			carve(inner);
			break;
		default:
			// half of the lattice, mostly straight on: long winding passages
			passages(std::max(1, nx * ny / 2), 60);
			break;
		}
		// help: "a small square room will be created around any events"
		keepPoints(keep, m_map.generator_mode == lcf::rpg::Map::GeneratorMode_single_passage);
		fillThinBands();
		connectAll();
		fillThinBands();
		walls();
		if (m_map.generator_mode == lcf::rpg::Map::GeneratorMode_open_room)
			obstacles(keep);
	}

	void write(lcf::rpg::Map& map) {
		const std::vector<int16_t> ids = tileIds(map);
		map.lower_layer.assign(cells.size(), ids[Ceiling]);
		map.upper_layer.assign(cells.size(), no_upper_tile);

		// help: floors A, B and C come with the frequency ratio 4:2:1
		std::vector<std::pair<int16_t, int>> floors{{ids[FloorA], 4}};
		if (map.generator_floor_b)
			floors.push_back({ids[FloorB], 2});
		if (map.generator_floor_c)
			floors.push_back({ids[FloorC], 1});
		int floor_weight = 0;
		for (const auto& floor : floors)
			floor_weight += floor.second;

		for (size_t i = 0; i < cells.size(); i++) {
			switch (cells[i]) {
			case Cell::UpperWall:
				map.lower_layer[i] = ids[UpperWall];
				break;
			case Cell::LowerWall:
				map.lower_layer[i] = ids[LowerWall];
				break;
			case Cell::Floor:
			case Cell::Obstacle: {
				int pick = random(1, floor_weight);
				for (const auto& floor : floors) {
					pick -= floor.second;
					if (pick <= 0) {
						map.lower_layer[i] = floor.first;
						break;
					}
				}
				break;
			}
			default:
				break;
			}
		}

		for (const Placed& obstacle : placed) {
			for (int part = 0; part < 4; part++) {
				const int16_t id = ids[static_cast<size_t>(obstacle.slot + part)];
				const size_t i = index(obstacle.x + part % 2, obstacle.y + part / 2);
				if (id == no_upper_tile || i >= cells.size())
					continue;
				// a lower tile chosen as obstacle replaces the floor
				if (id >= no_upper_tile)
					map.upper_layer[i] = id;
				else
					map.lower_layer[i] = id;
			}
		}
	}

private:
	struct Placed {
		int x, y, slot;
	};

	const lcf::rpg::Map& m_map;
	std::mt19937 m_rng;
	const int w, h, grain, wall;
	const Rect inner;
	std::vector<Cell> cells;
	std::vector<Placed> placed;
	int px, py, nx, ny, ox, oy;

	/** lo..hi, the same on every platform (unlike the std distributions) */
	int random(int lo, int hi) {
		if (hi <= lo)
			return lo;
		return lo + static_cast<int>(m_rng() % static_cast<uint32_t>(hi - lo + 1));
	}

	size_t index(int x, int y) const {
		return static_cast<size_t>(x + y * w);
	}

	bool inside(int x, int y) const {
		return x >= 0 && y >= 0 && x < w && y < h;
	}

	Cell& at(int x, int y) {
		return cells[index(x, y)];
	}

	/** Floor in r; nothing goes into the border */
	void carve(Rect r) {
		r.x0 = std::max(r.x0, inner.x0);
		r.y0 = std::max(r.y0, inner.y0);
		r.x1 = std::min(r.x1, inner.x1);
		r.y1 = std::min(r.y1, inner.y1);
		for (int y = r.y0; y <= r.y1; y++)
			for (int x = r.x0; x <= r.x1; x++)
				at(x, y) = Cell::Floor;
	}

	Rect node(int n) const {
		const int x = ox + (n % nx) * px;
		const int y = oy + (n / nx) * py;
		return {x, y, x + grain - 1, y + grain - 1};
	}

	Point center(const Rect& r) const {
		return {(r.x0 + r.x1) / 2, (r.y0 + r.y1) / 2};
	}

	/** A passage grain wide: along a row then a column, or the other way */
	void corridor(Point a, Point b) {
		auto row = [&](int y, int x0, int x1) {
			carve({std::min(x0, x1), y, std::max(x0, x1) + grain - 1, y + grain - 1});
		};
		auto column = [&](int x, int y0, int y1) {
			carve({x, std::min(y0, y1), x + grain - 1, std::max(y0, y1) + grain - 1});
		};
		if (random(0, 1)) {
			row(a.y, a.x, b.x);
			column(b.x, a.y, b.y);
		} else {
			column(a.x, a.y, b.y);
			row(b.y, a.x, b.x);
		}
	}

	/**
	 * A random depth-first walk on the lattice, up to count nodes. It goes on
	 * straight with straight% chances, then turns; back at a fork when stuck.
	 * All the nodes (count = all): a maze, its forks and dead ends.
	 */
	void passages(int count, int straight) {
		const int total = nx * ny;
		std::vector<char> seen(static_cast<size_t>(total), 0);
		std::vector<std::pair<int, int>> stack; // node, direction it came from
		const int first = random(0, total - 1);
		seen[static_cast<size_t>(first)] = 1;
		carve(node(first));
		stack.push_back({first, -1});
		int visited = 1;
		const int dx[4] = {1, -1, 0, 0};
		const int dy[4] = {0, 0, 1, -1};
		while (!stack.empty() && visited < count) {
			const int n = stack.back().first;
			const int came = stack.back().second;
			int options[4];
			int option_count = 0;
			int ahead = -1;
			for (int d = 0; d < 4; d++) {
				const int x = n % nx + dx[d];
				const int y = n / nx + dy[d];
				if (x < 0 || y < 0 || x >= nx || y >= ny || seen[static_cast<size_t>(x + y * nx)])
					continue;
				options[option_count++] = d;
				if (d == came)
					ahead = d;
			}
			if (option_count == 0) {
				stack.pop_back();
				continue;
			}
			const int d = (ahead >= 0 && random(1, 100) <= straight)
				? ahead : options[random(0, option_count - 1)];
			const int next = n + dx[d] + dy[d] * nx;
			seen[static_cast<size_t>(next)] = 1;
			visited++;
			const Rect a = node(n);
			const Rect b = node(next);
			carve({std::min(a.x0, b.x0), std::min(a.y0, b.y0), std::max(a.x1, b.x1), std::max(a.y1, b.y1)});
			stack.push_back({next, d});
		}
	}

	/** Rooms of the "standard dimensions", joined by passages */
	void rooms() {
		const int iw = inner.x1 - inner.x0 + 1;
		const int ih = inner.y1 - inner.y0 + 1;
		const int room_w = std::clamp(m_map.generator_width, 2, std::max(2, iw));
		const int room_h = std::clamp(m_map.generator_height, 2, std::max(2, ih));
		const int target = std::max(2, iw * ih / ((room_w + 3) * (room_h + wall + 2) * 2));
		std::vector<Rect> list;
		for (int attempt = 0; attempt < target * 10 && static_cast<int>(list.size()) < target; attempt++) {
			// the standard size, give or take a quarter
			const int rw = std::clamp(room_w + random(-room_w / 4, room_w / 4), std::min(2, iw), iw);
			const int rh = std::clamp(room_h + random(-room_h / 4, room_h / 4), std::min(2, ih), ih);
			const int x = random(inner.x0, inner.x1 - rw + 1);
			const int y = random(inner.y0, inner.y1 - rh + 1);
			const Rect r{x, y, x + rw - 1, y + rh - 1};
			// apart: a ceiling column beside, walls and a ceiling row between
			bool apart = true;
			for (const Rect& other : list)
				if (r.x0 <= other.x1 + 2 && other.x0 <= r.x1 + 2
						&& r.y0 <= other.y1 + wall + 2 && other.y0 <= r.y1 + wall + 2)
					apart = false;
			if (apart)
				list.push_back(r);
		}
		for (const Rect& r : list)
			carve(r);

		// each room joins the closest one placed before it: all are linked
		for (size_t k = 1; k < list.size(); k++) {
			const Point a = center(list[k]);
			size_t best = 0;
			int best_distance = -1;
			for (size_t j = 0; j < k; j++) {
				const Point b = center(list[j]);
				const int distance = std::abs(a.x - b.x) + std::abs(a.y - b.y);
				if (best_distance < 0 || distance < best_distance) {
					best = j;
					best_distance = distance;
				}
			}
			corridor(a, center(list[best]));
		}
		// a few more passages make loops
		for (size_t k = 0; list.size() > 2 && k < list.size(); k++)
			if (random(0, 3) == 0)
				corridor(center(list[k]), center(list[static_cast<size_t>(random(0, static_cast<int>(list.size()) - 1))]));
	}

	bool nearestFloor(Point p, Point& found) const {
		int best = -1;
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				if (cells[index(x, y)] != Cell::Floor)
					continue;
				const int distance = std::abs(x - p.x) + std::abs(y - p.y);
				if (best < 0 || distance < best) {
					best = distance;
					found = {x, y};
				}
			}
		return best >= 0;
	}

	void keepPoints(const std::vector<Point>& keep, bool with_room) {
		for (Point p : keep) {
			p = clampInside(m_map, p);
			Point target;
			const bool joined = nearestFloor(p, target);
			if (with_room)
				carve({p.x - 1, p.y - 1, p.x + grain, p.y + grain});
			else
				carve({p.x, p.y, p.x, p.y});
			if (joined)
				corridor(p, target);
		}
	}

	/**
	 * Walls only face down: between two floors of a column, solid cells need
	 * the room for the walls and a ceiling row above them, else they become
	 * floor too (no one-tile pillars, no walls standing on a floor).
	 */
	void fillThinBands() {
		for (int x = 0; x < w; x++) {
			int last_floor = -1;
			for (int y = 0; y < h; y++) {
				if (at(x, y) != Cell::Floor)
					continue;
				const int gap = y - last_floor - 1;
				if (last_floor >= 0 && gap > 0 && gap < wall + 1)
					for (int fill = last_floor + 1; fill < y; fill++)
						at(x, fill) = Cell::Floor;
				last_floor = y;
			}
		}
	}

	/** Labels the 4-connected floor areas; returns how many */
	int label(std::vector<int>& labels, std::vector<int>& sizes) const {
		labels.assign(cells.size(), -1);
		sizes.clear();
		std::vector<int> todo;
		for (size_t start = 0; start < cells.size(); start++) {
			if (cells[start] != Cell::Floor || labels[start] >= 0)
				continue;
			const int id = static_cast<int>(sizes.size());
			sizes.push_back(0);
			labels[start] = id;
			todo.assign(1, static_cast<int>(start));
			while (!todo.empty()) {
				const int i = todo.back();
				todo.pop_back();
				sizes.back()++;
				const int x = i % w;
				const int y = i / w;
				const int nxs[4] = {x + 1, x - 1, x, x};
				const int nys[4] = {y, y, y + 1, y - 1};
				for (int d = 0; d < 4; d++) {
					if (!inside(nxs[d], nys[d]))
						continue;
					const size_t n = index(nxs[d], nys[d]);
					if (cells[n] == Cell::Floor && labels[n] < 0) {
						labels[n] = id;
						todo.push_back(static_cast<int>(n));
					}
				}
			}
		}
		return static_cast<int>(sizes.size());
	}

	/** What the layouts left apart (rooms, keep cells) is joined to the largest area */
	void connectAll() {
		std::vector<int> labels, sizes;
		for (int pass = 0; pass < 4 && label(labels, sizes) > 1; pass++) {
			const int main = static_cast<int>(std::max_element(sizes.begin(), sizes.end()) - sizes.begin());
			std::vector<char> joined(sizes.size(), 0);
			joined[static_cast<size_t>(main)] = 1;
			for (size_t i = 0; i < cells.size(); i++) {
				const int id = labels[i];
				if (id < 0 || joined[static_cast<size_t>(id)])
					continue;
				joined[static_cast<size_t>(id)] = 1;
				const Point a{static_cast<int>(i) % w, static_cast<int>(i) / w};
				Point b = a;
				int best = -1;
				for (size_t j = 0; j < cells.size(); j++) {
					if (labels[j] != main)
						continue;
					const Point c{static_cast<int>(j) % w, static_cast<int>(j) / w};
					const int distance = std::abs(a.x - c.x) + std::abs(a.y - c.y);
					if (best < 0 || distance < best) {
						best = distance;
						b = c;
					}
				}
				corridor(a, b);
			}
		}
	}

	void walls() {
		for (int y = 0; y + 1 < h; y++)
			for (int x = 0; x < w; x++) {
				if (at(x, y) != Cell::Solid || at(x, y + 1) != Cell::Floor)
					continue;
				at(x, y) = Cell::LowerWall;
				if (wall == 2 && y > 0 && at(x, y - 1) == Cell::Solid)
					at(x, y - 1) = Cell::UpperWall;
			}
	}

	/**
	 * Obstacles A, B (if used) and C (if used) with the same frequency, one
	 * for about 24 floor cells. Each keeps a free floor ring around it, so
	 * that they never close a way.
	 */
	void obstacles(const std::vector<Point>& keep) {
		const std::vector<int16_t> ids = tileIds(m_map);
		struct Kind {
			int slot, width, height;
		};
		std::vector<Kind> kinds;
		auto addKind = [&](int slot) {
			auto used = [&](int part) { return ids[static_cast<size_t>(slot + part)] != no_upper_tile; };
			if (!used(0) && !used(1) && !used(2) && !used(3))
				return;
			// 1x1, 2x1, 1x2 or 2x2: the parts that are not the empty tile
			kinds.push_back({slot, (used(1) || used(3)) ? 2 : 1, (used(2) || used(3)) ? 2 : 1});
		};
		addKind(ObstacleA);
		if (m_map.generator_extra_b)
			addKind(ObstacleB);
		if (m_map.generator_extra_c)
			addKind(ObstacleC);
		if (kinds.empty())
			return;

		std::vector<Point> kept;
		for (const Point& p : keep)
			kept.push_back(clampInside(m_map, p));
		const int floors = static_cast<int>(std::count(cells.begin(), cells.end(), Cell::Floor));
		const int target = floors / 24;
		int count = 0;
		for (int attempt = 0; attempt < target * 8 && count < target; attempt++) {
			const Kind& kind = kinds[static_cast<size_t>(random(0, static_cast<int>(kinds.size()) - 1))];
			const int x = random(inner.x0, inner.x1 - kind.width + 1);
			const int y = random(inner.y0, inner.y1 - kind.height + 1);
			bool free = true;
			for (int ry = y - 1; free && ry <= y + kind.height; ry++)
				for (int rx = x - 1; free && rx <= x + kind.width; rx++)
					free = inside(rx, ry) && at(rx, ry) == Cell::Floor;
			for (const Point& p : kept)
				if (p.x >= x && p.x < x + kind.width && p.y >= y && p.y < y + kind.height)
					free = false;
			if (!free)
				continue;
			for (int ry = y; ry < y + kind.height; ry++)
				for (int rx = x; rx < x + kind.width; rx++)
					at(rx, ry) = Cell::Obstacle;
			placed.push_back({x, y, kind.slot});
			count++;
		}
	}
};

/**
 * The autotile borders, with the rules of MapScene::bind: a D block joins
 * its own terrain, water joins water and animated tiles, the map edge is no
 * border.
 */
void bindLowerLayer(lcf::rpg::Map& map) {
	const int w = map.width;
	const int h = map.height;
	std::vector<int> terrain(map.lower_layer.size());
	for (size_t i = 0; i < terrain.size(); i++)
		terrain[i] = core().translate(map.lower_layer[i]);
	auto at = [&](int x, int y) {
		return x < 0 || y < 0 || x >= w || y >= h ? -1 : terrain[static_cast<size_t>(x + y * w)];
	};
	auto water = [](int t) { return core().isWater(t) || core().isAnimation(t); };

	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			const int t = at(x, y);
			const bool dblock = core().isDblock(t);
			if (!dblock && !water(t))
				continue;
			auto border = [&](int nx, int ny) {
				if (nx < 0 || ny < 0 || nx >= w || ny >= h)
					return false;
				return dblock ? at(nx, ny) != t : !water(at(nx, ny));
			};
			const int u = border(x, y - 1) ? UP : 0;
			const int d = border(x, y + 1) ? DOWN : 0;
			const int l = border(x - 1, y) ? LEFT : 0;
			const int r = border(x + 1, y) ? RIGHT : 0;
			const int ul = u + l == 0 && border(x - 1, y - 1) ? UPLEFT : 0;
			const int ur = u + r == 0 && border(x + 1, y - 1) ? UPRIGHT : 0;
			const int dl = d + l == 0 && border(x - 1, y + 1) ? DOWNLEFT : 0;
			const int dr = d + r == 0 && border(x + 1, y + 1) ? DOWNRIGHT : 0;
			int scode = 0;
			if (!dblock) {
				// the corners where deep and shallow water meet
				auto ab = [&](int nx, int ny) { return core().isABWater(at(nx, ny)); };
				auto deep = [&](int nx, int ny) { return core().isDWater(at(nx, ny)); };
				auto any = [&](int nx, int ny) { return core().isWater(at(nx, ny)); };
				if (core().isDWater(t)) {
					scode += ab(x, y - 1) && ab(x - 1, y) && ab(x - 1, y - 1) ? UPLEFT : 0;
					scode += ab(x, y - 1) && ab(x + 1, y) && ab(x + 1, y - 1) ? UPRIGHT : 0;
					scode += ab(x, y + 1) && ab(x - 1, y) && ab(x - 1, y + 1) ? DOWNRIGHT : 0;
					scode += ab(x, y + 1) && ab(x + 1, y) && ab(x + 1, y + 1) ? DOWNLEFT : 0;
				} else {
					scode += deep(x, y - 1) && deep(x - 1, y) && any(x - 1, y - 1) ? UPLEFT : 0;
					scode += deep(x, y - 1) && deep(x + 1, y) && any(x + 1, y - 1) ? UPRIGHT : 0;
					scode += deep(x, y + 1) && deep(x - 1, y) && any(x - 1, y + 1) ? DOWNRIGHT : 0;
					scode += deep(x, y + 1) && deep(x + 1, y) && any(x + 1, y + 1) ? DOWNLEFT : 0;
				}
			}
			map.lower_layer[static_cast<size_t>(x + y * w)] = core().translate(t, u + d + l + r + ul + ur + dl + dr, scode);
		}
}

}

void generate(lcf::rpg::Map& map, uint32_t seed, const std::vector<Point>& keep) {
	map.width = std::max(map.width, 1);
	map.height = std::max(map.height, 1);
	if (map.generator_tile_ids.size() < TileSlotCount)
		map.generator_tile_ids = tileIds(map);
	Builder builder(map, seed);
	builder.build(keep);
	builder.write(map);
	bindLowerLayer(map);
}

std::string check(const lcf::rpg::Map& map, const std::vector<Point>& keep) {
	const int w = map.width;
	const int h = map.height;
	const size_t size = static_cast<size_t>(std::max(w * h, 0));
	if (w <= 0 || h <= 0 || map.lower_layer.size() != size || map.upper_layer.size() != size)
		return "the layers do not have the size of the map";

	const std::vector<int16_t> ids = tileIds(map);
	const std::set<int> floors = {core().translate(ids[FloorA]), core().translate(ids[FloorB]), core().translate(ids[FloorC])};
	auto walkable = [&](int x, int y) {
		const size_t i = static_cast<size_t>(x + y * w);
		return floors.count(core().translate(map.lower_layer[i])) > 0 && map.upper_layer[i] == no_upper_tile;
	};

	std::string problems;
	auto report = [&](const std::string& text) {
		problems += (problems.empty() ? "" : "; ") + text;
	};
	auto cell = [](int x, int y) { return std::to_string(x) + "," + std::to_string(y); };

	// every walkable cell is reached from the first one
	std::vector<char> seen(size, 0);
	std::vector<Point> todo;
	int total = 0;
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++)
			if (walkable(x, y)) {
				if (total++ == 0) {
					todo.push_back({x, y});
					seen[static_cast<size_t>(x + y * w)] = 1;
				}
			}
	if (total == 0) {
		report("no floor");
		return problems;
	}
	const Point first = todo.front();
	int reached = 0;
	while (!todo.empty()) {
		const Point p = todo.back();
		todo.pop_back();
		reached++;
		const Point next[4] = {{p.x + 1, p.y}, {p.x - 1, p.y}, {p.x, p.y + 1}, {p.x, p.y - 1}};
		for (const Point& n : next) {
			if (n.x < 0 || n.y < 0 || n.x >= w || n.y >= h)
				continue;
			const size_t i = static_cast<size_t>(n.x + n.y * w);
			if (!seen[i] && walkable(n.x, n.y)) {
				seen[i] = 1;
				todo.push_back(n);
			}
		}
	}
	if (reached != total) {
		for (int i = 0; i < w * h; i++)
			if (!seen[static_cast<size_t>(i)] && walkable(i % w, i / w)) {
				report(std::to_string(total - reached) + " of " + std::to_string(total)
					+ " floor cells cannot be reached from " + cell(first.x, first.y) + ", e.g. " + cell(i % w, i / w));
				break;
			}
	}

	for (const Point& p : keep) {
		const Point q = clampInside(map, p);
		if (!walkable(q.x, q.y))
			report("kept cell " + cell(q.x, q.y) + " is not floor");
		else if (!seen[static_cast<size_t>(q.x + q.y * w)])
			report("kept cell " + cell(q.x, q.y) + " cannot be reached");
	}

	if (map.generator_surround) {
		const int ceiling = core().translate(ids[Ceiling]);
		for (int i = 0; i < w * h; i++) {
			const int x = i % w;
			const int y = i / w;
			if ((x == 0 || y == 0 || x == w - 1 || y == h - 1)
					&& core().translate(map.lower_layer[static_cast<size_t>(i)]) != ceiling) {
				report("border cell " + cell(x, y) + " is not ceiling");
				break;
			}
		}
	}
	return problems;
}

std::string toText(const lcf::rpg::Map& map) {
	const std::vector<int16_t> ids = tileIds(map);
	// the first match wins when two parts use the same tile
	const std::pair<int, char> parts[] = {
		{core().translate(ids[Ceiling]), '#'},
		{core().translate(ids[LowerWall]), 'w'},
		{core().translate(ids[UpperWall]), 'W'},
		{core().translate(ids[FloorA]), '.'},
		{core().translate(ids[FloorB]), 'b'},
		{core().translate(ids[FloorC]), 'c'},
	};
	std::string text;
	for (int y = 0; y < map.height; y++) {
		for (int x = 0; x < map.width; x++) {
			const size_t i = static_cast<size_t>(x + y * map.width);
			char c = '?';
			if (i < map.upper_layer.size() && map.upper_layer[i] != no_upper_tile) {
				c = 'o';
			} else if (i < map.lower_layer.size()) {
				const int terrain = core().translate(map.lower_layer[i]);
				for (const auto& part : parts)
					if (part.first == terrain) {
						c = part.second;
						break;
					}
			}
			text += c;
		}
		text += '\n';
	}
	return text;
}

}
