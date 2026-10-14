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

#include "qml_shell.h"
#include <algorithm>
#include <optional>
#include <utility>
#include <QDir>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <kddockwidgets/LayoutSaver.h>
#include <lcf/rpg/treemap.h>
#include "core.h"
#include "common/editor_settings.h"
#include "common/script_link.h"
#include "controller/action_catalog.h"
#include "controller/dialog_service.h"
#include "controller/map_tabs_controller.h"
#include "controller/map_tree_drop.h"
#include "controller/project_controller.h"
#include "controller/shell_controller.h"
#include "model/export_controller.h"
#include "model/editor_update.h"
#include "model/map_document.h"
#include "model/map_tree_model.h"
#include "model/project.h"
#include "model/project_settings.h"
#include "model/test_play_options.h"
#include "database_windows.h"
#include "qml_engine.h"
#include "qml_shell_dialogs.h"
#include "qmlbinding/qml_host.h"
#include "qmlbinding/resource_windows.h"
#include "qmlbinding/test_play.h"
#include "model/event_editor_controller.h"
#include "ui/event/event_window.h"
#include "ui/map/map_editor.h"
#include "ui/picker/pickers.h"
#include "ui/other/search_window_host.h"

namespace {

/** The scale maps open at, the 1/2 ratio */
constexpr double default_map_scale = 2.0;

/** The name the window's geometry is kept under (WorkspaceStore::dialogGeometry) */
const QString window_state_name = QStringLiteral("MainWindow");

QString windowText(const char* text)
{
	return QCoreApplication::translate("MainWindow", text);
}

}

QmlShell::QmlShell(QObject* parent) :
	QObject(parent),
	m_shell(new ShellController(this)),
	m_project(m_shell->project()),
	m_tabs(m_shell->tabs()),
	m_actions(m_shell->actions())
{
	// The controllers ask their questions in QML windows over the window in
	// front
	m_dialogService = std::make_unique<QmlShellDialogService>([this] { return static_cast<QWindow*>(m_window.data()); });
	DialogService::install(m_dialogService.get());
	m_dialogs = std::make_unique<QmlShellDialogs>([this] { return static_cast<QWindow*>(m_window.data()); });
	m_shell->setDialogs(m_dialogs.get());
	m_project->setViewSync([this] { return syncExpandedFromView(); });
	m_statusTimer.setSingleShot(true);
	connect(&m_statusTimer, &QTimer::timeout, this, [this] { setStatusMessage(QString(), 0); });
	// before the window's views: the editor of a tab exists when its view is made
	connectTabs();
	connectController();
	connect(m_actions, &ActionCatalog::triggered, this, [this](const QString& name) {
		trigger(name, m_actions->isCheckable(name) ? !m_actions->isChecked(name) : false);
	});
	connect(&EditorSettings::instance(), &EditorSettings::changed, this, [this] {
		if (m_search)
			m_search->enableCache(EditorSettings::instance().searchCache());
	});
	connect(&EditorSettings::instance(), &EditorSettings::localStateReset, this, &QmlShell::resetLocalState);
	// Check for Updates: shown when updates are looked for, its text as they are
	EditorUpdater& updater = EditorUpdater::instance();
	m_actions->setVisible(QStringLiteral("actionCheckUpdates"), updater.isEnabled());
	connect(&updater, &EditorUpdater::stateChanged, this, &QmlShell::updateUpdateAction);
	connect(m_actions, &ActionCatalog::retranslated, this, &QmlShell::updateUpdateAction);
	connect(&updater, &EditorUpdater::restartRequested, this, &QmlShell::restartForUpdate);
	connect(qApp, &QCoreApplication::aboutToQuit, this, &QmlShell::saveWorkspace);
	connect(qApp, &QGuiApplication::focusWindowChanged, this, &QmlShell::windowFocusedChanged);
	m_shell->updateActions();
}

QmlShell::~QmlShell()
{
	// the views of the maps go before their editors and documents
	delete m_window.data();
	qDeleteAll(m_editors);
	m_editors.clear();
	m_project->setViewSync({});
	m_shell->setDialogs(nullptr);
}

bool QmlShell::show()
{
	if (!m_window) {
		m_window = QmlHost::make(QStringLiteral("MainWindow"), {{QStringLiteral("shell"), QVariant::fromValue(this)}});
		if (!m_window)
			return false;
		// the docks as the window made them, for a reset
		m_defaultLayout = dockLayout();
		const QRect geometry = m_workspaces.dialogGeometry(window_state_name);
		if (geometry.isValid())
			m_window->setGeometry(geometry);
		// without a project: the docks as last time
		if (!core().project())
			restoreDockLayout(m_workspaces.windowDockLayout());
		// a project opened before the window: the view of its workspace
		if (!m_pendingView.isEmpty())
			emit viewRestoreRequested(std::exchange(m_pendingView, {}));
	}
	m_window->show();
	// a project opened before the window: its docks now
	if (core().project())
		restoreDockLayout(m_workspaces.load(core().project()->projectDir().absolutePath()).dockLayout);
	m_window->raise();
	m_window->requestActivate();
	return true;
}

QQuickWindow* QmlShell::window() const
{
	return m_window;
}

ShellController* QmlShell::controller() const
{
	return m_shell;
}

ActionCatalog* QmlShell::actions() const
{
	return m_actions;
}

MapTabsController* QmlShell::tabs() const
{
	return m_tabs;
}

MapTreeModel* QmlShell::tree() const
{
	return m_project->treeModel();
}

EditorState* QmlShell::editorState() const
{
	return &core().editorState();
}

MapEditor* QmlShell::currentEditor() const
{
	return m_editors.value(m_tabs->currentMap());
}

bool QmlShell::hasProject() const
{
	return core().project() != nullptr;
}

QString QmlShell::statusMessage() const
{
	return m_statusMessage;
}

bool QmlShell::windowFocused() const
{
	return m_window && QGuiApplication::focusWindow() == m_window;
}

/*
 * EditorWindow
 */

QWindow* QmlShell::handle() const
{
	return m_window;
}

ShellController* QmlShell::shell() const
{
	return m_shell;
}

ProjectController* QmlShell::projectController() const
{
	return m_project;
}

void QmlShell::askForRtp()
{
	if (core().rtpPath("").isEmpty())
		m_dialogs->settings();
}

void QmlShell::loadLastProject()
{
	m_shell->openLastProject();
}

void QmlShell::LoadProject(QString folder)
{
	m_shell->openProject(folder);
}

bool QmlShell::hasMap(int id) const
{
	return id > 0 && core().project() && m_project->treeModel()->contains(id);
}

void QmlShell::openScene(int id)
{
	selectNode(id);
}

const lcf::rpg::Map* QmlShell::openMapData(int id)
{
	MapDocument* doc = m_tabs->isOpen(id) ? m_project->document(id) : nullptr;
	return doc && doc->hasMap() ? &doc->map() : nullptr;
}

bool QmlShell::openEvent(int map, int event_id, int page, int line)
{
	if (!hasMap(map))
		return false;
	selectNode(map);
	MapEditor* editor = m_editors.value(map);
	const lcf::rpg::Event* event = editor ? editor->document()->findEvent(event_id) : nullptr;
	if (!event || m_tabs->currentMap() != map)
		return false;
	// its cell selected on the event layer, in the middle of the view
	m_shell->setLayer(EditorState::EVENT);
	editor->selectTile(event->x, event->y);
	emit centerRequested(map, event->x, event->y);
	if (m_window) {
		m_window->raise();
		m_window->requestActivate();
	}
	// a copy: the window edits it, the map keeps its own
	const lcf::rpg::Event shown = *event;
	editEvent(map, shown, page, line);
	return true;
}

bool QmlShell::selectNode(int id, bool focus)
{
	if (!core().project() || !m_project->treeModel()->contains(id))
		return false;
	if (focus && m_window)
		QMetaObject::invokeMethod(m_window, "focusTree");
	m_shell->setSelectedMap(id);
	emit treeSelectRequested(id);
	openNode(id, false);
	return true;
}

bool QmlShell::editMapEvent(int map, int event_id)
{
	if (!hasMap(map))
		return false;
	selectNode(map);
	MapDocument* doc = m_tabs->currentMap() == map ? m_project->document(map) : nullptr;
	const lcf::rpg::Event* event = doc ? doc->findEvent(event_id) : nullptr;
	if (!event)
		return false;
	// a copy: the window edits it, the map keeps its own
	const lcf::rpg::Event shown = *event;
	editEvent(map, shown);
	return true;
}

/*
 * The window's requests
 */

MapEditor* QmlShell::editorOf(int id) const
{
	return m_editors.value(id);
}

void QmlShell::trigger(const QString& name, bool checked)
{
	if (!m_actions->isEnabled(name))
		return;
	// a checkable one is checked as the window shows it
	if (m_actions->isCheckable(name))
		m_actions->setChecked(name, checked);
	auto is = [&name](const char* action) { return name == QLatin1String(action); };
	MapEditor* editor = currentEditor();

	// Game Project
	if (is("actionProjectNew"))
		m_shell->newProject();
	else if (is("actionProjectOpen"))
		m_shell->openProjectWindow();
	else if (is("actionProjectClose"))
		m_shell->closeProject();
	else if (is("actionProjectSettings"))
		projectSettings();
	else if (is("actionGameDiskCreate"))
		exportGame();
	else if (is("actionProjectConvert2003"))
		m_shell->convertProject();
	else if (is("actionProjectImport"))
		m_shell->importProjectWindow();
	else if (is("actionQuit"))
		quit();
	// Maps
	else if (is("actionUndo")) {
		if (editor)
			editor->undo();
	} else if (is("actionRedo")) {
		if (editor)
			editor->redo();
	} else if (is("actionMapSave"))
		m_shell->saveMap();
	else if (is("actionMapSaveAll"))
		m_shell->saveAllMaps();
	else if (is("actionMapRevert"))
		m_shell->revertMap();
	else if (is("actionLayerLower"))
		m_shell->setLayer(EditorState::LOWER);
	else if (is("actionLayerUpper"))
		m_shell->setLayer(EditorState::UPPER);
	else if (is("actionLayerEvents"))
		m_shell->setLayer(EditorState::EVENT);
	else if (is("actionZoom100")) {
		if (editor)
			editor->setScale(default_map_scale);
	} else if (is("actionZoomIn")) {
		if (editor)
			editor->zoomIn();
	} else if (is("actionZoomOut")) {
		if (editor)
			editor->zoomOut();
	} else if (const double ratio = ShellController::zoomRatio(name); ratio > 0) {
		if (editor)
			editor->setScale(ratio);
		// a click on the checked ratio unchecks it: checked again
		updateZoomActions();
	}
	// the tools of the palette
	else if (is("actionDrawSelect"))
		m_shell->setTool(EditorState::SELECT);
	else if (is("actionZoom"))
		m_shell->setTool(EditorState::ZOOM);
	else if (is("actionDrawPen"))
		m_shell->setTool(EditorState::PENCIL);
	else if (is("actionDrawRectangle"))
		m_shell->setTool(EditorState::RECTANGLE);
	else if (is("actionDrawCircle"))
		m_shell->setTool(EditorState::CIRCLE);
	else if (is("actionDrawFill"))
		m_shell->setTool(EditorState::FILL);
	// View
	else if (is("actionPaletteToggle"))
		emit dockRequested(QStringLiteral("dockPalette"), checked);
	else if (is("actionMapTreeToggle"))
		emit dockRequested(QStringLiteral("dockMapTree"), checked);
	// Tools
	else if (is("actionDatabase"))
		openDatabase();
	else if (is("actionResourceManager"))
		ResourceWindows::resourceManager(m_window);
	else if (is("actionJukebox"))
		// one window, kept between openings
		ResourceWindows::jukebox(m_window);
	else if (is("actionSearch"))
		openSearch();
	else if (is("actionSettings"))
		m_dialogs->settings();
	// Game
	else if (is("actionPlayTest"))
		TestPlay::launch(m_window, {});
	else if (is("actionPlayTestOptions"))
		testPlayOptions();
	else if (is("actionFullScreenToggle"))
		m_shell->setTestPlayFullScreen(checked);
	else if (is("actionTitleBackgroundToggle"))
		m_shell->setTestPlayShowTitle(checked);
	// Help
	else if (is("actionContents"))
		m_dialogs->help(QStringLiteral("top"));
	else if (is("actionAbout"))
		m_dialogs->about();
	else if (is("actionCheckUpdates"))
		EditorUpdater::instance().activate();
	// the map tree's
	else if (is("actionMapNew"))
		m_shell->newMap();
	else if (is("actionMapCopy"))
		m_shell->copyMap();
	else if (is("actionMapPaste"))
		m_shell->pasteMap();
	else if (is("actionMapDelete"))
		m_shell->deleteMap();
	else if (is("actionMapProperties"))
		m_shell->mapProperties();
	else if (is("actionMapGenerateDungeon"))
		m_shell->generateDungeon();
	else if (is("actionMapShift"))
		m_shell->shiftMap();
	else if (is("actionAreaNew"))
		m_shell->newArea();
	else if (is("actionAreaProperties"))
		m_shell->areaProperties();
	else if (is("actionAreaDelete"))
		m_shell->deleteArea();
}

void QmlShell::openNode(int id, bool keep)
{
	m_shell->openNode(id, keep);
}

QList<int> QmlShell::treeIds() const
{
	return core().project() ? m_project->treeModel()->ids() : QList<int>();
}

QModelIndex QmlShell::treeIndex(int id) const
{
	return m_project->treeModel()->indexOf(id);
}

int QmlShell::treeId(const QModelIndex& index) const
{
	return m_project->treeModel()->idOf(index);
}

QString QmlShell::treeName(int id) const
{
	return m_project->treeModel()->indexOf(id).data(MapTreeModel::NameRole).toString();
}

int QmlShell::treeType(int id) const
{
	return m_project->treeModel()->type(id);
}

void QmlShell::setExpanded(int id, bool expanded)
{
	m_project->treeModel()->setExpanded(id, expanded);
}

bool QmlShell::canDrop(int id, int target, int position) const
{
	if (!core().project() || position < 0 || position > 2)
		return false;
	return MapTreeDrop::move(*m_project->treeModel(), id, target, static_cast<MapTreeDrop::Position>(position)).has_value();
}

bool QmlShell::dropMap(int id, int target, int position)
{
	if (!canDrop(id, target, position))
		return false;
	const auto move = MapTreeDrop::move(*m_project->treeModel(), id, target, static_cast<MapTreeDrop::Position>(position));
	m_shell->moveMaps({*move});
	return true;
}

QVariantList QmlShell::scriptItems(int id) const
{
	QVariantList items;
	if (!core().project())
		return items;
	// greyed without rpgsync: the tooltip says how to set it up
	const auto locations = ScriptLink::mapTreeLocations(id, treeType(id));
	const QString reason = ScriptLink::unavailableReason();
	auto add = [&](const QString& text, const QString& tip, const QStringList& location) {
		if (location.isEmpty())
			return;
		items.append(QVariantMap{{"text", text}, {"toolTip", reason.isEmpty() ? tip : reason},
			{"enabled", reason.isEmpty()}, {"location", location}});
	};
	add(windowText(QT_TRANSLATE_NOOP("MainWindow", "Open in Script Editor")), windowText(QT_TRANSLATE_NOOP("MainWindow", "Open the script of this map")), locations.script);
	if (id == 0)
		add(windowText(QT_TRANSLATE_NOOP("MainWindow", "Open Map Tree in Script Editor")), windowText(QT_TRANSLATE_NOOP("MainWindow", "Open map_tree.py, the map tree of the game")), locations.entry);
	else
		add(windowText(QT_TRANSLATE_NOOP("MainWindow", "Open Map Tree Entry in Script Editor")), windowText(QT_TRANSLATE_NOOP("MainWindow", "Open map_tree.py at this entry")), locations.entry);
	return items;
}

void QmlShell::openScript(const QStringList& location)
{
	ScriptLink::open(this, location, -1, [](const QString& error) {
		DialogService::instance().warning(ScriptLink::title(), error);
	});
}

bool QmlShell::requestClose()
{
	if (m_closing)
		return true;
	if (!m_project->saveAll())
		return false;
	saveWorkspace();
	m_closing = true;
	return true;
}

/*
 * The controllers
 */

void QmlShell::connectController()
{
	connect(m_shell, &ShellController::projectOpened, this, &QmlShell::onProjectOpened);
	connect(m_shell, &ShellController::projectClosing, this, &QmlShell::onProjectClosing);
	// what can be done depends on the node: project, map or area
	connect(m_shell, &ShellController::selectedMapChanged, m_shell, &ShellController::updateActions);
	connect(m_shell, &ShellController::selectMapRequested, this, [this](int id) {
		m_shell->setSelectedMap(id);
		emit treeSelectRequested(id);
	});
	// what only the view of a map does
	connect(m_shell, &ShellController::showAreaRequested, this, &QmlShell::showAreaRequested);
	connect(m_shell, &ShellController::floatingCommitRequested, this, [this](int id) {
		if (MapEditor* editor = m_editors.value(id))
			editor->commitFloating();
	});
	connect(m_shell, &ShellController::floatingCancelRequested, this, [this](int id) {
		if (MapEditor* editor = m_editors.value(id))
			editor->cancelFloating();
	});
	connect(m_shell, &ShellController::shiftRequested, this, [this](int id, int dx, int dy) {
		if (MapEditor* editor = m_editors.value(id))
			editor->shiftMap(dx, dy);
	});
	// what other programs (a script sync tool) write into the project
	connect(m_project, &ProjectController::statusMessage, this, &QmlShell::setStatusMessage);
	// the QML windows show the project too
	connect(m_project, &ProjectController::projectChanged, this, [this] {
		QmlEngine::setProject(core().project() ? &core().project()->projectData() : nullptr);
		emit projectChanged();
	});
	connect(m_project, &ProjectController::databaseReloaded, this, [this](bool chipsets_changed) {
		QmlEngine::projectReloaded();
		if (chipsets_changed)
			followCurrentMap();
	});
	connect(m_project, &ProjectController::treeMapReloaded, this, [] { QmlEngine::projectReloaded(); });
}

void QmlShell::connectTabs()
{
	connect(m_tabs, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex&, int first, int last) {
		for (int row = first; row <= last; row++)
			createEditor(m_tabs->mapAt(row));
	});
	// the view went with its row: its editor goes before the document
	connect(m_tabs, &MapTabsController::mapClosing, this, &QmlShell::destroyEditor);
	connect(m_tabs, &MapTabsController::currentMapChanged, this, &QmlShell::onCurrentMapChanged);
	// a change of a tab (its "*"): Save and Revert follow
	connect(m_tabs, &QAbstractItemModel::dataChanged, m_shell, &ShellController::updateActions);
}

void QmlShell::createEditor(int id)
{
	MapDocument* doc = m_project->document(id);
	if (!doc || m_editors.contains(id))
		return;
	// read with its tab, as its view would
	if (!doc->hasMap())
		doc->load();
	auto* editor = new MapEditor(doc, core().editorState(), this);
	m_editors.insert(id, editor);
	// as the workspace had it: its zoom, its scroll and the cell selected
	if (const auto view = m_pendingViews.take(id); view.id == id) {
		editor->setScale(view.zoom > 0 ? view.zoom : default_map_scale);
		if (view.scroll && editor->scale() > 0)
			editor->setScrollPosition(*view.scroll / editor->scale());
		const QSize size = editor->mapSize();
		if (view.cell.x() >= 0 && view.cell.y() >= 0 && !size.isEmpty())
			editor->selectTile(std::clamp(view.cell.x(), 0, size.width() - 1), std::clamp(view.cell.y(), 0, size.height() - 1));
	}
	connect(editor, &MapEditor::scaleChanged, this, [this, id] {
		if (id == m_tabs->currentMap())
			updateZoomActions();
	});
	// Undo, Redo, Save
	connect(editor, &MapEditor::historyChanged, m_shell, &ShellController::updateActions);
	connect(doc, &MapDocument::modifiedChanged, m_shell, &ShellController::updateActions);
	// the events and the chipset of the map shown are new
	for (auto signal : {&MapDocument::eventsChanged, &MapDocument::loaded, &MapDocument::reloaded}) {
		connect(doc, signal, this, [this, id] {
			if (id == m_tabs->currentMap())
				followCurrentMap();
		});
	}
	connect(doc, &MapDocument::mapReplaced, this, [this, id] {
		if (id == m_tabs->currentMap())
			followCurrentMap();
	});
	// What the map asks for: queued, the windows wait for their answer
	// outside of the input of the view
	connect(editor, &MapEditor::newEventRequested, this, [this, id, doc](const QPoint& cell) {
		editEvent(id, doc->newEvent(cell.x(), cell.y()));
	}, Qt::QueuedConnection);
	connect(editor, &MapEditor::editEventRequested, this, [this, id, doc](int event) {
		if (const lcf::rpg::Event* found = doc->findEvent(event)) {
			const lcf::rpg::Event shown = *found;
			editEvent(id, shown);
		}
	}, Qt::QueuedConnection);
	connect(editor, &MapEditor::placeTeleportRequested, this, [this, id](const QPoint& cell) {
		placeTeleport(id, cell);
	}, Qt::QueuedConnection);
	connect(editor, &MapEditor::runHereRequested, this, [this, id](const QPoint& cell) {
		TestPlay::runHere(m_window, id, cell.x(), cell.y());
	}, Qt::QueuedConnection);
}

void QmlShell::destroyEditor(int id)
{
	MapEditor* editor = m_editors.take(id);
	if (!editor)
		return;
	m_pendingViews.remove(id);
	editor->disconnect(this);
	// its document goes now: nothing reaches it until it is deleted, later
	// (the view removed with its row may still read it)
	QObject::disconnect(&core().editorState(), nullptr, editor, nullptr);
	QObject::disconnect(editor->document(), nullptr, editor, nullptr);
	editor->deleteLater();
	if (m_tabs->currentMap() == id)
		emit currentEditorChanged();
}

void QmlShell::editEvent(int map, const lcf::rpg::Event& event, int page, int line)
{
	MapDocument* doc = m_project->document(map);
	if (!doc || !core().project())
		return;
	ProjectData& project = core().project()->projectData();
	const int id = event.ID;
	EventEditorController controller(doc->id());
	controller.setRpg2k3(project.database().system.ldb_id == 2003);
	// new: an event this map does not have yet (New Event)
	controller.setEvent(event, doc->findEvent(id) == nullptr);
	// Apply puts it on the map at once, like RPG Maker
	connect(&controller, &EventEditorController::applied, doc, [doc](const lcf::rpg::Event& applied) { doc->commitEvent(applied); });
	// the event edited shows the version read
	connect(doc, &MapDocument::reloaded, &controller, [&controller, doc, id] { controller.reloadEvent(doc->findEvent(id)); });
	EventWindow::exec(m_window, project, controller, {doc->hasMap() ? doc->map().chipset_id : 0, page, line});
	if (controller.result() == EventEditorController::Ok)
		doc->commitEvent(controller.resultEvent());
}

void QmlShell::placeTeleport(int map, const QPoint& cell)
{
	MapDocument* doc = m_project->document(map);
	if (!doc || !core().project())
		return;
	// RPG Maker's Select Location starts on this map; RPG Maker 2003 also
	// asks the facing after the teleport (0 keeps it)
	const bool rpg2k3 = core().project()->projectData().database().system.ldb_id == 2003;
	Pickers::LocationRequest request;
	request.current = {doc->id(), cell.x(), cell.y(), 0};
	request.facing = rpg2k3;
	const auto picked = Pickers::pickLocation(m_window.data(), request);
	// the chipset of this map again, after the map shown in the picker
	core().LoadChipset(doc->map().chipset_id);
	if (!picked)
		return;
	doc->placeTeleport(cell.x(), cell.y(), picked->map, picked->x, picked->y,
		rpg2k3 ? std::optional<int>(picked->direction) : std::nullopt);
}

void QmlShell::onCurrentMapChanged()
{
	emit currentEditorChanged();
	const int id = m_tabs->currentMap();
	// the tree shows the map of the tab, Map Properties edits it
	if (id > 0 && m_shell->selectedMap() != id && treeType(m_shell->selectedMap()) != lcf::rpg::TreeMap::MapType_area)
		m_shell->setSelectedMap(id);
	followCurrentMap();
	updateZoomActions();
	m_shell->updateActions();
}

void QmlShell::followCurrentMap()
{
	MapDocument* doc = m_tabs->currentDocument();
	m_events.clear();
	if (!doc || !doc->hasMap()) {
		core().setCurrentMapEvents(nullptr);
		return;
	}
	core().LoadChipset(doc->map().chipset_id);
	// rebuilt each time: the events move in memory when one is added or removed
	for (const auto& event : doc->map().events)
		m_events.insert(event.ID, const_cast<lcf::rpg::Event*>(&event));
	core().setCurrentMapEvents(&m_events);
}

void QmlShell::updateZoomActions()
{
	// the ratio the map shown is at, rounded as its view rounds it
	double shown = 0;
	if (MapEditor* editor = currentEditor()) {
		for (const QString& action : m_actions->groupActions(ActionCatalog::Group::Zoom)) {
			const double ratio = ShellController::zoomRatio(action);
			if (qFuzzyCompare(editor->scale(), editor->snapScale(ratio)))
				shown = ratio;
		}
	}
	m_shell->setZoomRatio(shown);
}

void QmlShell::showStatusMessage(const QString& text, int timeout)
{
	setStatusMessage(text, timeout);
}

void QmlShell::setStatusMessage(const QString& text, int timeout)
{
	m_statusTimer.stop();
	if (m_statusMessage != text) {
		m_statusMessage = text;
		emit statusMessageChanged();
	}
	if (!text.isEmpty() && timeout > 0)
		m_statusTimer.start(timeout);
}

/*
 * The actions
 */

void QmlShell::projectSettings()
{
	auto project = core().project();
	if (!project)
		return;
	const QDir folder = project->projectDir();
	ProjectSettings settings(*project, true);
	if (QmlHost::exec(QStringLiteral("ProjectSettingsWindow"), m_window,
			{{QStringLiteral("settings"), QVariant::fromValue(&settings)}}) != QmlHost::Accepted)
		return;
	const QString folder_name = settings.folderName().trimmed();
	// a new folder or code page: the project is closed (asking to save),
	// changed, and opened again (its texts read in the new code page)
	const bool rename = folder_name != folder.dirName();
	const bool reload = rename || settings.encodingModified();
	const QString old_path = folder.absolutePath();
	if (reload && !m_shell->closeProject())
		return;
	const QString error = settings.write(*project);
	if (!error.isEmpty())
		DialogService::instance().warning(windowText(QT_TRANSLATE_NOOP("MainWindow", "Project Settings")), error);
	QString path = old_path;
	if (rename) {
		QDir parent(old_path);
		parent.cdUp();
		if (parent.rename(folder.dirName(), folder_name))
			path = parent.filePath(folder_name);
		else
			DialogService::instance().warning(windowText(QT_TRANSLATE_NOOP("MainWindow", "Project Settings")), windowText(QT_TRANSLATE_NOOP("MainWindow", "The folder could not be renamed.")));
	}
	project.reset();
	if (reload)
		LoadProject(path);
	else
		// the game title
		emit m_shell->windowTitleChanged();
	if (settings.convertRequested())
		m_shell->convertProject();
}

void QmlShell::exportGame()
{
	if (!core().project())
		return;
	// the export reads the files: unsaved maps would not be in it
	if (!m_project->saveModifiedMaps(windowText(QT_TRANSLATE_NOOP("MainWindow", "Export Game")), windowText(QT_TRANSLATE_NOOP("MainWindow", "Some maps have unsaved changes.\nSave them before the export?"))))
		return;
	ExportController controller;
	controller.setProject(*core().project(), core().rtpPath(QString()));
	QmlHost::exec(QStringLiteral("ExportWindow"), m_window, {{QStringLiteral("controller"), QVariant::fromValue(&controller)}});
}

void QmlShell::openDatabase()
{
	DatabaseWindows::open(*this);
}

void QmlShell::openSearch()
{
	if (m_search)
		m_search->show();
}

void QmlShell::testPlayOptions()
{
	if (!core().project())
		return;
	TestPlayOptions options(core().project()->projectData());
	QmlHost::exec(QStringLiteral("RunGameWindow"), m_window, {{QStringLiteral("options"), QVariant::fromValue(&options)}});
}

bool QmlShell::quit()
{
	if (!m_project->saveAll())
		return false;
	m_closing = true;
	qApp->quit();
	return true;
}

void QmlShell::restartForUpdate()
{
	// a quit the user cancels keeps the update for later (Restart to Update)
	EditorUpdater& updater = EditorUpdater::instance();
	updater.setRelaunchOnQuit(true);
	if (!quit())
		updater.setRelaunchOnQuit(false);
}

void QmlShell::updateUpdateAction()
{
	const EditorUpdater& updater = EditorUpdater::instance();
	const QString name = QStringLiteral("actionCheckUpdates");
	m_actions->setText(name, updater.actionText());
	m_actions->setEnabled(name, updater.actionEnabled());
}

/*
 * The workspace
 */

void QmlShell::onProjectOpened()
{
	// the search reads the data of one project
	delete m_search.data();
	m_search = new SearchWindowHost(core().project()->projectData(), this, this);
	m_search->enableCache(EditorSettings::instance().searchCache());
	restoreWorkspace();
}

void QmlShell::onProjectClosing()
{
	saveWorkspace();
	// its files are the project's
	ResourceWindows::closeJukebox();
	delete m_search.data();
	m_pendingViews.clear();
}

void QmlShell::restoreWorkspace()
{
	const Workspace workspace = m_workspaces.load(core().project()->projectDir().absolutePath());
	core().restoreEditingState(workspace.editing);
	const lcf::rpg::TreeMap& maps = core().project()->treeMap();
	if (workspace.maps) {
		QList<int> ids;
		for (const Workspace::MapView& map : *workspace.maps) {
			ids.append(map.id);
			m_pendingViews.insert(map.id, map);
		}
		m_tabs->restoreMaps(ids);
		m_pendingViews.clear();
		m_tabs->setCurrentMap(workspace.activeMap);
	} else if (maps.active_node > 0 && m_project->treeModel()->type(maps.active_node) == lcf::rpg::TreeMap::MapType_map) {
		// never saved: the map tree's active map, kept
		m_shell->setSelectedMap(maps.active_node);
		m_shell->openNode(maps.active_node, true);
	}
	if (workspace.selectedMap >= 0 && m_project->treeModel()->contains(workspace.selectedMap))
		m_shell->setSelectedMap(workspace.selectedMap);
	QVariantMap view{{QStringLiteral("treeScroll"), workspace.treeScroll},
		{QStringLiteral("paletteX"), workspace.paletteScroll.x()}, {QStringLiteral("paletteY"), workspace.paletteScroll.y()},
		{QStringLiteral("selected"), m_shell->selectedMap()}};
	if (workspace.expanded)
		view.insert(QStringLiteral("expanded"), QVariant::fromValue(*workspace.expanded));
	if (m_window)
		emit viewRestoreRequested(view);
	else
		m_pendingView = view;
	if (m_window && m_window->isVisible())
		restoreDockLayout(workspace.dockLayout);
	m_shell->updateActions();
}

void QmlShell::saveWorkspace()
{
	if (qApp->property("local_state_resetting").toBool() || !m_window)
		return;
	m_workspaces.setDialogGeometry(window_state_name, m_window->geometry());
	const QByteArray docks = dockLayout();
	m_workspaces.setWindowDockLayout(docks);
	if (!core().project()) {
		m_workspaces.sync();
		return;
	}
	const QString folder = core().project()->projectDir().absolutePath();
	// the layout of older versions' main window stays as it was saved
	Workspace workspace = m_workspaces.load(folder);
	QList<Workspace::MapView> maps;
	for (int id : m_tabs->mapIds()) {
		Workspace::MapView map;
		map.id = id;
		if (MapEditor* editor = m_editors.value(id)) {
			map.zoom = editor->scale() > 0 ? editor->scale() : default_map_scale;
			map.cell = editor->selectedCell();
			map.scroll = editor->scrollPosition() * map.zoom;
		}
		maps.append(map);
	}
	workspace.maps = maps;
	const QVariantMap view = viewState();
	if (view.contains(QStringLiteral("expanded")))
		workspace.expanded = view.value(QStringLiteral("expanded")).value<QList<int>>();
	workspace.activeMap = m_tabs->currentMap() > 0 ? m_tabs->currentMap() : -1;
	workspace.selectedMap = m_shell->selectedMap();
	workspace.treeScroll = view.value(QStringLiteral("treeScroll")).toInt();
	workspace.paletteScroll = QPoint(view.value(QStringLiteral("paletteX")).toInt(), view.value(QStringLiteral("paletteY")).toInt());
	workspace.editing = core().editingState();
	workspace.dockLayout = docks;
	m_workspaces.save(folder, workspace);
	m_workspaces.sync();
}

void QmlShell::resetLocalState()
{
	m_pendingViews.clear();
	if (m_window) {
		m_window->showNormal();
		m_window->resize(1024, 640);
		if (const QScreen* screen = m_window->screen())
			m_window->setPosition(screen->availableGeometry().center() - QPoint(m_window->width() / 2, m_window->height() / 2));
	}
	restoreDockLayout(m_defaultLayout);
	m_actions->setChecked(QStringLiteral("actionPaletteToggle"), true);
	m_actions->setChecked(QStringLiteral("actionMapTreeToggle"), true);
	emit dockRequested(QStringLiteral("dockPalette"), true);
	emit dockRequested(QStringLiteral("dockMapTree"), true);
	if (core().project()) {
		// unsaved maps stay; closing clean tabs only changes the view
		for (int id : m_tabs->mapIds()) {
			MapDocument* doc = m_project->document(id);
			if (doc && !doc->isModified())
				m_tabs->closeMap(id);
		}
		for (MapEditor* editor : std::as_const(m_editors)) {
			editor->setScale(default_map_scale);
			editor->setScrollPosition(QPoint());
		}
		const int active = core().project()->treeMap().active_node;
		if (active > 0 && treeType(active) == lcf::rpg::TreeMap::MapType_map)
			m_shell->openNode(active, true);
	}
	core().restoreEditingState({});
	emit viewResetRequested();
	m_shell->updateActions();
}

bool QmlShell::syncExpandedFromView()
{
	const QVariantMap view = viewState();
	if (!view.contains(QStringLiteral("expanded")))
		return false;
	const QList<int> expanded = view.value(QStringLiteral("expanded")).value<QList<int>>();
	bool changed = false;
	for (int id : m_project->treeModel()->ids())
		changed = m_project->treeModel()->setExpanded(id, expanded.contains(id)) || changed;
	return changed;
}

QVariantMap QmlShell::viewState() const
{
	QVariant state;
	if (m_window && m_window->metaObject()->indexOfMethod("viewState()") >= 0)
		QMetaObject::invokeMethod(m_window, "viewState", Q_RETURN_ARG(QVariant, state));
	QVariantMap view = state.toMap();
	// the ids, as numbers
	if (view.contains(QStringLiteral("expanded"))) {
		QList<int> ids;
		for (const QVariant& id : view.value(QStringLiteral("expanded")).toList())
			ids.append(id.toInt());
		view.insert(QStringLiteral("expanded"), QVariant::fromValue(ids));
	}
	return view;
}

QByteArray QmlShell::dockLayout() const
{
	if (!m_window)
		return {};
	KDDockWidgets::LayoutSaver saver;
	return saver.serializeLayout();
}

void QmlShell::restoreDockLayout(const QByteArray& layout)
{
	if (!m_window || layout.isEmpty())
		return;
	// the window keeps its place and size: the docks take their share of it
	KDDockWidgets::LayoutSaver saver(KDDockWidgets::RestoreOption_RelativeToMainWindow);
	saver.restoreLayout(layout);
	// a dock hidden is unchecked in the View menu
	QVariant open;
	for (const auto& [dock, action] : {std::pair{"dockPalette", "actionPaletteToggle"}, std::pair{"dockMapTree", "actionMapTreeToggle"}}) {
		if (QMetaObject::invokeMethod(m_window, "isDockOpen", Q_RETURN_ARG(QVariant, open), Q_ARG(QVariant, QString::fromLatin1(dock))))
			m_actions->setChecked(QString::fromLatin1(action), open.toBool());
	}
}
