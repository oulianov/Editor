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
#include <lcf/rpg/database.h>

class Project;

/** Choices of the conversion */
struct ProjectConvertOptions {
	/** The EasyRPG Player keeps the battles of RPG Maker 2000 (front view, party commands) */
	bool keep2kBattleSystem = false;
	/** The EasyRPG Player keeps the experience curve of RPG Maker 2000 */
	bool keep2kExperience = false;
};

/**
 * RPG Maker 2003's "convert an RPG Maker 2000 project": the database gets
 * the data of RPG Maker 2003 (ldb_id 2003, battle commands, battler
 * animations, menu, System2, terms) and the database, the map tree and the
 * maps are written in the format of RPG Maker 2003.
 */
class ProjectConvert {
	Q_DECLARE_TR_FUNCTIONS(ProjectConvert)
public:
	using Options = ProjectConvertOptions;

	/** The folder, inside the project, that receives the files of RPG Maker 2000 */
	static QString backupFolderName();

	/** An RPG Maker 2000 project (RPG_RT.ldb whose ldb_id is not 2003) */
	static bool canConvert(const Project& project);

	/**
	 * Converts the project: the files it rewrites (RPG_RT.ldb, RPG_RT.lmt,
	 * the maps of the map tree) are first copied, and RPG_RT.exe (the runtime
	 * of RPG Maker 2000) moved, to a backup folder inside the project. The
	 * new files are written next to the old ones, then renamed over them:
	 * on an error the project is left as it was. The project's database and
	 * map tree in memory are those of the converted project on success.
	 * @param backup_path receives the absolute path of the backup folder
	 * @return an error message, or an empty string
	 */
	static QString to2003(Project& project, const Options& options = {}, QString* backup_path = nullptr);

	/**
	 * Gives an RPG Maker 2000 database the data of RPG Maker 2003, in memory.
	 * @param system2_name the System2 graphic, when the database has none
	 */
	static void convertDatabase(lcf::rpg::Database& db, const Options& options, const QString& system2_name);

	/** The 32 battler animations of a new RPG Maker 2003 project (graphics of the RTP) */
	static std::vector<lcf::rpg::BattlerAnimation> defaultBattlerAnimations();
};
