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

#include "map_tree_model.h"
#include <algorithm>
#include <set>
#include "common/dbstring.h"
#include "model/database_list_data.h"

using lcf::rpg::TreeMap;

MapTreeModel::MapTreeModel(QObject* parent) :
	QAbstractItemModel(parent)
{
}

MapTreeModel::~MapTreeModel() = default;

void MapTreeModel::setTreeMap(lcf::rpg::TreeMap* tree)
{
	beginResetModel();
	m_tree = tree;
	build();
	endResetModel();
}

lcf::rpg::TreeMap* MapTreeModel::treeMap() const
{
	return m_tree;
}

void MapTreeModel::setRootName(const QString& name)
{
	m_rootName = name;
	if (const QModelIndex root = indexOf(0); root.isValid())
		emit dataChanged(root, root);
}

std::unordered_map<int, std::vector<int>> MapTreeModel::layout() const
{
	std::unordered_map<int, std::vector<int>> children;
	if (!m_tree)
		return children;
	children[0];
	// a node per map and area (the first entry is the project's)
	std::set<int> ids;
	const auto& maps = m_tree->maps;
	for (size_t i = 1; i < maps.size(); i++) {
		if (maps[i].ID != 0)
			ids.insert(maps[i].ID);
	}
	// the parent of each node placed
	std::unordered_map<int, int> parents;
	auto place = [&](int id, int parent) {
		children[parent].push_back(id);
		parents[id] = parent;
	};
	// The parents, in the tree order
	for (size_t i = 1; i < m_tree->tree_order.size(); i++) {
		const int id = m_tree->tree_order[i];
		const lcf::rpg::MapInfo* map = info(id);
		if (!ids.count(id) || !map || parents.count(id))
			continue;
		// a map whose parent is gone (older versions left some) goes under
		// the project, as one that would hold its own parent
		int parent = ids.count(map->parent_map) ? map->parent_map : 0;
		for (int above = parent; above != 0;) {
			if (above == id) {
				parent = 0;
				break;
			}
			auto placed = parents.find(above);
			if (placed == parents.end())
				break;
			above = placed->second;
		}
		place(id, parent);
	}
	// The maps missing from the tree order, by id
	for (int id : ids) {
		if (!parents.count(id))
			place(id, 0);
	}
	return children;
}

void MapTreeModel::build()
{
	m_nodes.clear();
	m_top.children.clear();
	m_infoIndex.clear();
	if (!m_tree)
		return;

	// The project, then the maps and areas under it
	const auto children = layout();
	auto project = std::make_unique<Node>();
	project->parent = &m_top;
	m_top.children.push_back(project.get());
	m_nodes[0] = std::move(project);
	std::vector<Node*> todo{node(0)};
	while (!todo.empty()) {
		Node* above = todo.back();
		todo.pop_back();
		auto found = children.find(above->id);
		if (found == children.end())
			continue;
		for (int id : found->second) {
			auto child = std::make_unique<Node>();
			child->id = id;
			child->parent = above;
			above->children.push_back(child.get());
			todo.push_back(child.get());
			m_nodes[id] = std::move(child);
		}
	}
}

QHash<int, int> MapTreeModel::update(const lcf::rpg::TreeMap& before)
{
	QHash<int, int> removed;
	if (!m_tree || !node(0)) {
		setTreeMap(m_tree);
		return removed;
	}
	m_infoIndex.clear();
	const auto wanted = layout();
	// the parent of each node of the new tree
	std::unordered_map<int, int> parents{{0, -1}};
	for (const auto& [parent, ids] : wanted) {
		for (int id : ids)
			parents[id] = parent;
	}
	auto wantedChildren = [&wanted](int id) {
		static const std::vector<int> none;
		auto found = wanted.find(id);
		return found == wanted.end() ? none : found->second;
	};
	// The ids removed, with the nearest node above them that stays
	std::set<int> old_ids;
	for (const auto& [id, entry] : m_nodes) {
		old_ids.insert(id);
		if (parents.count(id))
			continue;
		const Node* above = entry->parent;
		while (above && !isTop(above) && !parents.count(above->id))
			above = above->parent;
		removed.insert(id, above && !isTop(above) ? above->id : 0);
	}

	// Under each node that stays, the children that stay in their order
	// (the longest run of them in both orders) keep their rows; the others
	// go, with what they hold. A node under another parent, or in another
	// place, comes back below: the views move expanded nodes badly, and
	// show them again as they had them.
	std::vector<Node*> todo{node(0)};
	while (!todo.empty()) {
		Node* above = todo.back();
		todo.pop_back();
		auto& children = above->children;
		const std::vector<int>& ids = wantedChildren(above->id);
		const size_t n = children.size(), m = ids.size();
		std::vector<std::vector<int>> common(n + 1, std::vector<int>(m + 1, 0));
		for (size_t i = n; i-- > 0;) {
			for (size_t j = m; j-- > 0;) {
				common[i][j] = children[i]->id == ids[j] ? common[i + 1][j + 1] + 1
					: std::max(common[i + 1][j], common[i][j + 1]);
			}
		}
		std::vector<bool> keep(n, false);
		for (size_t i = 0, j = 0; i < n && j < m;) {
			if (children[i]->id == ids[j]) {
				keep[i++] = true;
				j++;
			} else if (common[i + 1][j] >= common[i][j + 1]) {
				i++;
			} else {
				j++;
			}
		}
		const QModelIndex at = indexOf(above);
		for (size_t i = n; i-- > 0;) {
			if (keep[i]) {
				todo.push_back(children[i]);
				continue;
			}
			QList<int> gone;
			collect(children[i], gone, true);
			const int row = static_cast<int>(i);
			beginRemoveRows(at, row, row);
			children.erase(children.begin() + row);
			for (int id : gone)
				m_nodes.erase(id);
			endRemoveRows();
		}
	}
	// The nodes missing, in their places, parents first
	std::vector<int> added;
	std::vector<int> walk{0};
	while (!walk.empty()) {
		Node* above = node(walk.back());
		walk.pop_back();
		const std::vector<int>& ids = wantedChildren(above->id);
		const QModelIndex at = indexOf(above);
		for (size_t row = 0; row < ids.size(); row++) {
			auto& children = above->children;
			if (row < children.size() && children[row]->id == ids[row])
				continue;
			if (node(ids[row])) {
				// not expected: the tree as read, from the start
				setTreeMap(m_tree);
				return removed;
			}
			beginInsertRows(at, static_cast<int>(row), static_cast<int>(row));
			auto entry = std::make_unique<Node>();
			entry->id = ids[row];
			entry->parent = above;
			children.insert(children.begin() + row, entry.get());
			m_nodes[ids[row]] = std::move(entry);
			endInsertRows();
			if (!old_ids.count(ids[row]))
				added.push_back(ids[row]);
		}
		walk.insert(walk.end(), ids.rbegin(), ids.rend());
	}

	// The names and types changed (what the rows show expanded stays the
	// views'; the views mix up the rows of a change of several rows), a
	// new node expanded as its data says
	for (const auto& map : before.maps) {
		const lcf::rpg::MapInfo* now = map.ID > 0 && node(map.ID) ? info(map.ID) : nullptr;
		if (now && (now->name != map.name || now->type != map.type)) {
			const QModelIndex changed = indexOf(map.ID);
			emit dataChanged(changed, changed, {Qt::DisplayRole, NameRole, TypeRole});
		}
	}
	for (int id : added) {
		const lcf::rpg::MapInfo* map = info(id);
		if (const QModelIndex changed = indexOf(id); map && map->expanded_node)
			emit dataChanged(changed, changed, {ExpandedRole});
	}
	namesChanged();
	return removed;
}

MapTreeModel::Node* MapTreeModel::node(int id) const
{
	auto it = m_nodes.find(id);
	return it == m_nodes.end() ? nullptr : it->second.get();
}

bool MapTreeModel::isTop(const Node* node) const
{
	return node == &m_top;
}

int MapTreeModel::rowOf(const Node* node) const
{
	if (!node || !node->parent)
		return -1;
	const auto& siblings = node->parent->children;
	auto it = std::find(siblings.begin(), siblings.end(), node);
	return it == siblings.end() ? -1 : static_cast<int>(it - siblings.begin());
}

QModelIndex MapTreeModel::indexOf(const Node* node) const
{
	if (!node || isTop(node))
		return {};
	const int row = rowOf(node);
	return row < 0 ? QModelIndex() : createIndex(row, 0, const_cast<Node*>(node));
}

lcf::rpg::MapInfo* MapTreeModel::info(int id) const
{
	if (!m_tree)
		return nullptr;
	auto& maps = m_tree->maps;
	auto cached = m_infoIndex.constFind(id);
	if (cached != m_infoIndex.cend() && *cached < static_cast<int>(maps.size()) && maps[*cached].ID == id)
		return &maps[*cached];
	// moved by a change of the tree: read the positions again (the first
	// entry of an id is the one used)
	m_infoIndex.clear();
	for (int i = static_cast<int>(maps.size()) - 1; i >= 0; i--)
		m_infoIndex.insert(maps[i].ID, i);
	cached = m_infoIndex.constFind(id);
	return cached == m_infoIndex.cend() ? nullptr : &maps[*cached];
}

QModelIndex MapTreeModel::index(int row, int column, const QModelIndex& parent) const
{
	if (column != 0 || row < 0 || (parent.isValid() && parent.column() != 0))
		return {};
	const Node* above = parent.isValid() ? static_cast<const Node*>(parent.internalPointer()) : &m_top;
	if (row >= static_cast<int>(above->children.size()))
		return {};
	return createIndex(row, 0, above->children[row]);
}

QModelIndex MapTreeModel::parent(const QModelIndex& child) const
{
	if (!child.isValid())
		return {};
	const Node* node = static_cast<const Node*>(child.internalPointer());
	return indexOf(node->parent);
}

int MapTreeModel::rowCount(const QModelIndex& parent) const
{
	if (parent.column() > 0)
		return 0;
	const Node* above = parent.isValid() ? static_cast<const Node*>(parent.internalPointer()) : &m_top;
	return static_cast<int>(above->children.size());
}

int MapTreeModel::columnCount(const QModelIndex& parent) const
{
	return parent.column() > 0 ? 0 : 1;
}

QVariant MapTreeModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.column() != 0)
		return {};
	const int id = static_cast<const Node*>(index.internalPointer())->id;
	const lcf::rpg::MapInfo* map = info(id);
	switch (role) {
	case Qt::DisplayRole:
	case Qt::EditRole:
	case NameRole:
		if (id == 0)
			return m_rootName;
		return map ? ToQString(map->name) : QString();
	case IdRole:
		return id;
	case TypeRole:
		return type(id);
	case ExpandedRole:
		return map ? map->expanded_node : false;
	default:
		return {};
	}
}

bool MapTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	const int id = idOf(index);
	if (id < 0)
		return false;
	switch (role) {
	case Qt::EditRole:
	case NameRole:
		return rename(id, value.toString());
	case ExpandedRole:
		setExpanded(id, value.toBool());
		return true;
	default:
		return false;
	}
}

Qt::ItemFlags MapTreeModel::flags(const QModelIndex& index) const
{
	if (!index.isValid())
		return Qt::NoItemFlags;
	Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	switch (type(idOf(index))) {
	case TreeMap::MapType_root:
		// the project stays where it is
		return flags | Qt::ItemIsDropEnabled;
	case TreeMap::MapType_map:
		return flags | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
	default:
		return flags;
	}
}

QHash<int, QByteArray> MapTreeModel::roleNames() const
{
	auto names = QAbstractItemModel::roleNames();
	names.insert(NameRole, "name");
	names.insert(IdRole, "mapId");
	names.insert(TypeRole, "mapType");
	names.insert(ExpandedRole, "expanded");
	return names;
}

bool MapTreeModel::contains(int id) const
{
	return node(id) != nullptr;
}

QModelIndex MapTreeModel::indexOf(int id) const
{
	return indexOf(node(id));
}

int MapTreeModel::idOf(const QModelIndex& index) const
{
	if (!index.isValid() || index.model() != this)
		return -1;
	return static_cast<const Node*>(index.internalPointer())->id;
}

int MapTreeModel::type(int id) const
{
	if (!m_tree || id < 0)
		return -1;
	if (id == 0)
		return TreeMap::MapType_root;
	const lcf::rpg::MapInfo* map = info(id);
	return map ? map->type : -1;
}

int MapTreeModel::parentId(int id) const
{
	const Node* child = node(id);
	return child && child->parent && !isTop(child->parent) ? child->parent->id : -1;
}

QList<int> MapTreeModel::childIds(int id) const
{
	QList<int> ids;
	if (const Node* above = node(id)) {
		for (const Node* child : above->children)
			ids.append(child->id);
	}
	return ids;
}

QList<int> MapTreeModel::subtreeIds(int id) const
{
	QList<int> ids;
	collect(node(id), ids, true);
	return ids;
}

QList<int> MapTreeModel::ids() const
{
	QList<int> ids;
	collect(node(0), ids, false);
	return ids;
}

const lcf::rpg::MapInfo* MapTreeModel::mapInfo(int id) const
{
	return info(id);
}

void MapTreeModel::collect(const Node* node, QList<int>& ids, bool children_first) const
{
	if (!node)
		return;
	if (!children_first)
		ids.append(node->id);
	for (const Node* child : node->children)
		collect(child, ids, children_first);
	if (children_first)
		ids.append(node->id);
}

int MapTreeModel::freeMapId(const std::function<bool(int)>& in_use) const
{
	std::set<int> used;
	if (m_tree) {
		for (const auto& map : m_tree->maps)
			used.insert(map.ID);
	}
	int id = 1;
	while (used.count(id) || (in_use && in_use(id)))
		id++;
	return id;
}

QModelIndex MapTreeModel::insertMapInfo(const lcf::rpg::MapInfo& info, int parent)
{
	if (!m_tree || info.ID <= 0 || contains(info.ID))
		return {};
	Node* above = node(parent);
	above = above ? above : node(0);
	const int row = static_cast<int>(above->children.size());
	beginInsertRows(indexOf(above), row, row);
	auto& maps = m_tree->maps;
	auto pos = std::lower_bound(maps.begin(), maps.end(), info.ID,
		[](const lcf::rpg::MapInfo& other, int id) { return other.ID < id; });
	maps.insert(pos, info);
	auto entry = std::make_unique<Node>();
	entry->id = info.ID;
	entry->parent = above;
	above->children.push_back(entry.get());
	m_nodes[info.ID] = std::move(entry);
	endInsertRows();
	syncOrder();
	// the parent shows its new child
	setExpanded(above->id, true);
	return indexOf(info.ID);
}

QList<int> MapTreeModel::removeMap(int id)
{
	Node* removed = node(id);
	if (!removed || id == 0)
		return {};
	QList<int> ids;
	collect(removed, ids, true);
	Node* above = removed->parent;
	const int row = rowOf(removed);
	beginRemoveRows(indexOf(above), row, row);
	above->children.erase(above->children.begin() + row);
	const std::set<int> gone(ids.cbegin(), ids.cend());
	auto& maps = m_tree->maps;
	maps.erase(std::remove_if(maps.begin(), maps.end(),
		[&gone](const lcf::rpg::MapInfo& map) { return gone.count(map.ID) > 0; }), maps.end());
	auto& order = m_tree->tree_order;
	order.erase(std::remove_if(order.begin(), order.end(),
		[&gone](int32_t entry) { return gone.count(entry) > 0; }), order.end());
	for (int gone_id : ids)
		m_nodes.erase(gone_id);
	m_infoIndex.clear();
	endRemoveRows();
	syncOrder();
	namesChanged();
	return ids;
}

bool MapTreeModel::moveMap(int id, int new_parent, int row)
{
	Node* moved = node(id);
	Node* to = node(new_parent);
	if (!moved || !to || id == 0 || type(id) != TreeMap::MapType_map || type(new_parent) == TreeMap::MapType_area)
		return false;
	for (const Node* above = to; above; above = above->parent) {
		if (above == moved)
			return false;
	}
	Node* from = moved->parent;
	const int from_row = rowOf(moved);
	const int count = static_cast<int>(to->children.size()) - (from == to ? 1 : 0);
	if (row < 0 || row > count)
		row = count;
	if (from != to || row != from_row) {
		// Qt counts the destination row before the move
		const int destination = from == to && row > from_row ? row + 1 : row;
		if (beginMoveRows(indexOf(from), from_row, from_row, indexOf(to), destination)) {
			from->children.erase(from->children.begin() + from_row);
			to->children.insert(to->children.begin() + row, moved);
			moved->parent = to;
			endMoveRows();
		}
	}
	return syncOrder();
}

bool MapTreeModel::rename(int id, const QString& name)
{
	lcf::rpg::MapInfo* map = id > 0 ? info(id) : nullptr;
	if (!map || ToQString(map->name) == name)
		return false;
	map->name = ToDBString(name);
	entryChanged(id);
	return true;
}

bool MapTreeModel::setMapInfo(int id, const lcf::rpg::MapInfo& changed)
{
	lcf::rpg::MapInfo* map = id > 0 ? info(id) : nullptr;
	if (!map)
		return false;
	*map = changed;
	map->ID = id;
	entryChanged(id);
	return true;
}

bool MapTreeModel::setExpanded(int id, bool expanded)
{
	lcf::rpg::MapInfo* map = info(id);
	if (!map || map->expanded_node == expanded)
		return false;
	map->expanded_node = expanded;
	if (const QModelIndex changed = indexOf(id); changed.isValid())
		emit dataChanged(changed, changed, {ExpandedRole});
	return true;
}

bool MapTreeModel::syncOrder()
{
	if (!m_tree || !node(0))
		return false;
	bool changed = false;
	std::vector<int32_t> order;
	std::function<void(const Node*, int)> walk = [&](const Node* at, int depth) {
		order.push_back(at->id);
		if (at->id != 0) {
			if (lcf::rpg::MapInfo* map = info(at->id)) {
				const int parent = at->parent ? at->parent->id : 0;
				changed = changed || map->parent_map != parent || map->indentation != depth;
				map->parent_map = parent;
				map->indentation = depth;
			}
		}
		for (const Node* child : at->children)
			walk(child, depth + 1);
	};
	walk(node(0), 0);
	changed = changed || order != m_tree->tree_order;
	m_tree->tree_order = std::move(order);
	return changed;
}

void MapTreeModel::entryChanged(int id)
{
	if (const QModelIndex changed = indexOf(id); changed.isValid())
		emit dataChanged(changed, changed);
	namesChanged();
}

void MapTreeModel::namesChanged()
{
	if (m_tree)
		DatabaseListNotifier::notify(&m_tree->maps);
}
