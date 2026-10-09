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

#include <QCoreApplication>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <lcf/rpg/database.h>
#include <lcf/rpg/map.h>
#include <lcf/rpg/treemap.h>

/**
 * What a game uses: the files of its resource folders and the database
 * entries, read in the database, the map tree and every map the way the
 * EasyRPG Player reads them. The export adds the RTP files the game needs
 * and clears the database entries that nothing uses.
 */
class GameReferences
{
	Q_DECLARE_TR_FUNCTIONS(GameReferences)
public:
	enum Kind {
		Actor, Class, Skill, Item, Enemy, Troop, Terrain, Attribute, State,
		Animation, Chipset, CommonEvent, BattlerAnimation, KindCount
	};

	using Maps = std::map<int, std::unique_ptr<lcf::rpg::Map>>;

	/** Everything the game refers to; encoding: the game's (texts of move routes) */
	void collect(const lcf::rpg::Database& db, const lcf::rpg::TreeMap& tree, const Maps& maps, const std::string& encoding);

	/** The ids of each kind used somewhere */
	QSet<int> ids[KindCount];
	/** An id of this kind comes from a variable: any entry may be used */
	bool dynamic[KindCount] = {};
	/** Resource folder (CharSet, Music...) -> file names, as the game writes them */
	QMap<QString, QSet<QString>> files;
	/** Resource folder -> the start of names completed by a variable (pictures of RPG Maker 2003 1.12) */
	QMap<QString, QSet<QString>> filePrefixes;
	/** Where a whole file name comes from a variable or a string */
	QStringList dynamicFiles;
	/** Maniac Patch or EasyRPG commands, DynRPG comments: ids can come from anywhere */
	bool extended = false;

	static QString kindName(Kind kind);

private:
	void addFile(const char* folder, std::string_view name);
	void addId(Kind kind, int id);
	template <typename Bits> void addBits(Kind kind, const Bits& bits);
	void collectDatabase(const lcf::rpg::Database& db);
	void collectCommands(const std::vector<lcf::rpg::EventCommand>& commands, const QString& where);
	void collectCommand(const lcf::rpg::EventCommand& com, const QString& where);
	void collectMove(const lcf::rpg::MoveCommand& move);

	std::string m_encoding;
};

namespace DatabaseCleanup {
	/**
	 * Clears the database entries that nothing uses (empty name and data,
	 * same id), then removes the cleared entries at the end of the lists
	 * whose ids nothing else counts. Entries in the middle keep their place:
	 * removing them would renumber the following ones and break every
	 * reference. Returns what was done, for the export log.
	 */
	QStringList clean(lcf::rpg::Database& db, const lcf::rpg::TreeMap& tree, const GameReferences::Maps& maps, const std::string& encoding);
}
