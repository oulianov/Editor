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

#include <QDir>
#include <QList>
#include <QSettings>
#include <QString>
#include <QCoreApplication>
#include <memory>

#include "common/filefinder.h"
#include "project_data.h"
#include <lcf/rpg/database.h>
#include <lcf/rpg/map.h>
#include <lcf/rpg/treemap.h>
#include <optional>

class Project {
	Q_DECLARE_TR_FUNCTIONS(Project)
public:
	using ProjectList = std::vector<std::shared_ptr<Project>>;

	static ProjectList enumerate(const QDir& path);

	static std::shared_ptr<Project> load(const QString& path);

	enum class Format { Rpg2003, Rpg2000, EasyRpg };
	/**
	 * Writes a new game in dir from the template bundled with the Editor
	 * (database using the RTP, a first map). The RPG Maker formats can be
	 * opened by RPG Maker 2000/2003 too. Returns an error message, or an
	 * empty string.
	 */
	static QString create(const QDir& dir, const QString& title, Format format);
	static std::shared_ptr<Project> load(const QDir& path);

	bool loadDatabaseAndMapTree();
	/** Read the database again, after another program changed it */
	bool reloadDatabase();
	/** Read the map tree again, after another program changed it */
	bool reloadTreeMap();
	/** Reads a map from its file. The Editor keeps the map it reads with
	 *  remember (an open map): its version is then the one the Editor has. */
	std::unique_ptr<lcf::rpg::Map> loadMap(int index, bool remember = false) const;
	bool saveMap(lcf::rpg::Map& map, int index, bool incSavecount = true);
	/** The files of the data, where they are or would be written */
	QString mapFile(int index) const;
	QString databaseFile() const;
	QString treeMapFile() const;
	/** The map tree differs from its file (the last map shown aside) */
	bool treeMapModified() const;
	/** The database and the map tree as their files are now, without
	 *  taking them: what another program changed, to merge */
	std::unique_ptr<lcf::rpg::Database> readDatabase() const;
	std::unique_ptr<lcf::rpg::TreeMap> readTreeMap() const;
	/** The map tree file changed since read: merges what changed there into
	 *  the map tree (true), else lists in conflicts what both sides changed */
	bool mergeTreeMap(QStringList* conflicts);

	void relocate(const QDir& newDir, FileFinder::ProjectType newProjectType);

	QString findFile(const QString& filename, FileFinder::FileType type = FileFinder::FileType::Default) const;
	QString findFile(const QString& dir, const QString& filename, FileFinder::FileType type = FileFinder::FileType::Default) const;
	/**
	 * Does a case-insensitive file search. When the file is not found the
	 * requested filename is returned.
	 * @param filename File to find
	 * @return Found file or the passed filename when not found
	 */
	QString findFileOrDefault(const QString& filename) const;

	QString findDirectory(const QString& dir) const;
	QString findDirectory(const QString& baseDir, const QString& dir) const;

	QString detectEncoding();
	QString encoding() const;
	void setEncoding(const QString& encoding);
	/** [EasyRPG] Encoding of RPG_RT.ini, as written (a code page, "1252"); empty: detected */
	QString encodingSetting() const;
	/** Writes it (empty: removes it) and detects the encoding again */
	bool saveEncodingSetting(const QString& codepage);

	QDir projectDir() const;
	void setProjectDir(const QDir& projectDir);

	QString gameTitle() const;
	void setGameTitle(const QString& gameTitle);
	/** Writes the game title to RPG_RT.ini (or EASY_RT.cfg), the rest of the file unchanged */
	bool saveGameTitle(const QString& gameTitle);

	FileFinder::ProjectType projectType() const;
	void setProjectType(FileFinder::ProjectType projectType);

	lcf::rpg::Database& database();
	const lcf::rpg::Database& database() const;
	lcf::rpg::TreeMap& treeMap();
	const lcf::rpg::TreeMap& treeMap() const;

	ProjectData& projectData();
	const ProjectData& projectData() const;

	bool saveDatabase(bool incSavecount = true);
	bool saveTreeMap();
	/** Writes the map tree as last read or written with only one entry
	 *  replaced by info (found by its id; its parent, its depth and its
	 *  expanded flag kept): what else the map tree holds and its file does
	 *  not (the start positions of the maps not saved yet) stays unsaved.
	 *  The map tree itself is the caller's to change. false when the file
	 *  has no such entry or could not be written. */
	bool saveTreeMapEntry(const lcf::rpg::MapInfo& info);

private:
	FileFinder::ProjectType m_projectType;
	QString m_encoding;
	QDir m_projectDir;
	std::shared_ptr<QSettings> m_projectSettings;
	QString m_gameTitle;
	bool reloadDatabase(lcf::rpg::Database* target);
	bool reloadTreeMap(lcf::rpg::TreeMap* target);
	bool writeTreeMap(const lcf::rpg::TreeMap& tree);

	ProjectData m_data;
	/** The map tree as last read or written */
	lcf::rpg::TreeMap m_savedTreeMap;
};
