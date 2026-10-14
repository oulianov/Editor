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

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <optional>
#include <lcf/rpg/map.h>
#include <lcf/rpg/mapinfo.h>
#include "controller/shell_dialogs.h"

class ProjectData;
class QWindow;

/**
 * The windows of the map tree's actions in QML, modal over a window: they
 * edit copies and give the data chosen, nothing when cancelled. The
 * ShellDialogs of the interfaces open them.
 */
namespace MapTreeDialogs {
	/** Map Properties (MapPropertiesWindow.qml), of a new map too: info and
	 *  map are written on OK; false when cancelled */
	bool mapProperties(QWindow* over, ProjectData& project, lcf::rpg::MapInfo& info, lcf::rpg::Map& map);

	/** Create Area and Area Properties (AreaPropertiesWindow.qml), on a map
	 *  of map_size cells */
	bool areaProperties(QWindow* over, ProjectData& project, lcf::rpg::MapInfo& info, const QSize& map_size);

	/** Generate Dungeon (GenerateDungeonWindow.qml): a new map from source,
	 *  with its name; seed: a fixed one (tests), else random */
	std::optional<ShellDialogs::Dungeon> generateDungeon(QWindow* over, ProjectData& project,
		const lcf::rpg::Map& source, const QString& name, std::optional<uint32_t> seed = std::nullopt);

	/** Shift Map (ShiftMapWindow.qml): by how many cells */
	std::optional<QPoint> shiftMap(QWindow* over);

	/** Define Area Boundaries (AreaBoundariesWindow.qml): a box dragged on
	 *  a map, starting with area (empty: none) */
	std::optional<QRect> areaBoundaries(QWindow* over, ProjectData& project, int map_id, const QRect& area);

	/** Select Tile (ChipSelectorWindow.qml): cells of the palette of a
	 *  layer of a chipset (6 a row), a block up to max_size */
	std::optional<QRect> chipSelector(QWindow* over, int chipset_id, bool lower, const QSize& max_size,
		const QRect& current = QRect());
}
