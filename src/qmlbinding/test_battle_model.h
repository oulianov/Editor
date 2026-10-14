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

#include <QObject>
#include <QPointer>
#include <QtQmlIntegration>
#include <memory>

#include "model/test_battle.h"
#include "qmlbinding/database_session.h"
#include "qmlbinding/json_view.h"

class QQuickItem;

/**
 * RPG Maker's Test Battle window of a monster group for QML
 * (TestBattleDialog.qml), in a database window's copy: the formation and
 * the special condition, kept in the database for the next test, by
 * TestBattle's rules; startTest() applies the window's database, as
 * RPG Maker saves it before a test battle, then runs the Player.
 */
class TestBattleModel : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	/** The root of the window's database */
	Q_PROPERTY(JsonView* database READ database WRITE setDatabase NOTIFY sourceChanged)
	/** The ID of the monster group */
	Q_PROPERTY(int troopId READ troopId WRITE setTroopId NOTIFY sourceChanged)
	/** The window's session (the party is edited in its copy) */
	Q_PROPERTY(DatabaseSession* session READ session NOTIFY sourceChanged)
	Q_PROPERTY(bool rpg2k3 READ rpg2k3 NOTIFY sourceChanged)
	/** lcf::rpg::System::BattleFormation */
	Q_PROPERTY(int formation READ formation WRITE setFormation NOTIFY changed)
	/** lcf::rpg::System::BattleCondition */
	Q_PROPERTY(int condition READ condition WRITE setCondition NOTIFY changed)
	/** The terrain of the Terrain formation (its ID) */
	Q_PROPERTY(int terrainId READ terrainId WRITE setTerrainId NOTIFY changed)
	Q_PROPERTY(bool formationEnabled READ formationEnabled NOTIFY changed)
	Q_PROPERTY(bool terrainEnabled READ terrainEnabled NOTIFY changed)

public:
	explicit TestBattleModel(QObject* parent = nullptr);
	~TestBattleModel() override;

	JsonView* database() const;
	void setDatabase(JsonView* database);
	int troopId() const;
	void setTroopId(int troop_id);
	DatabaseSession* session() const;
	bool rpg2k3() const;
	int formation() const;
	void setFormation(int formation);
	int condition() const;
	void setCondition(int condition);
	int terrainId() const;
	void setTerrainId(int terrain_id);
	bool formationEnabled() const;
	bool terrainEnabled() const;

	/** No surround or pincers attack when the heroes are placed by hand */
	Q_INVOKABLE bool conditionAllowed(int condition) const;
	/** Writes the settings to the database */
	Q_INVOKABLE void save();
	/** Applies the database window's copy (its Apply), then starts the
	 *  Player in a battle against the group; false when it did not start */
	Q_INVOKABLE bool startTest(QQuickItem* from);

signals:
	void sourceChanged();
	void changed();

private:
	void reset();

	QPointer<JsonView> m_database;
	int m_troopId = 0;
	std::unique_ptr<TestBattle> m_battle;
};
