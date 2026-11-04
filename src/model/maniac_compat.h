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
#include <QString>
#include <QStringList>
#include <functional>
#include <vector>

class Project;
namespace lcf::rpg {
	class Map;
}

/**
 * The compatibility of a game with the Maniac Patch (the Maniac mode of the
 * Editor): what the game uses that the Maniac Patch's RPG_RT does not run,
 * and the conversion of what has a Maniac equivalent.
 *
 * - The format of RPG Maker 2000: the Maniac Patch runs RPG Maker 2003
 *   games (ProjectConvert::to2003).
 * - EasyRPG Player's own commands (codes 2000-2999), its screens of Call
 *   System Functions, its @easyrpg_ comments and database options.
 * - Power Mode 2003 (warp.dll): variables 1 to 8 are registers of the
 *   engine, written by Control Variables (load menu and quit, the mouse,
 *   a key, decimal math, the rotation of pictures).
 * - The Ineluki Key Patch (harmony.dll): sounds whose names end in .script
 *   (a script of the patch, read by Play Sound Effect) or .link (the file
 *   played instead).
 *
 * What has no equivalent is listed and stays as it is.
 */
class ManiacCompat {
	Q_DECLARE_TR_FUNCTIONS(ManiacCompat)
public:
	enum class Group { Engine, EasyRpg, PowerMode, KeyPatch };

	/** A use, the same for each of its places */
	struct Finding {
		Group group = Group::EasyRpg;
		QString what;
		/** What the conversion makes of it; empty: it stays as it is */
		QString becomes;
		/** "Map 0003: Town, event 0005: Guard, page 1, line 12"... */
		QStringList places;
	};

	struct Report {
		/** By group, in the order found */
		std::vector<Finding> findings;
		/** An RPG Maker 2000 project, converted to RPG Maker 2003 first */
		bool rpg2000 = false;

		bool empty() const { return findings.empty(); }
		/** The places the conversion changes */
		int convertible() const;
		/** The places that stay as they are */
		int remaining() const;
		/** Changes with the findings: whether a report was already shown */
		QString fingerprint() const;
	};

	/** The maps of a check where they are open, with their unsaved changes:
	 *  nullptr for a map read from its file */
	using OpenMap = std::function<const lcf::rpg::Map*(int id)>;
	/** What the project uses that the Maniac Patch does not run */
	static Report check(const Project& project, const OpenMap& open_map = {});

	/** The folder, inside the project, that receives the files before they change */
	static QString backupFolderName();

	/**
	 * Converts what has a Maniac equivalent in a project read from its files
	 * (not the one open), of RPG Maker 2003. The database, the maps that
	 * change and EasyRPG.ini are first copied to a new backup folder inside
	 * the project. EasyRPG.ini's [Patch] then turns the Maniac Patch on, and
	 * Power Mode 2003 off: the Maniac Patch's RPG_RT does not run it, the
	 * registers become variables.
	 * @param converted receives the places converted
	 * @param backup_path receives the absolute path of the backup folder
	 * @return an error message, or an empty string
	 */
	static QString convert(Project& project, int* converted = nullptr, QString* backup_path = nullptr);

	static QString groupName(Group group);
};
