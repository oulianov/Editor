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

void MapTreeModel::reset()
{
	setTreeMap(m_tree);
}

void MapTreeModel::setRootName(const QString& name)
{
	m_rootName = name;
	if (const QModelIndex root = indexOf(0); root.isValid())
		emit dataChanged(root, root);
}

void MapTreeModel::build()
{
	m_nodes.clear();
	m_top.children.clear();
	m_infoIndex.clear();
	if (!m_tree)
		return;

	// The project, then a node per map and area (the first entry is the
	// project's)
	auto project = std::make_unique<Node>();
	project->parent = &m_top;
	m_top.children.push_back(project.get());
	Node* root = project.get();
	m_nodes[0] = std::move(project);
	const auto& maps = m_tree->maps;
	for (size_t i = 1; i < maps.size(); i++) {
		const int id = maps[i].ID;
		auto& entry = m_nodes[id];
		if (!entry) {
			entry = std::make_unique<Node>();
			entry->id = id;
		}
	}
	// The parents, in the tree order
	for (size_t i = 1; i < m_tree->tree_order.size(); i++) {
		const int id = m_tree->tree_order[i];
		Node* child = node(id);
		const lcf::rpg::MapInfo* map = info(id);
		if (!child || !map || child->parent || id == 0)
			continue;
		// a map whose parent is gone (older versions left some) goes under
		// the project, as one that would hold its own parent
		Node* parent = node(map->parent_map);
		for (Node* above = parent; above && parent != root; above = above->parent) {
			if (above == child)
				parent = root;
		}
		parent = parent ? parent : root;
		parent->children.push_back(child);
		child->parent = parent;
	}
	// The maps missing from the tree order, by id
	std::vector<int> missing;
	for (const auto& [id, entry] : m_nodes) {
		if (id != 0 && !entry->parent)
			missing.push_back(id);
	}
	std::sort(missing.begin(), missing.end());
	for (int id : missing) {
		Node* child = node(id);
		root->children.push_back(child);
		child->parent = root;
	}
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
}
