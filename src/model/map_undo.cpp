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

#include "map_undo.h"

UndoDraw::UndoDraw(MapDocument::Layer layer, std::vector<short> before, MapDocument *document) :
	m_layer(layer), m_data(std::move(before)), m_document(document)
{}

void UndoDraw::swap()
{
	std::vector<short> current = m_document->layerData(m_layer);
	m_document->setLayerData(m_layer, m_data);
	m_data = std::move(current);
}

UndoMap::UndoMap(const lcf::rpg::Map &before, const lcf::rpg::MapInfo &info_before, MapDocument *document) :
	m_map(before), m_info(info_before), m_document(document)
{}

void UndoMap::swap()
{
	lcf::rpg::Map map = m_document->map();
	lcf::rpg::MapInfo info = m_document->mapInfo();
	m_document->setMapData(m_map, m_info);
	m_map = std::move(map);
	m_info = std::move(info);
}

UndoStart::UndoStart(const lcf::rpg::Start &before, MapDocument *document) :
	m_start(before), m_document(document)
{}

void UndoStart::swap()
{
	lcf::rpg::Start current = m_document->startData();
	m_document->setStartData(m_start);
	m_start = current;
}

UndoEvent::UndoEvent(int id, std::optional<lcf::rpg::Event> before, MapDocument *document) :
	m_id(id), m_data(std::move(before)), m_document(document)
{}

void UndoEvent::swap()
{
	std::optional<lcf::rpg::Event> current = m_document->eventData(m_id);
	m_document->setEventData(m_id, m_data);
	m_data = std::move(current);
}
