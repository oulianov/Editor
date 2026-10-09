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

#include "undo_event.h"

UndoEvent::UndoEvent(int id, std::optional<lcf::rpg::Event> before, MapScene *scene) :
	m_id(id), m_data(std::move(before)), m_scene(scene)
{}

void UndoEvent::swap()
{
	std::optional<lcf::rpg::Event> current = m_scene->eventData(m_id);
	m_scene->setEventData(m_id, m_data);
	m_data = std::move(current);
}
