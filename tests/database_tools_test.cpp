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

// The editing rules of the database pages without their QML views: the
// Chipset page (ChipsetEditor and its painter), the members of a monster
// group (TroopLayout), the experience and parameter curves, and the Test
// Battle window (TestBattleParty, TestBattle), on data in memory.

#include <QImage>
#include <QRandomGenerator>
#include <QTest>
#include <lcf/rpg/database.h>
#include "common/dbstring.h"
#include "model/chipset_editor.h"
#include "model/experience_curve.h"
#include "model/test_battle.h"
#include "model/troop_layout.h"
#include "render/chipset_editor_painter.h"

namespace {
using Flag = ChipsetEditor::Flag;

/** The middle of a side of a tile, in tiles */
QPointF sidePoint(int tile, int side) {
	const QPointF center(tile % ChipsetEditor::Columns + 0.5, tile / ChipsetEditor::Columns + 0.5);
	switch (side) {
		case ChipsetEditor::Up: return center + QPointF(0, -0.4);
		case ChipsetEditor::Down: return center + QPointF(0, 0.4);
		case ChipsetEditor::Left: return center + QPointF(-0.4, 0);
		default: return center + QPointF(0.4, 0);
	}
}

QPointF tilePoint(int tile) {
	return {tile % ChipsetEditor::Columns + 0.5, tile / ChipsetEditor::Columns + 0.5};
}

lcf::rpg::Enemy enemy(int id, const char* name) {
	lcf::rpg::Enemy e;
	e.ID = id;
	e.name = ToDBString(QString::fromLatin1(name));
	return e;
}

lcf::rpg::Item item(int id, lcf::rpg::Item::Type type, int attack, int defense) {
	lcf::rpg::Item i;
	i.ID = id;
	i.type = type;
	i.atk_points1 = attack;
	i.def_points1 = defense;
	return i;
}

/** An RPG Maker 2003 database: two heroes (the second with two weapons),
 *  four items, two terrains and a monster group */
lcf::rpg::Database testDatabase() {
	lcf::rpg::Database database;
	database.system.ldb_id = 2003;

	lcf::rpg::Actor first;
	first.ID = 1;
	first.initial_level = 5;
	first.final_level = 20;
	first.initial_equipment.weapon_id = 1;
	first.initial_equipment.shield_id = 2;
	first.parameters.Setup(99);
	for (int level = 1; level <= 99; ++level) {
		first.parameters.maxhp[static_cast<size_t>(level) - 1] = static_cast<int16_t>(100 * level);
		first.parameters.attack[static_cast<size_t>(level) - 1] = static_cast<int16_t>(level);
	}
	lcf::rpg::Actor second;
	second.ID = 2;
	second.initial_level = 3;
	second.two_weapon = true;
	database.actors = {first, second};

	auto sword = item(1, lcf::rpg::Item::Type_weapon, 10, 0);
	auto shield = item(2, lcf::rpg::Item::Type_shield, 0, 5);
	// the second hero only
	auto axe = item(3, lcf::rpg::Item::Type_weapon, 7, 0);
	axe.actor_set = lcf::DBBitArray({false, true});
	auto armor = item(4, lcf::rpg::Item::Type_armor, 0, 3);
	database.items = {sword, shield, axe, armor};

	lcf::rpg::Terrain grass;
	grass.ID = 1;
	lcf::rpg::Terrain desert;
	desert.ID = 2;
	database.terrains = {grass, desert};

	lcf::rpg::Troop troop;
	troop.ID = 1;
	troop.auto_alignment = true;
	database.troops = {troop};
	database.battlecommands.placement = lcf::rpg::BattleCommands::Placement_automatic;
	return database;
}
}

class DatabaseToolsTest : public QObject
{
	Q_OBJECT

private slots:
	void chipsetTilesAndSides();
	void chipsetGrowsLists();
	void chipsetPassability();
	void chipsetStrokes();
	void chipsetTerrain();
	void chipsetHover();
	void chipsetPainter();

	void troopMembers();
	void troopAutoAlign();
	void troopNameAndTerrains();

	void experienceFormulas();
	void experienceSettings();
	void parameterCurves();
	void curveStroke();

	void testBattleParty();
	void testBattleSettings();
};

void DatabaseToolsTest::chipsetTilesAndSides()
{
	lcf::rpg::Chipset chipset;
	ChipsetEditor editor;
	editor.setChipset(&chipset);

	QCOMPARE(editor.tileAt({0.5, 0.5}), 0);
	QCOMPARE(editor.tileAt({5.99, 1.0}), 11);
	// past the six columns, above, below the last tile
	QCOMPARE(editor.tileAt({6.0, 0.5}), -1);
	QCOMPARE(editor.tileAt({0.5, -0.1}), -1);
	QCOMPARE(editor.tileAt({0.5, 26.5}), 156);
	QCOMPARE(editor.tileAt({0.5, 27.5}), -1);
	editor.setLowerLayer(false);
	QCOMPARE(editor.tileCount(), ChipsetEditor::UpperTiles);
	QCOMPARE(editor.tileAt({0.5, 23.5}), 138);
	QCOMPARE(editor.tileAt({0.5, 24.5}), -1);

	// the tile is cut along its diagonals
	for (int side : {ChipsetEditor::Up, ChipsetEditor::Down, ChipsetEditor::Left, ChipsetEditor::Right}) {
		QCOMPARE(ChipsetEditor::sideAt(sidePoint(7, side), 7), side);
	}
	QCOMPARE(ChipsetEditor::sideAt({1.9, 1.45}, 7), static_cast<int>(ChipsetEditor::Right));
	QCOMPARE(ChipsetEditor::sideAt({1.55, 1.95}, 7), static_cast<int>(ChipsetEditor::Down));

	// where the Player reads the tiles in the image
	QCOMPARE(ChipsetEditor::tilePosition(true, 0), QPoint(0, 0));
	QCOMPARE(ChipsetEditor::tilePosition(true, 1), QPoint(3, 0));
	QCOMPARE(ChipsetEditor::tilePosition(true, 2), QPoint(0, 4));
	QCOMPARE(ChipsetEditor::tilePosition(true, 4), QPoint(4, 4));
	QCOMPARE(ChipsetEditor::tilePosition(true, 6), QPoint(0, 8));
	QCOMPARE(ChipsetEditor::tilePosition(true, 9), QPoint(3, 12));
	QCOMPARE(ChipsetEditor::tilePosition(true, 10), QPoint(6, 0));
	QCOMPARE(ChipsetEditor::tilePosition(true, 18), QPoint(12, 0));
	QCOMPARE(ChipsetEditor::tilePosition(true, 114), QPoint(18, 0));
	QCOMPARE(ChipsetEditor::tilePosition(false, 0), QPoint(18, 8));
	QCOMPARE(ChipsetEditor::tilePosition(false, 48), QPoint(24, 0));
}

void DatabaseToolsTest::chipsetGrowsLists()
{
	// an RPG Maker 2000 file can have shorter lists
	lcf::rpg::Chipset chipset;
	chipset.terrain_data = {4, 5};
	chipset.passable_data_lower = {0};
	ChipsetEditor editor;
	QVERIFY(!editor.press(tilePoint(0)));
	editor.setChipset(&chipset);
	QCOMPARE(chipset.terrain_data.size(), size_t(ChipsetEditor::LowerTiles));
	QCOMPARE(chipset.passable_data_lower.size(), size_t(ChipsetEditor::LowerTiles));
	QCOMPARE(chipset.passable_data_upper.size(), size_t(ChipsetEditor::UpperTiles));
	QCOMPARE(editor.terrain(1), 5);
	QCOMPARE(editor.terrain(2), 1);
	QCOMPARE(editor.flags(0), 0);
	QCOMPARE(editor.flags(1), static_cast<int>(ChipsetEditor::AllDirections));
}

void DatabaseToolsTest::chipsetPassability()
{
	lcf::rpg::Chipset chipset;
	ChipsetEditor editor;
	editor.setChipset(&chipset);
	QCOMPARE(editor.mode(), ChipsetEditor::Mode::Passability);
	chipset.passable_data_lower[0] |= ChipsetEditor::Counter;

	// passable -> blocked -> above the hero -> passable; the counter stays
	QVERIFY(editor.press(tilePoint(0)));
	QCOMPARE(editor.flags(0), static_cast<int>(ChipsetEditor::Counter));
	editor.release();
	editor.press(tilePoint(0));
	QCOMPARE(editor.flags(0), ChipsetEditor::AllDirections | ChipsetEditor::Above | ChipsetEditor::Counter);
	editor.release();
	editor.press(tilePoint(0));
	QCOMPARE(editor.flags(0), ChipsetEditor::AllDirections | ChipsetEditor::Counter);
	editor.release();
	// backwards with the right button
	editor.press(tilePoint(0), true);
	QCOMPARE(editor.flags(0), ChipsetEditor::AllDirections | ChipsetEditor::Above | ChipsetEditor::Counter);
	editor.release();

	// the upper layer has its own flags; its first tile is above the hero
	editor.setLowerLayer(false);
	editor.press(tilePoint(1));
	QCOMPARE(static_cast<int>(chipset.passable_data_upper[1]), 0);
	QCOMPARE(chipset.passable_data_lower[1] & ChipsetEditor::AllDirections, static_cast<int>(ChipsetEditor::AllDirections));
	editor.release();
	editor.press(tilePoint(0));
	QCOMPARE(static_cast<int>(chipset.passable_data_upper[0]), static_cast<int>(ChipsetEditor::AllDirections));

	// outside the tiles: nothing, no drag
	editor.release();
	QVERIFY(!editor.press({7.0, 0.5}));
	QVERIFY(!editor.dragging());
}

void DatabaseToolsTest::chipsetStrokes()
{
	lcf::rpg::Chipset chipset;
	ChipsetEditor editor;
	editor.setChipset(&chipset);

	// a drag gives the other tiles what the click gave, once per tile
	QVERIFY(editor.press(tilePoint(0)));
	QVERIFY(editor.dragging());
	QVERIFY(!editor.drag(tilePoint(0)));
	QVERIFY(editor.drag(tilePoint(1)));
	QVERIFY(!editor.drag(tilePoint(1) + QPointF(0.2, 0.2)));
	QVERIFY(!editor.drag({8.0, 0.5}));
	QVERIFY(editor.drag(tilePoint(7)));
	editor.release();
	QVERIFY(!editor.dragging());
	QVERIFY(!editor.drag(tilePoint(2)));
	for (int tile : {0, 1, 7}) {
		QCOMPARE(editor.flags(tile), 0);
	}
	QCOMPARE(editor.flags(2), static_cast<int>(ChipsetEditor::AllDirections));

	// Directions: the side clicked, the same side of the tiles dragged over
	editor.setMode(ChipsetEditor::Mode::Directions);
	editor.press(sidePoint(3, ChipsetEditor::Up));
	QCOMPARE(editor.flags(3), ChipsetEditor::Down | ChipsetEditor::Left | ChipsetEditor::Right);
	editor.drag(sidePoint(4, ChipsetEditor::Left));
	QCOMPARE(editor.flags(4), ChipsetEditor::Down | ChipsetEditor::Left | ChipsetEditor::Right);
	editor.release();
	// on a closed side the click opens it, and so does the drag
	editor.press(sidePoint(3, ChipsetEditor::Up));
	QCOMPARE(editor.flags(3), static_cast<int>(ChipsetEditor::AllDirections));
	editor.drag(sidePoint(0, ChipsetEditor::Down));
	QCOMPARE(editor.flags(0), static_cast<int>(ChipsetEditor::Up));
	editor.release();

	// Counter
	editor.setMode(ChipsetEditor::Mode::Counter);
	editor.press(tilePoint(10));
	editor.drag(tilePoint(11));
	editor.release();
	QCOMPARE(editor.flags(10), ChipsetEditor::AllDirections | ChipsetEditor::Counter);
	QCOMPARE(editor.flags(11), ChipsetEditor::AllDirections | ChipsetEditor::Counter);
	editor.press(tilePoint(11));
	editor.drag(tilePoint(10));
	editor.release();
	QCOMPARE(editor.flags(10), static_cast<int>(ChipsetEditor::AllDirections));
}

void DatabaseToolsTest::chipsetTerrain()
{
	lcf::rpg::Chipset chipset;
	ChipsetEditor editor;
	editor.setChipset(&chipset);
	editor.setMode(ChipsetEditor::Mode::Terrain);
	editor.setTerrainId(5);
	QVERIFY(editor.press(tilePoint(3)));
	editor.drag(tilePoint(4));
	editor.release();
	QCOMPARE(editor.terrain(3), 5);
	QCOMPARE(editor.terrain(4), 5);
	QCOMPARE(editor.terrain(5), 1);

	// the upper tiles have no terrain
	editor.setLowerLayer(false);
	editor.setTerrainId(6);
	QVERIFY(editor.press(tilePoint(5)));
	editor.release();
	QCOMPARE(editor.terrain(5), 1);

	editor.setUniformTerrain(2);
	QVERIFY(std::all_of(chipset.terrain_data.begin(), chipset.terrain_data.end(), [](int16_t t) { return t == 2; }));
}

void DatabaseToolsTest::chipsetHover()
{
	lcf::rpg::Chipset chipset;
	ChipsetEditor editor;
	// only in the Directions mode, on a chipset
	editor.setMode(ChipsetEditor::Mode::Directions);
	QVERIFY(!editor.hoverAt(tilePoint(0)));
	editor.setChipset(&chipset);
	editor.setMode(ChipsetEditor::Mode::Passability);
	QVERIFY(!editor.hoverAt(tilePoint(0)));
	QCOMPARE(editor.hover().tile, -1);

	editor.setMode(ChipsetEditor::Mode::Directions);
	QVERIFY(editor.hoverAt(sidePoint(2, ChipsetEditor::Left)));
	QCOMPARE(editor.hover().tile, 2);
	QCOMPARE(editor.hover().side, static_cast<int>(ChipsetEditor::Left));
	QVERIFY(!editor.hoverAt(sidePoint(2, ChipsetEditor::Left) + QPointF(0.05, 0)));
	QVERIFY(editor.hoverAt(sidePoint(2, ChipsetEditor::Down)));
	QVERIFY(editor.hoverAt({9.0, 0.5}));
	QCOMPARE(editor.hover().tile, -1);
	editor.hoverAt(tilePoint(1));
	QVERIFY(editor.clearHover());
	QVERIFY(!editor.clearHover());
}

void DatabaseToolsTest::chipsetPainter()
{
	lcf::rpg::Chipset chipset;
	ChipsetEditor editor;
	editor.setChipset(&chipset);
	QCOMPARE(ChipsetEditorPainter::size(editor), QSize(192, 27 * 32));
	editor.setLowerLayer(false);
	QCOMPARE(ChipsetEditorPainter::size(editor), QSize(192, 24 * 32));
	editor.setLowerLayer(true);

	// RPG Maker 2003's Directions: an arrow on an open side, a dot on a
	// closed one, yellow under the mouse
	editor.setMode(ChipsetEditor::Mode::Directions);
	chipset.passable_data_lower[0] = ChipsetEditor::Up | ChipsetEditor::Left;
	editor.hoverAt(sidePoint(0, ChipsetEditor::Up));
	QImage image(ChipsetEditorPainter::size(editor), QImage::Format_ARGB32);
	image.fill(Qt::black);
	QPainter painter(&image);
	ChipsetEditorPainter::paint(painter, editor, QPixmap());
	painter.end();
	// the stems of the arrows up (at 8, 0) and left (at 0, 8), the dot down (at 8, 16)
	QCOMPARE(image.pixelColor(16, 10), QColor(255, 255, 0));
	QCOMPARE(image.pixelColor(10, 16), QColor(Qt::white));
	QCOMPARE(image.pixelColor(16, 24), QColor(Qt::white));
	// between the marks, the background
	QCOMPARE(image.pixelColor(16, 16), QColor(Qt::black));
}

void DatabaseToolsTest::troopMembers()
{
	lcf::rpg::Troop troop;
	for (int i = 0; i < TroopLayout::MaxMembers; ++i) {
		QCOMPARE(TroopLayout::addMember(troop, i + 1), i);
	}
	QCOMPARE(TroopLayout::addMember(troop, 1), -1);
	QCOMPARE(troop.members.size(), size_t(TroopLayout::MaxMembers));
	QCOMPARE(troop.members[3].ID, 4);
	QCOMPARE(troop.members[3].x, 160);
	QCOMPARE(troop.members[3].y, 120);

	// the selected member, else the last one; the others numbered again
	QVERIFY(TroopLayout::removeMember(troop, 2));
	QCOMPARE(troop.members.size(), size_t(7));
	QCOMPARE(troop.members[2].enemy_id, 4);
	QCOMPARE(troop.members[2].ID, 3);
	QVERIFY(TroopLayout::removeMember(troop));
	QCOMPARE(troop.members.back().enemy_id, 7);
	TroopLayout::removeAllMembers(troop);
	QVERIFY(!TroopLayout::removeMember(troop));

	// kept on the battle screen, in whole pixels
	QCOMPARE(TroopLayout::clamp({-5, 300.4}), QPoint(0, 240));
	QCOMPARE(TroopLayout::clamp({100.6, 50.2}), QPoint(101, 50));
	TroopLayout::addMember(troop, 1);
	TroopLayout::moveMember(troop, 0, {400, 12.4});
	QCOMPARE(troop.members[0].x, 320);
	QCOMPARE(troop.members[0].y, 12);
	TroopLayout::moveMember(troop, 3, {0, 0});

	// hidden at the start of the battle: drawn faint
	QCOMPARE(TroopLayout::opacity(troop.members[0], false), 1.0);
	QCOMPARE(TroopLayout::opacity(troop.members[0], true), 0.6);
	TroopLayout::setInvisible(troop, 0, true);
	QVERIFY(troop.members[0].invisible);
	QCOMPARE(TroopLayout::opacity(troop.members[0], true), 0.35);
}

void DatabaseToolsTest::troopAutoAlign()
{
	lcf::rpg::Troop troop;
	for (int i = 0; i < 3; ++i) {
		TroopLayout::addMember(troop, 1);
	}
	TroopLayout::autoAlign(troop);
	QCOMPARE(troop.members[0].x, 45);
	QCOMPARE(troop.members[1].x, 137);
	QCOMPARE(troop.members[2].x, 228);
	for (const auto& member : troop.members) {
		QCOMPARE(member.y, 120);
	}

	// two staggered rows from five on: three in front, two behind
	TroopLayout::addMember(troop, 1);
	TroopLayout::addMember(troop, 1);
	TroopLayout::autoAlign(troop);
	QCOMPARE(troop.members[0].x, 45);
	QCOMPARE(troop.members[0].y, 100);
	QCOMPARE(troop.members[3].x, 128);
	QCOMPARE(troop.members[4].x, 256);
	QCOMPARE(troop.members[4].y, 150);
}

void DatabaseToolsTest::troopNameAndTerrains()
{
	const std::vector<lcf::rpg::Enemy> enemies = {enemy(1, "Slime"), enemy(2, "Bat")};
	lcf::rpg::Troop troop;
	for (int id : {1, 2, 1}) {
		TroopLayout::addMember(troop, id);
	}
	QCOMPARE(TroopLayout::autoName(troop, enemies), QStringLiteral("Slime*2, Bat"));
	TroopLayout::addMember(troop, 9);
	QCOMPARE(TroopLayout::autoName(troop, enemies), QStringLiteral("Slime*2, Bat, "));

	// past the end of the set, the terrains are allowed
	QVERIFY(TroopLayout::terrainAllowed(troop, 5));
	TroopLayout::setTerrainAllowed(troop, 2, false, 4);
	QCOMPARE(troop.terrain_set.size(), size_t(4));
	QVERIFY(TroopLayout::terrainAllowed(troop, 0));
	QVERIFY(!TroopLayout::terrainAllowed(troop, 2));
	QVERIFY(TroopLayout::terrainAllowed(troop, 3));
	// grown again: the terrains it forbade stay forbidden
	TroopLayout::setTerrainAllowed(troop, 6, false, 4);
	QCOMPARE(troop.terrain_set.size(), size_t(7));
	QVERIFY(!TroopLayout::terrainAllowed(troop, 2));
	QVERIFY(TroopLayout::terrainAllowed(troop, 5));
	QVERIFY(!TroopLayout::terrainAllowed(troop, 6));
	TroopLayout::setTerrainAllowed(troop, 2, true, 4);
	QVERIFY(TroopLayout::terrainAllowed(troop, 2));
}

void DatabaseToolsTest::experienceFormulas()
{
	lcf::rpg::System system;
	system.ldb_id = 2003;
	auto curve = ExperienceCurve::of(system, 30, 20, 0);
	QVERIFY(!curve.rpg2k_formula);
	QCOMPARE(curve.max_exp, 9999999);
	QCOMPARE(curve.max_level, 99);
	// RPG Maker 2003: base + level * inflation + correction for each level
	QCOMPARE(curve.total(1), 0);
	QCOMPARE(curve.total(2), 50);
	QCOMPARE(curve.total(3), 120);
	QVERIFY(!curve.capped(3));

	// RPG Maker 2000: a growing rate
	system.ldb_id = 0;
	curve = ExperienceCurve::of(system, 30, 30, 0);
	QVERIFY(curve.rpg2k_formula);
	QCOMPARE(curve.max_exp, 999999);
	QCOMPARE(curve.max_level, 50);
	QCOMPARE(curve.total(2), 30);
	QCOMPARE(curve.total(3), 84);
	int previous = 0;
	for (int level = 2; level <= curve.max_level; ++level) {
		QVERIFY(curve.total(level) >= previous);
		QVERIFY(curve.total(level) <= curve.max_exp);
		previous = curve.total(level);
	}

	// EasyRPG: the other engine's formula, another maximum
	system.easyrpg_alternative_exp = 2;
	system.easyrpg_max_exp = 100;
	curve = ExperienceCurve::of(system, 30, 20, 0);
	QVERIFY(!curve.rpg2k_formula);
	QCOMPARE(curve.total(3), 100);
	QVERIFY(curve.capped(3));
	QVERIFY(!curve.capped(2));
	QVERIFY(!curve.capped(1));
	system.ldb_id = 2003;
	system.easyrpg_alternative_exp = 1;
	QVERIFY(ExperienceCurve::of(system, 0, 0, 0).rpg2k_formula);

	// large settings reach the maximum, they do not wrap around
	system = lcf::rpg::System();
	system.ldb_id = 2003;
	curve = ExperienceCurve::of(system, 2000000, 2000000, 2000000);
	QCOMPARE(curve.total(99), 9999999);
}

void DatabaseToolsTest::experienceSettings()
{
	lcf::rpg::System system;
	system.ldb_id = 2003;
	auto curve = ExperienceCurve::of(system, 5000, 20, 0);
	QCOMPARE(curve.value(0), 5000);
	QCOMPARE(curve.value(1), 20);
	QCOMPARE(curve.setting(0).name, QStringLiteral("Primary"));
	QCOMPARE(curve.setting(0).minimum, 1);
	// a stored value outside RPG Maker's range is kept
	QCOMPARE(curve.setting(0).maximum, 5000);
	QCOMPARE(curve.setting(1).maximum, 2000);
	QCOMPARE(curve.setting(2).name, QStringLiteral("Tertiary"));
	QCOMPARE(curve.summary(), QStringLiteral("Primary: 5000, Secondary: 20, Tertiary: 0 (level 99: %1)").arg(curve.total(99)));

	system.ldb_id = 0;
	curve = ExperienceCurve::of(system, 5, 30, 0);
	QCOMPARE(curve.setting(0).name, QStringLiteral("Base Value"));
	QCOMPARE(curve.setting(0).minimum, 5);
	QCOMPARE(curve.setting(0).maximum, 50);
	QCOMPARE(curve.setting(2).minimum, 0);
	QCOMPARE(curve.setting(2).maximum, 40);
	QVERIFY(curve.summary().endsWith(QStringLiteral("(level 50: %1)").arg(curve.total(50))));
}

void DatabaseToolsTest::parameterCurves()
{
	lcf::rpg::Parameters parameters;
	parameters.Setup(5);
	QCOMPARE(&ParameterCurves::values(parameters, ParameterCurves::MaxMp), &parameters.maxsp);
	QCOMPARE(&ParameterCurves::values(parameters, ParameterCurves::Intelligence), &parameters.spirit);
	QCOMPARE(ParameterCurves::minimum(ParameterCurves::MaxMp), 0);
	QCOMPARE(ParameterCurves::minimum(ParameterCurves::Attack), 1);
	QCOMPARE(ParameterCurves::maximum(ParameterCurves::MaxHp, 9999), 9999);
	QCOMPARE(ParameterCurves::maximum(ParameterCurves::Agility, 9999), 999);
	QVERIFY(ParameterCurves::color(0) != ParameterCurves::color(1));
	QCOMPARE(ParameterCurves::color(9), ParameterCurves::color(ParameterCurves::Agility));

	// Specify Endpoints: equal growth is a line, late growth a power curve
	auto& attack = parameters.attack;
	ParameterCurves::endpoints(attack, 5, 10, 50, 0);
	QCOMPARE(attack, (std::vector<int16_t>{10, 20, 30, 40, 50}));
	ParameterCurves::endpoints(attack, 5, 10, 50, 2);
	QCOMPARE(attack, (std::vector<int16_t>{10, 13, 20, 33, 50}));
	// no more levels than values
	ParameterCurves::endpoints(attack, 99, 1, 5, 0);
	QCOMPARE(attack, (std::vector<int16_t>{1, 2, 3, 4, 5}));

	// QuickCurve: in range, growing, higher for a better rank
	std::vector<int16_t> best(99), worst(99);
	QRandomGenerator first(42), second(42);
	ParameterCurves::quickCurve(best, 99, ParameterCurves::Attack, 0, 1, 999, first);
	ParameterCurves::quickCurve(worst, 99, ParameterCurves::Attack, 3, 1, 999, second);
	QVERIFY(std::is_sorted(best.begin(), best.end()));
	QVERIFY(best.front() >= 1 && best.back() <= 999);
	QVERIFY(best.back() > worst.back());
	QVERIFY(best.front() > worst.front());
	// a low maximum holds the curve
	ParameterCurves::quickCurve(best, 99, ParameterCurves::MaxHp, 0, 1, 999, first);
	QCOMPARE(best.back(), int16_t(999));
}

void DatabaseToolsTest::curveStroke()
{
	CurveStroke stroke;
	QCOMPARE(stroke.editAt({10, 10}, {100, 100}, false), 0);

	std::vector<int16_t> values(10, 0);
	stroke.setCurve(&values, 20, 1, 100);
	QCOMPARE(stroke.levels(), 10);
	// a click: the level under the pointer, the value at its height
	QCOMPARE(stroke.editAt({5, 0}, {100, 100}, false), 1);
	QCOMPARE(values[0], int16_t(100));
	// a fast drag fills the levels it skipped, on the line
	QCOMPARE(stroke.editAt({95, 50}, {100, 100}, true), 10);
	QCOMPARE(values[0], int16_t(100));
	QCOMPARE(values[4], int16_t(78));
	QCOMPARE(values[9], int16_t(50));
	// outside the graph: kept to the levels and the range
	QCOMPARE(stroke.editAt({-10, 200}, {100, 100}, false), 1);
	QCOMPARE(values[0], int16_t(1));
	QCOMPARE(stroke.editAt({500, -50}, {100, 100}, false), 10);
	QCOMPARE(values[9], int16_t(100));
	QCOMPARE(stroke.editAt({50, 50}, {0, 100}, false), 0);
}

void DatabaseToolsTest::testBattleParty()
{
	auto database = testDatabase();
	TestBattleParty party(database);
	// no party saved: the heroes in order with their initial stats
	QCOMPARE(party.size(), 0);
	QCOMPARE(party.member(0).actor_id, 1);
	QCOMPARE(party.member(0).level, 5);
	QCOMPARE(party.member(0).weapon_id, 1);
	QCOMPARE(party.member(0).shield_id, 2);
	QCOMPARE(party.member(1).actor_id, 2);
	QCOMPARE(party.member(1).level, 3);
	QCOMPARE(party.member(3).actor_id, 2);

	// the level from 1 to the hero's final level, else the engine's
	party.setLevel(0, 0);
	QCOMPARE(party.member(0).level, 1);
	party.setLevel(0, 500);
	QCOMPARE(party.member(0).level, 20);
	party.setLevel(1, 500);
	QCOMPARE(party.member(1).level, 99);

	// the items the hero can equip; two weapons: a weapon in the shield slot
	QCOMPARE(party.equipment(0, TestBattleParty::Weapon), (std::vector<int>{1}));
	QCOMPARE(party.equipment(0, TestBattleParty::Shield), (std::vector<int>{2}));
	QCOMPARE(party.slotType(1, TestBattleParty::Shield), static_cast<int>(lcf::rpg::Item::Type_weapon));
	QVERIFY(party.twoWeapons(1));
	QCOMPARE(party.equipment(1, TestBattleParty::Shield), (std::vector<int>{1, 3}));
	QCOMPARE(party.equipment(1, TestBattleParty::Armor), (std::vector<int>{4}));

	// the curves at the level, with what the equipment adds
	party.setLevel(0, 4);
	auto parameters = party.parameters(0);
	QCOMPARE(parameters[TestBattleParty::MaxHp], 400);
	QCOMPARE(parameters[TestBattleParty::Attack], 4 + 10);
	QCOMPARE(parameters[TestBattleParty::Defense], 1 + 5);
	party.setEquipment(0, TestBattleParty::Shield, 0);
	party.setEquipment(0, TestBattleParty::Armor, 4);
	parameters = party.parameters(0);
	QCOMPARE(parameters[TestBattleParty::Defense], 1 + 3);

	// another hero starts from their own stats
	party.setHero(0, 2);
	QCOMPARE(party.member(0).level, 3);
	QCOMPARE(party.member(0).weapon_id, 0);
	QCOMPARE(party.member(0).armor_id, 0);

	// saved without the members past the size nor the unknown heroes
	party.setHero(1, 9);
	party.setSize(3);
	party.save();
	const auto& data = database.system.battletest_data;
	QCOMPARE(data.size(), size_t(2));
	QCOMPARE(data[0].actor_id, 2);
	QCOMPARE(data[0].ID, 1);
	QCOMPARE(data[1].actor_id, 2);
	QCOMPARE(data[1].ID, 2);
	party.setSize(9);
	QCOMPARE(party.size(), TestBattleParty::MaxMembers);

	// read back, without RPG Maker's garbage in the upper bits
	database.system.battletest_data[0].weapon_id = 0x10003;
	database.system.battletest_data[0].level = 500;
	party.load();
	QCOMPARE(party.size(), 2);
	QCOMPARE(party.member(0).weapon_id, 3);
	QCOMPARE(party.member(0).level, 99);
	QCOMPARE(party.member(2).actor_id, 2);

	// RPG Maker 2000: level 50 at most
	database.system.ldb_id = 0;
	party.setLevel(1, 500);
	QCOMPARE(party.member(1).level, 50);
}

void DatabaseToolsTest::testBattleSettings()
{
	auto database = testDatabase();
	auto& system = database.system;
	auto& troop = database.troops[0];

	// unknown values: a loose formation, no condition, the first terrain
	system.battletest_formation = 7;
	system.battletest_condition = 9;
	system.battletest_terrain = 5;
	TestBattle battle(database, 1);
	QCOMPARE(battle.troopId(), 1);
	QVERIFY(battle.rpg2k3());
	QCOMPARE(battle.formation(), static_cast<int>(lcf::rpg::System::BattleFormation_loose));
	QCOMPARE(battle.condition(), static_cast<int>(lcf::rpg::System::BattleCondition_none));
	QCOMPARE(battle.terrainId(), 1);

	// the settings of the last test
	system.battletest_formation = lcf::rpg::System::BattleFormation_terrain;
	system.battletest_condition = lcf::rpg::System::BattleCondition_surround;
	system.battletest_terrain = 2;
	battle.load();
	QCOMPARE(battle.formation(), static_cast<int>(lcf::rpg::System::BattleFormation_terrain));
	QCOMPARE(battle.condition(), static_cast<int>(lcf::rpg::System::BattleCondition_surround));
	QCOMPARE(battle.terrainId(), 2);
	QVERIFY(battle.formationEnabled());
	QVERIFY(battle.terrainEnabled());
	QVERIFY(battle.conditionAllowed(lcf::rpg::System::BattleCondition_pincers));
	auto player = battle.playerBattle();
	QCOMPARE(player.formation, static_cast<int>(lcf::rpg::System::BattleFormation_terrain));
	QCOMPARE(player.condition, static_cast<int>(lcf::rpg::System::BattleCondition_surround));
	QCOMPARE(player.terrain_id, 2);

	// heroes placed by hand: no surround or pincers attack
	database.battlecommands.placement = lcf::rpg::BattleCommands::Placement_manual;
	QVERIFY(battle.manualHeroes());
	QVERIFY(!battle.conditionAllowed(lcf::rpg::System::BattleCondition_surround));
	QVERIFY(battle.conditionAllowed(lcf::rpg::System::BattleCondition_back));
	battle.load();
	QCOMPARE(battle.condition(), static_cast<int>(lcf::rpg::System::BattleCondition_none));
	// and with the monsters too, no formation
	QVERIFY(battle.formationEnabled());
	troop.auto_alignment = false;
	QVERIFY(battle.manualMonsters());
	QVERIFY(!battle.formationEnabled());
	QVERIFY(!battle.terrainEnabled());
	troop.auto_alignment = true;
	QVERIFY(!battle.manualMonsters());
	battle.setFormation(lcf::rpg::System::BattleFormation_tight);
	QVERIFY(!battle.terrainEnabled());

	// saved for the next test; no terrain: the first one for the Player
	battle.setCondition(lcf::rpg::System::BattleCondition_back);
	battle.setTerrainId(0);
	battle.save();
	QCOMPARE(system.battletest_formation, static_cast<int>(lcf::rpg::System::BattleFormation_tight));
	QCOMPARE(system.battletest_condition, static_cast<int>(lcf::rpg::System::BattleCondition_back));
	QCOMPARE(system.battletest_terrain, 0);
	QCOMPARE(battle.playerBattle().terrain_id, 1);

	// RPG Maker 2000: the group only, the condition kept as it was
	system.ldb_id = 0;
	system.battletest_condition = lcf::rpg::System::BattleCondition_surround;
	battle.load();
	QCOMPARE(battle.condition(), static_cast<int>(lcf::rpg::System::BattleCondition_surround));
	QVERIFY(!battle.formationEnabled());
	player = battle.playerBattle();
	QCOMPARE(player.formation, -1);
	QCOMPARE(player.condition, 0);
	QCOMPARE(player.terrain_id, 1);

	// a database without terrains
	database.terrains.clear();
	battle.load();
	QCOMPARE(battle.terrainId(), 0);
}

QTEST_MAIN(DatabaseToolsTest)
#include "database_tools_test.moc"
