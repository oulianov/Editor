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

// Where a map dragged in the map tree lands (MapTreeDrop): above, below or
// on a node, what the tree refuses, and the row the model then moves it to,
// on a tree in memory. The views drop through it (the QML map tree, the
// automation's drop-map; tests/scripts/qml_shell_tree.txt).

#include <QTest>
#include "common/dbstring.h"
#include "controller/map_tree_drop.h"
#include "model/map_tree_model.h"

using lcf::rpg::TreeMap;
using Position = MapTreeDrop::Position;

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

class MapTreeDropTest : public QObject
{
	Q_OBJECT

private slots:
	void init();
	void on();
	void aboveAndBelow();
	void besideAnArea();
	void refused();
	void movesTheModel();

private:
	/** The move as {id, parent, row}, {} when refused */
	QList<int> drop(int id, int target, Position position) const;

	// the project, Town (1) holding House (2) and its area Garden (3),
	// Forest (4) holding Cave (5)
	TreeMap m_tree;
	std::unique_ptr<MapTreeModel> m_model;
};

void MapTreeDropTest::init()
{
	m_tree = TreeMap();
	m_tree.maps = {entry(0, "", 0, TreeMap::MapType_root), entry(1, "Town", 0), entry(2, "House", 1),
		entry(3, "Garden", 2, TreeMap::MapType_area), entry(4, "Forest", 0), entry(5, "Cave", 4)};
	m_tree.tree_order = {0, 1, 2, 3, 4, 5};
	m_model = std::make_unique<MapTreeModel>();
	m_model->setTreeMap(&m_tree);
}

QList<int> MapTreeDropTest::drop(int id, int target, Position position) const
{
	const auto move = MapTreeDrop::move(*m_model, id, target, position);
	return move ? QList<int>{move->id, move->parent, move->row} : QList<int>();
}

void MapTreeDropTest::on()
{
	// the last child of a map, or of the project
	QCOMPARE(drop(4, 1, Position::On), QList<int>({4, 1, 1}));
	QCOMPARE(drop(5, 0, Position::On), QList<int>({5, 0, 2}));
	// on its own parent: its last child, counted without it
	QCOMPARE(drop(1, 0, Position::On), QList<int>({1, 0, 1}));
}

void MapTreeDropTest::aboveAndBelow()
{
	// among the siblings of the target, counted without the map
	QCOMPARE(drop(4, 1, Position::Above), QList<int>({4, 0, 0}));
	QCOMPARE(drop(1, 4, Position::Below), QList<int>({1, 0, 1}));
	QCOMPARE(drop(5, 1, Position::Below), QList<int>({5, 0, 1}));
	QCOMPARE(drop(2, 5, Position::Above), QList<int>({2, 4, 0}));
}

void MapTreeDropTest::besideAnArea()
{
	// beside an area: under its map
	QCOMPARE(drop(4, 3, Position::Above), QList<int>({4, 2, 0}));
	QCOMPARE(drop(5, 3, Position::Below), QList<int>({5, 2, 1}));
}

void MapTreeDropTest::refused()
{
	// the project and the areas are not dragged
	QVERIFY(drop(0, 1, Position::On).isEmpty());
	QVERIFY(drop(3, 4, Position::On).isEmpty());
	// an area holds nothing
	QVERIFY(drop(4, 3, Position::On).isEmpty());
	// the project is alone at the top
	QVERIFY(drop(4, 0, Position::Above).isEmpty());
	QVERIFY(drop(4, 0, Position::Below).isEmpty());
	// not on itself, not into what it holds
	QVERIFY(drop(1, 1, Position::On).isEmpty());
	QVERIFY(drop(1, 2, Position::On).isEmpty());
	QVERIFY(drop(1, 3, Position::Above).isEmpty());
	// nodes that are not in the tree
	QVERIFY(drop(42, 1, Position::On).isEmpty());
	QVERIFY(drop(4, 42, Position::On).isEmpty());
}

void MapTreeDropTest::movesTheModel()
{
	// the move is one the model takes: where the view showed the drop
	auto move = MapTreeDrop::move(*m_model, 4, 1, Position::Above);
	QVERIFY(move);
	QVERIFY(m_model->moveMap(move->id, move->parent, move->row));
	QCOMPARE(m_model->childIds(0), QList<int>({4, 1}));
	move = MapTreeDrop::move(*m_model, 4, 2, Position::Below);
	QVERIFY(move);
	QVERIFY(m_model->moveMap(move->id, move->parent, move->row));
	QCOMPARE(m_model->childIds(1), QList<int>({2, 4}));
	QCOMPARE(m_model->childIds(4), QList<int>({5}));
	move = MapTreeDrop::move(*m_model, 5, 1, Position::On);
	QVERIFY(move);
	QVERIFY(m_model->moveMap(move->id, move->parent, move->row));
	QCOMPARE(m_model->childIds(1), QList<int>({2, 4, 5}));
}

QTEST_GUILESS_MAIN(MapTreeDropTest)
#include "map_tree_drop_test.moc"
