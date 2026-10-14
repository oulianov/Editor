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

#include "map_tree_forms.h"
#include "map_tree_dialogs.h"
#include "model/map_tools.h"
#include "ui/picker/pickers.h"
#include <QQuickItem>
#include <QRandomGenerator>
#include <algorithm>
#include <memory>

/*
 * Map Properties
 */

MapPropertiesForm::MapPropertiesForm(ProjectData& project, const lcf::rpg::MapInfo& info, const lcf::rpg::Map& map,
		QObject* parent) :
	QObject(parent),
	m_map(map),
	m_info(info),
	m_mapJson(new JsonT<lcf::rpg::Map>(this)),
	m_infoJson(new JsonT<lcf::rpg::MapInfo>(this)),
	m_encounters(new EncounterListModel(project, this)),
	m_oldWidth(map.width),
	m_oldHeight(map.height)
{
	m_mapJson->setData(&m_map);
	m_infoJson->setData(&m_info);
	m_encounters->setEncounters(m_info.encounters);
	// a new map has RPG Maker's defaults
	m_generatorTiles = MapTools::generatorTiles(m_map);
	m_generatorX = m_map.generator_x;
	m_generatorY = m_map.generator_y;
}

Json* MapPropertiesForm::map() const
{
	return m_mapJson;
}

Json* MapPropertiesForm::info() const
{
	return m_infoJson;
}

EncounterListModel* MapPropertiesForm::encounters() const
{
	return m_encounters;
}

int MapPropertiesForm::mapId() const
{
	return m_info.ID;
}

bool MapPropertiesForm::hasParent() const
{
	return m_info.parent_map != 0;
}

int MapPropertiesForm::oldWidth() const
{
	return m_oldWidth;
}

int MapPropertiesForm::oldHeight() const
{
	return m_oldHeight;
}

QList<int> MapPropertiesForm::generatorTiles(int box) const
{
	return MapTools::generatorBoxTiles(m_generatorTiles, box);
}

QString MapPropertiesForm::generatorCellsText(int box) const
{
	const QRect cells = MapTools::generatorCells(m_generatorTiles, box);
	return QStringLiteral("%1,%2 %3x%4").arg(cells.x()).arg(cells.y()).arg(cells.width()).arg(cells.height());
}

void MapPropertiesForm::pickGeneratorTiles(int box, QQuickItem* from)
{
	if (box < 0 || box >= MapTools::GeneratorBoxes)
		return;
	const bool obstacle = box >= MapTools::ObstacleA;
	// the chipset chosen in the window, which may not be the map's yet
	const auto cells = MapTreeDialogs::chipSelector(Pickers::windowOf(from), m_map.chipset_id, !obstacle,
		obstacle ? QSize(2, 2) : QSize(1, 1), MapTools::generatorCells(m_generatorTiles, box));
	if (!cells || cells->isEmpty())
		return;
	MapTools::setGeneratorCells(m_generatorTiles, m_generatorX, m_generatorY, box, *cells);
	m_generatorChanged = true;
	emit generatorChanged();
}

lcf::rpg::Map MapPropertiesForm::resultMap() const
{
	lcf::rpg::Map map = m_map;
	if (!map.parallax_flag) {
		map.parallax_name = lcf::DBString();
		map.parallax_loop_x = false;
		map.parallax_loop_y = false;
		map.parallax_auto_loop_x = false;
		map.parallax_auto_loop_y = false;
		map.parallax_sx = 0;
		map.parallax_sy = 0;
	}
	// only this mode shows them: other maps keep values outside 2..20 x 2..15
	if (map.generator_mode == lcf::rpg::Map::GeneratorMode_linked_rooms) {
		map.generator_width = std::clamp(map.generator_width, 2, 20);
		map.generator_height = std::clamp(map.generator_height, 2, 15);
	}
	if (m_generatorChanged) {
		map.generator_x = m_generatorX;
		map.generator_y = m_generatorY;
		map.generator_tile_ids = m_generatorTiles;
	}
	// the size typed: the layers and events follow
	const int width = map.width;
	const int height = map.height;
	if (width != m_oldWidth || height != m_oldHeight) {
		map.width = m_oldWidth;
		map.height = m_oldHeight;
		MapTools::resizeMap(map, width, height);
	}
	return map;
}

lcf::rpg::MapInfo MapPropertiesForm::resultInfo() const
{
	lcf::rpg::MapInfo info = m_info;
	info.encounters = m_encounters->encounters();
	return info;
}

/*
 * Area Properties
 */

AreaPropertiesForm::AreaPropertiesForm(ProjectData& project, const lcf::rpg::MapInfo& info, const QSize& map_size,
		QObject* parent) :
	QObject(parent),
	m_project(project),
	m_info(info),
	m_infoJson(new JsonT<lcf::rpg::MapInfo>(this)),
	m_encounters(new EncounterListModel(project, this)),
	m_mapSize(std::max(map_size.width(), 1), std::max(map_size.height(), 1))
{
	m_infoJson->setData(&m_info);
	m_encounters->setEncounters(m_info.encounters);
	const auto& rect = info.area_rect;
	m_area = QRect(static_cast<int>(rect.l), static_cast<int>(rect.t),
		static_cast<int>(rect.r) - static_cast<int>(rect.l), static_cast<int>(rect.b) - static_cast<int>(rect.t));
}

Json* AreaPropertiesForm::info() const
{
	return m_infoJson;
}

EncounterListModel* AreaPropertiesForm::encounters() const
{
	return m_encounters;
}

int AreaPropertiesForm::mapWidth() const
{
	return m_mapSize.width();
}

int AreaPropertiesForm::mapHeight() const
{
	return m_mapSize.height();
}

QRect AreaPropertiesForm::area() const
{
	return m_area;
}

void AreaPropertiesForm::setArea(const QRect& area)
{
	if (area == m_area)
		return;
	m_area = area;
	emit areaChanged();
}

QRect AreaPropertiesForm::defineBoundaries(const QRect& area, QQuickItem* from)
{
	const auto chosen = MapTreeDialogs::areaBoundaries(Pickers::windowOf(from), m_project, m_info.parent_map, area);
	return chosen ? *chosen : QRect();
}

lcf::rpg::MapInfo AreaPropertiesForm::resultInfo() const
{
	lcf::rpg::MapInfo info = m_info;
	// the area stays inside the map
	const int left = std::clamp(m_area.x(), 0, m_mapSize.width() - 1);
	const int top = std::clamp(m_area.y(), 0, m_mapSize.height() - 1);
	info.area_rect.l = static_cast<uint32_t>(left);
	info.area_rect.t = static_cast<uint32_t>(top);
	info.area_rect.r = static_cast<uint32_t>(left + std::clamp(m_area.width(), 1, m_mapSize.width() - left));
	info.area_rect.b = static_cast<uint32_t>(top + std::clamp(m_area.height(), 1, m_mapSize.height() - top));
	info.encounters = m_encounters->encounters();
	return info;
}

/*
 * Generate Dungeon
 */

DungeonForm::DungeonForm(ProjectData& project, const lcf::rpg::Map& source, QObject* parent) :
	QObject(parent),
	m_source(source),
	m_seed(QRandomGenerator::global()->generate()),
	m_preview(new MapDocument(project, 0, this))
{
	// help: "a new map, retaining the same settings"
	m_source.events.clear();
	m_source.save_count = 0;
	m_source.save_count_2k3e = 0;
}

MapDocument* DungeonForm::preview() const
{
	return m_preview;
}

int DungeonForm::sourceChipset() const
{
	return m_source.chipset_id;
}

int DungeonForm::sourceWidth() const
{
	return m_source.width;
}

int DungeonForm::sourceHeight() const
{
	return m_source.height;
}

void DungeonForm::setSeed(uint32_t seed)
{
	m_seed = seed;
}

void DungeonForm::reseed()
{
	m_seed = QRandomGenerator::global()->generate();
	if (m_settings.size() == 7)
		generate(m_settings[0], m_settings[1], m_settings[2], m_settings[3], m_settings[4], m_settings[5], m_settings[6]);
}

void DungeonForm::generate(int chipset_id, int width, int height, int entrance_x, int entrance_y,
	int exit_x, int exit_y)
{
	m_settings = {chipset_id, width, height, entrance_x, entrance_y, exit_x, exit_y};
	auto map = std::make_unique<lcf::rpg::Map>(m_source);
	map->chipset_id = chipset_id;
	map->width = std::clamp(width, 20, 500);
	map->height = std::clamp(height, 15, 500);
	// cells kept on the floor and joined to the rest, to put the teleports
	// in and out there afterwards; 0, 0 is in the border: none
	std::vector<DungeonGenerator::Point> keep;
	for (const auto& [x, y] : {std::pair{entrance_x, entrance_y}, std::pair{exit_x, exit_y}})
		if (x != 0 || y != 0)
			keep.push_back({std::clamp(x, 0, map->width - 1), std::clamp(y, 0, map->height - 1)});
	DungeonGenerator::generate(*map, m_seed, keep);
	m_preview->setMap(std::move(map));
}

const lcf::rpg::Map& DungeonForm::map() const
{
	return m_preview->map();
}
