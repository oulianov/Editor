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
#include "controller/shell_controller.h"

class MapTreeModel;

/**
 * Where a map dragged in the map tree goes when it is dropped on a node:
 * above or below it, among its siblings, or on it, as its last child. Only
 * maps move (with what they hold); the project and the areas stay, and an
 * area holds nothing (MapTreeModel::flags). The views of the tree (the
 * QML one, the automation's drop-map) move the maps through
 * ShellController::moveMaps with it.
 */
namespace MapTreeDrop {
	enum class Position { Above, Below, On };

	/** The move of the map id dropped at target; none when the tree refuses it */
	std::optional<ShellController::MapMove> move(const MapTreeModel& tree, int id, int target, Position position);
}
