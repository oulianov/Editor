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

// The map tree as an item model, on a tree in memory: how a file's tree is
// read, and adding, removing and moving maps, with Qt's model checks
// watching every change. Reading and writing the file is the controller's
// (tests/scripts/map_tree.txt).

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>
#include "common/dbstring.h"
#include "model/map_tree_model.h"

using lcf::rpg::TreeMap;

namespace {
lcf::rpg::MapInfo entry(int id, const char* name, int parent, int type = TreeMap::MapType_map)
{
	lcf::rpg::MapInfo info;
	info.ID = id;
	info.name = lcf::DBString(name);
	info.parent_map = parent;
	info.type = type;
	return info;
}
}

class MapTreeModelTest : public QObject
{
	Q_OBJECT

private slots:
	void init();
	void readsTheTree();
	void freeMapId();
	void insert();
	void removeWithChildren();
	void move();
	void moveRefused();
	void syncOrder();
	void renameAndExpand();
	void reset();

private:
	/** The ids of the children of a node, as the model shows them */
	QList<int> shown(int id) const;

	// the project, Town (1) holding House (2) and its area Garden (3),
	// Forest (4); Cave (5) is missing from the order, Ruins (6) has lost
	// its parent
	TreeMap m_tree;
	std::unique_ptr<MapTreeModel> m_model;
	std::unique_ptr<QAbstractItemModelTester> m_tester;
};

void MapTreeModelTest::init()
{
	m_tree = TreeMap();
	auto root = entry(0, "", 0, TreeMap::MapType_root);
	m_tree.maps = {root, entry(1, "Town", 0), entry(2, "House", 1), entry(3, "Garden", 2, TreeMap::MapType_area),
		entry(4, "Forest", 0), entry(5, "Cave", 4), entry(6, "Ruins", 42)};
	m_tree.tree_order = {0, 1, 2, 3, 4, 6};
	m_tester.reset();
	m_model = std::make_unique<MapTreeModel>();
	m_model->setTreeMap(&m_tree);
	m_model->setRootName(QStringLiteral("My Game"));
	m_tester = std::make_unique<QAbstractItemModelTester>(m_model.get(), QAbstractItemModelTester::FailureReportingMode::QtTest);
}

QList<int> MapTreeModelTest::shown(int id) const
{
	QList<int> ids;
	const QModelIndex parent = m_model->indexOf(id);
	for (int row = 0; row < m_model->rowCount(parent); row++)
		ids << m_model->index(row, 0, parent).data(MapTreeModel::IdRole).toInt();
	return ids;
}

void MapTreeModelTest::readsTheTree()
{
	// one top-level row: the project
	QCOMPARE(m_model->rowCount(), 1);
	const QModelIndex project = m_model->index(0, 0);
	QCOMPARE(project.data().toString(), QStringLiteral("My Game"));
	QCOMPARE(project.data(MapTreeModel::TypeRole).toInt(), int(TreeMap::MapType_root));
	// in the tree order; a lost parent and a map missing from the order go
	// under the project, the missing one last
	QCOMPARE(shown(0), QList<int>({1, 4, 6, 5}));
	QCOMPARE(shown(1), QList<int>({2}));
	QCOMPARE(shown(2), QList<int>({3}));
	QCOMPARE(m_model->indexOf(3).data().toString(), QStringLiteral("Garden"));
	QCOMPARE(m_model->type(3), int(TreeMap::MapType_area));
	QCOMPARE(m_model->type(0), int(TreeMap::MapType_root));
	QCOMPARE(m_model->type(42), -1);
	QCOMPARE(m_model->parentId(3), 2);
	QCOMPARE(m_model->parentId(0), -1);
	QCOMPARE(m_model->ids(), QList<int>({0, 1, 2, 3, 4, 6, 5}));
	// what can be dragged and where: maps only, on maps and the project
	QVERIFY(m_model->flags(m_model->indexOf(1)) & Qt::ItemIsDragEnabled);
	QVERIFY(!(m_model->flags(m_model->indexOf(0)) & Qt::ItemIsDragEnabled));
	QVERIFY(m_model->flags(m_model->indexOf(0)) & Qt::ItemIsDropEnabled);
	QVERIFY(!(m_model->flags(m_model->indexOf(3)) & (Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled)));
}

void MapTreeModelTest::freeMapId()
{
	QCOMPARE(m_model->freeMapId(), 7);
	m_tree.maps.erase(m_tree.maps.begin() + 2); // House (2)
	QCOMPARE(m_model->freeMapId(), 2);
	// a map file without its entry keeps its id
	QCOMPARE(m_model->freeMapId([](int id) { return id == 2 || id == 7; }), 8);
}

void MapTreeModelTest::insert()
{
	QSignalSpy inserted(m_model.get(), &QAbstractItemModel::rowsInserted);
	const QModelIndex index = m_model->insertMapInfo(entry(7, "Mill", 0), 4);
	QVERIFY(index.isValid());
	QCOMPARE(inserted.count(), 1);
	QCOMPARE(index.parent(), m_model->indexOf(4));
	QCOMPARE(shown(4), QList<int>({7}));
	// the infos sorted by id, the tree order and the depths as the tree shows them
	QCOMPARE(m_tree.maps.back().ID, 7);
	QCOMPARE(m_tree.tree_order, std::vector<int32_t>({0, 1, 2, 3, 4, 7, 6, 5}));
	QCOMPARE(m_model->mapInfo(7)->parent_map, 4);
	QCOMPARE(m_model->mapInfo(7)->indentation, 2);
	// the parent shows its new child
	QVERIFY(m_model->mapInfo(4)->expanded_node);
	QVERIFY(m_model->indexOf(4).data(MapTreeModel::ExpandedRole).toBool());

	// an area, the ids kept sorted
	m_tree.maps.erase(m_tree.maps.begin() + 2);
	m_model->reset();
	QVERIFY(m_model->insertMapInfo(entry(2, "Yard", 1, TreeMap::MapType_area), 1).isValid());
	QCOMPARE(m_tree.maps[2].ID, 2);
	QCOMPARE(m_model->type(2), int(TreeMap::MapType_area));
	// a taken id is refused
	QVERIFY(!m_model->insertMapInfo(entry(4, "Again", 0), 0).isValid());
	// an unknown parent: the project
	QVERIFY(m_model->insertMapInfo(entry(8, "Lost", 0), 99).isValid());
	QCOMPARE(m_model->parentId(8), 0);
}

void MapTreeModelTest::removeWithChildren()
{
	QSignalSpy removed(m_model.get(), &QAbstractItemModel::rowsRemoved);
	// Town with House and its area: children first
	QCOMPARE(m_model->removeMap(1), QList<int>({3, 2, 1}));
	QCOMPARE(removed.count(), 1);
	QCOMPARE(shown(0), QList<int>({4, 6, 5}));
	for (int id : {1, 2, 3}) {
		QVERIFY(!m_model->contains(id));
		QVERIFY(!m_model->mapInfo(id));
	}
	QCOMPARE(m_tree.maps.size(), size_t(4));
	QCOMPARE(m_tree.tree_order, std::vector<int32_t>({0, 4, 6, 5}));
	// the project stays
	QVERIFY(m_model->removeMap(0).isEmpty());
	QVERIFY(m_model->removeMap(42).isEmpty());
}

void MapTreeModelTest::move()
{
	QSignalSpy moved(m_model.get(), &QAbstractItemModel::rowsMoved);
	// House (with its area) under Forest
	QVERIFY(m_model->moveMap(2, 4));
	QCOMPARE(moved.count(), 1);
	QCOMPARE(shown(1), QList<int>());
	QCOMPARE(shown(4), QList<int>({2}));
	QCOMPARE(shown(2), QList<int>({3}));
	QCOMPARE(m_model->mapInfo(2)->parent_map, 4);
	QCOMPARE(m_model->mapInfo(2)->indentation, 2);
	QCOMPARE(m_model->mapInfo(3)->indentation, 3);
	QCOMPARE(m_tree.tree_order, std::vector<int32_t>({0, 1, 4, 2, 3, 6, 5}));

	// among its siblings: Cave first, then down after Ruins (rows counted
	// without the map moved)
	QVERIFY(m_model->moveMap(5, 0, 0));
	QCOMPARE(shown(0), QList<int>({5, 1, 4, 6}));
	QVERIFY(m_model->moveMap(5, 0, 2));
	QCOMPARE(shown(0), QList<int>({1, 4, 5, 6}));
	QVERIFY(m_model->moveMap(5, 0, -1));
	QCOMPARE(shown(0), QList<int>({1, 4, 6, 5}));
	QCOMPARE(moved.count(), 4);
	// where it is already: nothing changes
	QVERIFY(!m_model->moveMap(5, 0, 3));
	QCOMPARE(moved.count(), 4);
}

void MapTreeModelTest::moveRefused()
{
	QSignalSpy moved(m_model.get(), &QAbstractItemModel::rowsMoved);
	// into itself, into an area, an area, the project, unknown maps
	QVERIFY(!m_model->moveMap(1, 2));
	QVERIFY(!m_model->moveMap(1, 1));
	QVERIFY(!m_model->moveMap(4, 3));
	QVERIFY(!m_model->moveMap(3, 4));
	QVERIFY(!m_model->moveMap(0, 4));
	QVERIFY(!m_model->moveMap(42, 0));
	QVERIFY(!m_model->moveMap(4, 42));
	QCOMPARE(moved.count(), 0);
}

void MapTreeModelTest::syncOrder()
{
	// the file's order lacks Cave and Ruins lost its parent: written as shown
	QVERIFY(m_model->syncOrder());
	QCOMPARE(m_tree.tree_order, std::vector<int32_t>({0, 1, 2, 3, 4, 6, 5}));
	QCOMPARE(m_model->mapInfo(6)->parent_map, 0);
	QCOMPARE(m_model->mapInfo(5)->parent_map, 0);
	QCOMPARE(m_model->mapInfo(5)->indentation, 1);
	QCOMPARE(m_model->mapInfo(3)->indentation, 3);
	QVERIFY(!m_model->syncOrder());
}

void MapTreeModelTest::renameAndExpand()
{
	QSignalSpy changed(m_model.get(), &QAbstractItemModel::dataChanged);
	QVERIFY(m_model->rename(2, QStringLiteral("Manor")));
	QCOMPARE(ToQString(m_model->mapInfo(2)->name), QStringLiteral("Manor"));
	QCOMPARE(m_model->indexOf(2).data().toString(), QStringLiteral("Manor"));
	QVERIFY(!m_model->rename(2, QStringLiteral("Manor")));
	QVERIFY(m_model->setData(m_model->indexOf(4), QStringLiteral("Woods")));
	QCOMPARE(ToQString(m_model->mapInfo(4)->name), QStringLiteral("Woods"));
	QCOMPARE(changed.count(), 2);

	QVERIFY(m_model->setExpanded(1, true));
	QVERIFY(!m_model->setExpanded(1, true));
	QVERIFY(m_model->mapInfo(1)->expanded_node);
	QCOMPARE(changed.count(), 3);
	QCOMPARE(changed.last().at(2).value<QList<int>>(), QList<int>({MapTreeModel::ExpandedRole}));

	// Map Properties: the whole entry, its place kept
	auto info = *m_model->mapInfo(2);
	info.name = lcf::DBString("Hall");
	info.music_type = lcf::rpg::MapInfo::MusicType_event;
	QVERIFY(m_model->setMapInfo(2, info));
	QCOMPARE(m_model->mapInfo(2)->music_type, int(lcf::rpg::MapInfo::MusicType_event));
	QCOMPARE(m_model->parentId(2), 1);
	QCOMPARE(changed.count(), 4);
}

void MapTreeModelTest::reset()
{
	QSignalSpy reset(m_model.get(), &QAbstractItemModel::modelReset);
	// read again from its file: another tree
	TreeMap other;
	other.maps = {entry(0, "", 0, TreeMap::MapType_root), entry(9, "Island", 0)};
	other.tree_order = {0, 9};
	m_tree = other;
	m_model->reset();
	QCOMPARE(reset.count(), 1);
	QCOMPARE(shown(0), QList<int>({9}));
	QVERIFY(!m_model->contains(1));
	// no tree: no rows
	m_model->setTreeMap(nullptr);
	QCOMPARE(m_model->rowCount(), 0);
	QCOMPARE(m_model->type(0), -1);
	QCOMPARE(m_model->freeMapId(), 1);
}

QTEST_GUILESS_MAIN(MapTreeModelTest)
#include "map_tree_model_test.moc"
