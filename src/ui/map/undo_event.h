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

#include <optional>
#include <lcf/rpg/event.h>
#include "undo_draw.h"

/** An event added, changed or removed */
class UndoEvent : public SwapCommand
{
public:
	/** before: the event as it was, none when it was added */
	UndoEvent(int id, std::optional<lcf::rpg::Event> before, MapScene *scene);

protected:
	void swap() override;

private:
	int m_id;
	std::optional<lcf::rpg::Event> m_data;
	MapScene* m_scene;
};
