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

#include <QStringList>

class Project;
namespace lcf::rpg {
	class Database;
}

/**
 * The options of the database only EasyRPG Player reads, as Project
 * Settings and Convert to RPG Maker 2003 write them: the battles of RPG
 * Maker 2000 in an RPG Maker 2003 game, and the experience curve.
 *
 * RPG Maker removes the fields it does not know when it saves the
 * database. The Editor keeps a copy of the choices it wrote in the
 * [EasyRPG Editor] section of EasyRPG.ini, which the Player does not read,
 * to write them back (dropped()).
 */
namespace PlayerOptions {
	/** Each option, as a flag */
	enum Option {
		/** system.easyrpg_use_rpg2k_battle_system and its commands and messages */
		Battle2k = 1,
		/** system.easyrpg_alternative_exp */
		AlternativeExp = 2,
	};

	struct Values {
		/** The battles of RPG Maker 2000 (an RPG Maker 2003 game) */
		bool battle2k = false;
		/** The experience curve: 0 the engine's, 1 RPG Maker 2000's, 2 RPG Maker 2003's */
		int alternativeExp = 0;
	};

	/** As the database says */
	Values of(const lcf::rpg::Database& database);
	/** The battles of RPG Maker 2000, as Convert to RPG Maker 2003 keeps them
	 *  (its commands, the messages of the English version for a database of
	 *  it), or none of their fields */
	void setBattle2k(lcf::rpg::Database& database, bool on);
	/** The options given (Option flags) */
	void set(lcf::rpg::Database& database, const Values& values, int options);

	/** The copy in EasyRPG.ini: the values, and the options it has (flags) */
	struct Kept {
		Values values;
		int options = 0;
	};
	Kept kept(const Project& project);
	/** Writes the copy of the options given (flags): an option at its
	 *  default (off, 0) is removed */
	bool keep(const Project& project, const Values& values, int options);

	/** The options of the copy the database of the project lost (flags) */
	int dropped(const Project& project);
	/** The options, as the question asking to write them back names them */
	QStringList describe(const Values& values, int options);
}
