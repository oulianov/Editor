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

#include "game_export.h"
#include "defines.h"
#include "export_files.h"
#include "export_references.h"
#include "web_player.h"
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <lcf/encoder.h>
#include <lcf/ldb/reader.h>
#include <lcf/lmt/reader.h>
#include <lcf/lmu/reader.h>
#include <algorithm>
#include <sstream>

namespace {
const ExportPlatform kPlatforms[] = { ExportPlatform::Windows, ExportPlatform::MacOS, ExportPlatform::Linux, ExportPlatform::Web };

std::string toStd(const QByteArray& data)
{
	return std::string(data.constData(), static_cast<size_t>(data.size()));
}

QByteArray readFile(const QString& path)
{
	QFile file(path);
	return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool writeFile(const QString& path, const QByteArray& data)
{
	QSaveFile file(path);
	return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}

/** The file type of a resource folder, for the extensions to try */
FileFinder::FileType typeOf(const QString& folder)
{
	if (folder == QLatin1String(MUSIC)) {
		return FileFinder::FileType::Music;
	} else if (folder == QLatin1String(SOUND)) {
		return FileFinder::FileType::Sound;
	} else if (folder == QLatin1String(MOVIE)) {
		return FileFinder::FileType::Video;
	}
	return FileFinder::FileType::Image;
}

/**
 * The files of the folders of a game or of the RTP, by lowercase name, as
 * the Player finds them: whatever the case, the extension given or tried.
 */
class ResourceIndex
{
public:
	explicit ResourceIndex(const QString& base) : m_base(base) {}

	/** The real name of the folder in the base, empty if none */
	QString folder(const QString& name)
	{
		if (m_base.isEmpty()) {
			return QString();
		}
		if (!m_folders.contains(name.toLower())) {
			QString real;
			for (const QString& entry : QDir(m_base).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
				if (entry.compare(name, Qt::CaseInsensitive) == 0) {
					real = entry;
					break;
				}
			}
			m_folders.insert(name.toLower(), real);
		}
		return m_folders.value(name.toLower());
	}

	/** The files of a folder: lowercase name -> real name */
	const QHash<QString, QString>& files(const QString& folder_name)
	{
		const QString key = folder_name.toLower();
		if (!m_files.contains(key)) {
			QHash<QString, QString> files;
			const QString real = folder(folder_name);
			if (!real.isEmpty()) {
				for (const QString& file : QDir(m_base + QLatin1Char('/') + real).entryList(QDir::Files)) {
					files.insert(file.toLower(), file);
				}
			}
			m_files.insert(key, files);
		}
		return m_files[key];
	}

	/** The real file name of a resource, empty if not there */
	QString find(const QString& folder_name, const QString& name)
	{
		const auto& list = files(folder_name);
		const QString lower = name.toLower();
		for (const QString& ext : FileFinder::GetFiltersForType(typeOf(folder_name))) {
			auto it = list.constFind(lower + ext);
			if (it != list.constEnd()) {
				return it.value();
			}
		}
		// a name written with its extension
		return list.value(lower);
	}

	QString path(const QString& folder_name, const QString& file) const
	{
		return m_base + QLatin1Char('/') + m_folders.value(folder_name.toLower()) + QLatin1Char('/') + file;
	}

private:
	QString m_base;
	QHash<QString, QString> m_folders;
	QHash<QString, QHash<QString, QString>> m_files;
};
}

QString ExportPreset::id(ExportPlatform platform)
{
	switch (platform) {
	case ExportPlatform::Windows: return QStringLiteral("windows");
	case ExportPlatform::MacOS: return QStringLiteral("macos");
	case ExportPlatform::Linux: return QStringLiteral("linux");
	case ExportPlatform::Web: return QStringLiteral("web");
	}
	return QString();
}

QString ExportPreset::name(ExportPlatform platform)
{
	switch (platform) {
	case ExportPlatform::Windows: return QStringLiteral("Windows");
	case ExportPlatform::MacOS: return QStringLiteral("macOS");
	case ExportPlatform::Linux: return QStringLiteral("Linux");
	case ExportPlatform::Web: return GameExporter::tr("Web (HTML5)");
	}
	return QString();
}

ExportSettings ExportSettings::load(const QDir& project_dir, const QString& title)
{
	ExportSettings settings;
	settings.title = title;
	// next to the project: "Game - Export/Windows"...
	const QString base = QStringLiteral("../%1 - Export/").arg(project_dir.dirName());
	for (ExportPlatform platform : kPlatforms) {
		ExportPreset preset;
		preset.platform = platform;
		// the system of this computer, first
#if defined(Q_OS_MACOS)
		preset.enabled = platform == ExportPlatform::MacOS;
#elif defined(Q_OS_WIN)
		preset.enabled = platform == ExportPlatform::Windows;
#else
		preset.enabled = platform == ExportPlatform::Linux;
#endif
		preset.path = base + ExportPreset::name(platform).section(QLatin1Char(' '), 0, 0);
		preset.zip = platform == ExportPlatform::Web;
		settings.presets << preset;
	}

	QFile file(project_dir.filePath(QString::fromLatin1(kFileName)));
	if (!file.open(QIODevice::ReadOnly)) {
		return settings;
	}
	const QJsonObject json = QJsonDocument::fromJson(file.readAll()).object();
	if (json.contains(QStringLiteral("title"))) {
		settings.title = json.value(QStringLiteral("title")).toString();
	}
	settings.version = json.value(QStringLiteral("version")).toString();
	settings.icon = json.value(QStringLiteral("icon")).toString();
	settings.include_rtp = json.value(QStringLiteral("include_rtp")).toBool(settings.include_rtp);
	settings.clean_database = json.value(QStringLiteral("clean_database")).toBool(settings.clean_database);
	settings.protect = json.value(QStringLiteral("protect")).toBool(settings.protect);
	const QJsonObject presets = json.value(QStringLiteral("presets")).toObject();
	for (ExportPreset& preset : settings.presets) {
		const QJsonObject p = presets.value(ExportPreset::id(preset.platform)).toObject();
		if (p.isEmpty()) {
			continue;
		}
		preset.enabled = p.value(QStringLiteral("enabled")).toBool(preset.enabled);
		preset.path = p.value(QStringLiteral("path")).toString(preset.path);
		preset.launcher = p.value(QStringLiteral("launcher")).toString();
		preset.zip = p.value(QStringLiteral("zip")).toBool(preset.zip);
		preset.customize = p.value(QStringLiteral("customize")).toBool(preset.customize);
	}
	return settings;
}

bool ExportSettings::save(const QDir& project_dir) const
{
	QJsonObject presets_json;
	for (const ExportPreset& preset : presets) {
		QJsonObject p;
		p.insert(QStringLiteral("enabled"), preset.enabled);
		// relative to the project, so that the settings travel with it
		p.insert(QStringLiteral("path"), QDir::isAbsolutePath(preset.path) ? project_dir.relativeFilePath(preset.path) : preset.path);
		p.insert(QStringLiteral("launcher"), preset.launcher);
		p.insert(QStringLiteral("zip"), preset.zip);
		p.insert(QStringLiteral("customize"), preset.customize);
		presets_json.insert(ExportPreset::id(preset.platform), p);
	}
	QJsonObject json;
	json.insert(QStringLiteral("title"), title);
	json.insert(QStringLiteral("version"), version);
	json.insert(QStringLiteral("icon"), icon.isEmpty() || !QDir::isAbsolutePath(icon) ? icon : project_dir.relativeFilePath(icon));
	json.insert(QStringLiteral("include_rtp"), include_rtp);
	json.insert(QStringLiteral("clean_database"), clean_database);
	json.insert(QStringLiteral("protect"), protect);
	json.insert(QStringLiteral("presets"), presets_json);
	return writeFile(project_dir.filePath(QString::fromLatin1(kFileName)), QJsonDocument(json).toJson());
}

ExportPreset& ExportSettings::preset(ExportPlatform platform)
{
	for (ExportPreset& preset : presets) {
		if (preset.platform == platform) {
			return preset;
		}
	}
	presets << ExportPreset{ platform };
	return presets.last();
}

GameExporter::GameExporter(const ExportSource& source, const ExportSettings& settings, QObject* parent) :
	QObject(parent), m_source(source), m_settings(settings)
{
}

void GameExporter::cancel()
{
	m_cancelled = true;
}

QStringList GameExporter::folders() const
{
	return m_folders;
}

bool GameExporter::isLauncher(ExportPlatform platform, const QString& path)
{
	const QFileInfo info(path);
	switch (platform) {
	case ExportPlatform::Windows:
		return path == QLatin1String(ExportPreset::kGameRuntime)
			|| (info.isFile() && info.suffix().compare(QStringLiteral("exe"), Qt::CaseInsensitive) == 0);
	case ExportPlatform::MacOS:
		return info.isDir() && info.suffix() == QStringLiteral("app") && QFileInfo(path + QStringLiteral("/Contents/MacOS")).isDir();
	case ExportPlatform::Linux:
		return info.isFile();
	case ExportPlatform::Web:
		// the shell page, the program and its WebAssembly: a release, or a Player build folder
		return !WebPlayer::page(path).isEmpty();
	}
	return false;
}

QString GameExporter::checkFolder(const QDir& project_dir, const QString& folder)
{
	if (folder.trimmed().isEmpty()) {
		return tr("No export folder.");
	}
	const QString path = QDir::cleanPath(project_dir.absoluteFilePath(folder));
	const QString project = QDir::cleanPath(project_dir.absolutePath());
	const auto inside = [](const QString& path, const QString& dir) {
		return path.compare(dir, Qt::CaseInsensitive) == 0 || path.startsWith(dir + QLatin1Char('/'), Qt::CaseInsensitive);
	};
	if (inside(path, project)) {
		return tr("The export folder cannot be in the game folder.");
	}
	if (inside(project, path)) {
		return tr("The export folder cannot contain the game folder.");
	}
	// the folder is emptied: never a folder of the system or of the user
	if (QDir(path).isRoot() || path == QDir::cleanPath(QDir::homePath()) || path.count(QLatin1Char('/')) < 2) {
		return tr("Choose a folder of its own for the export.");
	}
	return QString();
}

bool GameExporter::usesRpgsync(const QDir& project_dir)
{
	const QByteArray pyproject = readFile(project_dir.filePath(QStringLiteral("Scripts/pyproject.toml")));
	return pyproject.contains("[tool.rpgsync]") || QFileInfo(project_dir.filePath(QStringLiteral("Scripts/.rpgsync"))).isDir();
}

bool GameExporter::isExcluded(const QString& path, bool rpgsync)
{
	const QStringList parts = path.split(QLatin1Char('/'));
	for (const QString& part : parts) {
		// hidden files of tools: .git, .DS_Store, .rpgsync-*, .vscode...
		if (part.startsWith(QLatin1Char('.')) || part.compare(QStringLiteral("__pycache__"), Qt::CaseInsensitive) == 0) {
			return true;
		}
	}
	if (rpgsync && parts.size() > 1 && parts.first().compare(QStringLiteral("Scripts"), Qt::CaseInsensitive) == 0) {
		return true;
	}
	const QString name = parts.last().toLower();
	static const QRegularExpression backup(QStringLiteral("(\\.bak|\\.tmp|\\.rpgsync-tmp|\\.pyc|~)$"));
	if (name.contains(backup) || name == QStringLiteral("thumbs.db") || name == QStringLiteral("desktop.ini")) {
		return true;
	}
	if (parts.size() == 1) {
		// saves of the test plays (and of DynRPG), the export settings
		static const QRegularExpression save(QStringLiteral("^save\\d+\\.(lsd|dyn)$"));
		if (name.contains(save) || name == QLatin1String(ExportSettings::kFileName)) {
			return true;
		}
	}
	return false;
}

void GameExporter::log(const QString& text, int level)
{
	emit message(text, level);
}

bool GameExporter::step()
{
	emit progress(++m_step, m_steps);
	return !m_cancelled;
}

bool GameExporter::run()
{
	m_folders.clear();
	if (!prepare()) {
		return false;
	}
	int presets = 0;
	for (const ExportPreset& preset : m_settings.presets) {
		presets += preset.enabled;
	}
	if (presets == 0) {
		log(tr("No platform is checked."), Error);
		return false;
	}
	m_step = 0;
	m_steps = presets * (m_files.size() + 4);
	bool ok = true;
	for (const ExportPreset& preset : m_settings.presets) {
		if (!preset.enabled) {
			continue;
		}
		if (m_cancelled) {
			log(tr("Export cancelled."), Error);
			return false;
		}
		log(tr("== %1").arg(ExportPreset::name(preset.platform)));
		ok = exportPreset(preset) && ok;
	}
	emit progress(m_steps, m_steps);
	return ok && !m_cancelled;
}

bool GameExporter::prepare()
{
	const QDir& dir = m_source.dir;
	const bool legacy = m_source.type == FileFinder::ProjectType::Legacy;
	const std::string encoding = m_source.encoding.toStdString();

	// the game files, without the tool files
	const bool rpgsync = usesRpgsync(dir);
	QMap<QString, int> excluded;
	m_files.clear();
	QDirIterator it(dir.absolutePath(), QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QString source = it.next();
		const QString path = dir.relativeFilePath(source);
		if (isExcluded(path, rpgsync)) {
			excluded[path.section(QLatin1Char('/'), 0, 0)]++;
		} else {
			m_files << PlannedFile{ source, path };
		}
		if (m_cancelled) {
			return false;
		}
	}
	if (rpgsync) {
		log(tr("The game is edited with rpgsync: its Scripts folder is not exported."));
	}
	for (auto e = excluded.constBegin(); e != excluded.constEnd(); ++e) {
		log(tr("Not exported (tool or test file): %1%2").arg(e.key(), e.value() > 1 ? tr(" (%1 files)").arg(e.value()) : QString()));
	}

	// the real names of the data files
	auto rootFile = [&](const char* name) -> QString {
		for (const PlannedFile& file : m_files) {
			if (!file.path.contains(QLatin1Char('/')) && file.path.compare(QLatin1String(name), Qt::CaseInsensitive) == 0) {
				return file.path;
			}
		}
		return QString();
	};
	const QString db_name = rootFile(legacy ? RM_DB : EASY_DB);
	const QString tree_name = rootFile(legacy ? RM_MT : EASY_MT);
	if (db_name.isEmpty() || tree_name.isEmpty()) {
		log(tr("The database or the map tree of the game is missing."), Error);
		return false;
	}

	// what the game uses: database, map tree and every map file (a teleport can reach maps outside the tree)
	std::istringstream db_stream(toStd(readFile(dir.filePath(db_name))));
	std::istringstream tree_stream(toStd(readFile(dir.filePath(tree_name))));
	auto db = legacy ? lcf::LDB_Reader::Load(db_stream, encoding) : lcf::LDB_Reader::LoadXml(db_stream);
	auto tree = legacy ? lcf::LMT_Reader::Load(tree_stream, encoding) : lcf::LMT_Reader::LoadXml(tree_stream);
	if (!db || !tree) {
		log(tr("The database or the map tree of the game cannot be read."), Error);
		return false;
	}
	GameReferences::Maps maps;
	static const QRegularExpression map_file(QStringLiteral("^map(\\d{4})\\.(lmu|emu)$"), QRegularExpression::CaseInsensitiveOption);
	for (const PlannedFile& file : m_files) {
		const auto match = map_file.match(file.path);
		if (!match.hasMatch() || (match.captured(2).compare(QStringLiteral("lmu"), Qt::CaseInsensitive) == 0) != legacy) {
			continue;
		}
		std::istringstream stream(toStd(readFile(file.source)));
		auto map = legacy ? lcf::LMU_Reader::Load(stream, encoding) : lcf::LMU_Reader::LoadXml(stream);
		if (!map) {
			log(tr("%1 cannot be read: its files are not checked.").arg(file.path), Warning);
			continue;
		}
		maps[match.captured(1).toInt()] = std::move(map);
	}
	GameReferences refs;
	refs.collect(*db, *tree, maps, encoding);

	// the files the game uses that are not in it: from the RTP, or missing
	ResourceIndex game(dir.absolutePath());
	ResourceIndex rtp(m_source.rtp.isEmpty() ? QString() : QDir(m_source.rtp).absolutePath());
	QStringList from_rtp;
	auto addRtp = [&](const QString& folder, const QString& file) {
		const QString game_folder = game.folder(folder).isEmpty() ? folder : game.folder(folder);
		const QString path = game_folder + QLatin1Char('/') + file;
		if (from_rtp.contains(path)) {
			return;
		}
		from_rtp << path;
		if (m_settings.include_rtp) {
			m_files << PlannedFile{ rtp.path(folder, file), path };
			log(tr("RTP file added: %1").arg(path), Added);
		}
	};
	QStringList missing;
	for (auto f = refs.files.constBegin(); f != refs.files.constEnd(); ++f) {
		QStringList names(f.value().begin(), f.value().end());
		names.sort(Qt::CaseInsensitive);
		for (const QString& name : names) {
			if (!game.find(f.key(), name).isEmpty()) {
				continue;
			}
			const QString file = rtp.find(f.key(), name);
			if (!file.isEmpty()) {
				addRtp(f.key(), file);
			} else {
				missing << f.key() + QLatin1Char('/') + name;
			}
		}
	}
	// picture names completed by a variable: every RTP picture starting the same way
	for (auto f = refs.filePrefixes.constBegin(); f != refs.filePrefixes.constEnd(); ++f) {
		for (const QString& prefix : f.value()) {
			for (const QString& file : rtp.files(f.key())) {
				if (file.startsWith(prefix, Qt::CaseInsensitive) && game.find(f.key(), QFileInfo(file).completeBaseName()).isEmpty()) {
					addRtp(f.key(), file);
				}
			}
		}
	}
	for (const QString& file : missing) {
		log(tr("Used by the game but found neither in the game nor in the RTP: %1").arg(file), Warning);
	}
	refs.dynamicFiles.removeDuplicates();
	for (int i = 0; i < refs.dynamicFiles.size() && i < 20; ++i) {
		log(tr("Cannot be checked: %1").arg(refs.dynamicFiles[i]), Warning);
	}
	if (refs.dynamicFiles.size() > 20) {
		log(tr("...and %1 more names given by variables.").arg(refs.dynamicFiles.size() - 20), Warning);
	}
	if (!from_rtp.isEmpty() && !m_settings.include_rtp) {
		log(tr("The game uses %1 RTP files: players need the RTP (the web Player has none).").arg(from_rtp.size()), Warning);
	} else if (!from_rtp.isEmpty()) {
		log(tr("%1 RTP files added: the game does not need the RTP.").arg(from_rtp.size()));
	}
	if (m_source.rtp.isEmpty() && !missing.isEmpty()) {
		log(tr("No RTP folder in the settings: the RTP files cannot be added."), Warning);
	}

	// RPG_RT.ini: the title, and no RTP needed
	m_iniName = rootFile(RM_INI);
	QList<QPair<QByteArray, QByteArray>> values;
	if (!m_settings.title.isEmpty()) {
		std::string title = m_settings.title.toStdString();
		if (legacy) {
			// in the code page of the game, which must be able to write it
			lcf::Encoder encoder(encoding);
			std::string check = title;
			encoder.Encode(check);
			std::string back = check;
			encoder.Decode(back);
			if (encoder.IsOk() && back == title) {
				title = check;
			} else {
				log(tr("The title cannot be written in the code page of the game (%1): RPG_RT.ini keeps its title.").arg(m_source.encoding), Warning);
				title.clear();
			}
		}
		if (!title.empty()) {
			values << qMakePair(QByteArray("GameTitle"), QByteArray::fromStdString(title));
		}
	}
	if (m_settings.include_rtp) {
		values << qMakePair(QByteArray("FullPackageFlag"), QByteArray("1"));
	}
	m_ini.clear();
	if (!values.isEmpty()) {
		m_ini = ExportFiles::setIniValues(m_iniName.isEmpty() ? QByteArray() : readFile(dir.filePath(m_iniName)), values);
		if (m_iniName.isEmpty()) {
			m_iniName = QStringLiteral(RM_INI);
		}
	}

	// a cleaner database: only in the exported copy
	m_database.clear();
	if (m_settings.clean_database) {
		lcf::rpg::Database clean = *db;
		for (const QString& line : DatabaseCleanup::clean(clean, *tree, maps, encoding)) {
			log(line);
		}
		std::ostringstream out;
		const bool saved = legacy ? lcf::LDB_Reader::Save(out, clean, encoding) : lcf::LDB_Reader::SaveXml(out, clean);
		if (!saved) {
			log(tr("The cleaned database cannot be written."), Error);
			return false;
		}
		m_database = QByteArray::fromStdString(out.str());
		m_files.removeIf([&](const PlannedFile& file) { return file.path == db_name; });
	}

	// the protected map tree (the original file, its bytes shifted: nothing lost)
	m_mapTree.clear();
	if (m_settings.protect) {
		if (!legacy) {
			log(tr("Only games in the RPG Maker format can be protected: the EasyRPG format is exported as it is."), Warning);
		} else {
			QString error;
			m_mapTree = ExportFiles::protectMapTree(readFile(dir.filePath(tree_name)), &error);
			if (m_mapTree.isEmpty()) {
				log(error + QLatin1Char(' ') + tr("The game is exported without protection."), Warning);
			}
		}
	}
	m_treeName = tree_name;
	m_files.removeIf([&](const PlannedFile& file) {
		return (!m_ini.isEmpty() && file.path == m_iniName) || (!m_mapTree.isEmpty() && file.path == tree_name);
	});
	return !m_cancelled;
}

bool GameExporter::writeGame(const QString& target)
{
	QDir out(target);
	for (const PlannedFile& file : m_files) {
		const QString path = out.filePath(file.path);
		if (!QDir().mkpath(QFileInfo(path).absolutePath()) || !QFile::copy(file.source, path)) {
			log(tr("Cannot copy %1.").arg(QDir::toNativeSeparators(file.source)), Error);
			return false;
		}
		if (!step()) {
			return false;
		}
	}
	const bool legacy = m_source.type == FileFinder::ProjectType::Legacy;
	if (!m_ini.isEmpty() && !writeFile(out.filePath(m_iniName), m_ini)) {
		log(tr("Cannot write %1.").arg(m_iniName), Error);
		return false;
	}
	if (!m_database.isEmpty() && !writeFile(out.filePath(QString::fromLatin1(legacy ? RM_DB : EASY_DB)), m_database)) {
		log(tr("Cannot write the database."), Error);
		return false;
	}
	if (!m_mapTree.isEmpty()) {
		// the Player looks for this exact name
		if (!writeFile(out.filePath(QStringLiteral(RM_MT)), m_mapTree)) {
			log(tr("Cannot write the map tree."), Error);
			return false;
		}
	}
	return true;
}

bool GameExporter::exportPreset(const ExportPreset& preset)
{
	const QString folder_error = checkFolder(m_source.dir, preset.path);
	if (!folder_error.isEmpty()) {
		log(folder_error, Error);
		return false;
	}
	const QString dir = QDir::cleanPath(m_source.dir.absoluteFilePath(preset.path));
	const bool game_runtime = preset.platform == ExportPlatform::Windows && preset.launcher == QLatin1String(ExportPreset::kGameRuntime);
	if (!isLauncher(preset.platform, preset.launcher)) {
		log(tr("No EasyRPG Player for %1: skipped. Download it or locate it.").arg(ExportPreset::name(preset.platform)), Error);
		return false;
	}
	if (game_runtime && std::none_of(m_files.begin(), m_files.end(), [](const PlannedFile& f) {
			return f.path.compare(QStringLiteral("RPG_RT.exe"), Qt::CaseInsensitive) == 0; })) {
		log(tr("The game has no RPG_RT.exe: skipped."), Error);
		return false;
	}

	// a clean folder (the window asked before replacing a folder that is not empty)
	if (QDir(dir).exists() && !QDir(dir).removeRecursively()) {
		log(tr("Cannot empty %1.").arg(QDir::toNativeSeparators(dir)), Error);
		return false;
	}
	if (!QDir().mkpath(dir)) {
		log(tr("Cannot create %1.").arg(QDir::toNativeSeparators(dir)), Error);
		return false;
	}

	if (preset.platform == ExportPlatform::Web) {
		if (!writeWeb(preset, dir)) {
			return false;
		}
	} else {
		// RPG_RT.exe cannot read the protected map tree
		const QByteArray tree = m_mapTree;
		if (game_runtime && !m_mapTree.isEmpty()) {
			log(tr("RPG_RT.exe cannot read a protected game: exported without protection."), Warning);
			m_mapTree.clear();
			m_files << PlannedFile{ m_source.dir.filePath(m_treeName), m_treeName };
		}
		const bool written = writeGame(dir);
		if (game_runtime && !tree.isEmpty()) {
			m_files.removeLast();
			m_mapTree = tree;
		}
		if (!written || !writeLauncher(preset, dir)) {
			return false;
		}
		if (preset.zip) {
			QString name = ExportFiles::slug(m_settings.title);
			if (!m_settings.version.isEmpty()) {
				name += QLatin1Char('-') + ExportFiles::safeFileName(m_settings.version);
			}
			const QString zip = QFileInfo(dir).absolutePath() + QLatin1Char('/') + name + QLatin1Char('-') + ExportPreset::id(preset.platform) + QStringLiteral(".zip");
			QString error;
			if (!ExportFiles::writeZip(zip, dir, &error, [this] { return m_cancelled.load(); })) {
				log(error, Error);
				return false;
			}
			log(tr("Archive: %1").arg(QDir::toNativeSeparators(zip)));
		}
	}
	m_folders << dir;
	log(tr("Exported to %1").arg(QDir::toNativeSeparators(dir)));
	return true;
}

bool GameExporter::writeLauncher(const ExportPreset& preset, const QString& dir)
{
	const QImage icon = m_settings.icon.isEmpty() ? QImage() : QImage(m_source.dir.absoluteFilePath(m_settings.icon));
	if (!m_settings.icon.isEmpty() && icon.isNull()) {
		log(tr("The icon %1 cannot be read.").arg(m_settings.icon), Warning);
	}
	const QString name = ExportFiles::safeFileName(m_settings.title);
	QString error;

	switch (preset.platform) {
	case ExportPlatform::Windows: {
		QString exe = dir + QStringLiteral("/RPG_RT.exe");
		if (preset.launcher != QLatin1String(ExportPreset::kGameRuntime)) {
			// the Player under the name of the game (RPG_RT.exe stays: the Player reads it)
			exe = dir + QLatin1Char('/') + name + QStringLiteral(".exe");
			if (QFileInfo::exists(exe)) {
				exe = dir + QStringLiteral("/Player.exe");
			}
			if (!QFile::copy(preset.launcher, exe)) {
				log(tr("Cannot copy %1.").arg(QDir::toNativeSeparators(preset.launcher)), Error);
				return false;
			}
			log(tr("Launcher: %1").arg(QFileInfo(exe).fileName()));
		}
		if (!icon.isNull()) {
			QString message;
			const int replaced = ExportFiles::replaceExeIcon(exe, icon, &message);
			log(tr("Icon of %1: %2").arg(QFileInfo(exe).fileName(), message), replaced > 0 ? Info : Warning);
		}
		return step();
	}
	case ExportPlatform::MacOS: {
		// the Player looks for the game in the folder of its application
		const QString app = dir + QLatin1Char('/') + name + QStringLiteral(".app");
		if (!ExportFiles::copyTree(preset.launcher, app, &error)) {
			log(error, Error);
			return false;
		}
		log(tr("Launcher: %1").arg(QFileInfo(app).fileName()));
		if (preset.customize) {
			// the name and icon in the bundle: its signature must be made again
			const QString plist_path = app + QStringLiteral("/Contents/Info.plist");
			QString plist = QString::fromUtf8(readFile(plist_path));
			if (plist.startsWith(QStringLiteral("bplist"))) {
				log(tr("Info.plist of the Player is binary: name and icon not changed."), Warning);
			} else {
				auto setKey = [&](const QString& key, const QString& value) {
					const QRegularExpression re(QStringLiteral("(<key>%1</key>\\s*<string>)[^<]*(</string>)").arg(key));
					const QString escaped = value.toHtmlEscaped();
					if (plist.contains(re)) {
						plist.replace(re, QStringLiteral("\\1") + escaped + QStringLiteral("\\2"));
					} else {
						plist.replace(QStringLiteral("<dict>"), QStringLiteral("<dict>\n\t<key>%1</key>\n\t<string>%2</string>").arg(key, escaped));
					}
				};
				setKey(QStringLiteral("CFBundleName"), m_settings.title);
				setKey(QStringLiteral("CFBundleDisplayName"), m_settings.title);
				if (!m_settings.version.isEmpty()) {
					setKey(QStringLiteral("CFBundleShortVersionString"), m_settings.version);
				}
				if (!icon.isNull()) {
					static const QRegularExpression icon_key(QStringLiteral("<key>CFBundleIconFile</key>\\s*<string>([^<]*)</string>"));
					QString icon_file = icon_key.match(plist).captured(1);
					if (icon_file.isEmpty()) {
						icon_file = QStringLiteral("GameIcon.icns");
						setKey(QStringLiteral("CFBundleIconFile"), icon_file);
					} else if (!icon_file.endsWith(QStringLiteral(".icns"))) {
						icon_file += QStringLiteral(".icns");
					}
					writeFile(app + QStringLiteral("/Contents/Resources/") + icon_file, ExportFiles::makeIcns(icon));
				}
				writeFile(plist_path, plist.toUtf8());
#ifdef Q_OS_MACOS
				QProcess codesign;
				codesign.start(QStringLiteral("/usr/bin/codesign"), { QStringLiteral("--force"), QStringLiteral("--deep"), QStringLiteral("--sign"), QStringLiteral("-"), app });
				if (codesign.waitForFinished(120000) && codesign.exitCode() == 0) {
					log(tr("The application was signed again for this computer: other Macs ask to allow it (Open in the context menu, or Privacy & Security)."), Warning);
				} else {
					log(tr("The application could not be signed again: macOS will refuse it.") + QLatin1Char(' ') + QString::fromLocal8Bit(codesign.readAllStandardError()), Warning);
				}
#else
				log(tr("The application must be signed again on a Mac (codesign --force --deep --sign -), else macOS refuses it."), Warning);
#endif
			}
		}
		log(tr("macOS runs a downloaded application from a hidden copy until it is moved: tell players to move the game folder (out of Downloads) before playing."));
		return step();
	}
	case ExportPlatform::Linux: {
		const QString program = dir + QStringLiteral("/easyrpg-player");
		if (!QFile::copy(preset.launcher, program)) {
			log(tr("Cannot copy %1.").arg(QDir::toNativeSeparators(preset.launcher)), Error);
			return false;
		}
		QFile::setPermissions(program, QFile::permissions(program) | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
		// a desktop entry that starts the Player in its folder (%k: the entry itself)
		const QString slug = ExportFiles::slug(m_settings.title);
		QString desktop = QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\n").arg(m_settings.title);
		if (!icon.isNull()) {
			QImage(icon).save(dir + QLatin1Char('/') + slug + QStringLiteral(".png"));
			desktop += QStringLiteral("Icon=%1.png\n").arg(slug);
		}
		// quoted for the desktop entry format: the shell gets cd "$(dirname "$0")" && exec ./easyrpg-player
		desktop += QString::fromLatin1(R"(Exec=sh -c "cd \\"\\$(dirname \\"\\$0\\")\\" && exec ./easyrpg-player" %k)" "\n");
		desktop += QStringLiteral("Terminal=false\nCategories=Game;\n");
		const QString entry = dir + QLatin1Char('/') + slug + QStringLiteral(".desktop");
		writeFile(entry, desktop.toUtf8());
		QFile::setPermissions(entry, QFile::permissions(entry) | QFile::ExeOwner);
		log(tr("Launcher: easyrpg-player, %1.desktop").arg(slug));
		return step();
	}
	case ExportPlatform::Web:
		break;
	}
	return true;
}

bool GameExporter::writeWeb(const ExportPreset& preset, const QString& dir)
{
	const QString html = dir + QStringLiteral("/html");
	QString error;
	// the Player's files, without its own games; from a build folder only its page and program
	const QDir launcher(preset.launcher);
	const QString player_page = WebPlayer::page(preset.launcher);
	for (const QString& name : WebPlayer::files(preset.launcher)) {
		const QFileInfo info(launcher.filePath(name));
		// the page of a build is published as index.html
		const QString target = html + QLatin1Char('/') + (name == player_page ? QStringLiteral("index.html") : name);
		const bool ok = info.isDir() ? ExportFiles::copyTree(info.filePath(), target, &error)
			: (QDir().mkpath(html) && QFile::copy(info.filePath(), target));
		if (!ok) {
			log(error.isEmpty() ? tr("Cannot copy %1.").arg(info.filePath()) : error, Error);
			return false;
		}
	}
	if (WebPlayer::isBuild(preset.launcher)) {
		log(tr("Web Player taken from a build folder: %1 (as index.html) and its program.").arg(player_page));
	}

	// the game has a name of its own: itch.io gives one storage to all its games, saves are kept by name
	const QString slug = ExportFiles::slug(m_settings.title);
	QString page = QString::fromUtf8(readFile(html + QStringLiteral("/index.html")));
	QString game_name = slug;
	static const QRegularExpression game_option(QStringLiteral("game:\\s*undefined"));
	if (page.contains(game_option)) {
		page.replace(game_option, QStringLiteral("game: \"%1\"").arg(slug));
	} else {
		game_name = QStringLiteral("default");
		log(tr("The page of this web Player cannot be given the game name: games/default is used, saves may mix with other games on itch.io."), Warning);
	}
	static const QRegularExpression title_tag(QStringLiteral("<title>[^<]*</title>"));
	page.replace(title_tag, QStringLiteral("<title>%1</title>").arg(m_settings.title.toHtmlEscaped()));
	const QImage icon = m_settings.icon.isEmpty() ? QImage() : QImage(m_source.dir.absoluteFilePath(m_settings.icon));
	if (!icon.isNull()) {
		icon.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(html + QStringLiteral("/favicon.png"));
		page.replace(QStringLiteral("</head>"), QStringLiteral("<link rel=\"icon\" href=\"favicon.png\">\n</head>"));
		// the cover of the itch.io page: 630 x 500
		QImage cover(630, 500, QImage::Format_ARGB32);
		cover.fill(Qt::transparent);
		const QImage scaled = icon.scaled(400, 400, Qt::KeepAspectRatio, icon.width() < 400 ? Qt::FastTransformation : Qt::SmoothTransformation);
		for (int y = 0; y < scaled.height(); ++y) {
			for (int x = 0; x < scaled.width(); ++x) {
				cover.setPixel(x + (630 - scaled.width()) / 2, y + (500 - scaled.height()) / 2, scaled.pixel(x, y));
			}
		}
		cover.save(dir + QStringLiteral("/itch-cover.png"));
	}
	writeFile(html + QStringLiteral("/index.html"), page.toUtf8());

	const QString game_dir = html + QStringLiteral("/games/") + game_name;
	if (!writeGame(game_dir)) {
		return false;
	}
	// the list of files the web Player downloads from
	writeFile(game_dir + QStringLiteral("/index.json"), QJsonDocument(ExportFiles::webIndex(QDir(game_dir))).toJson(QJsonDocument::Compact));
	log(tr("Web game: games/%1 with its index.json").arg(game_name));
	if (!m_settings.include_rtp) {
		log(tr("The web Player has no RTP: check \"Include the RTP files the game uses\"."), Warning);
	}

	QString zip_name;
	if (preset.zip) {
		zip_name = slug + (m_settings.version.isEmpty() ? QString() : QLatin1Char('-') + ExportFiles::safeFileName(m_settings.version)) + QStringLiteral("-web.zip");
		if (!ExportFiles::writeZip(dir + QLatin1Char('/') + zip_name, html, &error, [this] { return m_cancelled.load(); })) {
			log(error, Error);
			return false;
		}
		log(tr("Archive for itch.io: %1").arg(zip_name));
	}

	const QString readme = tr(
		"%1 %2 - web version, for itch.io\n"
		"\n"
		"1. Make a zip of the CONTENT of the \"html\" folder: index.html must be at the root of the\n"
		"   zip, not in a folder.%3\n"
		"2. On itch.io: Dashboard > Create new project.\n"
		"3. Kind of project: HTML.\n"
		"4. Uploads: upload the zip and check \"This file will be played in the browser\".\n"
		"5. Embed options:\n"
		"   - Viewport dimensions: 640 x 480 (the game is 320 x 240, shown twice as big; 960 x 720\n"
		"     for three times). Keep 4:3.\n"
		"   - Check \"Fullscreen button\" (the Player also has its own, at the top right).\n"
		"   - \"Mobile friendly\" can be checked: the Player shows touch buttons on phones.\n"
		"   - \"SharedArrayBuffer support\" is not needed: leave it unchecked.\n"
		"6. Cover image: itch-cover.png (630 x 500), if an icon was chosen.\n"
		"7. Save, then \"View page\" to play.\n"
		"\n"
		"Saves stay in the browser of each player, under the game name \"%4\": keep the same\n"
		"title for the next versions so that players keep their saves.\n"
		"\n"
		"To try it before uploading: browsers do not run it from the disk. Start a small web\n"
		"server in the html folder (for example: python3 -m http.server) and open\n"
		"http://localhost:8000\n")
		.arg(m_settings.title, m_settings.version,
			zip_name.isEmpty() ? QString() : tr("\n   The Editor made it: %1").arg(zip_name), game_name);
	writeFile(dir + QStringLiteral("/ITCH.txt"), readme.toUtf8());
	return step();
}
