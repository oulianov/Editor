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

#include "test_battle_model.h"
#include "model/project_data.h"
#include "qmlbinding/test_play.h"
#include <QQuickItem>
#include <QQuickWindow>

TestBattleModel::TestBattleModel(QObject* parent) : QObject(parent)
{
}

TestBattleModel::~TestBattleModel() = default;

JsonView* TestBattleModel::database() const
{
	return m_database;
}

void TestBattleModel::setDatabase(JsonView* database)
{
	if (m_database != database) {
		m_database = database;
		reset();
	}
}

int TestBattleModel::troopId() const
{
	return m_troopId;
}

void TestBattleModel::setTroopId(int troop_id)
{
	if (m_troopId != troop_id) {
		m_troopId = troop_id;
		reset();
	}
}

DatabaseSession* TestBattleModel::session() const
{
	return m_database ? DatabaseSession::owning(m_database->json()) : nullptr;
}

void TestBattleModel::reset()
{
	DatabaseSession* owner = session();
	// the settings of the last test
	m_battle = owner ? std::make_unique<TestBattle>(owner->copy().database(), m_troopId) : nullptr;
	emit sourceChanged();
	emit changed();
}

bool TestBattleModel::rpg2k3() const
{
	return m_battle && m_battle->rpg2k3();
}

int TestBattleModel::formation() const
{
	return m_battle ? m_battle->formation() : 0;
}

void TestBattleModel::setFormation(int formation)
{
	if (m_battle && formation != m_battle->formation()) {
		m_battle->setFormation(formation);
		emit changed();
	}
}

int TestBattleModel::condition() const
{
	return m_battle ? m_battle->condition() : 0;
}

void TestBattleModel::setCondition(int condition)
{
	if (m_battle && condition != m_battle->condition()) {
		m_battle->setCondition(condition);
		emit changed();
	}
}

int TestBattleModel::terrainId() const
{
	return m_battle ? m_battle->terrainId() : 0;
}

void TestBattleModel::setTerrainId(int terrain_id)
{
	if (m_battle && terrain_id != m_battle->terrainId()) {
		m_battle->setTerrainId(terrain_id);
		emit changed();
	}
}

bool TestBattleModel::formationEnabled() const
{
	return m_battle && m_battle->rpg2k3() && m_battle->formationEnabled();
}

bool TestBattleModel::terrainEnabled() const
{
	return formationEnabled() && m_battle->terrainEnabled();
}

bool TestBattleModel::conditionAllowed(int condition) const
{
	return m_battle && m_battle->conditionAllowed(condition);
}

void TestBattleModel::save()
{
	if (!m_battle) {
		return;
	}
	m_battle->save();
	if (m_database && m_database->json()) {
		m_database->json()->notifyChanged(QStringLiteral("/system"));
	}
}

bool TestBattleModel::startTest(QQuickItem* from)
{
	if (!m_battle) {
		return false;
	}
	save();
	// the Player reads the files: the database window applies its copy
	// first (its Apply, with its questions), as RPG Maker saves it
	DatabaseSession* owner = session();
	QWindow* window = owner ? owner->window() : nullptr;
	if (window) {
		QVariant applied;
		QMetaObject::invokeMethod(window, "applyChanges", Q_RETURN_ARG(QVariant, applied));
		if (!applied.toBool()) {
			return false;
		}
	}
	const auto battle = m_battle->playerBattle();
	return TestPlay::runBattle(from ? from->window() : nullptr, m_battle->troopId(), battle.formation, battle.condition, battle.terrain_id);
}
