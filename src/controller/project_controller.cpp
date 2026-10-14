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

#include "project_controller.h"
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTimer>
#include <algorithm>
#include <cassert>
#include <tuple>
#include <vector>
#include "core.h"
#include "defines.h"
#include "dialog_service.h"
#include "edit_locks.h"
#include "common/dbstring.h"
#include "common/editor_settings.h"
#include "common/logging.h"
#include "model/game_runner.h"
#include "model/map_document.h"
#include "model/map_tools.h"
#include "model/project.h"

using lcf::rpg::TreeMap;

namespace {

DialogService& dialogs()
{
	return DialogService::instance();
}

void recurseAddDir(QDir d, QStringList& list)
{
	QStringList qsl = d.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files);

	foreach (QString file, qsl) {

		QFileInfo finfo(QString("%1/%2").arg(d.path()).arg(file));

		if (finfo.isSymLink())
			return;

		if (finfo.isDir()) {

			QString dirname = finfo.fileName();
			QDir sd(finfo.filePath());

			recurseAddDir(sd, list);

		} else
			list << QDir::toNativeSeparators(finfo.filePath());
	}
}

}

ProjectController::ProjectController(QObject* parent) :
	QObject(parent)
{
	// Show what other programs (like a script sync tool) write into the
	// project, once the editors holding that data let it go
	auto& watcher = core().watcher();
	watcher.setCanDeliver([](int kind) { return EditLocks::instance().canReload(kind); });
	connect(&watcher, &ProjectWatcher::databaseChanged, this, &ProjectController::onDatabaseChanged);
	connect(&watcher, &ProjectWatcher::treeMapChanged, this, &ProjectController::reloadTreeMap);
	connect(&watcher, &ProjectWatcher::mapChanged, this, &ProjectController::onMapChanged);
	GameRunner::setBeforeLaunch([this] { return saveBeforeTestPlay(); });
}

ProjectController::~ProjectController()
{
	GameRunner::setBeforeLaunch({});
	core().watcher().setCanDeliver({});
}

MapTreeModel* ProjectController::treeModel()
{
	return &m_tree;
}

const QStringList& ProjectController::resourceDirs()
{
	static const QStringList dirs =
		{BACKDROP, BATTLE, BATTLE2, BATTLECHARSET, BATTLEWEAPON, CHARSET, CHIPSET, FACESET,
		FRAME, GAMEOVER, MONSTER, MOVIE, MUSIC, PANORAMA, PICTURE, SOUND, SYSTEM, SYSTEM2, TITLE};
	return dirs;
}

QString ProjectController::mapFileName(int id)
{
	const bool easyrpg = core().project()->projectType() == FileFinder::ProjectType::EasyRpg;
	return QString("Map%1.%2").arg(id, 4, 10, QChar('0')).arg(easyrpg ? "emu" : "lmu");
}

QString ProjectController::defaultMapName(int id)
{
	return tr("MAP%1").arg(id, 4, 10, QChar('0'));
}

DatabaseApply::File ProjectController::databaseFile()
{
	DatabaseApply::File file;
	file.isStale = [] { return core().watcher().isStale(core().project()->databaseFile()); };
	file.read = [] { return core().project()->readDatabase(); };
	file.save = [] { return core().project()->saveDatabase(); };
	file.reload = [] { core().watcher().requestReload(ProjectWatcher::Database); };
	return file;
}

void ProjectController::setDocumentOrder(std::function<QList<int>()> order)
{
	m_documentOrder = std::move(order);
}

void ProjectController::setViewSync(std::function<bool()> sync)
{
	m_viewSync = std::move(sync);
}

bool ProjectController::syncView()
{
	return m_viewSync && m_viewSync();
}

bool ProjectController::open(const QString& folder)
{
	auto prj = read(folder);
	if (!prj)
		return false;
	open(prj);
	return true;
}

std::shared_ptr<Project> ProjectController::read(const QString& folder)
{
	auto prj = Project::load(folder);

	if (!prj) {
		dialogs().error(tr("Error loading project"), QString(tr("Failed to load project %1")).arg(folder));
		return nullptr;
	}

	if (!prj->loadDatabaseAndMapTree()) {
		dialogs().error(tr("Error loading project"), QString(tr("Failed to load database or maptree of %1")).arg(folder));
		return nullptr;
	}
	return prj;
}

void ProjectController::open(const std::shared_ptr<Project>& project)
{
	core().project() = project;
	m_tree.setRootName(project->gameTitle());
	m_tree.setTreeMap(&project->treeMap());
	m_treeRemoved.clear();
	emit projectChanged();
}

void ProjectController::close()
{
	for (int id : m_documents.keys())
		closeDocument(id);
	m_tree.setTreeMap(nullptr);
	core().project().reset();
	updateWatcher();

	m_copiedMap.reset();
	emit projectChanged();
}

void ProjectController::updateWatcher()
{
	const auto& project = core().project();
	if (project && EditorSettings::instance().reloadExternalChanges()) {
		core().watcher().setProject(project->projectDir().absolutePath(),
			project->projectType() == FileFinder::ProjectType::EasyRpg);
	} else {
		core().watcher().setProject(QString(), false);
	}
}

ProjectController::ImportResult ProjectController::importProject(const QDir& src_dir, QDir& target_dir, bool convert_xyz,
	const ImportProgress& progress, QString* folder)
{
	if (!FileFinder::IsRPG2kProject(src_dir)) {
		dialogs().error(tr("Error loading project"),
			QString(tr("The selected project %1 is not a legacy RPG Maker project")).arg(src_dir.dirName()));
		return ImportResult::Failed;
	}

	auto prj = Project::load(src_dir);

	if (!prj) {
		dialogs().error(tr("Error loading project"), QString(tr("Failed to load project %1")).arg(src_dir.dirName()));
		return ImportResult::Failed;
	}

	if (!prj->loadDatabaseAndMapTree()) {
		dialogs().error(tr("Error loading project"),
			QString(tr("Failed to load database or maptree of %1")).arg(src_dir.dirName()));
		return ImportResult::Failed;
	}

	// Load all maps
	lcf::rpg::TreeMap& maps = prj->treeMap();
	progress.begin(tr("Processing Maps..."), static_cast<int>(maps.maps.size()));

	std::vector<std::tuple<int, std::unique_ptr<lcf::rpg::Map>>> loaded_maps;
	// the maps not imported: unreadable, or not written (named as in the
	// source game: core().project() is not this project yet)
	QStringList skipped_maps;
	auto source_name = [](int id) { return QString("Map%1.lmu").arg(id, 4, 10, QChar('0')); };

	for (size_t i = 1; i < maps.maps.size(); i++) {
		progress.value(static_cast<int>(i));
		if (maps.maps[i].type == lcf::rpg::TreeMap::MapType_area)
			continue;
		const int id = maps.maps[i].ID;
		auto map = prj->loadMap(id);
		if (!map) {
			skipped_maps << source_name(id);
			continue;
		}
		loaded_maps.push_back(std::make_tuple(id, std::move(map)));
	}

	// Save new database and treemap
	prj->relocate(target_dir, FileFinder::ProjectType::EasyRpg);
	progress.label(tr("Importing database and map tree..."));
	progress.value(0);
	progress.maximum(0);
	if (!prj->saveDatabase(false) || !prj->saveTreeMap()) {
		dialogs().error(tr("Error importing project"),
			tr("The database or the map tree could not be written to %1.").arg(QDir::toNativeSeparators(target_dir.path())));
		return ImportResult::Failed;
	}

	progress.label(tr("Importing maps..."));
	progress.maximum(static_cast<int>(loaded_maps.size()));
	int i = 1;
	for (auto& [id, map] : loaded_maps) {
		if (!prj->saveMap(*map, id, false))
			skipped_maps << source_name(id);
		progress.value(i++);
	}
	loaded_maps.clear();

	progress.label(tr("Importing resources..."));

	// Create all asset directories
	for (const QString& dir : resourceDirs())
		target_dir.mkpath(dir);

	QStringList entries;
	for (const QString& dir : resourceDirs()) {
		// Enumerate source files
		QDir subdir = src_dir;
		if (subdir.cd(dir)) {
			recurseAddDir(subdir, entries);
		}
	}

	progress.maximum(entries.count());

	// Copy all assets from the asset subdirectories
	for (int i = 0; i < entries.count(); i++) {
		progress.value(i);
		QFileInfo info(entries[i]);
		QString dest_file = target_dir.path() + "/" + info.dir().dirName() + "/" + info.fileName();
		if (!QFile::copy(entries[i], dest_file))
		{
			dialogs().error(tr("Error"), tr("Could not copy file %1 to \n"
				"%2").arg(entries[i]).arg(dest_file));
			return ImportResult::CopyFailed;
		}
		if (convert_xyz && info.dir().dirName() != MUSIC && info.dir().dirName() != SOUND)
		{
			QFile file(dest_file);
			bool success = file.open(QIODevice::ReadOnly);
			if (success && file.read(4) == "XYZ1")
			{
				QString conv_path = target_dir.path() + "/" + info.dir().dirName() + "/" + info.completeBaseName() + ".png";
				if (convertXYZtoPNG(file, conv_path))
					file.remove();
				else
					qWarning() << QString(tr("Failed to convert %1 to PNG")).arg(dest_file);
			}
		}

	}
	progress.value(entries.count());

	if (!skipped_maps.isEmpty()) {
		dialogs().warning(tr("Import Project"),
			tr("These maps could not be imported: %1.").arg(skipped_maps.join(QStringLiteral(", "))));
	}

	*folder = prj->projectDir().absolutePath();
	return ImportResult::Imported;
}

bool ProjectController::convertXYZtoPNG(QFile& xyz_file, const QString& out_path)
{
	QByteArray compressed_data(xyz_file.readAll());
	assert(!compressed_data.isEmpty());
	uint16_t width = static_cast<uint16_t>(static_cast<uint8_t>(compressed_data[0]) + (static_cast<uint8_t>(compressed_data[1]) << 8));
	uint16_t height = static_cast<uint16_t>(static_cast<uint8_t>(compressed_data[2]) + (static_cast<uint8_t>(compressed_data[3]) << 8));
	uint32_t size = width * height;
	// Put the total size in the first 4 bytes to make it valid for qUncompress
	for (int i = 0; i < 4; ++i)
		compressed_data[i] = static_cast<char>((reinterpret_cast<uint8_t*>(&size))[3-i]);

	QByteArray xyz_data(qUncompress(compressed_data));

	if (xyz_data.isEmpty())
		return false;

	// the rows are "width" bytes apart, not aligned on 4 bytes like a QImage's
	const auto* pixels = reinterpret_cast<const uchar*>(xyz_data.constData());
	QImage image(pixels + 768, width, height, width, QImage::Format_Indexed8);
	image = image.copy();
	// Add color table (unsigned bytes: colors above 127)
	for (int i = 0; i < 256; ++i)
		image.setColor(i, qRgb(pixels[i * 3], pixels[i * 3 + 1], pixels[i * 3 + 2]));

	return image.save(out_path, "PNG");
}

QString ProjectController::convertTo2003(const QString& folder, const ProjectConvert::Options& options, QString* backup)
{
	// every file is rewritten: converted from its files
	auto converted = Project::load(folder);
	if (!converted || !converted->loadDatabaseAndMapTree())
		return tr("The project cannot be read.");
	return ProjectConvert::to2003(*converted, options, backup);
}

MapDocument* ProjectController::openDocument(int id)
{
	if (MapDocument* open = m_documents.value(id))
		return open;
	auto* doc = new MapDocument(core().project()->projectData(), id, this);
	connect(doc, &MapDocument::saveFailed, this, [](const QString& message) {
		dialogs().error(tr("Save Map"), message);
	});
	// the questions of a file changed on disk
	doc->setStaleHandler([](const QString& what, bool can_reload, const QStringList& conflicts) {
		return dialogs().askStale(what, can_reload, conflicts);
	});
	doc->setReloadHandler([doc]() {
		return dialogs().question(tr("Map changed on disk"),
			tr("%1 was changed by another program.\n"
			   "Reload it and discard your unsaved changes?").arg(doc->name())) == DialogService::Answer::Yes;
	});
	doc->setDeletedHandler([this, id]() { return askDeletedMap(id); });
	// the tree entry of the map is written with it (its name...)
	connect(doc, &MapDocument::saved, this, [this, id] { m_tree.entryChanged(id); });
	m_documents.insert(id, doc);
	return doc;
}

StaleChoice ProjectController::askDeletedMap(int id)
{
	MapDocument* doc = document(id);
	const auto answer = dialogs().question(tr("Map deleted on disk"),
		tr("%1 was deleted by another program.\n"
		   "Add it back to the map tree and save your changes? No discards them and closes the map.").arg(doc->name()),
		DialogService::Buttons::YesNoCancel);
	if (answer == DialogService::Answer::Cancel)
		return StaleChoice::Cancel;
	if (answer == DialogService::Answer::No) {
		// once the save that asked is over: its views let it go first
		QTimer::singleShot(0, this, [this, id] { closeDocument(id); });
		return StaleChoice::Reload;
	}
	// another program can have put it back meanwhile
	refreshTreeMap();
	if (!m_tree.contains(id)) {
		// under its parent when it is still there, else under the project
		insertMapInfo(doc->mapInfo());
	} else if (m_tree.type(id) != TreeMap::MapType_map) {
		dialogs().error(tr("Map deleted on disk"),
			tr("%1 cannot be added back: an area of the map tree has its id now.").arg(doc->name()));
		return StaleChoice::Cancel;
	}
	return StaleChoice::Overwrite;
}

MapDocument* ProjectController::document(int id) const
{
	return m_documents.value(id);
}

QList<int> ProjectController::documentIds() const
{
	return m_documents.keys();
}

void ProjectController::closeDocument(int id)
{
	// taken first: closing it again while its views go does nothing
	MapDocument* doc = m_documents.take(id);
	if (!doc)
		return;
	emit documentClosing(id);
	delete doc;
}

QList<int> ProjectController::documentOrder() const
{
	QList<int> order = m_documentOrder ? m_documentOrder() : m_documents.keys();
	order.removeIf([this](int id) { return !m_documents.contains(id); });
	return order;
}

bool ProjectController::saveModifiedMaps(const QString& title, const QString& question)
{
	const QList<int> order = documentOrder();
	const bool modified = std::any_of(order.cbegin(), order.cend(),
		[this](int id) { return m_documents.value(id)->isModified(); });
	if (!modified)
		return true;
	const auto answer = dialogs().question(title, question, DialogService::Buttons::YesNoCancel);
	if (answer == DialogService::Answer::Cancel)
		return false;
	if (answer == DialogService::Answer::Yes) {
		for (int id : order) {
			if (MapDocument* doc = m_documents.value(id); doc && !doc->save())
				return false;
		}
	}
	return true;
}

bool ProjectController::saveAll()
{
	// a map not saved (cancelled, or failed) keeps its changes open
	if (!saveModifiedMaps(tr("Save map changes"),
			tr("Some maps have unsaved changes.\n"
			   "Do you want to save them before closing them?")))
		return false;

	// The map tree is written whenever the Editor changes it (maps saved,
	// added, removed...): writing it again here could only overwrite the
	// changes of another program
	return true;
}

bool ProjectController::saveBeforeTestPlay()
{
	return saveModifiedMaps(tr("Test Play"), tr("Some maps have unsaved changes.\nSave them before the test play?"));
}

void ProjectController::saveAllMaps()
{
	for (int id : m_documents.keys()) {
		if (MapDocument* doc = m_documents.value(id); doc && doc->isModified())
			doc->save();
	}
}

int ProjectController::freeMapId() const
{
	// a file without its tree entry (another program's new map) is kept
	return m_tree.freeMapId([](int id) { return QFileInfo::exists(core().project()->mapFile(id)); });
}

void ProjectController::refreshTreeMap()
{
	if (core().project() && core().watcher().isStale(core().project()->treeMapFile()))
		reloadTreeMap();
}

bool ProjectController::writeTreeMap()
{
	auto& project = *core().project();
	QStringList conflicts;
	const bool stale = core().watcher().isStale(project.treeMapFile());
	const bool merged = stale && project.mergeTreeMap(&conflicts);
	if (stale && !merged && dialogs().askStale(tr("The map tree"), true, conflicts) != StaleChoice::Overwrite) {
		reloadTreeMap(); // the tree of the file, without this change
		return false;
	}
	if (!project.saveTreeMap()) {
		dialogs().error(tr("Map Tree"), tr("The map tree could not be written."));
		return false;
	}
	if (merged)
		reloadTreeMap(); // the tree shown takes the other program's changes
	return true;
}

void ProjectController::writeTreeMapAfterSave()
{
	auto& project = *core().project();
	const bool stale = core().watcher().isStale(project.treeMapFile());
	if (stale && !project.mergeTreeMap(nullptr)) {
		reloadTreeMap(); // its order is the one of the file
		return;
	}
	if (!project.saveTreeMap())
		dialogs().error(tr("Map Tree"), tr("The map tree could not be written."));
	if (stale)
		reloadTreeMap();
}

bool ProjectController::reloadTreeMap()
{
	if (!core().project())
		return false;
	// the names shown before, for the rows renamed
	const lcf::rpg::TreeMap before = core().project()->treeMap();
	if (!core().project()->reloadTreeMap()) {
		emit statusMessage(tr("The map tree changed on disk but could not be read"), 5000);
		return false;
	}
	m_tree.setRootName(core().project()->gameTitle());
	m_treeRemoved = m_tree.update(before);
	emit treeMapReloaded();
	emit statusMessage(tr("Map tree reloaded"), 3000);
	qCInfo(lcReload) << "map tree reloaded";
	return true;
}

int ProjectController::keptNode(int id) const
{
	if (m_tree.contains(id))
		return id;
	const int above = m_treeRemoved.value(id, 0);
	return m_tree.contains(above) ? above : 0;
}

void ProjectController::keepTreeAfterMapSave()
{
	// the tree's parents and order can have changed since it was opened
	if (!core().project())
		return;
	const bool expanded = m_viewSync && m_viewSync();
	const bool order = m_tree.syncOrder();
	if (expanded || order)
		writeTreeMapAfterSave();
}

lcf::rpg::MapInfo ProjectController::newMapInfo(int parent) const
{
	lcf::rpg::MapInfo info;
	info.ID = freeMapId();
	info.name = ToDBString(defaultMapName(info.ID));
	info.type = lcf::rpg::TreeMap::MapType_map;
	info.parent_map = m_tree.type(parent) == lcf::rpg::TreeMap::MapType_map ? parent : 0;
	if (info.parent_map == 0)
	{
		// nothing to inherit from
		info.music_type = lcf::rpg::MapInfo::MusicType_event;
		info.background_type = lcf::rpg::MapInfo::BGMType_terrain;
		info.teleport = lcf::rpg::MapInfo::TriState_allow;
		info.escape = lcf::rpg::MapInfo::TriState_allow;
		info.save = lcf::rpg::MapInfo::TriState_allow;
	}
	return info;
}

int ProjectController::addNewMap(lcf::rpg::MapInfo info, lcf::rpg::Map& map)
{
	return addMap(info, map, tr("New Map"));
}

int ProjectController::addMap(lcf::rpg::MapInfo info, lcf::rpg::Map& map, const QString& title)
{
	// another program can have added a map while the dialog was open
	refreshTreeMap();
	if (m_tree.mapInfo(info.ID) || QFileInfo::exists(core().project()->mapFile(info.ID)))
		info.ID = freeMapId();

	if (!core().project()->saveMap(map, info.ID))
	{
		dialogs().error(title, tr("The file %1 cannot be written.").arg(mapFileName(info.ID)));
		return 0;
	}
	insertMapInfo(info);
	return info.ID;
}

void ProjectController::insertMapInfo(const lcf::rpg::MapInfo& info)
{
	if (m_viewSync)
		m_viewSync();
	m_tree.insertMapInfo(info, info.parent_map);
	writeTreeMap();
}

bool ProjectController::hasCopiedMap() const
{
	return m_copiedMap != nullptr;
}

bool ProjectController::copyMap(int id)
{
	if (m_tree.type(id) != lcf::rpg::TreeMap::MapType_map)
		return false;
	MapDocument* doc = document(id);
	if (doc && doc->isModified())
	{
		const auto answer = dialogs().question(tr("Copy Map"),
			tr("%1 has unsaved changes.\nSave them, to copy them too?").arg(doc->name()),
			DialogService::Buttons::YesNoCancel);
		if (answer == DialogService::Answer::Cancel)
			return false;
		if (answer == DialogService::Answer::Yes && !doc->save())
			return false;
	}
	auto map = core().project()->loadMap(id);
	const lcf::rpg::MapInfo* info = m_tree.mapInfo(id);
	if (!map || !info)
	{
		dialogs().error(tr("Copy Map"), tr("The file %1 cannot be read.").arg(mapFileName(id)));
		return false;
	}
	m_copiedMap = std::move(map);
	m_copiedMapInfo = *info;
	return true;
}

int ProjectController::pasteMap(int parent)
{
	const int type = m_tree.type(parent);
	if (!m_copiedMap || type == lcf::rpg::TreeMap::MapType_area)
		return 0;

	// The copy becomes a child of the selected map
	lcf::rpg::MapInfo info = m_copiedMapInfo;
	info.ID = freeMapId();
	info.type = lcf::rpg::TreeMap::MapType_map;
	info.parent_map = type == lcf::rpg::TreeMap::MapType_map ? parent : 0;
	info.scrollbar_x = 0;
	info.scrollbar_y = 0;
	info.expanded_node = false;
	if (info.parent_map == 0)
	{
		if (info.music_type == lcf::rpg::MapInfo::MusicType_parent)
			info.music_type = lcf::rpg::MapInfo::MusicType_event;
		if (info.background_type == lcf::rpg::MapInfo::BGMType_parent)
			info.background_type = lcf::rpg::MapInfo::BGMType_terrain;
		if (info.teleport == lcf::rpg::MapInfo::TriState_parent)
			info.teleport = lcf::rpg::MapInfo::TriState_allow;
		if (info.escape == lcf::rpg::MapInfo::TriState_parent)
			info.escape = lcf::rpg::MapInfo::TriState_allow;
		if (info.save == lcf::rpg::MapInfo::TriState_parent)
			info.save = lcf::rpg::MapInfo::TriState_allow;
	}

	// saving counts the saves: the clipboard keeps its own copy
	lcf::rpg::Map map = *m_copiedMap;
	if (!core().project()->saveMap(map, info.ID))
	{
		dialogs().error(tr("Paste Map"), tr("The file %1 cannot be written.").arg(mapFileName(info.ID)));
		return 0;
	}
	insertMapInfo(info);
	return info.ID;
}

bool ProjectController::deleteMap(int id, const QString& name)
{
	if (m_tree.type(id) != lcf::rpg::TreeMap::MapType_map)
		return false;
	if (dialogs().question(tr("Delete Map"),
			tr("%1 will be deleted, with its child maps and areas.\nThis cannot be undone. Proceed?").arg(name))
			!= DialogService::Answer::Yes)
		return false;

	// read again after the question: the file deleted and the tree written
	// stay together (no dialog between them)
	refreshTreeMap();
	if (!m_tree.mapInfo(id))
		return false;
	removeMap(id);
	writeTreeMap();
	return true;
}

void ProjectController::removeMap(int id)
{
	if (id == 0 || !m_tree.contains(id))
		return;
	if (m_viewSync)
		m_viewSync();
	// Children first; areas have no file
	for (int removed : m_tree.subtreeIds(id))
	{
		if (m_tree.type(removed) != lcf::rpg::TreeMap::MapType_map)
			continue;
		closeDocument(removed);
		const QString path = core().project()->findFile(mapFileName(removed));
		if (!path.isEmpty())
			QFile::remove(path);
		else
			qWarning() << QString("file not found: %1").arg(mapFileName(removed));
	}
	m_tree.removeMap(id);
}

std::optional<ProjectController::MapEdit> ProjectController::readMapProperties(int id)
{
	// its version before reading it: another program can write it while
	// the dialog is open
	MapEdit edit;
	edit.stamp = ProjectWatcher::stampOf(core().project()->mapFile(id));
	edit.map = core().project()->loadMap(id);
	if (!edit.map || !m_tree.mapInfo(id))
	{
		dialogs().error(tr("Map Properties"), tr("The file %1 cannot be read.").arg(mapFileName(id)));
		return std::nullopt;
	}
	edit.info = *m_tree.mapInfo(id);
	return edit;
}

bool ProjectController::saveMapProperties(int id, const MapEdit& edit)
{
	if (ProjectWatcher::stampOf(core().project()->mapFile(id)) != edit.stamp
			&& dialogs().askStale(tr("The map “%1”").arg(ToQString(edit.info.name)), false, {}) != StaleChoice::Overwrite)
		return false;
	if (!core().project()->saveMap(*edit.map, id))
	{
		dialogs().error(tr("Map Properties"), tr("The file %1 cannot be written.").arg(mapFileName(id)));
		return false;
	}
	if (m_tree.mapInfo(id))
		m_tree.setMapInfo(id, edit.info);
	writeTreeMap();
	return true;
}

std::optional<ProjectController::DungeonSource> ProjectController::dungeonSource(int id)
{
	if (m_tree.type(id) != lcf::rpg::TreeMap::MapType_map)
		return std::nullopt;
	const MapDocument* doc = document(id);
	DungeonSource source;
	source.map = doc ? std::make_unique<lcf::rpg::Map>(doc->map()) : core().project()->loadMap(id);
	const lcf::rpg::MapInfo* source_info = doc ? &doc->mapInfo() : m_tree.mapInfo(id);
	if (!source.map || !source_info)
	{
		dialogs().error(tr("Generate Dungeon"), tr("The file %1 cannot be read.").arg(mapFileName(id)));
		return std::nullopt;
	}
	if (!source.map->generator_flag)
	{
		dialogs().information(tr("Generate Dungeon"),
			tr("%1 does not use the Random Dungeon Generator.\n"
				"Check \"Use Random Dungeon Generator\" in its Map Properties first.").arg(ToQString(source_info->name)));
		return std::nullopt;
	}

	// help: "a new map, retaining the same settings"; a child of the source
	source.info = *source_info;
	source.info.ID = freeMapId();
	source.info.parent_map = id;
	source.info.scrollbar_x = 0;
	source.info.scrollbar_y = 0;
	source.info.expanded_node = false;
	return source;
}

int ProjectController::addDungeon(lcf::rpg::MapInfo info, lcf::rpg::Map& map)
{
	return addMap(info, map, tr("Generate Dungeon"));
}

bool ProjectController::shiftMap(int id, int dx, int dy)
{
	auto map = core().project()->loadMap(id);
	if (!map)
	{
		dialogs().error(tr("Shift Map"), tr("The file %1 cannot be read.").arg(mapFileName(id)));
		return false;
	}
	MapTools::shiftMap(*map, dx, dy);
	if (!core().project()->saveMap(*map, id))
	{
		dialogs().error(tr("Shift Map"), tr("The file %1 cannot be written.").arg(mapFileName(id)));
		return false;
	}

	// the start positions on this map move with it
	auto& start = core().project()->treeMap().start;
	bool start_moved = false;
	auto shiftStart = [&](int map_id, int32_t& x, int32_t& y) {
		if (map_id == id)
		{
			x = MapTools::wrap(x + dx, map->width);
			y = MapTools::wrap(y + dy, map->height);
			start_moved = true;
		}
	};
	shiftStart(start.party_map_id, start.party_x, start.party_y);
	shiftStart(start.boat_map_id, start.boat_x, start.boat_y);
	shiftStart(start.ship_map_id, start.ship_x, start.ship_y);
	shiftStart(start.airship_map_id, start.airship_x, start.airship_y);
	if (start_moved)
		writeTreeMap();
	return true;
}

QSize ProjectController::mapSize(int id) const
{
	auto map = core().project()->loadMap(id);
	return map ? QSize(map->width, map->height) : QSize();
}

std::optional<ProjectController::AreaEdit> ProjectController::newArea(int parent)
{
	if (m_tree.type(parent) != lcf::rpg::TreeMap::MapType_map)
		return std::nullopt;
	const QSize size = mapSize(parent);
	if (!size.isValid())
	{
		dialogs().error(tr("Create Area"), tr("The file %1 cannot be read.").arg(mapFileName(parent)));
		return std::nullopt;
	}

	lcf::rpg::MapInfo info;
	info.ID = freeMapId();
	info.name = ToDBString(tr("AREA%1").arg(info.ID, 4, 10, QChar('0')));
	info.type = lcf::rpg::TreeMap::MapType_area;
	info.parent_map = parent;
	info.area_rect.r = static_cast<uint32_t>(std::min(5, size.width()));
	info.area_rect.b = static_cast<uint32_t>(std::min(5, size.height()));
	return AreaEdit{info, size};
}

std::optional<ProjectController::AreaEdit> ProjectController::editArea(int id)
{
	const lcf::rpg::MapInfo* stored = m_tree.mapInfo(id);
	if (!stored || stored->type != lcf::rpg::TreeMap::MapType_area)
		return std::nullopt;
	const QSize size = mapSize(stored->parent_map);
	if (!size.isValid())
	{
		dialogs().error(tr("Area Properties"), tr("The file %1 cannot be read.").arg(mapFileName(stored->parent_map)));
		return std::nullopt;
	}
	return AreaEdit{*stored, size};
}

int ProjectController::addArea(lcf::rpg::MapInfo info)
{
	// another program can have added a map while the dialog was open
	refreshTreeMap();
	if (!m_tree.mapInfo(info.parent_map))
		return 0;
	if (m_tree.mapInfo(info.ID))
		info.ID = freeMapId();
	insertMapInfo(info);
	return info.ID;
}

void ProjectController::setAreaInfo(int id, const lcf::rpg::MapInfo& info)
{
	if (m_tree.mapInfo(id))
		m_tree.setMapInfo(id, info);
	writeTreeMap();
}

bool ProjectController::deleteArea(int id, const QString& name)
{
	if (m_tree.type(id) != lcf::rpg::TreeMap::MapType_area)
		return false;
	if (dialogs().question(tr("Delete Area"), tr("%1 will be deleted. Proceed?").arg(name)) != DialogService::Answer::Yes)
		return false;
	refreshTreeMap();
	if (!m_tree.mapInfo(id))
		return false;
	removeMap(id);
	writeTreeMap();
	return true;
}

void ProjectController::onDatabaseChanged()
{
	if (!core().project()) {
		return;
	}
	const auto chipsets = core().project()->database().chipsets;
	if (!core().project()->reloadDatabase()) {
		emit statusMessage(tr("The database changed on disk but could not be read"), 5000);
		return;
	}
	// The maps are drawn again only when a chipset changed
	emit databaseReloaded(!(core().project()->database().chipsets == chipsets));
	emit statusMessage(tr("Database reloaded"), 3000);
	qCInfo(lcReload) << "database reloaded";
}

void ProjectController::onMapChanged(int id)
{
	if (MapDocument* doc = document(id)) {
		doc->reloadFromDisk();
		emit statusMessage(tr("%1 reloaded").arg(doc->name()), 3000);
		qCInfo(lcReload) << "map" << id << "reloaded";
	} else {
		qCDebug(lcReload) << "map" << id << "changed on disk, not open";
	}
}
