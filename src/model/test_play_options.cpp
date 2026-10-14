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

#include "test_play_options.h"
#include "common/dbstring.h"
#include "model/game_runner.h"
#include "model/project_data.h"
#include <QCoreApplication>
#include <QSettings>
#include <algorithm>

// The texts are translated in the context RunGameDialog, as RunGameWindow.qml's
// (TestPlayOptions::tr would be another one)
TestPlayOptions::TestPlayOptions(const ProjectData& project, QObject* parent) :
	QObject(parent)
{
	m_fullscreen = QSettings().value(TESTPLAY_FULLSCREEN_KEY, false).toBool();

	for (const auto& map : project.treeMap().maps) {
		if (map.ID > 0 && map.type == lcf::rpg::TreeMap::MapType_map) {
			m_maps << QStringLiteral("%1: %2").arg(map.ID, 4, 10, QChar('0')).arg(ToQString(map.name));
			m_mapIds << map.ID;
		}
	}

	const auto& database = project.database();
	m_troops << QCoreApplication::translate("RunGameDialog", "<Test Troop>");
	for (const auto& troop : database.troops) {
		m_troops << ToQString(troop.name);
	}
	m_troopIndex = database.troops.empty() ? 0 : 1;

	// surround and pincers attacks place the heroes
	m_formationEnabled = static_cast<bool>(database.battlecommands.placement);
	m_conditions << QCoreApplication::translate("RunGameDialog", "No condition")
		<< QCoreApplication::translate("RunGameDialog", "Initiative")
		<< QCoreApplication::translate("RunGameDialog", "Back attack");
	if (m_formationEnabled) {
		m_conditions << QCoreApplication::translate("RunGameDialog", "Surround attack")
			<< QCoreApplication::translate("RunGameDialog", "Pincers attack");
	}
	for (const auto& terrain : database.terrains) {
		m_terrains << ToQString(terrain.name);
	}

	m_heroes << QCoreApplication::translate("RunGameDialog", "<None>");
	m_heroIds << 0;
	for (const auto& actor : database.actors) {
		m_heroes << ToQString(actor.name);
		m_heroIds << actor.ID;
	}
	// the party of the game start
	for (int place = 0; place < PartySize; ++place) {
		const auto& party = database.system.party;
		const int id = place < static_cast<int>(party.size()) ? party[static_cast<size_t>(place)] : 0;
		m_party << std::max(0, static_cast<int>(m_heroIds.indexOf(id)));
	}
}

void TestPlayOptions::setMode(Mode mode)
{
	if (mode != m_mode) {
		m_mode = mode;
		emit changed();
	}
}

void TestPlayOptions::setPartyMember(int place, int hero_index)
{
	if (place >= 0 && place < PartySize && hero_index >= 0 && hero_index < m_heroIds.size()
			&& m_party[place] != hero_index) {
		m_party[place] = hero_index;
		emit changed();
	}
}

int TestPlayOptions::mapId() const
{
	return m_mapIndex >= 0 && m_mapIndex < m_mapIds.size() ? m_mapIds[m_mapIndex] : 0;
}

QStringList TestPlayOptions::arguments() const
{
	QStringList args;
	args << (m_fullscreen ? QStringLiteral("--fullscreen") : QStringLiteral("--window"));

	if (m_mode == NewGame) {
		args << QStringLiteral("--new-game");
	} else if (m_mode == LoadGame) {
		args << QStringLiteral("--load-game-id") << QString::number(m_saveSlot);
	} else if (m_mode == TestBattle) {
		// the first entry is the test troop of the database, then the troops
		args << QStringLiteral("--battle-test") << QString::number(std::max(1, m_troopIndex));
		if (m_formationEnabled) {
			args << QString::number(m_formation) << QString::number(m_condition)
				<< QString::number(m_terrainIndex + 1);
		}
	}

	const bool start = m_mode == Normal || m_mode == NewGame;
	if (start && m_replacePosition && mapId() > 0) {
		args << QStringLiteral("--start-map-id") << QString::number(mapId())
			<< QStringLiteral("--start-position") << QString::number(m_x) << QString::number(m_y);
	}
	if (start && m_replaceParty) {
		QStringList party;
		for (int index : m_party) {
			if (index > 0 && index < m_heroIds.size()) {
				party << QString::number(m_heroIds[index]);
			}
		}
		if (!party.isEmpty()) {
			args << QStringLiteral("--start-party") << party;
		}
	}

	if (m_noSound && m_noMusic) {
		args << QStringLiteral("--no-audio");
	} else if (m_noSound) {
		args << QStringLiteral("--sound-volume") << QStringLiteral("0");
	} else if (m_noMusic) {
		args << QStringLiteral("--music-volume") << QStringLiteral("0");
	}
	if (m_noRtp) {
		args << QStringLiteral("--no-rtp");
	}
	return args;
}
