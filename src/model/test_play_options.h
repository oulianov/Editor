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

#include <QList>
#include <QObject>
#include <QStringList>

class ProjectData;

/**
 * The Test Play Options of a project (Game > Test Play Options): how the
 * EasyRPG Player starts (normal, new game, a savegame, a battle test),
 * where the party starts and who is in it, the audio, and the battle of a
 * battle test. arguments() are what the Player is given (GameRunner::launch
 * without the settings).
 *
 * The choices are lists of texts (maps, troops...) and indexes into them.
 * The texts are in the context RunGameDialog, as RunGameWindow.qml's, so
 * their translations stay.
 */
class TestPlayOptions : public QObject
{
	Q_OBJECT
	Q_PROPERTY(Mode mode MEMBER m_mode NOTIFY changed)
	Q_PROPERTY(bool fullscreen MEMBER m_fullscreen NOTIFY changed)
	Q_PROPERTY(bool noSound MEMBER m_noSound NOTIFY changed)
	Q_PROPERTY(bool noMusic MEMBER m_noMusic NOTIFY changed)
	Q_PROPERTY(bool noRtp MEMBER m_noRtp NOTIFY changed)

	/** The party starts on a map of maps, at x, y (not with a savegame or in battle) */
	Q_PROPERTY(bool replacePosition MEMBER m_replacePosition NOTIFY changed)
	Q_PROPERTY(int mapIndex MEMBER m_mapIndex NOTIFY changed)
	Q_PROPERTY(int x MEMBER m_x NOTIFY changed)
	Q_PROPERTY(int y MEMBER m_y NOTIFY changed)
	Q_PROPERTY(int saveSlot MEMBER m_saveSlot NOTIFY changed)

	/** The troop of the battle test: 0 the test troop of the database, then the troops */
	Q_PROPERTY(int troopIndex MEMBER m_troopIndex NOTIFY changed)
	/** The heroes are placed automatically (Battle Layout): the formation, condition and terrain are given */
	Q_PROPERTY(bool formationEnabled READ formationEnabled CONSTANT)
	/** lcf::rpg::System::BattleFormation (terrain: the formation of terrainIndex) */
	Q_PROPERTY(int formation MEMBER m_formation NOTIFY changed)
	/** lcf::rpg::System::BattleCondition, an index of conditions */
	Q_PROPERTY(int condition MEMBER m_condition NOTIFY changed)
	Q_PROPERTY(int terrainIndex MEMBER m_terrainIndex NOTIFY changed)

	/** The party starts as party says (not with a savegame or in battle) */
	Q_PROPERTY(bool replaceParty MEMBER m_replaceParty NOTIFY changed)
	/** The four places: an index of heroes each (0: nobody) */
	Q_PROPERTY(QList<int> party READ party NOTIFY changed)

	Q_PROPERTY(QStringList maps READ maps CONSTANT)
	Q_PROPERTY(QStringList troops READ troops CONSTANT)
	Q_PROPERTY(QStringList conditions READ conditions CONSTANT)
	Q_PROPERTY(QStringList terrains READ terrains CONSTANT)
	/** "<None>", then the heroes of the database */
	Q_PROPERTY(QStringList heroes READ heroes CONSTANT)

public:
	enum Mode { Normal, NewGame, LoadGame, TestBattle };
	Q_ENUM(Mode)

	static constexpr int PartySize = 4;

	/** The options of the project: Full Screen of the settings, the party of the game start */
	explicit TestPlayOptions(const ProjectData& project, QObject* parent = nullptr);

	Mode mode() const {
		return m_mode;
	}
	void setMode(Mode mode);
	bool formationEnabled() const {
		return m_formationEnabled;
	}
	QList<int> party() const {
		return m_party;
	}
	/** Who is at a place of the party: an index of heroes */
	Q_INVOKABLE void setPartyMember(int place, int hero_index);

	QStringList maps() const {
		return m_maps;
	}
	QStringList troops() const {
		return m_troops;
	}
	QStringList conditions() const {
		return m_conditions;
	}
	QStringList terrains() const {
		return m_terrains;
	}
	QStringList heroes() const {
		return m_heroes;
	}

	/** The ID of the map of mapIndex, 0 for none */
	int mapId() const;

	/** What the EasyRPG Player is given for these options */
	Q_INVOKABLE QStringList arguments() const;

signals:
	void changed();

private:
	Mode m_mode = Normal;
	bool m_fullscreen = false;
	bool m_noSound = false;
	bool m_noMusic = false;
	bool m_noRtp = false;
	bool m_replacePosition = false;
	int m_mapIndex = 0;
	int m_x = 0;
	int m_y = 0;
	int m_saveSlot = 1;
	int m_troopIndex = 0;
	bool m_formationEnabled = false;
	int m_formation = 0;
	int m_condition = 0;
	int m_terrainIndex = 0;
	bool m_replaceParty = false;
	QList<int> m_party;

	QStringList m_maps;
	QList<int> m_mapIds;
	QStringList m_troops;
	QStringList m_conditions;
	QStringList m_terrains;
	QStringList m_heroes;
	QList<int> m_heroIds;
};
