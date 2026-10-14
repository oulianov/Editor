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

// The Test Play Options without their window, on a project in memory: what
// the EasyRPG Player is given for each play mode (TestPlayOptions), and the
// party of the battle tests for QML (TestBattlePartyModel, checked by
// QAbstractItemModelTester) over the project's database and over a
// database window's copy (DatabaseSession).

#include <QAbstractItemModelTester>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include "database_session.h"
#include "model/game_runner.h"
#include "model/project_data.h"
#include "model/test_play_options.h"
#include "project_data_gadget.h"
#include "test_battle_party_model.h"

namespace {

lcf::rpg::Actor actor(int id, const char* name, int level, int weapon)
{
	lcf::rpg::Actor hero;
	hero.ID = id;
	hero.name = lcf::DBString(name);
	hero.initial_level = level;
	hero.final_level = 50;
	hero.initial_equipment.weapon_id = weapon;
	hero.parameters.maxhp = std::vector<int16_t>(50, 100);
	hero.parameters.maxsp = std::vector<int16_t>(50, 10);
	hero.parameters.attack = std::vector<int16_t>(50, 20);
	hero.parameters.defense = std::vector<int16_t>(50, 30);
	hero.parameters.spirit = std::vector<int16_t>(50, 40);
	hero.parameters.agility = std::vector<int16_t>(50, 50);
	return hero;
}

lcf::rpg::Item weapon(int id, const char* name, int attack)
{
	lcf::rpg::Item item;
	item.ID = id;
	item.name = lcf::DBString(name);
	item.type = lcf::rpg::Item::Type_weapon;
	item.atk_points1 = attack;
	return item;
}

/** Two maps (and an area), two troops, two terrains, three heroes, two weapons */
ProjectData project(bool auto_placement)
{
	ProjectData data;
	auto& tree = data.treeMap();
	tree.maps.resize(4);
	for (int i = 0; i < 4; ++i) {
		tree.maps[static_cast<size_t>(i)].ID = i;
		tree.maps[static_cast<size_t>(i)].type = lcf::rpg::TreeMap::MapType_map;
	}
	tree.maps[0].type = lcf::rpg::TreeMap::MapType_root;
	tree.maps[1].name = lcf::DBString("Town");
	tree.maps[2].type = lcf::rpg::TreeMap::MapType_area;
	tree.maps[3].name = lcf::DBString("Cave");

	auto& db = data.database();
	db.troops.resize(2);
	db.troops[0].ID = 1;
	db.troops[0].name = lcf::DBString("Slimes");
	db.troops[1].ID = 2;
	db.troops[1].name = lcf::DBString("Bats");
	db.terrains.resize(2);
	db.terrains[0].ID = 1;
	db.terrains[0].name = lcf::DBString("Plains");
	db.terrains[1].ID = 2;
	db.terrains[1].name = lcf::DBString("Forest");
	db.actors = {actor(1, "Alex", 1, 1), actor(2, "Brian", 5, 2), actor(3, "Carol", 3, 0)};
	db.items = {weapon(1, "Sword", 10), weapon(2, "Axe", 15)};
	db.system.party = {2, 3};
	db.system.ldb_id = 2003;
	db.battlecommands.placement = auto_placement ? lcf::rpg::BattleCommands::Placement_automatic
		: lcf::rpg::BattleCommands::Placement_manual;
	return data;
}

}

class TestPlayOptionsTest : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void choices();
	void normal();
	void modes();
	void battle();
	void startPositionAndParty();
	void audio();
	void partyModel();
	void partyModelOfSession();

private:
	/** The settings of the test (Full Screen), not the user's */
	QTemporaryDir m_settings;
};

void TestPlayOptionsTest::initTestCase()
{
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settings.path());
	QSettings().setValue(TESTPLAY_FULLSCREEN_KEY, false);
}

void TestPlayOptionsTest::choices()
{
	const ProjectData data = project(true);
	TestPlayOptions options(data);
	// the maps, not the areas nor the project
	QCOMPARE(options.maps(), QStringList({QStringLiteral("0001: Town"), QStringLiteral("0003: Cave")}));
	QCOMPARE(options.troops(), QStringList({QStringLiteral("<Test Troop>"), QStringLiteral("Slimes"), QStringLiteral("Bats")}));
	QCOMPARE(options.property("troopIndex").toInt(), 1);
	QCOMPARE(options.terrains(), QStringList({QStringLiteral("Plains"), QStringLiteral("Forest")}));
	QCOMPARE(options.heroes(), QStringList({QStringLiteral("<None>"), QStringLiteral("Alex"), QStringLiteral("Brian"), QStringLiteral("Carol")}));
	// the party of the game start
	QCOMPARE(options.party(), QList<int>({2, 3, 0, 0}));
	QVERIFY(options.formationEnabled());
	QCOMPARE(options.conditions().size(), 5);

	// heroes placed by hand: no formation, no surround nor pincers attack
	const ProjectData manual = project(false);
	TestPlayOptions options2(manual);
	QVERIFY(!options2.formationEnabled());
	QCOMPARE(options2.conditions().size(), 3);
}

void TestPlayOptionsTest::normal()
{
	const ProjectData data = project(true);
	TestPlayOptions options(data);
	QCOMPARE(options.arguments(), QStringList({QStringLiteral("--window")}));
	options.setProperty("fullscreen", true);
	QCOMPARE(options.arguments(), QStringList({QStringLiteral("--fullscreen")}));

	// the Full Screen setting
	QSettings().setValue(TESTPLAY_FULLSCREEN_KEY, true);
	QVERIFY(TestPlayOptions(data).property("fullscreen").toBool());
	QSettings().setValue(TESTPLAY_FULLSCREEN_KEY, false);
}

void TestPlayOptionsTest::modes()
{
	const ProjectData data = project(true);
	TestPlayOptions options(data);
	QSignalSpy changed(&options, &TestPlayOptions::changed);
	options.setMode(TestPlayOptions::NewGame);
	QCOMPARE(changed.size(), 1);
	QCOMPARE(options.arguments(), QStringList({QStringLiteral("--window"), QStringLiteral("--new-game")}));
	options.setMode(TestPlayOptions::LoadGame);
	options.setProperty("saveSlot", 3);
	QCOMPARE(options.arguments(), QStringList({QStringLiteral("--window"), QStringLiteral("--load-game-id"), QStringLiteral("3")}));
}

void TestPlayOptionsTest::battle()
{
	const ProjectData data = project(true);
	TestPlayOptions options(data);
	options.setMode(TestPlayOptions::TestBattle);
	// the formation of a terrain, by default: the first one
	QCOMPARE(options.arguments(), QStringList({QStringLiteral("--window"), QStringLiteral("--battle-test"),
		QStringLiteral("1"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("1")}));
	options.setProperty("troopIndex", 2);
	options.setProperty("formation", lcf::rpg::System::BattleFormation_tight);
	options.setProperty("condition", lcf::rpg::System::BattleCondition_pincers);
	options.setProperty("terrainIndex", 1);
	QCOMPARE(options.arguments(), QStringList({QStringLiteral("--window"), QStringLiteral("--battle-test"),
		QStringLiteral("2"), QStringLiteral("2"), QStringLiteral("4"), QStringLiteral("2")}));
	// the test troop of the database is the first troop for the Player
	options.setProperty("troopIndex", 0);
	QCOMPARE(options.arguments().mid(1, 3), QStringList({QStringLiteral("--battle-test"), QStringLiteral("1"), QStringLiteral("2")}));
	// the start position and party do not apply
	options.setProperty("replacePosition", true);
	options.setProperty("replaceParty", true);
	QVERIFY(!options.arguments().contains(QStringLiteral("--start-map-id")));
	QVERIFY(!options.arguments().contains(QStringLiteral("--start-party")));

	// without the formation: only the troop
	const ProjectData manual = project(false);
	TestPlayOptions options2(manual);
	options2.setMode(TestPlayOptions::TestBattle);
	QCOMPARE(options2.arguments(), QStringList({QStringLiteral("--window"), QStringLiteral("--battle-test"), QStringLiteral("1")}));
}

void TestPlayOptionsTest::startPositionAndParty()
{
	const ProjectData data = project(true);
	TestPlayOptions options(data);
	options.setProperty("mapIndex", 1);
	options.setProperty("x", 12);
	options.setProperty("y", 7);
	// not replaced unless checked
	QCOMPARE(options.arguments(), QStringList({QStringLiteral("--window")}));
	options.setProperty("replacePosition", true);
	QCOMPARE(options.mapId(), 3);
	QCOMPARE(options.arguments(), QStringList({QStringLiteral("--window"), QStringLiteral("--start-map-id"), QStringLiteral("3"),
		QStringLiteral("--start-position"), QStringLiteral("12"), QStringLiteral("7")}));

	options.setProperty("replacePosition", false);
	options.setProperty("replaceParty", true);
	options.setPartyMember(0, 0);
	options.setPartyMember(2, 1);
	// the places of nobody are left out
	QCOMPARE(options.arguments(), QStringList({QStringLiteral("--window"), QStringLiteral("--start-party"), QStringLiteral("3"), QStringLiteral("1")}));
	options.setPartyMember(1, 0);
	options.setPartyMember(2, 0);
	QCOMPARE(options.arguments(), QStringList({QStringLiteral("--window")}));
	// out of range: ignored
	options.setPartyMember(4, 1);
	options.setPartyMember(0, 9);
	QCOMPARE(options.party(), QList<int>({0, 0, 0, 0}));

	// a new game too, a savegame not
	options.setPartyMember(0, 2);
	options.setMode(TestPlayOptions::NewGame);
	QCOMPARE(options.arguments(), QStringList({QStringLiteral("--window"), QStringLiteral("--new-game"), QStringLiteral("--start-party"), QStringLiteral("2")}));
	options.setMode(TestPlayOptions::LoadGame);
	QVERIFY(!options.arguments().contains(QStringLiteral("--start-party")));
}

void TestPlayOptionsTest::audio()
{
	const ProjectData data = project(true);
	TestPlayOptions options(data);
	options.setProperty("noSound", true);
	QCOMPARE(options.arguments().mid(1), QStringList({QStringLiteral("--sound-volume"), QStringLiteral("0")}));
	options.setProperty("noMusic", true);
	QCOMPARE(options.arguments().mid(1), QStringList({QStringLiteral("--no-audio")}));
	options.setProperty("noSound", false);
	options.setProperty("noRtp", true);
	QCOMPARE(options.arguments().mid(1), QStringList({QStringLiteral("--music-volume"), QStringLiteral("0"), QStringLiteral("--no-rtp")}));
}

void TestPlayOptionsTest::partyModel()
{
	ProjectData data = project(true);
	ProjectDataGadget gadget;
	gadget.setProjectData(&data);
	TestBattlePartyModel model;
	QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
	QCOMPARE(model.rowCount(), 0);
	model.setProjectData(&gadget);
	QCOMPARE(model.rowCount(), 4);
	QCOMPARE(model.heroes(), QStringList({QStringLiteral("0001: Alex"), QStringLiteral("0002: Brian"), QStringLiteral("0003: Carol")}));

	// no battle test party yet: none in it, the next heroes as they start
	QCOMPARE(model.size(), 0);
	auto value = [&model](int row, TestBattlePartyModel::Role role) {
		return model.data(model.index(row), role);
	};
	QCOMPARE(value(0, TestBattlePartyModel::HeroIndexRole).toInt(), 0);
	QCOMPARE(value(1, TestBattlePartyModel::HeroIndexRole).toInt(), 1);
	QCOMPARE(value(1, TestBattlePartyModel::LevelRole).toInt(), 5);
	QCOMPARE(value(1, TestBattlePartyModel::MaxLevelRole).toInt(), 50);
	QVERIFY(!value(0, TestBattlePartyModel::InPartyRole).toBool());

	QSignalSpy edited(&model, &TestBattlePartyModel::edited);
	QSignalSpy size(&model, &TestBattlePartyModel::sizeChanged);
	model.setSize(2);
	QCOMPARE(size.size(), 1);
	QVERIFY(value(1, TestBattlePartyModel::InPartyRole).toBool());
	QVERIFY(!value(2, TestBattlePartyModel::InPartyRole).toBool());

	// the weapons: none, then those of the database
	const QVariantList choices = value(0, TestBattlePartyModel::SlotChoicesRole).toList();
	QCOMPARE(choices.size(), static_cast<int>(TestBattleParty::SlotCount));
	QCOMPARE(choices[TestBattleParty::Weapon].toStringList(),
		QStringList({QStringLiteral("<None>"), QStringLiteral("0001: Sword"), QStringLiteral("0002: Axe")}));
	QCOMPARE(value(0, TestBattlePartyModel::SlotIndexesRole).toList()[TestBattleParty::Weapon].toInt(), 1);
	// Alex: attack 20 and the sword's 10
	QCOMPARE(value(0, TestBattlePartyModel::ParametersRole).toList()[TestBattleParty::Attack].toInt(), 30);

	QSignalSpy data_changed(&model, &QAbstractItemModel::dataChanged);
	model.setEquipment(0, TestBattleParty::Weapon, 2);
	QCOMPARE(data_changed.size(), 1);
	QCOMPARE(data_changed[0][0].toModelIndex().row(), 0);
	QCOMPARE(value(0, TestBattlePartyModel::ParametersRole).toList()[TestBattleParty::Attack].toInt(), 35);
	model.setLevel(0, 99);
	QCOMPARE(value(0, TestBattlePartyModel::LevelRole).toInt(), 50);
	// another hero: as they start
	model.setHero(0, 2);
	QCOMPARE(value(0, TestBattlePartyModel::HeroIndexRole).toInt(), 2);
	QCOMPARE(value(0, TestBattlePartyModel::LevelRole).toInt(), 3);
	QCOMPARE(value(0, TestBattlePartyModel::SlotIndexesRole).toList()[TestBattleParty::Weapon].toInt(), 0);
	model.setLevel(0, 10);
	model.initialStats(0);
	QCOMPARE(value(0, TestBattlePartyModel::LevelRole).toInt(), 3);
	QCOMPARE(edited.size(), 6);
	// out of range: ignored
	model.setHero(0, 7);
	model.setEquipment(0, TestBattleParty::Weapon, 9);
	model.setEquipment(5, TestBattleParty::Weapon, 1);
	QCOMPARE(edited.size(), 6);

	// written into the database by save() only
	QVERIFY(data.database().system.battletest_data.empty());
	model.save();
	const auto& saved = data.database().system.battletest_data;
	QCOMPARE(saved.size(), size_t(2));
	QCOMPARE(saved[0].actor_id, 3);
	QCOMPARE(saved[0].level, 3);
	QCOMPARE(saved[1].actor_id, 2);

	// read again: a model on the same database sees it
	TestBattlePartyModel other;
	other.setProjectData(&gadget);
	QCOMPARE(other.size(), 2);
	QCOMPARE(other.data(other.index(0), TestBattlePartyModel::HeroIndexRole).toInt(), 2);
}

void TestPlayOptionsTest::partyModelOfSession()
{
	ProjectData data = project(true);
	data.database().system.battletest_data.resize(1);
	data.database().system.battletest_data[0].actor_id = 1;
	data.database().system.battletest_data[0].level = 4;
	DatabaseSession session;
	session.open(&data);
	TestBattlePartyModel model;
	QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
	model.setSession(&session);
	QCOMPARE(model.size(), 1);
	QCOMPARE(model.data(model.index(0), TestBattlePartyModel::LevelRole).toInt(), 4);
	model.setLevel(0, 8);
	model.save();
	// the session's copy, not the project
	QCOMPARE(session.copy().database().system.battletest_data[0].level, 8);
	QCOMPARE(data.database().system.battletest_data[0].level, 4);
}

QTEST_GUILESS_MAIN(TestPlayOptionsTest)
#include "test_play_options_test.moc"
