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

#include "shell_controller.h"
#include <QCoreApplication>
#include <QEvent>
#include <QSettings>
#include <algorithm>
#include "core.h"
#include "action_catalog.h"
#include "dialog_service.h"
#include "map_tabs_controller.h"
#include "project_controller.h"
#include "shell_dialogs.h"
#include "common/dbstring.h"
#include "common/editor_settings.h"
#include "common/logging.h"
#include "common/translations.h"
#include "model/game_runner.h"
#include "model/map_document.h"
#include "model/map_tools.h"
#include "model/project.h"
#include "model/project_convert.h"

using lcf::rpg::TreeMap;

namespace {

DialogService& dialogs()
{
	return DialogService::instance();
}

}

ShellController::ShellController(QObject* parent) :
	QObject(parent),
	m_project(new ProjectController(this)),
	m_tabs(new MapTabsController(m_project, this)),
	m_actions(new ActionCatalog(this))
{
	// The map tree read again: the node selected (removed: the one above it
	// that stays), its view, then the tabs of the maps removed close
	connect(m_project, &ProjectController::treeMapReloaded, this, [this] {
		if (m_selectedMap >= 0)
			setSelectedMap(m_project->keptNode(m_selectedMap));
		emit treeMapReloaded();
		m_tabs->followTreeMap();
		// the start map can be gone, or another one
		updateActions();
	});
	connect(m_tabs, &MapTabsController::currentMapChanged, this, &ShellController::statusChanged);
	// the tab of the map shown: its name, "*"
	connect(m_tabs, &QAbstractItemModel::dataChanged, this, &ShellController::statusChanged);
	connect(&core(), &Core::layerChanged, this, [this] {
		updateLayerChecks();
		emit statusChanged();
	});
	connect(&core(), &Core::toolChanged, this, [this] {
		updateToolChecks();
		emit statusChanged();
	});
	// Test play: the options are remembered
	const QSettings settings;
	m_actions->setChecked(QStringLiteral("actionFullScreenToggle"), settings.value(TESTPLAY_FULLSCREEN_KEY, false).toBool());
	m_actions->setChecked(QStringLiteral("actionTitleBackgroundToggle"), settings.value(TESTPLAY_SHOW_TITLE_KEY, true).toBool());
	core().setDefDir(settings.value(DEFAULT_DIR_KEY, QCoreApplication::applicationDirPath()).toString());
	// the options of the Settings window, now and when they change
	applySettings();
	connect(&EditorSettings::instance(), &EditorSettings::changed, this, &ShellController::applySettings);
	// the texts of the actions in a new language
	QCoreApplication::instance()->installEventFilter(this);
	updateLayerChecks();
	updateToolChecks();
	updateActions();
}

ShellController::~ShellController()
{
	if (QCoreApplication::instance())
		QCoreApplication::instance()->removeEventFilter(this);
}

ProjectController* ShellController::project() const
{
	return m_project;
}

MapTabsController* ShellController::tabs() const
{
	return m_tabs;
}

ActionCatalog* ShellController::actions() const
{
	return m_actions;
}

void ShellController::setDialogs(ShellDialogs* dialogs)
{
	m_dialogs = dialogs;
}

bool ShellController::eventFilter(QObject* watched, QEvent* event)
{
	if (event->type() == QEvent::LanguageChange && watched == QCoreApplication::instance()) {
		m_actions->retranslate();
		emit windowTitleChanged();
		emit statusChanged();
	}
	return QObject::eventFilter(watched, event);
}

int ShellController::selectedMap() const
{
	return m_selectionSource ? m_selectionSource() : m_selectedMap;
}

void ShellController::setSelectedMap(int id)
{
	if (m_selectedMap == id)
		return;
	m_selectedMap = id;
	emit selectedMapChanged(id);
}

void ShellController::setSelectionSource(std::function<int()> source)
{
	m_selectionSource = std::move(source);
}

QString ShellController::nodeName(int id) const
{
	// a map open shows the name it is edited with
	if (const MapDocument* doc = m_project->document(id))
		return doc->name();
	const lcf::rpg::MapInfo* info = m_project->treeModel()->mapInfo(id);
	return info ? ToQString(info->name) : QString();
}

/*
 * The project
 */

bool ShellController::openProject(const QString& folder)
{
	// read first: a folder that cannot be read leaves the project open
	auto project = m_project->read(folder);
	if (!project)
		return false;
	// The project open goes then, its maps asking about their changes: its
	// tabs would show data that is freed (Cancel keeps it open)
	if (core().project() && !closeProject())
		return false;
	m_project->open(project);
	QSettings().setValue(CURRENT_PROJECT_KEY, core().project()->projectDir().absolutePath());
	emit projectOpened();
	emit windowTitleChanged();
	m_project->updateWatcher();
	qCInfo(lcProject).noquote() << "project loaded:" << folder
		<< "maps:" << (core().project()->treeMap().maps.size() - 1)
		<< "reload changed files:" << EditorSettings::instance().reloadExternalChanges();
	updateActions();
	return true;
}

void ShellController::openLastProject()
{
	const QString last = QSettings().value(CURRENT_PROJECT_KEY, QString()).toString();
	if (!last.isEmpty())
		openProject(last);
}

bool ShellController::closeProject()
{
	if (!m_project->saveAll())
		return false;
	// its workspace, its windows
	emit projectClosing();
	QSettings().setValue(CURRENT_PROJECT_KEY, QString());
	m_tabs->closeAll();
	m_project->close();
	updateActions();
	emit windowTitleChanged();
	return true;
}

void ShellController::newProject()
{
	if (!m_dialogs)
		return;
	const auto chosen = m_dialogs->newProject(core().defDir());
	if (!chosen || chosen->folder.isEmpty())
		return;
	// in the folder of the games chosen in the window
	QDir folder(chosen->defDir + chosen->folder);
	// as RPG Maker: a folder that exists is refused, never emptied
	if (folder.exists()) {
		dialogs().warning(QCoreApplication::translate("MainWindow", "New Project"),
			QCoreApplication::translate("MainWindow", "The folder %1 already exists. Choose another folder name.")
				.arg(QDir::toNativeSeparators(folder.absolutePath())));
		return;
	}
	folder.mkpath(QStringLiteral("."));
	core().setTileSize(chosen->tileSize);
	core().setDefDir(chosen->defDir);
	for (const QString& dir : ProjectController::resourceDirs())
		folder.mkpath(dir);
	QSettings().setValue(DEFAULT_DIR_KEY, chosen->defDir);

	const QString error = Project::create(folder, chosen->title, static_cast<Project::Format>(chosen->format));
	if (!error.isEmpty()) {
		dialogs().error(QCoreApplication::translate("MainWindow", "Creating new project failed"), error);
		return;
	}
	openProject(folder.absolutePath());
}

void ShellController::openProjectWindow()
{
	if (!m_dialogs)
		return;
	const auto chosen = m_dialogs->openProject(core().defDir());
	core().setDefDir(chosen.defDir);
	QSettings().setValue(DEFAULT_DIR_KEY, chosen.defDir);
	if (chosen.choice == ShellDialogs::OpenProject::Choice::Import)
		importProjectWindow();
	else if (chosen.choice == ShellDialogs::OpenProject::Choice::Open)
		openProject(chosen.folder);
}

void ShellController::importProjectWindow()
{
	if (!m_dialogs)
		return;
	const auto chosen = m_dialogs->importProject(core().defDir());
	if (chosen.accepted) {
		core().setDefDir(chosen.defDir);
		if (chosen.folder.isEmpty())
			return;
		QDir target(chosen.defDir + chosen.folder);
		// as RPG Maker: a folder that exists is refused, never emptied
		if (target.exists()) {
			dialogs().warning(QCoreApplication::translate("MainWindow", "Import Project"),
				QCoreApplication::translate("MainWindow", "The folder %1 already exists. Choose another folder name.")
					.arg(QDir::toNativeSeparators(target.absolutePath())));
			return;
		}
		// the imported game is opened: the project open is closed first
		if (core().project() && !closeProject())
			return;
		target.mkpath(QStringLiteral("."));
		core().setTileSize(16);
		importProject(QDir(chosen.sourceFolder), target, chosen.convertXyz);
	}
	QSettings().setValue(DEFAULT_DIR_KEY, chosen.defDir);
	updateActions();
}

void ShellController::importProject(const QDir& src_dir, QDir& target_dir, bool convert_xyz)
{
	if (!m_dialogs)
		return;
	QString folder;
	auto result = ProjectController::ImportResult::Failed;
	// the progress over the window, while it is imported
	m_dialogs->withImportProgress([&](const ProjectController::ImportProgress& progress) {
		result = m_project->importProject(src_dir, target_dir, convert_xyz, progress, &folder);
	});
	switch (result) {
	case ProjectController::ImportResult::Imported:
		// opened like any project (map tree, search, watcher, workspace)
		openProject(folder);
		break;
	case ProjectController::ImportResult::CopyFailed:
		closeProject();
		break;
	case ProjectController::ImportResult::Failed:
		break;
	}
}

void ShellController::convertProject()
{
	auto project = core().project();
	if (!m_dialogs || !project || !ProjectConvert::canConvert(*project))
		return;
	const auto options = m_dialogs->convertProject(*project);
	if (!options)
		return;
	// every file is rewritten: the project is closed (asking to save the
	// maps), converted from its files, and opened again
	const QString path = project->projectDir().absolutePath();
	project.reset();
	if (!closeProject())
		return;
	QString backup;
	const QString error = m_project->convertTo2003(path, *options, &backup);
	openProject(path);
	m_dialogs->convertResult(error, backup);
}

/*
 * The map tree
 */

bool ShellController::openNode(int id, bool keep)
{
	if (!core().project() || id <= 0)
		return false;
	const MapTreeModel* tree = m_project->treeModel();
	if (tree->type(id) == TreeMap::MapType_area) {
		// an area has no map file: it is shown on its map
		const int parent = tree->parentId(id);
		if (parent <= 0)
			return false;
		const bool shown = openNode(parent, keep);
		emit selectMapRequested(id);
		const lcf::rpg::MapInfo* info = tree->mapInfo(id);
		if (m_tabs->isOpen(parent) && info) {
			const auto& r = info->area_rect;
			emit showAreaRequested(parent, QRect(r.l, r.t, r.r - r.l, r.b - r.t));
		}
		return shown;
	}
	return m_tabs->openMap(id, keep);
}

void ShellController::showNewNode(int id, bool open)
{
	// read again (merged, or the file's tree kept): it can be gone
	if (!m_project->treeModel()->contains(id))
		return;
	emit selectMapRequested(id);
	if (open)
		openNode(id, true);
}

void ShellController::newMap()
{
	if (!m_dialogs || !core().project())
		return;
	// the map tree as another program left it: these write it at once
	m_project->refreshTreeMap();
	lcf::rpg::MapInfo info = m_project->newMapInfo(selectedMap());
	// RPG Maker asks for the properties first: Cancel creates nothing
	lcf::rpg::Map map = MapTools::newMap();
	if (!m_dialogs->mapProperties(info, map))
		return;
	if (const int id = m_project->addNewMap(info, map))
		showNewNode(id, true);
}

void ShellController::copyMap()
{
	if (core().project() && m_project->copyMap(selectedMap()))
		updateActions();
}

void ShellController::pasteMap()
{
	if (!core().project())
		return;
	m_project->refreshTreeMap();
	if (const int id = m_project->pasteMap(selectedMap()))
		showNewNode(id, true);
}

void ShellController::deleteMap()
{
	if (!core().project())
		return;
	m_project->refreshTreeMap();
	const int id = selectedMap();
	if (m_project->treeModel()->type(id) != TreeMap::MapType_map)
		return;
	if (m_project->deleteMap(id, nodeName(id)))
		updateActions();
}

void ShellController::mapProperties()
{
	if (!m_dialogs || !core().project())
		return;
	m_project->refreshTreeMap();
	// the map selected in the tree, else the map shown
	int id = selectedMap();
	const int type = m_project->treeModel()->type(id);
	if (type == TreeMap::MapType_area) {
		areaProperties();
		return;
	}
	if (type != TreeMap::MapType_map) {
		if (!m_tabs->currentMap())
			return;
		id = m_tabs->currentMap();
	}

	// an open map: its document changes, with undo
	if (MapDocument* doc = m_tabs->isOpen(id) ? m_project->document(id) : nullptr) {
		emit floatingCommitRequested(id);
		// the dialog changes copies
		lcf::rpg::Map map = doc->map();
		lcf::rpg::MapInfo info = doc->mapInfo();
		if (!m_dialogs->mapProperties(info, map))
			return;
		// A new name goes into the map tree file at once, as the tree shows
		// it; the other properties are an edit of the map, saved with it
		if (info.name != doc->mapInfo().name)
			m_project->renameNode(id, ToQString(info.name));
		doc->setMapProperties(map, info);
		return;
	}
	// a map that is not open is edited on disk
	auto edit = m_project->readMapProperties(id);
	if (!edit)
		return;
	if (!m_dialogs->mapProperties(edit->info, *edit->map))
		return;
	m_project->saveMapProperties(id, *edit);
}

void ShellController::generateDungeon()
{
	if (!m_dialogs || !core().project())
		return;
	m_project->refreshTreeMap();
	auto source = m_project->dungeonSource(selectedMap());
	if (!source)
		return;
	lcf::rpg::MapInfo info = source->info;
	auto dungeon = m_dialogs->generateDungeon(*source->map, ProjectController::defaultMapName(info.ID));
	if (!dungeon)
		return;
	info.name = ToDBString(dungeon->name);
	if (const int id = m_project->addDungeon(info, dungeon->map))
		showNewNode(id, true);
}

void ShellController::shiftMap()
{
	if (!m_dialogs || !core().project())
		return;
	m_project->refreshTreeMap();
	const int id = selectedMap();
	if (m_project->treeModel()->type(id) != TreeMap::MapType_map)
		return;
	const auto offset = m_dialogs->shiftMap();
	if (!offset)
		return;
	// An open map is shifted in its tab, with undo, and saved with it;
	// another one on disk
	if (m_tabs->isOpen(id))
		emit shiftRequested(id, offset->x(), offset->y());
	else
		m_project->shiftMap(id, offset->x(), offset->y());
}

void ShellController::newArea()
{
	if (!m_dialogs || !core().project())
		return;
	m_project->refreshTreeMap();
	auto area = m_project->newArea(selectedMap());
	if (!area || !m_dialogs->areaProperties(area->info, area->map_size))
		return;
	if (const int id = m_project->addArea(area->info))
		showNewNode(id, false);
}

void ShellController::areaProperties()
{
	if (!m_dialogs || !core().project())
		return;
	m_project->refreshTreeMap();
	const int id = selectedMap();
	auto area = m_project->editArea(id);
	if (!area || !m_dialogs->areaProperties(area->info, area->map_size))
		return;
	m_project->setAreaInfo(id, area->info);
}

void ShellController::deleteArea()
{
	if (!core().project())
		return;
	m_project->refreshTreeMap();
	const int id = selectedMap();
	if (m_project->treeModel()->type(id) != TreeMap::MapType_area)
		return;
	if (m_project->deleteArea(id, nodeName(id)))
		updateActions();
}

void ShellController::moveMaps(const QList<MapMove>& moves)
{
	if (!core().project())
		return;
	// where the view put the maps dropped, and what it shows expanded
	MapTreeModel* tree = m_project->treeModel();
	bool changed = false;
	for (const MapMove& move : moves)
		changed = tree->moveMap(move.id, move.parent, move.row) || changed;
	changed = m_project->syncView() || changed;
	changed = tree->syncOrder() || changed;
	if (changed) {
		m_project->writeTreeMap();
		// the open maps keep a copy of their tree entry
		for (int id : m_tabs->mapIds()) {
			if (MapDocument* doc = m_project->document(id))
				doc->refreshMapInfo();
		}
	}
	updateActions();
}

/*
 * Going to a map
 */

int ShellController::startMap() const
{
	if (!core().project())
		return 0;
	const int id = core().project()->treeMap().start.party_map_id;
	return m_project->treeModel()->type(id) == TreeMap::MapType_map ? id : 0;
}

QList<ShellController::MapEntry> ShellController::maps() const
{
	QList<MapEntry> maps;
	if (!core().project())
		return maps;
	const MapTreeModel* tree = m_project->treeModel();
	for (int id : tree->ids()) {
		if (tree->type(id) == TreeMap::MapType_map)
			maps.append({id, nodeName(id)});
	}
	std::ranges::sort(maps, {}, &MapEntry::id);
	return maps;
}

bool ShellController::goToMap(int id)
{
	if (!core().project() || m_project->treeModel()->type(id) != TreeMap::MapType_map)
		return false;
	emit selectMapRequested(id);
	return openNode(id, true);
}

bool ShellController::openStartMap()
{
	return goToMap(startMap());
}

/*
 * The map shown
 */

void ShellController::saveMap()
{
	if (MapDocument* doc = m_tabs->currentDocument())
		doc->save();
}

void ShellController::saveAllMaps()
{
	m_project->saveAllMaps();
}

void ShellController::revertMap()
{
	MapDocument* doc = m_tabs->currentDocument();
	if (!doc)
		return;
	if (dialogs().question(QCoreApplication::translate("MainWindow", "Revert Map"), QCoreApplication::translate("MainWindow", "Map data will revert to the last time it was saved. Proceed?"))
			!= DialogService::Answer::Yes)
		return;
	emit floatingCancelRequested(doc->id());
	doc->load(true);
}

void ShellController::setLayer(EditorState::Layer layer)
{
	core().setLayer(layer);
	// the same layer again: checked again (a click unchecked it)
	updateLayerChecks();
}

void ShellController::setTool(EditorState::Tool tool)
{
	core().setTool(tool);
}

double ShellController::zoomRatio(const QString& action)
{
	// RPG Maker's ratios; maps open at 1/2, tiles twice their size
	if (action == QLatin1String("actionZoom1_1"))
		return 4.0;
	if (action == QLatin1String("actionZoom1_2"))
		return 2.0;
	if (action == QLatin1String("actionZoom1_4"))
		return 1.0;
	if (action == QLatin1String("actionZoom1_8"))
		return 0.5;
	return 0;
}

void ShellController::setZoomRatio(double ratio)
{
	m_zoomRatio = ratio;
	// none checked at the other scales
	for (const QString& action : m_actions->groupActions(ActionCatalog::Group::Zoom))
		m_actions->setChecked(action, ratio > 0 && zoomRatio(action) == ratio);
}

void ShellController::setTestPlayFullScreen(bool on)
{
	QSettings().setValue(TESTPLAY_FULLSCREEN_KEY, on);
	m_actions->setChecked(QStringLiteral("actionFullScreenToggle"), on);
}

void ShellController::setTestPlayShowTitle(bool on)
{
	QSettings().setValue(TESTPLAY_SHOW_TITLE_KEY, on);
	m_actions->setChecked(QStringLiteral("actionTitleBackgroundToggle"), on);
}

/*
 * What the window shows
 */

void ShellController::updateLayerChecks()
{
	m_actions->setChecked(QStringLiteral("actionLayerLower"), core().layer() == Core::LOWER);
	m_actions->setChecked(QStringLiteral("actionLayerUpper"), core().layer() == Core::UPPER);
	m_actions->setChecked(QStringLiteral("actionLayerEvents"), core().layer() == Core::EVENT);
}

void ShellController::updateToolChecks()
{
	m_actions->setChecked(QStringLiteral("actionZoom"), core().tool() == Core::ZOOM);
	m_actions->setChecked(QStringLiteral("actionDrawPen"), core().tool() == Core::PENCIL);
	m_actions->setChecked(QStringLiteral("actionDrawRectangle"), core().tool() == Core::RECTANGLE);
	m_actions->setChecked(QStringLiteral("actionDrawCircle"), core().tool() == Core::CIRCLE);
	m_actions->setChecked(QStringLiteral("actionDrawSelect"), core().tool() == Core::SELECT);
	m_actions->setChecked(QStringLiteral("actionDrawFill"), core().tool() == Core::FILL);
}

void ShellController::updateActions()
{
	const bool has_project = core().project() != nullptr;
	const MapDocument* doc = has_project ? m_tabs->currentDocument() : nullptr;
	// the selected node of the map tree (nothing selected: the project)
	const int type = has_project ? m_project->treeModel()->type(selectedMap()) : -1;
	const bool on_root = has_project && (type == TreeMap::MapType_root || type == -1);
	const bool on_map = type == TreeMap::MapType_map;
	const bool on_area = type == TreeMap::MapType_area;
	auto enable = [this](const char* name, bool enabled) {
		m_actions->setEnabled(QLatin1String(name), enabled);
	};

	for (const char* name : {"actionDrawCircle", "actionDrawSelect", "actionGameDiskCreate", "actionDatabase",
			"actionDrawPen", "actionDrawFill", "actionDrawRectangle", "actionResourceManager", "actionSearch",
			"actionJukebox", "actionProjectSettings", "actionMapSaveAll", "actionZoom", "actionProjectClose",
			"actionLayerEvents", "actionFullScreenToggle", "actionLayerLower", "actionLayerUpper", "actionPlayTest",
			"actionPlayTestOptions", "actionScriptEditor", "actionTitleBackgroundToggle"})
		enable(name, has_project);
	// zooming needs a map
	for (const char* name : {"actionZoom100", "actionZoomIn", "actionZoomOut"})
		enable(name, doc);
	for (const QString& ratio : m_actions->groupActions(ActionCatalog::Group::Zoom))
		m_actions->setEnabled(ratio, doc);
	enable("actionMapRevert", doc && doc->isModified());
	enable("actionMapSave", doc && doc->isModified());
	enable("actionUndo", doc && doc->canUndo());
	enable("actionRedo", doc && doc->canRedo());
	enable("actionProjectConvert2003", has_project && ProjectConvert::canConvert(*core().project()));
	enable("actionMapOpenStart", startMap() > 0);
	enable("actionMapGoTo", has_project);
	enable("actionProjectNew", !has_project);
	enable("actionProjectOpen", !has_project);

	// Map tree: what RPG Maker's menus offer on the selected node
	enable("actionMapNew", on_root || on_map);
	enable("actionMapProperties", on_map || on_area || doc);
	enable("actionMapCopy", on_map);
	enable("actionMapPaste", m_project->hasCopiedMap() && (on_root || on_map));
	enable("actionMapDelete", on_map);
	enable("actionMapShift", on_map);
	enable("actionAreaNew", on_map);
	enable("actionMapGenerateDungeon", on_map);
	enable("actionAreaProperties", on_area);
	enable("actionAreaDelete", on_area);
	emit statusChanged();
}

QString ShellController::windowTitle() const
{
	return core().project() ? QCoreApplication::translate("MainWindow", "EasyRPG Editor") + QStringLiteral(" - ") + core().project()->gameTitle()
		: QCoreApplication::translate("MainWindow", "EasyRPG Editor");
}

QString ShellController::editModeText() const
{
	if (!core().project() || !m_tabs->currentDocument())
		return {};
	QString mode;
	switch (core().layer()) {
	case Core::LOWER:
		mode = QCoreApplication::translate("MainWindow", "Lower Layer Editing Mode");
		break;
	case Core::UPPER:
		mode = QCoreApplication::translate("MainWindow", "Upper Layer Editing Mode");
		break;
	case Core::EVENT:
		mode = QCoreApplication::translate("MainWindow", "Event Editing Mode");
		break;
	}
	// the tools draw on the tile layers only
	if (core().layer() == Core::EVENT)
		return mode;
	QString tool;
	switch (core().tool()) {
	case Core::ZOOM:
		tool = QCoreApplication::translate("MainWindow", "Zoom Tool");
		break;
	case Core::PENCIL:
		tool = QCoreApplication::translate("MainWindow", "Pen Tool");
		break;
	case Core::RECTANGLE:
		tool = QCoreApplication::translate("MainWindow", "Rectangle Tool");
		break;
	case Core::CIRCLE:
		tool = QCoreApplication::translate("MainWindow", "Ellipse Tool");
		break;
	case Core::FILL:
		tool = QCoreApplication::translate("MainWindow", "Flood Fill");
		break;
	case Core::SELECT:
		tool = QCoreApplication::translate("MainWindow", "Selection Tool");
		break;
	}
	//: Status bar: the edit mode and the tool
	return QCoreApplication::translate("MainWindow", "%1 - %2").arg(mode, tool);
}

QString ShellController::mapText() const
{
	const MapDocument* doc = core().project() ? m_tabs->currentDocument() : nullptr;
	if (!doc)
		return {};
	//: Status bar: the id and the name of the map shown
	return QCoreApplication::translate("MainWindow", "%1: %2").arg(doc->id(), 4, 10, QChar('0')).arg(doc->name());
}

void ShellController::applySettings()
{
	const EditorSettings& settings = EditorSettings::instance();
	core().setRtpDir(settings.rtpPath());
	core().setAnimationEnabled(settings.animateTiles());
	m_project->updateWatcher();
	// a new language at once: every window shows its texts again (the one
	// the Editor started in can come from --language, not from the settings)
	if (m_languageKnown && m_language != settings.language())
		Translations::apply(settings.language());
	m_language = settings.language();
	m_languageKnown = true;
}
