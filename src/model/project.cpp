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
#include "common/ini_file.h"
#include "defines.h"
#include "common/filefinder.h"
#include "common/scope_guard.h"
#include "core.h"
#include "data_merge.h"
#include "common/logging.h"
#include <lcf/inireader.h>
#include <lcf/reader_util.h>
#include <lcf/ldb/reader.h>
#include <lcf/lmu/reader.h>
#include <lcf/lmt/reader.h>
#include <lcf/encoder.h>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryFile>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <sstream>
#include "common/dbstring.h"
#include <memory>
#include <fstream>

namespace {
/** The bytes of a file, read at once and checked to be one version (another
 *  program can replace it meanwhile). std::ifstream would keep the file open
 *  while parsing, which blocks a rename over it on Windows. */
std::optional<std::istringstream> readFile(const QString& path, ProjectWatcher::Stamp* stamp = nullptr) {
	for (int attempt = 0; attempt < 3; ++attempt) {
		const auto before = ProjectWatcher::stampOf(path);
		QFile file(path);
		if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
			return std::nullopt;
		}
		QByteArray bytes = file.readAll();
		file.close();
		if (ProjectWatcher::stampOf(path) == before || attempt == 2) {
			if (stamp) {
				*stamp = before;
			}
			return std::istringstream(bytes.toStdString());
		}
	}
	return std::nullopt;
}

/** Writes what write makes into path at once: a file written next to it is
 *  renamed over it, so another program never reads half a file. Only a
 *  written version is the Editor's (acknowledged to the watcher). */
bool writeFile(const QString& path, const std::function<bool(std::ostream&)>& write) {
	std::ostringstream out;
	if (!write(out)) {
		return false;
	}
	const std::string bytes = out.str();
	const QFileInfo info(path);
	QTemporaryFile file(info.dir().filePath(QStringLiteral(".%1.XXXXXX").arg(info.fileName())));
	if (!file.open() || file.write(bytes.data(), static_cast<qint64>(bytes.size())) != static_cast<qint64>(bytes.size())
			|| !file.flush()) {
		return false;
	}
	file.setPermissions(info.exists() ? info.permissions()
		: QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther);
	file.close();
	// the version written: a rename keeps it, whoever writes the file next
	const auto stamp = ProjectWatcher::stampOf(file.fileName());
	std::error_code error;
	std::filesystem::rename(std::filesystem::path(file.fileName().toStdU16String()),
		std::filesystem::path(info.absoluteFilePath().toStdU16String()), error);
	if (error) {
		return false;
	}
	file.setAutoRemove(false);
	core().watcher().acknowledge(info.absoluteFilePath(), stamp);
	return true;
}
}

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
		if (!file.open(QIODevice::ReadOnly)) {
			// nothing to read: the template is reported below
			return std::istringstream();
		}
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
			// from UTF-8 to the code page of the game
			encoder.Decode(ini);
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
	}
	auto db = std::make_unique<lcf::rpg::Database>();
	if (!reloadDatabase(db.get())) {
		return false;
	}
	auto treemap = std::make_unique<lcf::rpg::TreeMap>();
	if (!reloadTreeMap(treemap.get())) {
		return false;
	}
	m_data = { *this, *db, *treemap };
	m_savedTreeMap = *treemap;
	return true;
}

bool Project::reloadDatabase() {
	return reloadDatabase(&m_data.database());
}

bool Project::reloadDatabase(lcf::rpg::Database* target) {
	const QString file = databaseFile();
	ProjectWatcher::Stamp stamp;
	auto in = readFile(file, &stamp);
	if (!in) {
		return false;
	}
	auto db = projectType() == FileFinder::ProjectType::Legacy
		? lcf::LDB_Reader::Load(*in, encoding().toStdString())
		: lcf::LDB_Reader::LoadXml(*in);
	if (db == nullptr) {
		return false;
	}
	*target = std::move(*db);
	core().watcher().acknowledge(file, stamp);
	return true;
}

bool Project::reloadTreeMap() {
	if (!reloadTreeMap(&m_data.treeMap())) {
		return false;
	}
	m_savedTreeMap = m_data.treeMap();
	return true;
}

bool Project::reloadTreeMap(lcf::rpg::TreeMap* target) {
	const QString file = treeMapFile();
	ProjectWatcher::Stamp stamp;
	auto in = readFile(file, &stamp);
	if (!in) {
		return false;
	}
	auto treemap = projectType() == FileFinder::ProjectType::Legacy
		? lcf::LMT_Reader::Load(*in, encoding().toStdString())
		: lcf::LMT_Reader::LoadXml(*in);
	if (treemap == nullptr) {
		return false;
	}
	*target = std::move(*treemap);
	core().watcher().acknowledge(file, stamp);
	return true;
}

QString Project::mapFile(int index) const {
	const QString ext = projectType() == FileFinder::ProjectType::EasyRpg ? "emu" : "lmu";
	return findFileOrDefault(QString("Map%1.%2").arg(QString::number(index), 4, QLatin1Char('0')).arg(ext));
}

QString Project::databaseFile() const {
	return findFileOrDefault(projectType() == FileFinder::ProjectType::Legacy ? RM_DB : EASY_DB);
}

QString Project::treeMapFile() const {
	return findFileOrDefault(projectType() == FileFinder::ProjectType::Legacy ? RM_MT : EASY_MT);
}

std::unique_ptr<lcf::rpg::Map> Project::loadMap(int index, bool remember) const {
	const QString file = mapFile(index);
	ProjectWatcher::Stamp stamp;
	auto in = readFile(file, &stamp);
	if (!in) {
		return nullptr;
	}
	auto map = projectType() == FileFinder::ProjectType::EasyRpg
		? lcf::LMU_Reader::LoadXml(*in)
		: lcf::LMU_Reader::Load(*in, encoding().toStdString());
	if (map && remember) {
		core().watcher().acknowledge(file, stamp);
	}
	return map;
}

bool Project::saveMap(lcf::rpg::Map& map, int index, bool incSavecount) {
	auto lcf_engine = lcf::GetEngineVersion(m_data.database());
	if (incSavecount) {
		lcf::LMU_Reader::PrepareSave(map);
	}
	const bool saved = writeFile(mapFile(index), [&](std::ostream& out) {
		return projectType() == FileFinder::ProjectType::EasyRpg
			? lcf::LMU_Reader::SaveXml(out, map, lcf_engine)
			: lcf::LMU_Reader::Save(out, map, lcf_engine, encoding().toStdString());
	});
	// not written: not counted, a second try counts once
	if (!saved && incSavecount) {
		--map.save_count;
	}
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

bool Project::saveGameTitle(const QString& gameTitle) {
	if (projectType() == FileFinder::ProjectType::EasyRpg) {
		QSettings ini(projectDir().filePath(EASY_CFG), QSettings::IniFormat);
		ini.setValue(QStringLiteral("RPG_RT/" GAMETITLE), gameTitle);
		ini.sync();
		if (ini.status() != QSettings::NoError) {
			return false;
		}
		setGameTitle(gameTitle);
		return true;
	}

	// RPG_RT.ini is in the code page of the game and RPG Maker's line ends:
	// only the GameTitle line of [RPG_RT] is replaced (or added)
	std::string title = gameTitle.toStdString();
	lcf::Encoder encoder(encoding().toStdString());
	if (encoder.IsOk()) {
		// from UTF-8 to the code page of the game
		encoder.Decode(title);
	}
	if (!IniFile::write(findFileOrDefault(RM_INI), "RPG_RT", {{GAMETITLE, QByteArray::fromStdString(title)}})) {
		return false;
	}
	setGameTitle(gameTitle);
	return true;
}

QString Project::encodingSetting() const {
	if (projectType() == FileFinder::ProjectType::EasyRpg) {
		return {};
	}
	for (const auto& [key, value] : IniFile::read(findFileOrDefault(RM_INI), "EasyRPG")) {
		if (key.compare("Encoding", Qt::CaseInsensitive) == 0) {
			return QString::fromLatin1(value);
		}
	}
	return {};
}

bool Project::saveEncodingSetting(const QString& codepage) {
	// [EasyRPG] Encoding of RPG_RT.ini, the code page the Player reads (none: detected)
	std::optional<QByteArray> value;
	if (!codepage.isEmpty()) {
		value = codepage.toLatin1();
	}
	if (!IniFile::write(findFileOrDefault(RM_INI), "EasyRPG", {{"Encoding", value}})) {
		return false;
	}
	detectEncoding();
	return true;
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

	const bool saved = writeFile(databaseFile(), [&](std::ostream& out) {
		return projectType() == FileFinder::ProjectType::Legacy
			? lcf::LDB_Reader::Save(out, m_data.database(), encoding().toStdString())
			: lcf::LDB_Reader::SaveXml(out, m_data.database());
	});
	// not written: not counted, a second try counts once
	if (!saved && inc_savecount) {
		--m_data.database().system.save_count;
	}
	return saved;
}

bool Project::saveTreeMap() {
	const bool saved = writeTreeMap(m_data.treeMap());
	if (saved) {
		m_savedTreeMap = m_data.treeMap();
	}
	return saved;
}

bool Project::saveTreeMapEntry(const lcf::rpg::MapInfo& info) {
	lcf::rpg::TreeMap tree = m_savedTreeMap;
	auto entry = std::find_if(tree.maps.begin(), tree.maps.end(), [&](const lcf::rpg::MapInfo& map) {
		return map.ID == info.ID;
	});
	if (entry == tree.maps.end()) {
		return false;
	}
	// the file's tree keeps its shape: only the entry's own data changes
	lcf::rpg::MapInfo changed = info;
	changed.parent_map = entry->parent_map;
	changed.indentation = entry->indentation;
	changed.expanded_node = entry->expanded_node;
	*entry = std::move(changed);
	if (!writeTreeMap(tree)) {
		return false;
	}
	m_savedTreeMap = std::move(tree);
	return true;
}

bool Project::writeTreeMap(const lcf::rpg::TreeMap& tree) {
	auto lcf_engine = lcf::GetEngineVersion(m_data.database());
	return writeFile(treeMapFile(), [&](std::ostream& out) {
		return projectType() == FileFinder::ProjectType::Legacy
			? lcf::LMT_Reader::Save(out, tree, lcf_engine, encoding().toStdString())
			: lcf::LMT_Reader::SaveXml(out, tree, lcf_engine);
	});
}

std::unique_ptr<lcf::rpg::Database> Project::readDatabase() const {
	auto in = readFile(databaseFile());
	if (!in) {
		return nullptr;
	}
	return projectType() == FileFinder::ProjectType::Legacy
		? lcf::LDB_Reader::Load(*in, encoding().toStdString())
		: lcf::LDB_Reader::LoadXml(*in);
}

std::unique_ptr<lcf::rpg::TreeMap> Project::readTreeMap() const {
	auto in = readFile(treeMapFile());
	if (!in) {
		return nullptr;
	}
	return projectType() == FileFinder::ProjectType::Legacy
		? lcf::LMT_Reader::Load(*in, encoding().toStdString())
		: lcf::LMT_Reader::LoadXml(*in);
}

bool Project::mergeTreeMap(QStringList* conflicts) {
	auto theirs = readTreeMap();
	if (!theirs) {
		return false;
	}
	lcf::rpg::TreeMap merged;
	const auto result = DataMerge::treeMap(m_savedTreeMap, m_data.treeMap(), *theirs, merged);
	if (!result.ok()) {
		if (conflicts) {
			*conflicts = result.conflicts;
		}
		return false;
	}
	qCInfo(lcReload) << "map tree merged with the changes on disk";
	m_data.treeMap() = std::move(merged);
	m_savedTreeMap = std::move(*theirs);
	return true;
}

bool Project::treeMapModified() const {
	lcf::rpg::TreeMap tree = m_data.treeMap();
	tree.active_node = m_savedTreeMap.active_node;
	return !(tree == m_savedTreeMap);
}
