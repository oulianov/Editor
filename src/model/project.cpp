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

#include "project.h"
#include "defines.h"
#include "common/filefinder.h"
#include "common/scope_guard.h"
#include "core.h"
#include <lcf/inireader.h>
#include <lcf/reader_util.h>
#include <lcf/ldb/reader.h>
#include <lcf/lmu/reader.h>
#include <lcf/lmt/reader.h>
#include <lcf/encoder.h>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <sstream>
#include "common/dbstring.h"
#include <memory>
#include <fstream>

Project::ProjectList Project::enumerate(const QDir& path) {
	ProjectList prj_list;

	const auto& list = path.entryList(QDir::Dirs);
	for (const QString& item: list) {
		auto res = Project::load(FileFinder::CombinePath(path.absolutePath(), item));
		if (res) {
			prj_list.push_back(res);
		}
	}

	return prj_list;
}

std::shared_ptr<Project> Project::load(const QString& path) {
	return load(QDir(path));
}

std::shared_ptr<Project> Project::load(const QDir& dir) {
	FileFinder::ProjectType project_type = FileFinder::GetProjectType(dir);

	if (project_type == FileFinder::ProjectType::None) {
		return nullptr;
	}

	auto p = std::make_shared<Project>();

	p->setProjectType(project_type);
	p->setProjectDir(dir);
	p->setEncoding("utf-8");

	QString cfg;
	if (project_type == FileFinder::ProjectType::EasyRpg) {
		cfg = FileFinder::Find(dir, EASY_CFG);
	} else {
		cfg = FileFinder::Find(dir, RM_INI);
	}

	if (!cfg.isNull()) {
		lcf::INIReader ini(cfg.toStdString());
		std::string title = std::string(ini.GetString("RPG_RT", GAMETITLE, tr("Untitled").toStdString()));

		if (project_type == FileFinder::ProjectType::Legacy) {
			// Check for game encoding
			std::string enc = std::string(ini.GetString("EasyRPG", "Encoding", ""));
			if (enc.empty()) {
				// Only use the title for encoding detection
				// This is called for all games in the "Open Project" list
				// Upon project load a smarter detection is used
				enc = lcf::ReaderUtil::DetectEncoding(title);
			}

			p->setEncoding(QString::fromStdString(std::string(enc)));
			title = lcf::ReaderUtil::Recode(title, enc);
		}

		p->setGameTitle(QString::fromStdString(std::string(title)));
	}

	return p;
}

QString Project::create(const QDir& dir, const QString& title, Format format) {
	auto read = [](const QString& resource) {
		QFile file(resource);
		file.open(QIODevice::ReadOnly);
		return std::istringstream(file.readAll().toStdString());
	};
	auto db_stream = read(QStringLiteral(":/templates/EASY_RT.edb"));
	auto mt_stream = read(QStringLiteral(":/templates/EASY_RT.emt"));
	auto map_stream = read(QStringLiteral(":/templates/Map0001.emu"));
	auto db = lcf::LDB_Reader::LoadXml(db_stream);
	auto treemap = lcf::LMT_Reader::LoadXml(mt_stream);
	auto map = lcf::LMU_Reader::LoadXml(map_stream);
	if (!db || !treemap || !map) {
		return tr("The template of new projects cannot be read.");
	}

	// The root of the map tree is the game
	if (!treemap->maps.empty()) {
		treemap->maps[0].name = ToDBString(title);
	}

	auto path = [&](const char* name) { return dir.filePath(QString::fromLatin1(name)).toStdString(); };
	bool saved;
	if (format == Format::EasyRpg) {
		saved = lcf::LDB_Reader::SaveXml(path(EASY_DB), *db)
			&& lcf::LMT_Reader::SaveXml(path(EASY_MT), *treemap, lcf::EngineVersion::e2k3)
			&& lcf::LMU_Reader::SaveXml(path("Map0001.emu"), *map, lcf::EngineVersion::e2k3);
		QSettings ini(dir.filePath(EASY_CFG), QSettings::IniFormat);
		ini.setValue(QStringLiteral("RPG_RT/" GAMETITLE), title);
	} else {
		const bool rpg2k3 = format == Format::Rpg2003;
		const auto engine = rpg2k3 ? lcf::EngineVersion::e2k3 : lcf::EngineVersion::e2k;
		db->system.ldb_id = rpg2k3 ? 2003 : 0;
		// the code page of the system, like RPG Maker
		const std::string encoding = lcf::ReaderUtil::GetLocaleEncoding();
		saved = lcf::LDB_Reader::Save(path(RM_DB), *db, encoding)
			&& lcf::LMT_Reader::Save(path(RM_MT), *treemap, engine, encoding)
			&& lcf::LMU_Reader::Save(path("Map0001.lmu"), *map, engine, encoding);

		// RPG_RT.ini in the code page of the game
		std::string ini = "[RPG_RT]\r\nGameTitle=" + title.toStdString() + "\r\nMapEditMode=1\r\nMapEditZoom=1\r\nFullPackageFlag=0\r\n"
			"\r\n[EasyRPG]\r\nEncoding=" + encoding + "\r\n";
		lcf::Encoder encoder(encoding);
		if (encoder.IsOk()) {
			encoder.Encode(ini);
		}
		std::ofstream out(path(RM_INI), std::ios::binary);
		out << ini;
		saved = saved && out.good();
	}
	return saved ? QString() : tr("The files of the new project cannot be written in %1.").arg(dir.absolutePath());
}

bool Project::loadDatabaseAndMapTree() {
	if (projectType() == FileFinder::ProjectType::Legacy) {
		detectEncoding();
		auto db = lcf::LDB_Reader::Load(findFile(RM_DB).toStdString(), encoding().toStdString());
		if (db == nullptr) {
			return false;
		}
		auto treemap = lcf::LMT_Reader::Load(findFile(RM_MT).toStdString(), encoding().toStdString());
		if (treemap == nullptr) {
			return false;
		}
		m_data = { *this, *db, *treemap };
	} else {
		auto db = lcf::LDB_Reader::LoadXml(findFile(EASY_DB).toStdString());
		if (db == nullptr) {
			return false;
		}
		auto treemap = lcf::LMT_Reader::LoadXml(findFile(EASY_MT).toStdString());
		if (treemap == nullptr) {
			return false;
		}
		m_data = { *this, *db, *treemap };
	}

	return true;
}

bool Project::reloadDatabase() {
	std::unique_ptr<lcf::rpg::Database> db;
	if (projectType() == FileFinder::ProjectType::Legacy) {
		db = lcf::LDB_Reader::Load(findFile(RM_DB).toStdString(), encoding().toStdString());
	} else {
		db = lcf::LDB_Reader::LoadXml(findFile(EASY_DB).toStdString());
	}
	if (db == nullptr) {
		return false;
	}
	m_data.database() = std::move(*db);
	return true;
}

bool Project::reloadTreeMap() {
	std::unique_ptr<lcf::rpg::TreeMap> treemap;
	if (projectType() == FileFinder::ProjectType::Legacy) {
		treemap = lcf::LMT_Reader::Load(findFile(RM_MT).toStdString(), encoding().toStdString());
	} else {
		treemap = lcf::LMT_Reader::LoadXml(findFile(EASY_MT).toStdString());
	}
	if (treemap == nullptr) {
		return false;
	}
	m_data.treeMap() = std::move(*treemap);
	return true;
}

std::unique_ptr<lcf::rpg::Map> Project::loadMap(int index) const {
	QString ext = projectType() == FileFinder::ProjectType::EasyRpg ? "emu" : "lmu";

	QString file = QString("Map%1.%2")
			.arg(QString::number(index), 4, QLatin1Char('0')).arg(ext);
	QString mapFile = findFile(file);

	if (mapFile.isNull()) {
		return nullptr;
	}

	if (projectType() == FileFinder::ProjectType::EasyRpg) {
		return lcf::LMU_Reader::LoadXml(mapFile.toStdString());
	} else {
		return lcf::LMU_Reader::Load(mapFile.toStdString(), encoding().toStdString());
	}
}

bool Project::saveMap(lcf::rpg::Map& map, int index, bool incSavecount) {
	auto lcf_engine = lcf::GetEngineVersion(m_data.database());
	QString ext = projectType() == FileFinder::ProjectType::EasyRpg ? "emu" : "lmu";

	QString file = QString("Map%1.%2")
			.arg(QString::number(index), 4, QLatin1Char('0')).arg(ext);
	QString mapFile = findFileOrDefault(file);

	if (incSavecount) {
		lcf::LMU_Reader::PrepareSave(map);
	}

	bool saved;
	if (projectType() == FileFinder::ProjectType::EasyRpg) {
		saved = lcf::LMU_Reader::SaveXml(mapFile.toStdString(), map, lcf_engine);
	} else {
		saved = lcf::LMU_Reader::Save(mapFile.toStdString(), map, lcf_engine, encoding().toStdString());
	}
	core().watcher().acknowledge(mapFile);
	return saved;
}

void Project::relocate(const QDir& newDir, FileFinder::ProjectType newProjectType) {
	setProjectDir(newDir);
	setProjectType(newProjectType);
}

QString Project::findFile(const QString& filename, FileFinder::FileType type) const {
	return FileFinder::Find(projectDir().absolutePath(), filename, type);
}

QString Project::findFile(const QString& dir, const QString& filename, FileFinder::FileType type) const {
	QString found = FileFinder::Find(projectDir().absolutePath(), dir, filename, type);
	const QString rtp = core().rtpPath(QString());
	if (found.isEmpty() && !rtp.isEmpty()) {
		// the game's own files first, then the RTP's (monsters, charsets... of RPG Maker)
		found = FileFinder::Find(QDir(rtp), dir, filename, type);
	}
	return found;
}

QString Project::findFileOrDefault(const QString& filename) const {
	QString found = findFile(filename);
	return found.isEmpty() ? FileFinder::CombinePath(projectDir().absolutePath(), filename) : found;
}

QString Project::findDirectory(const QString& dir) const {
	return FileFinder::Find(projectDir().absolutePath(), dir, FileFinder::FileType::Default, QDir::Dirs);
}

QString Project::findDirectory(const QString& baseDir, const QString& dir) const {
	return FileFinder::Find(projectDir().absolutePath(), baseDir, dir, FileFinder::FileType::Default, QDir::Dirs);
}

QString Project::detectEncoding() {
	if (projectType() == FileFinder::ProjectType::EasyRpg) {
		setEncoding("utf-8");
		return encoding();
	}

	auto cfg = findFile(RM_INI);
	std::string enc;
	std::string title;
	if (!cfg.isNull()) {
		lcf::INIReader ini(cfg.toStdString());
		title = ini.GetString("RPG_RT", GAMETITLE, tr("Untitled").toStdString());
		enc = ini.GetString("EasyRPG", "Encoding", "");
	}

	if (enc.empty()) {
		std::ifstream i(findFile(RM_DB).toStdString(), std::ios::binary);
		if (i) {
			auto db = lcf::LDB_Reader::Load(i);
			if (db) {
				std::string dbenc = lcf::ReaderUtil::DetectEncoding(*db);
				lcf::Encoder encoder(dbenc);
				if (encoder.IsOk()) {
					enc = dbenc;
				}
			}
		}
	}

	setEncoding(QString::fromStdString(enc));
	setGameTitle(QString::fromStdString(lcf::ReaderUtil::Recode(title, enc)));

	return encoding();
}

QString Project::encoding() const {
	return m_encoding;
}

void Project::setEncoding(const QString& encoding) {
	m_encoding = encoding;
}

QDir Project::projectDir() const {
	return m_projectDir;
}

void Project::setProjectDir(const QDir& projectDir) {
	m_projectDir = projectDir;
}

QString Project::gameTitle() const {
	return m_gameTitle;
}

void Project::setGameTitle(const QString& gameTitle) {
	m_gameTitle = gameTitle;
}

FileFinder::ProjectType Project::projectType() const {
	return m_projectType;
}

void Project::setProjectType(FileFinder::ProjectType projectType) {
	m_projectType = projectType;
}

ProjectData &Project::projectData() {
	return m_data;
}

const ProjectData &Project::projectData() const {
	return m_data;
}

lcf::rpg::Database& Project::database() {
	return m_data.database();
}

const lcf::rpg::Database& Project::database() const {
	return m_data.database();
}

lcf::rpg::TreeMap& Project::treeMap() {
	return m_data.treeMap();
}

const lcf::rpg::TreeMap& Project::treeMap() const {
	return m_data.treeMap();
}

bool Project::saveDatabase(bool inc_savecount) {
	if (inc_savecount) {
		lcf::LDB_Reader::PrepareSave(m_data.database());
	}

	bool saved;
	QString file;
	if (projectType() == FileFinder::ProjectType::Legacy) {
		file = findFileOrDefault(RM_DB);
		saved = lcf::LDB_Reader::Save(file.toStdString(), m_data.database(), encoding().toStdString());
	} else {
		file = findFileOrDefault(EASY_DB);
		saved = lcf::LDB_Reader::SaveXml(file.toStdString(), m_data.database());
	}
	core().watcher().acknowledge(file);
	return saved;
}

bool Project::saveTreeMap() {
	auto lcf_engine = lcf::GetEngineVersion(m_data.database());
	bool saved;
	QString file;
	if (projectType() == FileFinder::ProjectType::Legacy) {
		file = findFileOrDefault(RM_MT);
		saved = lcf::LMT_Reader::Save(file.toStdString(), m_data.treeMap(), lcf_engine, encoding().toStdString());
	} else {
		file = findFileOrDefault(EASY_MT);
		saved = lcf::LMT_Reader::SaveXml(file.toStdString(), m_data.treeMap(), lcf_engine);
	}
	core().watcher().acknowledge(file);
	return saved;
}
