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

#include "map_tree_drop.h"
#include "model/map_tree_model.h"

std::optional<ShellController::MapMove> MapTreeDrop::move(const MapTreeModel& tree, int id, int target, Position position)
{
	if (id == target || !tree.contains(id) || !tree.contains(target))
		return std::nullopt;
	if (!(tree.flags(tree.indexOf(id)) & Qt::ItemIsDragEnabled))
		return std::nullopt;
	// beside the project: nowhere, it is the only node at the top
	const int parent = position == Position::On ? target : tree.parentId(target);
	if (parent < 0 || !(tree.flags(tree.indexOf(parent)) & Qt::ItemIsDropEnabled))
		return std::nullopt;
	// not into itself
	for (int above = parent; above > 0; above = tree.parentId(above)) {
		if (above == id)
			return std::nullopt;
	}
	// the row among the others, the map taken out first
	QList<int> siblings = tree.childIds(parent);
	siblings.removeOne(id);
	const int row = position == Position::On ? static_cast<int>(siblings.size())
		: static_cast<int>(siblings.indexOf(target)) + (position == Position::Below ? 1 : 0);
	return ShellController::MapMove{id, parent, row};
}
