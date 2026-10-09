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

#include "undo_draw.h"

UndoDraw::UndoDraw(Core::Layer layer, std::vector<short> before, MapScene *scene) :
	m_layer(layer), m_data(std::move(before)), m_scene(scene)
{}

void UndoDraw::swap()
{
	std::vector<short> current = m_scene->layerData(m_layer);
	m_scene->setLayerData(m_layer, m_data);
	m_data = std::move(current);
}

UndoMap::UndoMap(const lcf::rpg::Map &before, const lcf::rpg::MapInfo &info_before, MapScene *scene) :
	m_map(before), m_info(info_before), m_scene(scene)
{}

void UndoMap::swap()
{
	lcf::rpg::Map map = m_scene->mapData();
	lcf::rpg::MapInfo info = m_scene->mapInfo();
	m_scene->setMapData(m_map, m_info);
	m_map = std::move(map);
	m_info = std::move(info);
}

UndoStart::UndoStart(const lcf::rpg::Start &before, MapScene *scene) :
	m_start(before), m_scene(scene)
{}

void UndoStart::swap()
{
	lcf::rpg::Start current = m_scene->startData();
	m_scene->setStartData(m_start);
	m_start = current;
}
