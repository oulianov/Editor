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

#include "map_scene.h"
#include <QAction>
#include <QKeyEvent>
#include <QDialogButtonBox>
#include <QGraphicsBlurEffect>
#include <QGraphicsOpacityEffect>
#include <QGraphicsSceneWheelEvent>
#include <QMessageBox>
#include <QPainter>
#include <QScrollBar>
#include <QStatusBar>
#include <lcf/rpg/event.h>
#include <cmath>
#include <iomanip>
#include <sstream>
#include "core.h"
#include "common/dbstring.h"
#include "ui/event/event_dialog.h"
#include "ui/other/run_game_dialog.h"
#include "ui/maptree/map_properties_dialog.h"
#include "ui/maptree/map_location_dialog.h"
#include "ui/main_window.h"
#include "undo_draw.h"
#include "model/map_tools.h"
#include "undo_event.h"
#include "map_cursors.h"
#include "ui/common/theme_icon.h"
#include "ui/common/stale_file.h"
#include "model/data_merge.h"
#include "common/logging.h"
#include <lcf/lmu/reader.h>
#include <lcf/lmt/reader.h>

MapScene::MapScene(ProjectData& project, int id, QGraphicsView *view, QObject *parent) :
	QGraphicsScene(parent), m_project(project)
{
	m_init = false;
	m_view = view;
	m_view->setMouseTracking(true);
	// for scripts (focus mapView): the view of the tab shown is found first
	m_view->setObjectName(QStringLiteral("mapView"));
	m_view->installEventFilter(this);
	// the mouse leaving the map hides the frame of the tool
	m_view->viewport()->installEventFilter(this);
	m_undoStack = new QUndoStack(this);
	m_selectionTile = new QGraphicsRectItem(QRectF(QRect(0,32,32,32)));
	m_selecting = false;
	const auto& treeMap = project.treeMap();
	for (unsigned int i = 1; i < treeMap.maps.size(); i++)
		if (treeMap.maps[i].ID == id)
		{
			n_mapInfo = treeMap.maps[i];
			break;
		}

	// The shortcuts are shown in the menus (as in RPG Maker) and handled by
	// keyPressEvent: as widget shortcuts they only work while the menu is open.
	// The names are for scripts (action actionEventCopy...)
	auto addAction = [this](QMenu* menu, const QString& name, const QString& icon, const QString& text,
			void (MapScene::*slot)(), const QKeySequence& key = QKeySequence()) {
		auto* action = new QAction(icon.isEmpty() ? QIcon() : themeIcon(m_view, icon), text, this);
		action->setObjectName(name);
		action->setShortcut(key);
		action->setShortcutContext(Qt::WidgetShortcut);
		action->setShortcutVisibleInContextMenu(true);
		connect(action, &QAction::triggered, this, slot);
		menu->addAction(action);
		return action;
	};
	m_eventMenu = new QMenu(m_view);
	m_actionNew = addAction(m_eventMenu, "actionEventNew", QStringLiteral("new"), QString(),
		&MapScene::on_actionNewEvent, Qt::Key_Return);
	m_actionEdit = addAction(m_eventMenu, "actionEventEdit", QStringLiteral("properties"), QString(),
		&MapScene::on_actionEditEvent, Qt::Key_Return);
	m_eventMenu->addSeparator();
	m_actionCut = addAction(m_eventMenu, "actionEventCut", QStringLiteral("edit-cut"), QString(),
		&MapScene::on_actionCutEvent, QKeySequence::Cut);
	m_actionCopy = addAction(m_eventMenu, "actionEventCopy", QStringLiteral("edit-copy"), QString(),
		&MapScene::on_actionCopyEvent, QKeySequence::Copy);
	m_actionPaste = addAction(m_eventMenu, "actionEventPaste", QStringLiteral("edit-paste"), QString(),
		&MapScene::on_actionPasteEvent, QKeySequence::Paste);
	m_actionDelete = addAction(m_eventMenu, "actionEventDelete", QStringLiteral("edit-delete"), QString(),
		&MapScene::on_actionDeleteEvent, Qt::Key_Delete);
	m_eventMenu->addSeparator();
	m_actionTeleport = addAction(m_eventMenu, "actionPlaceTeleport", QStringLiteral("place-teleport"),
		QString(), &MapScene::on_actionPlaceTeleport);
	m_eventMenu->addSeparator();
	addAction(m_eventMenu, "actionPlacePartyStart", QStringLiteral("place-start"),
		QString(), &MapScene::on_actionSetStartPosition);
	m_vehicleMenu = m_eventMenu->addMenu(themeIcon(m_view, QStringLiteral("place-vehicle")), QString());
	const QStringList vehicle_actions = {"actionPlaceBoatStart", "actionPlaceShipStart", "actionPlaceAirshipStart"};
	for (int i = 0; i < vehicle_actions.size(); i++) {
		addAction(m_vehicleMenu, vehicle_actions[i], QString(), QString(),
			&MapScene::on_actionSetVehiclePosition)->setData(i);
	}
	m_eventMenu->addSeparator();
	addAction(m_eventMenu, "actionRunHere", QStringLiteral("play"),
		QString(), &MapScene::on_actionRunHere);

	// RPG Maker's menu of the selection tool
	m_areaMenu = new QMenu(m_view);
	m_actionCutArea = addAction(m_areaMenu, "actionAreaCut", QStringLiteral("edit-cut"), QString(),
		&MapScene::on_actionCutArea, QKeySequence::Cut);
	m_actionCopyArea = addAction(m_areaMenu, "actionAreaCopy", QStringLiteral("edit-copy"), QString(),
		&MapScene::on_actionCopyArea, QKeySequence::Copy);
	m_actionPasteArea = addAction(m_areaMenu, "actionAreaPaste", QStringLiteral("edit-paste"), QString(),
		&MapScene::on_actionPasteArea, QKeySequence::Paste);
	m_actionClearArea = addAction(m_areaMenu, "actionAreaClear", QStringLiteral("edit-delete"), QString(),
		&MapScene::on_actionClearArea, Qt::Key_Delete);
	addAction(m_areaMenu, "actionAreaSelectAll", QStringLiteral("select-all"), QString(),
		&MapScene::on_actionSelectAll, QKeySequence::SelectAll);
	// the texts, again when the language changes
	retranslate();

	m_lowerpix = new QGraphicsPixmapItem();
	m_upperpix = new QGraphicsPixmapItem();
	addItem(m_lowerpix);
	addItem(m_upperpix);

	// The selection of the selection tool: white under black dashes moving
	// with a timer (RPG Maker's marching ants)
	QPen white_pen(Qt::white, 1);
	white_pen.setCosmetic(true);
	QPen ants_pen(Qt::black, 1, Qt::DashLine);
	ants_pen.setCosmetic(true);
	m_selAreaItem = new QGraphicsRectItem();
	m_selAreaItem->setPen(white_pen);
	m_selAreaItem->setZValue(3);
	m_selAreaItem->setVisible(false);
	m_selAntsItem = new QGraphicsRectItem(m_selAreaItem);
	m_selAntsItem->setPen(ants_pen);
	addItem(m_selAreaItem);
	m_antsTimer.setInterval(150);
	connect(&m_antsTimer, &QTimer::timeout, this, [this]() {
		m_antsOffset = (m_antsOffset + 1) % 8;
		QPen pen = m_selAntsItem->pen();
		pen.setDashOffset(m_antsOffset);
		m_selAntsItem->setPen(pen);
	});
	// RPG Maker frames the cells the tool draws on, under the mouse
	QPen brush_outer(Qt::black, 3);
	brush_outer.setCosmetic(true);
	QPen brush_inner(Qt::white, 1);
	brush_inner.setCosmetic(true);
	m_brushItem = new QGraphicsRectItem();
	m_brushItem->setPen(brush_outer);
	m_brushItem->setZValue(3);
	m_brushItem->setVisible(false);
	m_brushInner = new QGraphicsRectItem(m_brushItem);
	m_brushInner->setPen(brush_inner);
	addItem(m_brushItem);
	// the boundaries of an area of this map
	QPen area_pen(Qt::magenta, 2, Qt::DashLine);
	area_pen.setCosmetic(true);
	m_areaItem = new QGraphicsRectItem();
	m_areaItem->setPen(area_pen);
	m_areaItem->setZValue(4);
	m_areaItem->setVisible(false);
	addItem(m_areaItem);
	m_floatItem = new QGraphicsPixmapItem();
	m_floatItem->setZValue(2);
	m_floatItem->setVisible(false);
	addItem(m_floatItem);

	Load();
	QPen selPen(Qt::yellow);
	selPen.setWidth(2);
	m_selectionTile->setPen(selPen);
	m_selectionTile->setZValue(3);
	m_selectionTile->setVisible(false);
	this->addItem(m_selectionTile);
	m_drawing = false;
	m_cancelled = false;
	m_selecting = false;
	QGraphicsBlurEffect * effect = new QGraphicsBlurEffect(this);
	effect->setBlurRadius(2.0);
	effect->setBlurHints(QGraphicsBlurEffect::PerformanceHint);
	m_lowerpix->setGraphicsEffect(effect);
	m_upperpix->setGraphicsEffect(new QGraphicsOpacityEffect(this));
	onLayerChanged();
	onToolChanged();
}

MapScene::~MapScene()
{
	// Core points to the event list of the map shown
	if (core().currentMapEvents() == &m_events)
		core().setCurrentMapEvents(nullptr);
	delete m_lowerpix;
	delete m_upperpix;
	delete m_lines;
	delete m_selectionTile;
	delete m_undoStack;
}

void MapScene::Init()
{
	connect(m_view->verticalScrollBar(),
			SIGNAL(actionTriggered(int)),
			this,
			SLOT(on_user_interaction()));
	connect(m_view->horizontalScrollBar(),
			SIGNAL(actionTriggered(int)),
			this,
			SLOT(on_user_interaction()));
	connect(m_view->verticalScrollBar(),
			SIGNAL(valueChanged(int)),
			this,
			SLOT(redrawMap()));
	connect(m_view->horizontalScrollBar(),
			SIGNAL(valueChanged(int)),
			this,
			SLOT(redrawMap()));
	connect(m_view->verticalScrollBar(),
			SIGNAL(rangeChanged(int,int)),
			this,
			SLOT(redrawMap()));
	connect(m_view->horizontalScrollBar(),
			SIGNAL(rangeChanged(int,int)),
			this,
			SLOT(redrawMap()));
	connect(m_view->verticalScrollBar(),
			SIGNAL(valueChanged(int)),
			this,
			SLOT(on_view_V_Scroll()));
	connect(m_view->horizontalScrollBar(),
			SIGNAL(valueChanged(int)),
			this,
			SLOT(on_view_H_Scroll()));
	connect(&core(),
			SIGNAL(toolChanged()),
			this,
			SLOT(onToolChanged()));
	connect(&core(),
			SIGNAL(layerChanged()),
			this,
			SLOT(onLayerChanged()));
	connect(&core(), &Core::animationFrameChanged, this, &MapScene::animateTiles);
	// not cast to int: 0 at the scales below 1
	m_view->verticalScrollBar()->setValue(n_mapInfo.scrollbar_y * static_cast<double>(m_scale));
	m_view->horizontalScrollBar()->setValue(n_mapInfo.scrollbar_x * static_cast<double>(m_scale));
	m_init = true;
	redrawMap();
}

float MapScene::scale() const
{
	return m_scale;
}

QString MapScene::mapName() const
{
	return ToQString(n_mapInfo.name);
}

bool MapScene::isModified() const
{
	// undoing back to the saved map makes it unmodified again
	return !m_undoStack->isClean();
}

bool MapScene::canUndo() const
{
	return m_undoStack->canUndo();
}

bool MapScene::canRedo() const
{
	return m_undoStack->canRedo();
}

int MapScene::id() const
{
	return n_mapInfo.ID;
}

int MapScene::chipsetId() const
{
	return m_map->chipset_id;
}

const std::vector<short>& MapScene::layerData(Core::Layer layer) const
{
	return layer == Core::LOWER ? m_map->lower_layer : m_map->upper_layer;
}

void MapScene::setLayerData(Core::Layer layer, std::vector<short> data)
{
	if (layer == Core::LOWER)
	{
		m_lower = data;
		m_map->lower_layer = data;
	}
	else
	{
		m_upper = data;
		m_map->upper_layer = data;
	}
	redrawLayer(layer);
}

std::optional<lcf::rpg::Event> MapScene::eventData(int id) const
{
	for (const auto& ev : m_map->events)
		if (ev.ID == id)
			return ev;
	return std::nullopt;
}

void MapScene::setEventData(int id, const std::optional<lcf::rpg::Event> &data)
{
	auto& events = m_map->events;
	auto ev = std::find_if(events.begin(), events.end(), [id](const auto& e) { return e.ID == id; });
	if (ev != events.end() && data)
		*ev = *data;
	else if (ev != events.end())
		events.erase(ev);
	else if (data)
		events.push_back(*data);
	redrawMap();
	// the events moved in memory
	emit mapReloaded();
}

const lcf::rpg::Map& MapScene::mapData() const
{
	return *m_map;
}

const lcf::rpg::MapInfo& MapScene::mapInfo() const
{
	return n_mapInfo;
}

void MapScene::setMapData(const lcf::rpg::Map &map, const lcf::rpg::MapInfo &info)
{
	clearSelectionArea();
	const bool resized = map.width != m_map->width || map.height != m_map->height;
	*m_map = map;
	n_mapInfo = info;
	m_lower = m_map->lower_layer;
	m_upper = m_map->upper_layer;
	if (resized) {
		redrawGrid();
		setScale(m_scale);
	}
	redrawPanorama();
	redrawMap();
	// the events are new objects; the name can have changed
	emit mapReloaded();
}

const lcf::rpg::Start& MapScene::startData() const
{
	return m_project.treeMap().start;
}

void MapScene::setStartData(const lcf::rpg::Start &start)
{
	m_project.treeMap().start = start;
	redrawMap();
}

QMap<int, lcf::rpg::Event*> *MapScene::mapEvents()
{
	// rebuilt each time: the events move in memory when one is added or removed
	m_events.clear();
	for (auto& ev : m_map->events)
		m_events.insert(ev.ID, &ev);
	return &m_events;
}

int MapScene::tileAt(Core::Layer layer, int x, int y) const
{
	if (x < 0 || y < 0 || x >= m_map->width || y >= m_map->height || layer == Core::EVENT)
		return -1;
	const size_t i = static_cast<size_t>(x + y * m_map->width);
	return layer == Core::LOWER ? m_map->lower_layer[i] : m_map->upper_layer[i];
}

void MapScene::showArea(const QRect &cells)
{
	const int ts = core().tileSize();
	m_areaItem->setRect(QRectF(cells.x() * ts, cells.y() * ts, cells.width() * ts, cells.height() * ts));
	m_areaItem->setVisible(!cells.isEmpty());
	if (!cells.isEmpty())
		m_view->centerOn(m_areaItem->mapToScene(m_areaItem->rect().center()));
}

int MapScene::eventIdAt(int x, int y) const
{
	for (const auto& ev : m_map->events)
		if (ev.x == x && ev.y == y)
			return ev.ID;
	return 0;
}

void MapScene::editMapProperties(QTreeWidgetItem *item)
{
	commitFloating();
	const lcf::rpg::Map old_map = *m_map;
	const lcf::rpg::MapInfo old_info = n_mapInfo;

	MapPropertiesDialog dlg(m_project, n_mapInfo, *m_map, m_view);
	if (dlg.exec() != QDialog::Accepted)
		return;
	if (*m_map == old_map && n_mapInfo == old_info)
		return;

	// Kept in memory with undo and saved with the map, like a drawing:
	// saving here would also save the drawing and forget its undo
	m_undoStack->push(new UndoMap(old_map, old_info, this));
	m_lower = m_map->lower_layer;
	m_upper = m_map->upper_layer;
	if (m_map->width != old_map.width || m_map->height != old_map.height) {
		redrawGrid();
	}
	redrawPanorama();
	setScale(m_scale);
	// a resize can have removed events
	emit mapReloaded();
	emit mapChanged();

	if (item && n_mapInfo.name != old_info.name) {
		item->setData(0, Qt::DisplayRole, ToQString(n_mapInfo.name));
	}
}

void MapScene::redrawMap()
{
	if (!m_init)
		return;
	s_tileSize = core().tileSize() * static_cast<double>(m_scale);
	// A hidden tab would load its chipset and events over those of the tab
	// shown: it is redrawn when shown
	if (!m_view->isVisible()) {
		m_needsRedraw = true;
		return;
	}
	m_needsRedraw = false;
	core().LoadChipset(m_map->chipset_id);
	core().setCurrentMapEvents(mapEvents());
	redrawLayer(Core::LOWER);
	redrawLayer(Core::UPPER);
}

float MapScene::snapScale(float scale) const
{
	// Pixel art stays sharp only when each of its pixels covers a power of
	// two of screen pixels (1/4, 1/2, 1, 2, 4...): the scale is rounded to
	// one, in device pixels (Retina, Windows scaling...)
	const double dpr = m_view ? m_view->devicePixelRatioF() : 1.0;
	const double device = std::clamp(static_cast<double>(scale) * dpr, 1.0 / 8, 16.0);
	return static_cast<float>(std::exp2(std::round(std::log2(device))) / dpr);
}

QVariantMap MapScene::viewState() const
{
	QVariantMap scroll = m_view->property("workspace_scroll").toMap();
	if (scroll.isEmpty()) scroll = {{"x", m_view->horizontalScrollBar()->value()}, {"y", m_view->verticalScrollBar()->value()}};
	return {{"id", id()}, {"zoom", m_scale}, {"cell", m_selecting ? QPoint(lst_x, lst_y) : QPoint(-1, -1)}, {"scroll", scroll}};
}

void MapScene::restoreViewState(const QVariantMap& state)
{
	setScale(state.value("zoom", 2.0).toFloat());
	const QPoint cell = state.value("cell", QPoint(-1, -1)).toPoint();
	if (cell.x() >= 0 && cell.y() >= 0)
		selectTile(std::clamp(cell.x(), 0, m_map->width - 1), std::clamp(cell.y(), 0, m_map->height - 1));
	m_view->setProperty("workspace_scroll", state.value("scroll"));
}

void MapScene::setScale(float scale)
{
	scale = snapScale(scale);
	// the first call has no previous scale
	const float old_scale = m_scale > 0 ? m_scale : scale;
	int map_x = m_view->horizontalScrollBar()->value();
	int map_y = m_view->verticalScrollBar()->value();

	m_scale = scale;
	m_lines->setScale(static_cast<double>(m_scale));
	m_selectionTile->setScale(static_cast<double>(m_scale));
	m_selAreaItem->setScale(static_cast<double>(m_scale));
	m_floatItem->setScale(static_cast<double>(m_scale));
	m_areaItem->setScale(static_cast<double>(m_scale));
	m_brushItem->setScale(static_cast<double>(m_scale));
	this->setSceneRect(0,
		0,
		m_map->width * core().tileSize() * static_cast<double>(m_scale),
		m_map->height * core().tileSize() * static_cast<double>(m_scale));

	if (m_view->horizontalScrollBar()->isVisible()) {
		m_view->horizontalScrollBar()->setValue((map_x + m_view->horizontalScrollBar()->pageStep() / 2.0) * m_scale / old_scale - m_view->horizontalScrollBar()->pageStep() / 2.0);
	} else {
		m_view->horizontalScrollBar()->setValue(m_view->horizontalScrollBar()->maximum() / 2.0);
	}
	if (m_view->verticalScrollBar()->isVisible()) {
		m_view->verticalScrollBar()->setValue((map_y + m_view->verticalScrollBar()->pageStep() / 2.0) * m_scale / old_scale - m_view->verticalScrollBar()->pageStep() / 2.0);
	} else {
		m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->maximum() / 2.0);
	}
	redrawMap();
	emit scaleChanged();
}

void MapScene::onLayerChanged()
{
	if (m_drawing)
		stopDrawing();
	if (m_selecting)
		stopSelecting();
	m_dragEventId = 0;
	commitFloating();
	switch (core().layer())
	{
	case Core::LOWER:
		m_lowerpix->graphicsEffect()->setEnabled(false);
		m_upperpix->graphicsEffect()->setEnabled(true);
		m_lines->setVisible(false);
		break;
	case Core::UPPER:
		m_lowerpix->graphicsEffect()->setEnabled(true);
		m_upperpix->graphicsEffect()->setEnabled(false);
		m_lines->setVisible(false);
		break;
	case Core::EVENT:
		m_lowerpix->graphicsEffect()->setEnabled(false);
		m_upperpix->graphicsEffect()->setEnabled(false);
		m_lines->setVisible(true);
		// the selection tool works on the tile layers only
		clearSelectionArea();
		break;
//	  default:
//		  Q_ASSERT(false);
	}
	applyCursor();
	updateBrush();
}

void MapScene::onToolChanged()
{
	if (m_drawing)
		stopDrawing();
	if (m_selecting)
		stopSelecting();
	// RPG Maker: another tool ends the selection
	commitFloating();
	clearSelectionArea();
	applyCursor();
	updateBrush();
}

void MapScene::applyCursor()
{
	// the event layer has the arrow, and the move cursor while an event is dragged
	if (core().layer() != Core::EVENT && core().tool() == Core::SELECT && m_selArea.contains(cur_x, cur_y))
		m_lowerpix->setCursor(MapCursors::move());
	else if (core().layer() != Core::EVENT)
		m_lowerpix->setCursor(MapCursors::forTool(core().tool()));
	else if (m_dragEventId)
		m_lowerpix->setCursor(MapCursors::move());
	else
		m_lowerpix->unsetCursor();
}

void MapScene::updateBrush()
{
	const Core::Tool tool = core().tool();
	const bool shown = cur_x >= 0 && cur_y >= 0 && core().layer() != Core::EVENT
		&& tool != Core::ZOOM && tool != Core::SELECT;
	m_brushItem->setVisible(shown);
	if (!shown)
		return;
	// the pen draws the whole selection of the palette, cut at the edges of the map
	const int w = tool == Core::PENCIL ? std::min(core().selWidth(), m_map->width - cur_x) : 1;
	const int h = tool == Core::PENCIL ? std::min(core().selHeight(), m_map->height - cur_y) : 1;
	const int ts = core().tileSize();
	const QRectF rect(cur_x * ts, cur_y * ts, w * ts, h * ts);
	m_brushItem->setRect(rect);
	m_brushInner->setRect(rect);
}

void MapScene::Save(bool properties_changed)
{
	commitFloating();
	if (!isModified() && !properties_changed)
		return;

	// Another program (rpgsync...) changed the map since it was read:
	// never replace its changes without asking
	auto& project = *core().project();
	auto& watcher = core().watcher();
	QStringList conflicts;
	if (watcher.isStale(project.mapFile(n_mapInfo.ID)) && !mergeFromDisk(&conflicts)) {
		switch (askStale(m_view, tr("The map \u201C%1\u201D").arg(mapName()), true, conflicts)) {
		case StaleChoice::Cancel:
			return;
		case StaleChoice::Reload:
			reloadFromDisk(false);
			return;
		case StaleChoice::Overwrite:
			break;
		}
	}

	auto& treeMap = m_project.treeMap();
	for (unsigned int i = 1; i < treeMap.maps.size(); i++)
		if (treeMap.maps[i].ID == n_mapInfo.ID)
		{
			// the tree owns the hierarchy (it can have changed by drag and drop)
			n_mapInfo.parent_map = treeMap.maps[i].parent_map;
			n_mapInfo.indentation = treeMap.maps[i].indentation;
			n_mapInfo.expanded_node = treeMap.maps[i].expanded_node;
			treeMap.maps[i] = n_mapInfo; //Apply info changes
			break;
		}
	// Remember last active map
	treeMap.active_node = n_mapInfo.ID;
	// The tree is written only when it changed (map properties, start
	// positions): another program can have changed it meanwhile
	if (project.treeMapModified()) {
		StaleChoice choice = StaleChoice::Overwrite;
		bool merged = false;
		if (watcher.isStale(project.treeMapFile())) {
			QStringList tree_conflicts;
			merged = project.mergeTreeMap(&tree_conflicts);
			if (!merged)
				choice = askStale(m_view, tr("The map tree"), true, tree_conflicts);
		}
		if (choice == StaleChoice::Cancel)
			return;
		if (choice == StaleChoice::Reload) {
			watcher.requestReload(ProjectWatcher::TreeMap);
		} else {
			project.saveTreeMap();
			// the tree shown takes what the other program changed in it
			if (merged)
				watcher.requestReload(ProjectWatcher::TreeMap);
		}
	}
	// Tiles the Player cannot draw (the old palette eraser wrote 0x7FFD)
	// become the empty tiles: water below, the transparent tile above
	auto repair = [](std::vector<short>& lower, std::vector<short>& upper) {
		for (auto& tile : lower) {
			const int terrain = core().translate(tile);
			if (terrain < 0 || core().isFblock(terrain))
				tile = 0;
		}
		for (auto& tile : upper) {
			if (!core().isFblock(core().translate(tile)))
				tile = 10000;
		}
	};
	repair(m_map->lower_layer, m_map->upper_layer);
	repair(m_lower, m_upper);
	// FIXME: ProjectData.Project is Const
	// saveMap increases the save count
	if (core().project()->saveMap(*m_map, n_mapInfo.ID))
		m_savedMap = *m_map;
	m_undoStack->clear();
	emit mapSaved();
}

void MapScene::Load(bool revert)
{
	if (revert) {
		// The start positions are in the map tree, not in the map file
		cancelFloating();
		while (m_undoStack->canUndo())
			m_undoStack->undo();
	}
	auto map = m_project.project().loadMap(n_mapInfo.ID, true);
	if (!map) {
		if (m_map)
			return; // the map shown stays
		// unreadable when opened: an empty map, whose layers can be drawn
		map = std::make_unique<lcf::rpg::Map>();
		map->lower_layer.assign(static_cast<size_t>(map->width * map->height), 0);
		map->upper_layer.assign(static_cast<size_t>(map->width * map->height), 10000);
	}
	setMap(std::move(map), revert);
}

void MapScene::setMap(std::unique_ptr<lcf::rpg::Map> map, bool keep_view)
{
	// FIXME: Many calls to core()
	const auto& treeMap = m_project.treeMap();
	for (unsigned int i = 1; i < treeMap.maps.size(); i++)
		if (treeMap.maps[i].ID == n_mapInfo.ID)
		{
			n_mapInfo = treeMap.maps[i]; //Revert info changes
			break;
		}

	const QSize old_size = m_map ? QSize(m_map->width, m_map->height) : QSize();
	m_savedMap = *map;
	m_map = std::move(map);
	m_lower =  m_map->lower_layer;
	m_upper =  m_map->upper_layer;
	clearSelectionArea();

	redrawPanorama();

	if (!keep_view || old_size != QSize(m_map->width, m_map->height)) {
		redrawGrid();
		if (m_scale > 0)
			setScale(m_scale);
	}

	redrawMap();
	m_undoStack->clear();
	emit mapReverted();
}

void MapScene::undo()
{
	clearSelectionArea();
	m_undoStack->undo();
	emit mapChanged();
}

void MapScene::redo()
{
	clearSelectionArea();
	m_undoStack->redo();
	emit mapChanged();
}

void MapScene::on_actionNewEvent()
{
	if (getEventAt(lst_x, lst_y))
		return;
	// Find first free id
	int id = getFirstFreeId();

	lcf::rpg::Event event;
	event.ID = id;
	event.name = ToDBString(QString("EV%1").arg(QString::number(id), 4, QLatin1Char('0')));
	event.x = lst_x;
	event.y = lst_y;
	event.pages.push_back(lcf::rpg::EventPage());
	editEvent(event);
}

void MapScene::on_actionEditEvent() {
	if (auto* ev = getEventAt(lst_x, lst_y)) {
		editEvent(*ev);
	}
}

void MapScene::editEvent(const lcf::rpg::Event& event) {
	const int id = event.ID;
	EventDialog dlg(m_project, m_view, n_mapInfo.ID);
	dlg.setEvent(event);
	connect(&dlg, &EventDialog::applied, this, &MapScene::commitEvent);
	m_eventDialog = &dlg;
	m_eventDialogId = id;
	dlg.exec();
	m_eventDialog = nullptr;

	if (dlg.editResult() == QDialogButtonBox::Ok) {
		commitEvent(dlg.getEvent());
	}
	redrawMap();
}

void MapScene::commitEvent(const lcf::rpg::Event& event) {
	// The map can have been reloaded while the dialog was open: look the event up again
	if (auto* ev = findEvent(event.ID)) {
		if (*ev == event) {
			return;
		}
		lcf::rpg::Event backup = *ev;
		*ev = event;
		m_undoStack->push(new UndoEvent(event.ID, backup, this));
	} else {
		m_map->events.push_back(event);
		m_undoStack->push(new UndoEvent(event.ID, std::nullopt, this));
		// the events moved in memory
		emit mapReloaded();
	}
	redrawMap();
	emit mapChanged();
}

void MapScene::refreshMapInfo() {
	if (isModified()) {
		return;
	}
	for (const auto& info : m_project.treeMap().maps) {
		if (info.ID == n_mapInfo.ID) {
			n_mapInfo = info;
			break;
		}
	}
}

bool MapScene::mergeFromDisk(QStringList* conflicts) {
	auto theirs = m_project.project().loadMap(n_mapInfo.ID);
	if (!theirs || !m_savedMap) {
		return false;
	}
	lcf::rpg::Map merged;
	const auto result = DataMerge::map(*m_savedMap, *m_map, *theirs, merged);
	if (!result.ok()) {
		*conflicts = result.conflicts;
		return false;
	}
	qCInfo(lcReload) << "map" << n_mapInfo.ID << "merged with the changes on disk";
	// the map shows both sides: what the other program changed is drawn
	const QSize old_size(m_map->width, m_map->height);
	*m_map = std::move(merged);
	m_savedMap = std::move(*theirs);
	m_lower = m_map->lower_layer;
	m_upper = m_map->upper_layer;
	if (old_size != QSize(m_map->width, m_map->height)) {
		redrawGrid();
		setScale(m_scale);
	}
	redrawMap();
	// the events moved in memory
	emit mapReloaded();
	return true;
}

void MapScene::reloadFromDisk(bool ask) {
	if (ask && isModified()) {
		auto answer = QMessageBox::question(m_view,
			tr("Map changed on disk"),
			tr("%1 was changed by another program.\n"
			   "Reload it and discard your unsaved changes?").arg(mapName()),
			QMessageBox::Yes | QMessageBox::No);
		if (answer != QMessageBox::Yes) {
			return;
		}
	}

	// read once: a file caught while written stays as shown (the next
	// change reloads it)
	auto map = m_project.project().loadMap(n_mapInfo.ID, true);
	if (!map) {
		return;
	}
	// The undo steps belong to the version replaced: undoing them would
	// write their old values into the map tree just read
	cancelFloating();
	m_undoStack->clear();
	setMap(std::move(map), true);
	emit mapReloaded();

	if (m_eventDialog) {
		m_eventDialog->reloadEvent(findEvent(m_eventDialogId));
	}
}

void MapScene::on_actionCopyEvent() {
	if (auto* ev = getEventAt(lst_x, lst_y)) {
		// in Core: an event can be pasted on another map
		core().setEventClipboard(*ev);
	}
}

void MapScene::on_actionCutEvent() {
	on_actionCopyEvent();
	on_actionDeleteEvent();
}

void MapScene::on_actionPasteEvent() {
	const lcf::rpg::Event* clipboard = core().eventClipboard();
	if (!clipboard || getEventAt(lst_x, lst_y))
		return;

	// RPG Maker keeps the name, the event gets the first free id
	lcf::rpg::Event event = *clipboard;
	event.ID = getFirstFreeId();
	event.x = lst_x;
	event.y = lst_y;

	m_map->events.push_back(event);
	m_undoStack->push(new UndoEvent(event.ID, std::nullopt, this));
	redrawMap();
	emit mapChanged();
}

void MapScene::on_actionDeleteEvent()
{
	std::vector<lcf::rpg::Event>::iterator ev;
	for (ev = m_map->events.begin(); ev != m_map->events.end(); ++ev)
		if (_index(lst_x,lst_y) == _index(ev->x,ev->y))
			break;

	if (ev != m_map->events.end())
	{
		lcf::rpg::Event backup = *ev;
		m_map->events.erase(ev);
		m_undoStack->push(new UndoEvent(backup.ID, backup, this));
		redrawMap();
		emit mapChanged();
	}
}

void MapScene::moveEvent(int id, int x, int y)
{
	auto* ev = findEvent(id);
	if (!ev || getEventAt(x, y))
		return;
	lcf::rpg::Event moved = *ev;
	moved.x = x;
	moved.y = y;
	// pushes the undo of the old position
	commitEvent(moved);
}

void MapScene::on_actionRunHere()
{
	core().runGameHere(id(), lst_x, lst_y);
}

void MapScene::shiftMap(int dx, int dy)
{
	// tiles, events and the start positions on this map: one undo step
	commitFloating();
	const lcf::rpg::Map old_map = *m_map;
	const lcf::rpg::Start old_start = m_project.treeMap().start;
	lcf::rpg::Map shifted = *m_map;
	MapTools::shiftMap(shifted, dx, dy);
	auto& start = m_project.treeMap().start;
	auto shiftStart = [&](int map_id, int32_t& x, int32_t& y) {
		if (map_id == id()) {
			x = MapTools::wrap(x + dx, m_map->width);
			y = MapTools::wrap(y + dy, m_map->height);
		}
	};
	shiftStart(start.party_map_id, start.party_x, start.party_y);
	shiftStart(start.boat_map_id, start.boat_x, start.boat_y);
	shiftStart(start.ship_map_id, start.ship_x, start.ship_y);
	shiftStart(start.airship_map_id, start.airship_x, start.airship_y);
	setMapData(shifted, n_mapInfo);
	m_undoStack->beginMacro(tr("Shift Map"));
	m_undoStack->push(new UndoMap(old_map, n_mapInfo, this));
	if (!(old_start == start))
		m_undoStack->push(new UndoStart(old_start, this));
	m_undoStack->endMacro();
	emit mapChanged();
}

void MapScene::pushStart(const lcf::rpg::Start &old_start)
{
	if (old_start == m_project.treeMap().start)
		return;
	// The start is in the map tree, saved with this map
	m_undoStack->push(new UndoStart(old_start, this));
	redrawMap();
	emit mapChanged();
}

void MapScene::on_actionSetStartPosition()
{
	auto& start = m_project.treeMap().start;
	const lcf::rpg::Start old_start = start;
	start.party_map_id = this->id();
	start.party_x = lst_x;
	start.party_y = lst_y;
	pushStart(old_start);
}

void MapScene::on_actionSetVehiclePosition()
{
	auto* action = qobject_cast<QAction*>(sender());
	if (!action)
		return;
	auto& start = m_project.treeMap().start;
	const lcf::rpg::Start old_start = start;
	switch (action->data().toInt()) {
	case 0:
		start.boat_map_id = this->id();
		start.boat_x = lst_x;
		start.boat_y = lst_y;
		break;
	case 1:
		start.ship_map_id = this->id();
		start.ship_x = lst_x;
		start.ship_y = lst_y;
		break;
	default:
		start.airship_map_id = this->id();
		start.airship_x = lst_x;
		start.airship_y = lst_y;
		break;
	}
	pushStart(old_start);
}

void MapScene::retranslate()
{
	static const std::pair<const char*, const char*> texts[] = {
		{"actionEventNew", QT_TR_NOOP("New Event")},
		{"actionEventEdit", QT_TR_NOOP("Edit Event")},
		{"actionEventCut", QT_TR_NOOP("Cut Event")},
		{"actionEventCopy", QT_TR_NOOP("Copy Event")},
		{"actionEventPaste", QT_TR_NOOP("Paste Event")},
		{"actionEventDelete", QT_TR_NOOP("Delete Event")},
		{"actionPlaceTeleport", QT_TR_NOOP("Place Teleport Event")},
		{"actionPlacePartyStart", QT_TR_NOOP("Place Party Starting Position")},
		{"actionRunHere", QT_TR_NOOP("Start Game Here")},
		{"actionAreaCut", QT_TR_NOOP("Cut")},
		{"actionAreaCopy", QT_TR_NOOP("Copy")},
		{"actionAreaPaste", QT_TR_NOOP("Paste")},
		{"actionAreaClear", QT_TR_NOOP("Clear")},
		{"actionAreaSelectAll", QT_TR_NOOP("Select All")},
		{"actionPlaceBoatStart", QT_TR_NOOP("Skiff")},
		{"actionPlaceShipStart", QT_TR_NOOP("Ship")},
		{"actionPlaceAirshipStart", QT_TR_NOOP("Airship")},
	};
	for (const auto& [name, text] : texts) {
		if (auto* action = findChild<QAction*>(QLatin1String(name)))
			action->setText(tr(text));
	}
	m_vehicleMenu->setTitle(tr("Place Vehicle Starting Position"));
}

void MapScene::on_actionPlaceTeleport()
{
	if (getEventAt(lst_x, lst_y))
		return;
	// RPG Maker's Select Location starts on this map
	int map_id = id();
	int x = lst_x;
	int y = lst_y;
	// RPG Maker 2003 also asks the facing after the teleport (0 keeps it)
	const bool rpg2k3 = m_project.database().system.ldb_id == 2003;
	int direction = 0;
	const bool picked = rpg2k3 ? MapLocationDialog::pick(m_project, map_id, x, y, direction, m_view)
		: MapLocationDialog::pick(m_project, map_id, x, y, m_view);
	// the chipset of this map again, after the map shown in the dialog
	redrawMap();
	if (!picked)
		return;

	// As RPG Maker: a normal event touched by the hero, with Hide Screen,
	// Teleport and Show Screen (the default transitions)
	using Code = lcf::rpg::EventCommand::Code;
	auto command = [](Code code, const std::vector<int32_t>& parameters) {
		lcf::rpg::EventCommand cmd;
		cmd.code = static_cast<int32_t>(code);
		cmd.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
		return cmd;
	};
	std::vector<int32_t> teleport = {map_id, x, y};
	if (rpg2k3)
		teleport.push_back(direction);
	lcf::rpg::EventPage page;
	page.ID = 1;
	page.trigger = lcf::rpg::EventPage::Trigger_touched;
	page.event_commands = {command(Code::EraseScreen, {-1}), command(Code::Teleport, teleport),
		command(Code::ShowScreen, {-1})};

	lcf::rpg::Event event;
	event.ID = getFirstFreeId();
	event.name = ToDBString(QString("EV%1").arg(QString::number(event.ID), 4, QLatin1Char('0')));
	event.x = lst_x;
	event.y = lst_y;
	event.pages.push_back(page);
	// added with undo, as a new event
	commitEvent(event);
}

void MapScene::on_actionCopyArea()
{
	if (m_floating) {
		core().setTileClipboard(m_float);
		return;
	}
	if (m_selArea.isEmpty())
		return;
	Core::TileClipboard clipboard;
	clipboard.width = m_selArea.width();
	clipboard.height = m_selArea.height();
	// both layers, as RPG Maker; the drawing in progress is not taken
	for (int y = m_selArea.top(); y <= m_selArea.bottom(); y++)
		for (int x = m_selArea.left(); x <= m_selArea.right(); x++)
		{
			const size_t i = static_cast<size_t>(_index(x, y));
			clipboard.lower.push_back(m_map->lower_layer[i]);
			clipboard.upper.push_back(m_map->upper_layer[i]);
		}
	core().setTileClipboard(clipboard);
}

void MapScene::on_actionCutArea()
{
	if (m_floating) {
		core().setTileClipboard(m_float);
		cancelFloating();
		return;
	}
	on_actionCopyArea();
	fillArea(m_selArea);
}

void MapScene::on_actionPasteArea()
{
	commitFloating();
	clearSelectionArea();
	const Core::TileClipboard& clipboard = core().tileClipboard();
	if (clipboard.width <= 0 || clipboard.height <= 0)
		return;
	m_float = clipboard;
	// in the middle of the view, moved inside the map when it fits
	const QPointF center = m_view->mapToScene(m_view->viewport()->rect().center());
	int x = static_cast<int>(center.x() / s_tileSize) - m_float.width / 2;
	int y = static_cast<int>(center.y() / s_tileSize) - m_float.height / 2;
	x = std::max(0, std::min(x, m_map->width - m_float.width));
	y = std::max(0, std::min(y, m_map->height - m_float.height));
	m_floatPos = QPoint(x, y);
	m_floating = true;
	showFloating();
}

void MapScene::on_actionClearArea()
{
	if (m_floating) {
		cancelFloating();
		return;
	}
	fillArea(m_selArea);
}

void MapScene::on_actionSelectAll()
{
	commitFloating();
	clearSelectionArea();
	m_selArea = QRect(0, 0, m_map->width, m_map->height);
	updateSelectionArea();
}

void MapScene::fillArea(const QRect &area)
{
	if (area.isEmpty() || core().layer() == Core::EVENT)
		return;
	m_selContents = {};
	m_selBackground = {};
	// RPG Maker fills the area of the current layer with the current tiles
	for (int y = area.top(); y <= area.bottom(); y++)
		for (int x = area.left(); x <= area.right(); x++)
		{
			const size_t i = static_cast<size_t>(_index(x, y));
			const short tile = core().selection(x - area.left(), y - area.top());
			if (core().layer() == Core::LOWER)
				m_lower[i] = tile;
			else
				m_upper[i] = tile;
		}
	if (core().layer() == Core::LOWER)
		bindArea(area.left() - 1, area.top() - 1, area.right() + 1, area.bottom() + 1);
	pushLayers(tr("Clear"));
}

void MapScene::pushLayers(const QString &text)
{
	const bool lower = m_lower != m_map->lower_layer;
	const bool upper = m_upper != m_map->upper_layer;
	if (!lower && !upper) {
		return;
	}
	m_undoStack->beginMacro(text);
	if (lower) {
		std::vector<short> before = std::exchange(m_map->lower_layer, m_lower);
		m_undoStack->push(new UndoDraw(Core::LOWER, std::move(before), this));
	}
	if (upper) {
		std::vector<short> before = std::exchange(m_map->upper_layer, m_upper);
		m_undoStack->push(new UndoDraw(Core::UPPER, std::move(before), this));
	}
	m_undoStack->endMacro();
	redrawLayer(Core::LOWER);
	redrawLayer(Core::UPPER);
	emit mapChanged();
}

void MapScene::updateSelectionArea()
{
	const int ts = core().tileSize();
	const QRectF rect(m_selArea.x() * ts, m_selArea.y() * ts, m_selArea.width() * ts, m_selArea.height() * ts);
	m_selAreaItem->setRect(rect);
	m_selAntsItem->setRect(rect);
	m_selAreaItem->setVisible(!m_selArea.isEmpty());
	if (m_selArea.isEmpty())
		m_antsTimer.stop();
	else if (!m_antsTimer.isActive())
		m_antsTimer.start();
}

void MapScene::clearSelectionArea()
{
	if (m_selMoving) {
		m_lower = m_map->lower_layer;
		m_upper = m_map->upper_layer;
		redrawLayer(Core::LOWER);
		redrawLayer(Core::UPPER);
	}
	m_selMoving = false;
	m_selContents = {};
	m_selBackground = {};
	m_selDragging = false;
	m_selArea = QRect();
	updateSelectionArea();
}

void MapScene::showFloating()
{
	const int ts = core().tileSize();
	QPixmap pix(m_float.width * ts, m_float.height * ts);
	pix.fill(Qt::transparent);
	core().setBackground(m_panorama);
	core().beginPainting(pix);
	for (int y = 0; y < m_float.height; y++)
		for (int x = 0; x < m_float.width; x++)
		{
			const size_t i = static_cast<size_t>(x + y * m_float.width);
			const QRect rect(x * ts, y * ts, ts, ts);
			core().renderTile(m_float.lower[i], rect);
			core().renderTile(m_float.upper[i], rect);
		}
	core().endPainting();
	m_floatItem->setPixmap(pix);
	m_floatItem->setOffset(m_floatPos * ts);
	m_floatItem->setVisible(true);
	m_selArea = QRect(m_floatPos, QSize(m_float.width, m_float.height));
	updateSelectionArea();
}

void MapScene::commitFloating()
{
	if (!m_floating)
		return;
	m_floating = false;
	m_floatDragging = false;
	m_floatItem->setVisible(false);
	for (int y = 0; y < m_float.height; y++)
		for (int x = 0; x < m_float.width; x++)
		{
			const int map_x = m_floatPos.x() + x;
			const int map_y = m_floatPos.y() + y;
			if (map_x < 0 || map_y < 0 || map_x >= m_map->width || map_y >= m_map->height)
				continue;
			const size_t i = static_cast<size_t>(_index(map_x, map_y));
			const size_t j = static_cast<size_t>(x + y * m_float.width);
			m_lower[i] = m_float.lower[j];
			m_upper[i] = m_float.upper[j];
		}
	// RPG Maker transfers the stored tile variants unchanged, including
	// autotile edges. Pasting must not reshape them or their neighbours.
	// RPG Maker: the dotted line disappears
	clearSelectionArea();
	pushLayers(tr("Paste"));
}

void MapScene::cancelFloating()
{
	m_floating = false;
	m_floatDragging = false;
	m_floatItem->setVisible(false);
	clearSelectionArea();
}

void MapScene::pickTile()
{
	// RPG Maker's eyedropper: the tile under the cursor becomes the current tile
	const size_t i = static_cast<size_t>(_index(cur_x, cur_y));
	const short tile = core().layer() == Core::LOWER ? m_lower[i] : m_upper[i];
	const int terrain = core().translate(tile);
	if (terrain < 0 || core().isFblock(terrain) != (core().layer() == Core::UPPER))
		return;
	core().setSelection({core().translate(terrain, SAMPLE)}, 1, 1);
}

void MapScene::selectCell(int x, int y)
{
	lst_x = x;
	lst_y = y;
	m_selecting = true;
	m_selectionTile->setVisible(true);
	m_selectionTile->setRect(QRectF(QRect(x*core().tileSize(),y*core().tileSize(),
										  core().tileSize(),core().tileSize())));
}

bool MapScene::updateCursor(const QPointF &pos)
{
	if (!sceneRect().contains(pos) || s_tileSize <= 0) {
		cur_x = -1;
		cur_y = -1;
		return false;
	}
	cur_x = std::max(0, std::min(m_map->width - 1, static_cast<int>(pos.x() / s_tileSize)));
	cur_y = std::max(0, std::min(m_map->height - 1, static_cast<int>(pos.y() / s_tileSize)));
	return true;
}

void MapScene::on_user_interaction()
{
	m_userInteraction = true;
}

void MapScene::on_view_V_Scroll()
{
	if (!m_userInteraction || !m_init)
		return;
	if (m_view->verticalScrollBar()->isVisible())
	{
		n_mapInfo.scrollbar_y = m_view->verticalScrollBar()->value() / static_cast<double>(m_scale);
	}
	m_userInteraction = false;
}

void MapScene::on_view_H_Scroll()
{
	if (!m_userInteraction || !m_init)
		return;
	if (m_view->horizontalScrollBar()->isVisible())
	{
		n_mapInfo.scrollbar_x = m_view->horizontalScrollBar()->value() / static_cast<double>(m_scale);
	}
	m_userInteraction = false;
}

void MapScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	const bool inside = updateCursor(event->scenePos());
	if (event->button() == Qt::RightButton)
	{
		if (m_drawing)
		{
			stopDrawing();
			return;
		}
		if (core().layer() == Core::EVENT)
		{
			// the tools are not used on the event layer (RPG Maker)
			m_dragEventId = 0;
			applyCursor();
			if (!inside)
				return;
			selectCell(cur_x, cur_y);
			const bool has_event = getEventAt(lst_x, lst_y);
			m_actionNew->setVisible(!has_event);
			m_actionEdit->setVisible(has_event);
			m_actionCopy->setEnabled(has_event);
			m_actionCut->setEnabled(has_event);
			m_actionPaste->setEnabled(!has_event && core().eventClipboard());
			m_actionDelete->setEnabled(has_event);
			m_actionTeleport->setEnabled(!has_event);
			m_eventMenu->popup(event->screenPos());
			return;
		}
		if (core().tool() == Core::ZOOM)
		{
			if (static_cast<double>(m_scale) > 0.125)
				setScale(m_scale/2);
			return;
		}
		if (!inside || m_selDragging || m_selMoving || m_floatDragging)
			return;
		if (core().tool() == Core::SELECT)
		{
			const bool has_area = m_floating || !m_selArea.isEmpty();
			m_actionCutArea->setEnabled(has_area);
			m_actionCopyArea->setEnabled(has_area);
			m_actionClearArea->setEnabled(has_area);
			m_actionPasteArea->setEnabled(core().tileClipboard().width > 0);
			m_areaMenu->popup(event->screenPos());
			return;
		}
		pickTile();
		return;
	}
	if (event->button() != Qt::LeftButton)
		return;

	if (core().layer() == Core::EVENT) // Select tile, maybe to drag its event
	{
		// Do not allow selecting the (-1, -1) spot
		if (!inside)
			return;
		selectCell(cur_x, cur_y);
		m_dragEventId = eventIdAt(cur_x, cur_y);
		applyCursor();
		return;
	}
	if (core().tool() == Core::ZOOM)
	{
		if (static_cast<double>(m_scale) < 8.0)
			setScale(m_scale*2);
		return;
	}
	// Do not draw on invalid coordinates
	if (!inside)
		return;

	fst_x = cur_x;
	fst_y = cur_y;
	switch(core().tool())
	{
	case Core::PENCIL:
		m_drawing = true;
		drawPen();
		break;
	case Core::RECTANGLE:
		m_drawing = true;
		drawRect();
		break;
	case Core::CIRCLE:
		m_drawing = true;
		drawEllipse();
		break;
	case Core::FILL:
		m_drawing = true;
		drawFill();
		break;
	case Core::SELECT:
		if (m_floating)
		{
			// drag the pasted area, or put it on the map with a click outside
			if (QRect(m_floatPos, QSize(m_float.width, m_float.height)).contains(cur_x, cur_y))
			{
				m_floatDragging = true;
				m_floatGrab = QPoint(cur_x, cur_y) - m_floatPos;
			}
			else
				commitFloating();
			break;
		}
		if (m_selArea.contains(cur_x, cur_y)) {
			if (m_selBackground.lower.empty()) {
				m_selContents.width = m_selArea.width();
				m_selContents.height = m_selArea.height();
				m_selBackground.lower = m_map->lower_layer;
				m_selBackground.upper = m_map->upper_layer;
				// The original editor leaves the first lower palette tile
				// (without an autotile variant) and a transparent upper tile.
				short background = core().selection(0, 0, Core::LOWER);
				if (background < 5000)
					background -= background % 50;
				for (int y = m_selArea.top(); y <= m_selArea.bottom(); ++y)
					for (int x = m_selArea.left(); x <= m_selArea.right(); ++x) {
						const size_t i = static_cast<size_t>(_index(x, y));
						m_selContents.lower.push_back(m_map->lower_layer[i]);
						m_selContents.upper.push_back(m_map->upper_layer[i]);
						m_selBackground.lower[i] = background;
						m_selBackground.upper[i] = 10000;
					}
			}
			m_selMoving = true;
			m_selGrab = QPoint(cur_x, cur_y) - m_selArea.topLeft();
			applyCursor();
			break;
		}
		clearSelectionArea();
		m_selDragging = true;
		m_selArea = QRect(cur_x, cur_y, 1, 1);
		updateSelectionArea();
		break;
	default:
		break;
	}
}

void MapScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	const int old_x = cur_x;
	const int old_y = cur_y;
	if (!updateCursor(event->scenePos())) {
		m_brushItem->setVisible(false);
		return;
	}
	if (cur_x == old_x && cur_y == old_y)
		return;
	updateBrush();
	if (m_drawing)
	{
		switch (core().tool())
		{
		case (Core::PENCIL):
			drawPen();
			break;
		case (Core::RECTANGLE):
			drawRect();
			break;
		case (Core::CIRCLE):
			drawEllipse();
			break;
		default:
			break;
		}
	}
	if (m_selDragging)
	{
		m_selArea = QRect(QPoint(fst_x, fst_y), QPoint(cur_x, cur_y)).normalized();
		updateSelectionArea();
	}
	if (m_selMoving) {
		const QPoint pos(std::clamp(cur_x - m_selGrab.x(), 0, m_map->width - m_selContents.width),
			std::clamp(cur_y - m_selGrab.y(), 0, m_map->height - m_selContents.height));
		m_lower = m_selBackground.lower;
		m_upper = m_selBackground.upper;
		for (int y = 0; y < m_selContents.height; ++y)
			for (int x = 0; x < m_selContents.width; ++x) {
				const size_t i = static_cast<size_t>(_index(pos.x() + x, pos.y() + y));
				const size_t j = static_cast<size_t>(x + y * m_selContents.width);
				m_lower[i] = m_selContents.lower[j];
				m_upper[i] = m_selContents.upper[j];
			}
		m_selArea.moveTopLeft(pos);
		updateSelectionArea();
		redrawLayer(Core::LOWER);
		redrawLayer(Core::UPPER);
	}
	if (m_floatDragging)
	{
		m_floatPos = QPoint(cur_x, cur_y) - m_floatGrab;
		m_floatItem->setOffset(m_floatPos * core().tileSize());
		m_selArea = QRect(m_floatPos, QSize(m_float.width, m_float.height));
		updateSelectionArea();
	}
	if (core().tool() == Core::SELECT)
		applyCursor();
	if (m_dragEventId && (event->buttons() & Qt::LeftButton))
	{
		// the cell the event is dragged to
		m_selectionTile->setRect(QRectF(QRect(cur_x*core().tileSize(),cur_y*core().tileSize(),
											  core().tileSize(),core().tileSize())));
	}

	// Show coordinates of current tile
	QString status_msg = QString("(%0, %1)").arg(cur_x).arg(cur_y);

	if (core().layer() == Core::EVENT)
	{
		// Show events on current tile
		for (const lcf::rpg::Event& evt : m_map->events)
		{
			if (_index(cur_x,cur_y) == _index(evt.x,evt.y))
			{
				status_msg.append(QString(" - " + tr("Event") + " %0: %1").arg(evt.ID).arg(evt.name.c_str()));
			}
		}
	}

	emit cellHovered(status_msg);
}

void MapScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_cancelled && !event->buttons())
	{
		m_cancelled = false;
		return;
	}
	if (event->button() != Qt::LeftButton)
		return;
	if (m_drawing && !m_cancelled)
	{
		m_drawing = false;
		pushLayers(QString());
	}
	m_selDragging = false;
	if (m_selMoving) {
		m_selMoving = false;
		pushLayers(tr("Move"));
		// The next drag fills its vacated cells with the current lower tile.
		m_selContents = {};
		m_selBackground = {};
	}
	m_floatDragging = false;
	if (m_dragEventId)
	{
		const int id = m_dragEventId;
		m_dragEventId = 0;
		applyCursor();
		updateCursor(event->scenePos());
		if (cur_x >= 0 && (cur_x != lst_x || cur_y != lst_y) && !getEventAt(cur_x, cur_y))
		{
			moveEvent(id, cur_x, cur_y);
			selectCell(cur_x, cur_y);
		}
		else
			selectCell(lst_x, lst_y);
	}
}

void MapScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
	if (core().layer() != Core::EVENT)
		return;

	// Do not allow putting events on invalid coordinates
	if (!updateCursor(event->scenePos())) {
		return;
	}

	m_dragEventId = 0;
	applyCursor();
	selectCell(cur_x, cur_y);
	if (getEventAt(cur_x, cur_y)) {
		on_actionEditEvent();
	} else {
		on_actionNewEvent();
	}
}

void MapScene::wheelEvent(QGraphicsSceneWheelEvent *event)
{
	// Ctrl + wheel zooms, by the steps of the zoom tool
	if (!(event->modifiers() & Qt::ControlModifier) || event->orientation() != Qt::Vertical) {
		QGraphicsScene::wheelEvent(event);
		return;
	}
	if (event->delta() > 0 && static_cast<double>(m_scale) < 8.0)
		setScale(m_scale*2);
	else if (event->delta() < 0 && static_cast<double>(m_scale) > 0.125)
		setScale(m_scale/2);
	event->accept();
}

bool MapScene::handleKey(QKeyEvent *event, bool run)
{
	const bool no_modifier = (event->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;
	const bool enter = no_modifier && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter);
	// Backspace: the Delete key of Mac keyboards
	const bool del = no_modifier && (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace);
	void (MapScene::*slot)() = nullptr;

	if (core().layer() == Core::EVENT)
	{
		// the cell selected with the mouse
		if (!m_selectionTile->isVisible())
			return false;
		const bool has_event = getEventAt(lst_x, lst_y);
		if (enter)
			slot = has_event ? &MapScene::on_actionEditEvent : &MapScene::on_actionNewEvent;
		else if (event->matches(QKeySequence::Cut) && has_event)
			slot = &MapScene::on_actionCutEvent;
		else if (event->matches(QKeySequence::Copy) && has_event)
			slot = &MapScene::on_actionCopyEvent;
		else if (event->matches(QKeySequence::Paste) && !has_event && core().eventClipboard())
			slot = &MapScene::on_actionPasteEvent;
		else if (del && has_event)
			slot = &MapScene::on_actionDeleteEvent;
	}
	else if (core().tool() == Core::SELECT && !m_drawing)
	{
		const bool has_area = m_floating || !m_selArea.isEmpty();
		if (event->matches(QKeySequence::Cut) && has_area)
			slot = &MapScene::on_actionCutArea;
		else if (event->matches(QKeySequence::Copy) && has_area)
			slot = &MapScene::on_actionCopyArea;
		else if (event->matches(QKeySequence::Paste) && core().tileClipboard().width > 0)
			slot = &MapScene::on_actionPasteArea;
		else if (del && has_area)
			slot = &MapScene::on_actionClearArea;
		else if (event->matches(QKeySequence::SelectAll))
			slot = &MapScene::on_actionSelectAll;
		else if (no_modifier && event->key() == Qt::Key_Escape && has_area)
			slot = &MapScene::cancelFloating;
	}
	if (!slot)
		return false;
	if (run)
		(this->*slot)();
	return true;
}

void MapScene::keyPressEvent(QKeyEvent *event)
{
	if (handleKey(event, true))
		event->accept();
	else
		QGraphicsScene::keyPressEvent(event);
}

bool MapScene::event(QEvent *event)
{
	// Take the keys of the map before the shortcuts of the window
	// (Ctrl+X, Del...), as RPG Maker does in its map view
	if (event->type() == QEvent::ShortcutOverride && handleKey(static_cast<QKeyEvent*>(event), false))
	{
		event->accept();
		return true;
	}
	return QGraphicsScene::event(event);
}

bool MapScene::eventFilter(QObject *watched, QEvent *event)
{
	// a tab shown again: its chipset, events and start positions
	if (watched == m_view && event->type() == QEvent::Show)
		QTimer::singleShot(0, this, &MapScene::redrawMap);
	if (watched == m_view->viewport() && event->type() == QEvent::Leave)
		m_brushItem->setVisible(false);
	return QGraphicsScene::eventFilter(watched, event);
}

int MapScene::_x(int index)
{
	return (index%m_map->width);
}

int MapScene::_y(int index)
{
	return (index/m_map->width);
}

int MapScene::_index(int x, int y)
{
	return (m_map->width*y+x);
}

void MapScene::redrawTile(const Core::Layer &layer,
								   const int &x,
								   const int &y,
								   const QRect &dest_rec)
{
	switch (layer)
	{
	case (Core::LOWER):
		core().renderTile(m_lower[static_cast<size_t>(_index(x,y))],dest_rec);
		break;
	case (Core::UPPER):
		core().renderTile(m_upper[static_cast<size_t>(_index(x,y))],dest_rec);
		break;
	default:
		break;
	}
}

void MapScene::stopDrawing()
{
	m_cancelled = true;
	m_drawing = false;
	m_lower = m_map->lower_layer;
	m_upper = m_map->upper_layer;
	redrawLayer(Core::LOWER);
	redrawLayer(Core::UPPER);
}

void MapScene::stopSelecting()
{
	// no m_cancelled: it would drop the next stroke on another layer
	m_selecting = false;
	m_selectionTile->setVisible(false);
}

void MapScene::updateArea(int x1, int y1, int x2, int y2)
{
	if (core().layer() == Core::LOWER)
		bindArea(x1, y1, x2, y2);
	redrawLayer(core().layer());
}

void MapScene::bindArea(int x1, int y1, int x2, int y2)
{
	//Normalize
	if (x1 < 0)
		x1 = 0;
	if (y1 < 0)
		y1 = 0;
	if (x2 >= m_map->width)
		x2 = m_map->width - 1;
	if (y2 >= m_map->height)
		y2 = m_map->height - 1;

	for (int x = x1; x <= x2; x++)
		for (int y = y1; y <= y2; y++)
		{
			// E tiles have no borders; unknown ids are left as they are
			const int terrain = core().translate(m_lower[static_cast<size_t>(_index(x, y))]);
			if (terrain >= 0 && !core().isEblock(terrain))
				m_lower[static_cast<size_t>(_index(x,y))] = bind(x, y);
		}
}

void MapScene::redrawLayer(Core::Layer layer)
{
	QSize size = m_view->size();
	if (size.width() > m_map->width*s_tileSize)
		size.setWidth(m_map->width*s_tileSize);
	else
		size.setWidth(size.width()+s_tileSize);
	if (size.height() > m_map->height*s_tileSize)
		size.setHeight(m_map->height*s_tileSize);
	else
		size.setHeight(size.height()+s_tileSize);
	int start_x = m_view->horizontalScrollBar()->value()/s_tileSize;
	int start_y = m_view->verticalScrollBar()->value()/s_tileSize;
	int end_x = start_x+(size.width()-1)/s_tileSize;
	int end_y = start_y+(size.height()-1)/s_tileSize;
	QPixmap pix(size);
	pix.fill(QColor(0,0,0,0));
	if (layer == Core::LOWER)
	{
		m_waterCells.clear();
		m_animatedCells.clear();
		m_lowerOrigin = QPoint(start_x, start_y);
		m_drawnWaterFrame = core().waterFrame();
		m_drawnTileFrame = core().animatedTileFrame();
	}
	// the panorama of this map, not of the last map loaded
	core().setBackground(m_panorama);
	core().beginPainting(pix);
	for (int x = start_x; x <= end_x; x++)
		for (int y = start_y; y <= end_y; y++)
		{
			if (x >= m_map->width || y >= m_map->height)
				continue;
			QRect dest_rect((x-start_x)* s_tileSize,
					   (y-start_y)* s_tileSize,
					   s_tileSize,
					   s_tileSize);
			redrawTile(layer, x, y, dest_rect);
			if (layer != Core::LOWER)
				continue;
			const short tile = m_lower[static_cast<size_t>(_index(x, y))];
			if (Core::isAnimatedTile(tile))
				(tile < 3000 ? m_waterCells : m_animatedCells).push_back(QPoint(x, y));
		}
	if (layer == Core::UPPER)
	{
		for (unsigned int i = 0; i <  m_map->events.size(); i++)
		{
			QRect rect((m_map->events[i].x-start_x)* s_tileSize,
					   (m_map->events[i].y-start_y)* s_tileSize,
					   s_tileSize,
					   s_tileSize);
			core().renderEvent(m_map->events[i], rect);
		}

		// The start positions on this map, as RPG Maker shows them
		const auto& start = m_project.treeMap().start;
		const auto& database = m_project.database();
		auto marker = [&](int map_id, int x, int y, const lcf::DBString& charset, int index, const QString& label) {
			if (map_id != id())
				return;
			QRect rect((x-start_x)* s_tileSize, (y-start_y)* s_tileSize, s_tileSize, s_tileSize);
			core().renderMarker(ToQString(charset), index, label, rect);
		};
		// the party has the graphic of its first hero
		lcf::DBString hero_charset;
		int hero_index = 0;
		const auto& party = database.system.party;
		if (!party.empty() && party[0] >= 1 && party[0] <= static_cast<int>(database.actors.size())) {
			hero_charset = database.actors[static_cast<size_t>(party[0] - 1)].character_name;
			hero_index = database.actors[static_cast<size_t>(party[0] - 1)].character_index;
		}
		//: Marker of the party start position on the map (one letter)
		marker(start.party_map_id, start.party_x, start.party_y, hero_charset, hero_index, tr("P"));
		//: Marker of the skiff start position on the map (one letter)
		marker(start.boat_map_id, start.boat_x, start.boat_y, database.system.boat_name, database.system.boat_index, tr("B"));
		//: Marker of the ship start position on the map (one letter)
		marker(start.ship_map_id, start.ship_x, start.ship_y, database.system.ship_name, database.system.ship_index, tr("S"));
		//: Marker of the airship start position on the map (one letter)
		marker(start.airship_map_id, start.airship_x, start.airship_y, database.system.airship_name, database.system.airship_index, tr("A"));
	}
	core().endPainting();
	if (layer == Core::LOWER)
	{
		m_lowerpix->setPixmap(pix);
		m_lowerpix->setPos(start_x*s_tileSize,start_y*s_tileSize);
	}
	else
	{
		m_upperpix->setPixmap(pix);
		m_upperpix->setPos(start_x*s_tileSize,start_y*s_tileSize);
	}
}

void MapScene::animateTiles()
{
	// a hidden tab is drawn again when shown; a map view of a dialog can
	// have loaded another chipset
	if (!m_init || m_needsRedraw || !m_view->isVisible() || core().chipsetId() != m_map->chipset_id)
		return;
	// A cell takes about 130 ns to draw (measured): a 500x500 map seen whole
	// at 1/8 can have 250000 water cells, too small to see them move anyway
	const size_t max_cells = 40000;
	const bool water = core().waterFrame() != m_drawnWaterFrame
		&& !m_waterCells.empty() && m_waterCells.size() <= max_cells;
	const bool tiles = core().animatedTileFrame() != m_drawnTileFrame
		&& !m_animatedCells.empty() && m_animatedCells.size() <= max_cells;
	if (!water && !tiles)
		return;
	if (water)
		m_drawnWaterFrame = core().waterFrame();
	if (tiles)
		m_drawnTileFrame = core().animatedTileFrame();
	QPixmap pix = m_lowerpix->pixmap();
	core().setBackground(m_panorama);
	core().beginPainting(pix);
	auto draw = [this](const std::vector<QPoint>& cells) {
		for (const QPoint& cell : cells)
		{
			if (cell.x() >= m_map->width || cell.y() >= m_map->height)
				continue;
			const QPoint pos = (cell - m_lowerOrigin) * s_tileSize;
			redrawTile(Core::LOWER, cell.x(), cell.y(), QRect(pos, QSize(s_tileSize, s_tileSize)));
		}
	};
	if (water)
		draw(m_waterCells);
	if (tiles)
		draw(m_animatedCells);
	core().endPainting();
	m_lowerpix->setPixmap(pix);
}

void MapScene::drawPen()
{
	for (int x = cur_x; x < cur_x + core().selWidth(); x++)
		for (int y = cur_y; y < cur_y + core().selHeight(); y++)
		{
			// a brush of several tiles stops at the right and bottom edges
			if (x >= m_map->width || y >= m_map->height)
				continue;
			if (core().layer() == Core::LOWER)
				m_lower[static_cast<size_t>(_index(x,y))] = core().selection(x-fst_x,y-fst_y);
			else if (core().layer() == Core::UPPER)
				m_upper[static_cast<size_t>(_index(x,y))] = core().selection(x-fst_x,y-fst_y);
		}
	updateArea(cur_x-1,cur_y-1,cur_x+core().selWidth()+1,cur_y+core().selHeight()+1);
}

void MapScene::drawRect()
{
	switch (core().layer())
	{
	case (Core::LOWER):
		m_lower = m_map->lower_layer;
		break;
	case (Core::UPPER):
		m_upper = m_map->upper_layer;
		break;
	default:
		break;
	}

	int x1 = fst_x > cur_x ? cur_x : fst_x;
	int x2 = fst_x > cur_x ? fst_x : cur_x;
	int y1 = fst_y > cur_y ? cur_y : fst_y;
	int y2 = fst_y > cur_y ? fst_y : cur_y;
	for (int x = x1; x <= x2; x++)
		for (int y = y1; y <= y2; y++)
		{
			if (core().layer() == Core::LOWER)
				m_lower[static_cast<size_t>(_index(x,y))] = core().selection(x-fst_x,y-fst_y);
			else if (core().layer() == Core::UPPER)
				m_upper[static_cast<size_t>(_index(x,y))] = core().selection(x-fst_x,y-fst_y);
		}
	updateArea(x1-2, y1-2, x2+2, y2+2);
}

void MapScene::drawEllipse()
{
	switch (core().layer())
	{
	case (Core::LOWER):
		m_lower = m_map->lower_layer;
		break;
	case (Core::UPPER):
		m_upper = m_map->upper_layer;
		break;
	default:
		break;
	}

	const int x1 = std::min(fst_x, cur_x);
	const int x2 = std::max(fst_x, cur_x);
	const int y1 = std::min(fst_y, cur_y);
	const int y2 = std::max(fst_y, cur_y);
	// the filled ellipse inscribed in the rectangle dragged, cell by cell
	const double cx = (x1 + x2 + 1) / 2.0;
	const double cy = (y1 + y2 + 1) / 2.0;
	const double rx = (x2 - x1 + 1) / 2.0;
	const double ry = (y2 - y1 + 1) / 2.0;
	for (int x = x1; x <= x2; x++)
		for (int y = y1; y <= y2; y++)
		{
			const double dx = (x + 0.5 - cx) / rx;
			const double dy = (y + 0.5 - cy) / ry;
			if (dx * dx + dy * dy > 1.0)
				continue;
			if (core().layer() == Core::LOWER)
				m_lower[static_cast<size_t>(_index(x,y))] = core().selection(x-fst_x,y-fst_y);
			else if (core().layer() == Core::UPPER)
				m_upper[static_cast<size_t>(_index(x,y))] = core().selection(x-fst_x,y-fst_y);
		}
	updateArea(x1-2, y1-2, x2+2, y2+2);
}

void MapScene::drawFill()
{
	if (core().layer() == Core::EVENT)
		return;
	const bool lower = core().layer() == Core::LOWER;
	std::vector<short>& layer = lower ? m_lower : m_upper;
	const std::vector<short> orig = layer;
	// lower: the same terrain (autotiles differ by their borders); upper: the same tile
	auto key = [&](short tile) { return lower ? core().translate(tile) : static_cast<int>(tile); };
	const int target = key(orig[static_cast<size_t>(_index(fst_x, fst_y))]);
	// filling with the tile already there would change nothing
	if (core().selWidth() == 1 && core().selHeight() == 1 && key(core().selection(0, 0)) == target)
		return;

	// iterative: a recursion overflows the stack on large maps
	std::vector<bool> seen(orig.size(), false);
	std::vector<QPoint> todo = {QPoint(fst_x, fst_y)};
	seen[static_cast<size_t>(_index(fst_x, fst_y))] = true;
	const QPoint neighbours[] = {QPoint(0, -1), QPoint(-1, 0), QPoint(1, 0), QPoint(0, 1)};
	while (!todo.empty())
	{
		const QPoint p = todo.back();
		todo.pop_back();
		layer[static_cast<size_t>(_index(p.x(), p.y()))] = core().selection(p.x()-fst_x, p.y()-fst_y);
		for (const QPoint& d : neighbours)
		{
			const QPoint n = p + d;
			if (n.x() < 0 || n.y() < 0 || n.x() >= m_map->width || n.y() >= m_map->height)
				continue;
			const size_t i = static_cast<size_t>(_index(n.x(), n.y()));
			if (seen[i] || key(orig[i]) != target)
				continue;
			seen[i] = true;
			todo.push_back(n);
		}
	}
	updateArea(0, 0, m_map->width-1, m_map->height-1);
}

short MapScene::bind(int x, int y)
{
#define tile_u core().translate(m_lower[static_cast<size_t>(_index(x, y-1))])
#define tile_d core().translate(m_lower[static_cast<size_t>(_index(x, y+1))])
#define tile_l core().translate(m_lower[static_cast<size_t>(_index(x-1, y))])
#define tile_r core().translate(m_lower[static_cast<size_t>(_index(x+1, y))])
#define tile_ul core().translate(m_lower[static_cast<size_t>(_index(x-1, y-1))])
#define tile_ur core().translate(m_lower[static_cast<size_t>(_index(x+1, y-1))])
#define tile_dl core().translate(m_lower[static_cast<size_t>(_index(x-1, y+1))])
#define tile_dr core().translate(m_lower[static_cast<size_t>(_index(x+1, y+1))])
	int _code = 0, _scode = 0;
	int terrain_id = core().translate(m_lower[static_cast<size_t>(_index(x, y))]);
	int u=0,d=0,l=0,r=0,ul=0,ur=0,dl=0,dr=0,sul=0,sur=0,sdl=0,sdr=0;
	if (core().isDblock(terrain_id))
	{
		if (y > 0 && terrain_id != tile_u)
			u = UP;
		if (y < m_map->height-1 && terrain_id != tile_d)
			d = DOWN;
		if (x > 0 && terrain_id != tile_l)
			l = LEFT;
		if (x < m_map->width-1 && terrain_id != tile_r)
			r = RIGHT;
		if (u+l == 0 && x > 0 && y > 0 && terrain_id != tile_ul)
			ul = UPLEFT;
		if (u+r == 0 && x < m_map->width-1 && y > 0 && terrain_id != tile_ur)
			ur = UPRIGHT;
		if (d+l == 0 && x > 0 && y < m_map->height-1 && terrain_id != tile_dl)
			dl = DOWNLEFT;
		if (d+r == 0 && x < m_map->width-1 &&
				y < m_map->height-1 && terrain_id != tile_dr)
			dr = DOWNRIGHT;
		_code = u+d+l+r+ul+ur+dl+dr;
	}
	else if (core().isWater(terrain_id) || core().isAnimation(terrain_id))
	{
		if (y > 0 && (!core().isWater(tile_u) &&
					  !core().isAnimation(tile_u)))
			u = UP;
		if (y < m_map->height-1 && (!core().isWater(tile_d) &&
										  !core().isAnimation(tile_d)))
			d = DOWN;
		if (x > 0 && (!core().isWater(tile_l) &&
					  !core().isAnimation(tile_l)))
			l = LEFT;
		if (x < m_map->width-1 && (!core().isWater(tile_r) &&
										 !core().isAnimation(tile_r)))
			r = RIGHT;
		if ((u+l) == 0 && x > 0 && y > 0 && !core().isWater(tile_ul) && !core().isAnimation(tile_ul))
			ul = UPLEFT;
		if ((u+r) == 0 && x < m_map->width-1 && y > 0 && !core().isWater(tile_ur) && !core().isAnimation(tile_ur))
			ur = UPRIGHT;
		if ((d+l) == 0 && x > 0 && y < m_map->height-1 && !core().isWater(tile_dl) && !core().isAnimation(tile_dl))
			dl = DOWNLEFT;
		if ((d+r) == 0 && x < m_map->width-1 &&
				y < m_map->height-1 && !core().isWater(tile_dr) && !core().isAnimation(tile_dr))
			dr = DOWNRIGHT;
		_code = u+d+l+r+ul+ur+dl+dr;
		// DeepWater Special Corners
		if (core().isDWater(terrain_id))
		{
			if (x > 0 && y > 0 && core().isABWater(tile_u) && core().isABWater (tile_l) && core().isABWater (tile_ul))
				sul = UPLEFT;
			if (x < m_map->width-1 && y > 0 && core().isABWater(tile_u) && core().isABWater (tile_r) && core().isABWater (tile_ur))
				sur = UPRIGHT;
			if (x > 0 && y < m_map->height-1 && core().isABWater(tile_d) && core().isABWater (tile_l) && core().isABWater (tile_dl))
				sdl = DOWNRIGHT;
			if (x < m_map->width-1 && y < m_map->height-1 &&
					core().isABWater(tile_d) && core().isABWater (tile_r) && core().isABWater (tile_dr))
				sdr = DOWNLEFT;
		}
		else
		{
			if (x > 0 && y > 0 && core().isDWater (tile_u) && core().isDWater (tile_l) && core().isWater(tile_ul))
				sul = UPLEFT;
			if (x < m_map->width-1 && y > 0 && core().isDWater (tile_u) && core().isDWater (tile_r) && core().isWater(tile_ur))
				sur = UPRIGHT;
			if (x > 0 && y < m_map->height-1 && core().isDWater (tile_d) && core().isDWater (tile_l) && core().isWater(tile_dl))
				sdl = DOWNRIGHT;
			if (x < m_map->width-1 && y < m_map->height-1 &&
					core().isDWater (tile_d) && core().isDWater (tile_r) && core().isWater(tile_dr))
				sdr = DOWNLEFT;
		}
		_scode = sul+sur+sdl+sdr;
	}
	return core().translate(terrain_id, _code, _scode);
#undef tile_u
#undef tile_d
#undef tile_l
#undef tile_r
#undef tile_ul
#undef tile_ur
#undef tile_dl
#undef tile_dr
}

void MapScene::selectTile(int x, int y)
{
	cur_x = x;
	cur_y = y;
	if (core().layer() == Core::EVENT)
		selectCell(x, y);
}

void MapScene::centerOnTile(int x, int y)
{
	m_view->centerOn((x + 0.5) * core().tileSize() * m_scale, (y + 0.5) * core().tileSize() * m_scale);
}

lcf::rpg::Event* MapScene::findEvent(int id) {
	for (auto& ev : m_map->events) {
		if (ev.ID == id) {
			return &ev;
		}
	}
	return nullptr;
}

lcf::rpg::Event* MapScene::getEventAt(int x, int y) {
	std::vector<lcf::rpg::Event>::iterator ev;
	for (ev = m_map->events.begin(); ev != m_map->events.end(); ++ev) {
		if (_index(x, y) == _index(ev->x, ev->y)) {
			break;
		}
	}

	if (ev != m_map->events.end()) {
		return &(*ev);
	}

	return nullptr;
}

int MapScene::getFirstFreeId() {
	std::vector<lcf::rpg::Event>::iterator ev;
	int id = 1;
	for (;;++id) {
		bool valid = true;
		for (ev = m_map->events.begin(); ev != m_map->events.end(); ++ev) {
			if (ev->ID == id) {
				valid = false;
				break;
			}
		}
		if (valid) {
			break;
		}
	}

	return id;
}

void MapScene::redrawPanorama() {
	if (m_map->parallax_flag) {
		core().LoadBackground(m_map->parallax_name.c_str());
	} else {
		core().LoadBackground(QString());
	}
	// each map draws its own panorama (Core has one for the map drawn)
	m_panorama = core().background();
}

void MapScene::redrawGrid() {
	if (!grid_lines.empty()) {
		while (!grid_lines.empty()) {
			QGraphicsItem* line = grid_lines.takeLast();
			delete line;
		}
		destroyItemGroup(m_lines);
	}

	for (int x = 0; x <= m_map->width; x++) {
		grid_lines.append(new QGraphicsLineItem(x*core().tileSize(),
			0,
			x*core().tileSize(),
			m_map->height*core().tileSize()));
	}

	for (int y = 0; y <= m_map->height; y++) {
		grid_lines.append(new QGraphicsLineItem(0,
			y*core().tileSize(),
			m_map->width*core().tileSize(),
			y*core().tileSize()));
	}

	m_lines = createItemGroup(grid_lines);

	m_lines->setVisible(core().layer() == Core::EVENT);
}
