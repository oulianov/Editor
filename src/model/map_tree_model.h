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

#include <QAbstractItemModel>
#include <QHash>
#include <QList>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <lcf/rpg/mapinfo.h>
#include <lcf/rpg/treemap.h>

/**
 * The map tree of a project (its lcf::rpg::TreeMap) as an item model: one
 * top-level row, the project, holding the maps and their areas as RPG Maker
 * shows them. Every change of the tree's structure goes through it, which
 * keeps the order, the parents and the depths of the map infos as the tree
 * shows them (tree_order lists the nodes depth first, parents before
 * children).
 *
 * The TreeMap stays the project's: when it is replaced (read again, merged),
 * update() reads it again. Reading and writing its file is the controller's.
 */
class MapTreeModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	enum Role {
		NameRole = Qt::UserRole + 1,
		IdRole,
		/** lcf::rpg::TreeMap::MapType: root (the project), map or area */
		TypeRole,
		/** The node shows its children (expanded_node, saved in the tree) */
		ExpandedRole,
	};
	Q_ENUM(Role)

	explicit MapTreeModel(QObject* parent = nullptr);
	~MapTreeModel() override;

	/** The tree shown, nullptr for none; it is read at once */
	void setTreeMap(lcf::rpg::TreeMap* tree);
	lcf::rpg::TreeMap* treeMap() const;
	/** Reads the tree again, after it was replaced (read again from its
	 *  file, merged): row by row, as the tree changed. The rows kept stay,
	 *  so do the views' expanded nodes and scroll; a new node is shown
	 *  expanded when its data says so (ExpandedRole changes), the others
	 *  as the views had them (the data's expanded flags are not shown
	 *  again). before: the tree data it replaced, for the names and types
	 *  changed. The ids removed, each with the nearest node above it that
	 *  stays. */
	QHash<int, int> update(const lcf::rpg::TreeMap& before);
	/** The name of the project row (the game title) */
	void setRootName(const QString& name);

	// QAbstractItemModel
	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& child) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	/** EditRole and NameRole rename, ExpandedRole expands */
	bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
	/** Maps can be dragged; maps and the project take drops; an area
	 *  belongs to its map: it is not moved and holds nothing */
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	QHash<int, QByteArray> roleNames() const override;

	/*
	 * The nodes, by id (0: the project)
	 */

	bool contains(int id) const;
	QModelIndex indexOf(int id) const;
	/** The id of a row, -1 for none */
	int idOf(const QModelIndex& index) const;
	/** TreeMap::MapType of a node, -1 if there is none */
	int type(int id) const;
	/** The id of the parent of a node, -1 for none (and for the project) */
	int parentId(int id) const;
	QList<int> childIds(int id) const;
	/** The ids of a node and of everything under it, children first */
	QList<int> subtreeIds(int id) const;
	/** The ids of the tree, depth first (the project first) */
	QList<int> ids() const;
	/** The map info of a node, nullptr for none */
	const lcf::rpg::MapInfo* mapInfo(int id) const;

	/*
	 * Changes
	 */

	/** RPG Maker takes the first free id, maps and areas share them; in_use
	 *  tells about an id taken elsewhere (a map file without its entry) */
	int freeMapId(const std::function<bool(int)>& in_use = {}) const;
	/** Adds a map or an area as the last child of parent (the project when
	 *  parent is not in the tree), which shows its children. The map infos
	 *  stay sorted by id: the Player looks the maps up by id. Its index. */
	QModelIndex insertMapInfo(const lcf::rpg::MapInfo& info, int parent);
	/** Removes a node, with its children and their areas; the ids removed,
	 *  children first */
	QList<int> removeMap(int id);
	/** Moves a map (with what it holds) under new_parent, a map or the
	 *  project, at row (counted without it; -1: last). true when the tree
	 *  data changed. */
	bool moveMap(int id, int new_parent, int row = -1);
	bool rename(int id, const QString& name);
	/** Replaces the map info of a node (Map Properties), its place kept */
	bool setMapInfo(int id, const lcf::rpg::MapInfo& info);
	/** true when it changed */
	bool setExpanded(int id, bool expanded);
	/** Writes the order, the parents and the depths of the tree into the
	 *  tree data (tree_order, parent_map, indentation); true when they
	 *  changed (a file not in the order of its tree, a parent missing) */
	bool syncOrder();
	/** The map info of a node was changed in the tree data by someone
	 *  else (a map saving its entry): the views show it again */
	void entryChanged(int id);

private:
	struct Node {
		int id = 0;
		Node* parent = nullptr;
		std::vector<Node*> children;
	};

	void build();
	/** The children of the nodes of the tree data, by id (0: the
	 *  project), as the tree shows them */
	std::unordered_map<int, std::vector<int>> layout() const;
	Node* node(int id) const;
	/** The tree has no other root than the project row */
	bool isTop(const Node* node) const;
	int rowOf(const Node* node) const;
	QModelIndex indexOf(const Node* node) const;
	lcf::rpg::MapInfo* info(int id) const;
	void collect(const Node* node, QList<int>& ids, bool children_first) const;

	lcf::rpg::TreeMap* m_tree = nullptr;
	QString m_rootName;
	// the invisible root, holding the project row
	Node m_top;
	std::unordered_map<int, std::unique_ptr<Node>> m_nodes;
	// where the map infos are in the tree, by id (checked on use: the tree
	// is changed by others too)
	mutable QHash<int, int> m_infoIndex;
};
