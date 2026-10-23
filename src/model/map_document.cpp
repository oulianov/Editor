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

// The editing of a map; reading and writing its file are in
// map_document_file.cpp, which the unit test does without

#include "map_document.h"
#include <QUndoStack>
#include <algorithm>
#include <lcf/rpg/eventcommand.h>
#include "autotile.h"
#include "event_page_defaults.h"
#include "map_tools.h"
#include "map_undo.h"
#include "project_data.h"
#include "common/dbstring.h"

namespace {
// the start positions, in the order of TilePainter::StartMarker
enum { Party, Boat, Ship, Airship };

// the map and the cell of a start position
struct StartFields {
	int32_t& map_id;
	int32_t& x;
	int32_t& y;
};

StartFields startFields(lcf::rpg::Start& start, int kind)
{
	switch (kind) {
	case Boat:
		return {start.boat_map_id, start.boat_x, start.boat_y};
	case Ship:
		return {start.ship_map_id, start.ship_x, start.ship_y};
	case Airship:
		return {start.airship_map_id, start.airship_x, start.airship_y};
	default:
		return {start.party_map_id, start.party_x, start.party_y};
	}
}

// Every document there is, whatever its project: a start position is in the
// map tree, and the map it leaves can be open in another document than the
// one that moves it
QList<MapDocument*>& documents()
{
	static QList<MapDocument*> list;
	return list;
}
}

short MapDocument::Brush::at(int off_x, int off_y) const
{
	// as EditorState::selection: negative offsets count from the end
	off_x %= width;
	off_y %= height;
	if (off_x < 0)
		off_x += width;
	if (off_y < 0)
		off_y += height;
	return tiles[static_cast<size_t>(off_x + off_y * width)];
}

MapDocument::MapDocument(ProjectData& project, int id, QObject* parent) :
	QObject(parent), m_project(project), m_undoStack(new QUndoStack(this))
{
	const auto& treeMap = project.treeMap();
	for (unsigned int i = 1; i < treeMap.maps.size(); i++)
		if (treeMap.maps[i].ID == id)
		{
			m_info = treeMap.maps[i];
			break;
		}
	m_savedInfo = m_info;
	connect(m_undoStack, &QUndoStack::cleanChanged, this, [this](bool clean) {
		emit modifiedChanged(!clean);
	});
	documents().append(this);
}

MapDocument::~MapDocument()
{
	documents().removeOne(this);
}

int MapDocument::id() const
{
	return m_info.ID;
}

bool MapDocument::hasMap() const
{
	return m_map != nullptr;
}

QString MapDocument::name() const
{
	return ToQString(m_info.name);
}

const lcf::rpg::Map& MapDocument::map() const
{
	return *m_map;
}

const lcf::rpg::MapInfo& MapDocument::mapInfo() const
{
	return m_info;
}

QRect MapDocument::bounds() const
{
	return QRect(0, 0, m_map->width, m_map->height);
}

QUndoStack* MapDocument::undoStack() const
{
	return m_undoStack;
}

bool MapDocument::isModified() const
{
	return !m_undoStack->isClean();
}

bool MapDocument::canUndo() const
{
	return m_undoStack->canUndo();
}

bool MapDocument::canRedo() const
{
	return m_undoStack->canRedo();
}

void MapDocument::undo()
{
	m_undoStack->undo();
	emit edited();
}

void MapDocument::redo()
{
	m_undoStack->redo();
	emit edited();
}

void MapDocument::setMap(std::unique_ptr<lcf::rpg::Map> map)
{
	// the info of the map tree: its unsaved changes go too
	const auto& treeMap = m_project.treeMap();
	for (unsigned int i = 1; i < treeMap.maps.size(); i++)
		if (treeMap.maps[i].ID == m_info.ID)
		{
			m_info = treeMap.maps[i];
			break;
		}
	m_savedInfo = m_info;
	// the first map has no size before it
	const bool size_changed = !m_map || map->width != m_map->width || map->height != m_map->height;
	m_savedMap = *map;
	m_map = std::move(map);
	m_lower = m_map->lower_layer;
	m_upper = m_map->upper_layer;
	m_moveContents = {};
	m_moveBackground = {};
	m_undoStack->clear();
	emit mapReplaced(size_changed);
	emit loaded();
}

void MapDocument::refreshMapInfo()
{
	// Map Properties' edit is kept, saving writes it; the tiles and the
	// events do not change the info: a map renamed by another program
	// (rpgsync) shows its new name and is saved with it
	const bool keep_edit = isModified() && infoEdited();
	for (const auto& info : m_project.treeMap().maps)
		if (info.ID == m_info.ID)
		{
			if (keep_edit)
			{
				// the name is not part of that edit: renaming writes it at once
				m_info.name = info.name;
				m_savedInfo.name = info.name;
				break;
			}
			m_info = info;
			m_savedInfo = info;
			break;
		}
}

void MapDocument::setSavedName(const QString& name)
{
	m_info.name = ToDBString(name);
	m_savedInfo.name = m_info.name;
	emit renamed();
}

bool MapDocument::hasTreeEntry() const
{
	const auto& maps = m_project.treeMap().maps;
	return std::any_of(maps.cbegin(), maps.cend(), [this](const lcf::rpg::MapInfo& info) {
		// an area given its id is not the map
		return info.ID == m_info.ID && info.type != lcf::rpg::TreeMap::MapType_area;
	});
}

bool MapDocument::infoEdited() const
{
	// where the view is scrolled to is not an edit
	lcf::rpg::MapInfo info = m_info;
	info.scrollbar_x = m_savedInfo.scrollbar_x;
	info.scrollbar_y = m_savedInfo.scrollbar_y;
	return !(info == m_savedInfo);
}

void MapDocument::setScrollPosition(int x, int y)
{
	m_info.scrollbar_x = x;
	m_info.scrollbar_y = y;
}

const std::vector<short>& MapDocument::layerData(Layer layer) const
{
	return layer == EditorState::LOWER ? m_map->lower_layer : m_map->upper_layer;
}

void MapDocument::setLayerData(Layer layer, std::vector<short> data)
{
	if (layer == EditorState::LOWER)
	{
		m_lower = data;
		m_map->lower_layer = std::move(data);
	}
	else
	{
		m_upper = data;
		m_map->upper_layer = std::move(data);
	}
	emit cellsChanged(bounds(), layer);
}

std::optional<lcf::rpg::Event> MapDocument::eventData(int id) const
{
	if (const auto* ev = findEvent(id))
		return *ev;
	return std::nullopt;
}

void MapDocument::setEventData(int id, const std::optional<lcf::rpg::Event>& data)
{
	auto& events = m_map->events;
	auto ev = std::find_if(events.begin(), events.end(), [id](const auto& e) { return e.ID == id; });
	if (ev != events.end() && data)
		*ev = *data;
	else if (ev != events.end())
		events.erase(ev);
	else if (data)
		events.push_back(*data);
	emit eventsChanged();
}

void MapDocument::setMapData(const lcf::rpg::Map& map, const lcf::rpg::MapInfo& info)
{
	const bool size_changed = map.width != m_map->width || map.height != m_map->height;
	*m_map = map;
	// the name is the map tree's, written there when it was renamed: undoing
	// Map Properties does not bring the old one back
	const auto name = m_info.name;
	m_info = info;
	m_info.name = name;
	m_lower = m_map->lower_layer;
	m_upper = m_map->upper_layer;
	m_moveContents = {};
	m_moveBackground = {};
	emit mapReplaced(size_changed);
}

const lcf::rpg::Start& MapDocument::startData() const
{
	return m_project.treeMap().start;
}

void MapDocument::setStartData(const lcf::rpg::Start& start)
{
	const lcf::rpg::Start before = startData();
	m_project.treeMap().start = start;
	notifyStartChanged(m_project, before);
}

void MapDocument::notifyStartChanged(const ProjectData& project, const lcf::rpg::Start& before)
{
	// the maps a start position left or came to
	lcf::rpg::Start old_start = before;
	lcf::rpg::Start new_start = project.treeMap().start;
	QList<int> maps;
	for (int kind = Party; kind <= Airship; kind++) {
		const StartFields was = startFields(old_start, kind);
		const StartFields is = startFields(new_start, kind);
		if (was.map_id != is.map_id || was.x != is.x || was.y != is.y)
			maps << was.map_id << is.map_id;
	}
	if (maps.isEmpty())
		return;
	// a copy: a view told can open or close a map (one closed is not told)
	const QList<MapDocument*> open = documents();
	for (MapDocument* doc : open)
		if (documents().contains(doc) && &doc->m_project == &project && maps.contains(doc->id()))
			emit doc->startChanged();
}

int MapDocument::index(int x, int y) const
{
	return m_map->width * y + x;
}

std::vector<short>& MapDocument::working(Layer layer)
{
	return layer == EditorState::LOWER ? m_lower : m_upper;
}

int MapDocument::tileAt(Layer layer, int x, int y) const
{
	if (x < 0 || y < 0 || x >= m_map->width || y >= m_map->height || layer == EditorState::EVENT)
		return -1;
	const size_t i = static_cast<size_t>(index(x, y));
	return layer == EditorState::LOWER ? m_map->lower_layer[i] : m_map->upper_layer[i];
}

const std::vector<short>& MapDocument::workingLayer(Layer layer) const
{
	return layer == EditorState::LOWER ? m_lower : m_upper;
}

short MapDocument::workingTile(Layer layer, int x, int y) const
{
	return workingLayer(layer)[static_cast<size_t>(index(x, y))];
}

void MapDocument::drawPen(Layer layer, const QPoint& cell, const QPoint& origin, const Brush& brush)
{
	if (layer == EditorState::EVENT)
		return;
	std::vector<short>& tiles = working(layer);
	for (int x = cell.x(); x < cell.x() + brush.width; x++)
		for (int y = cell.y(); y < cell.y() + brush.height; y++)
		{
			// a brush of several tiles stops at the right and bottom edges
			if (x >= m_map->width || y >= m_map->height)
				continue;
			tiles[static_cast<size_t>(index(x, y))] = brush.at(x - origin.x(), y - origin.y());
		}
	updateArea(layer, cell.x() - 1, cell.y() - 1, cell.x() + brush.width + 1, cell.y() + brush.height + 1, brush.bind);
}

void MapDocument::drawRect(Layer layer, const QPoint& origin, const QPoint& cell, const Brush& brush)
{
	if (layer == EditorState::EVENT)
		return;
	// in place of the rectangle drawn at the last cell
	std::vector<short>& tiles = working(layer);
	tiles = layerData(layer);

	const int x1 = std::min(origin.x(), cell.x());
	const int x2 = std::max(origin.x(), cell.x());
	const int y1 = std::min(origin.y(), cell.y());
	const int y2 = std::max(origin.y(), cell.y());
	for (int x = x1; x <= x2; x++)
		for (int y = y1; y <= y2; y++)
			tiles[static_cast<size_t>(index(x, y))] = brush.at(x - origin.x(), y - origin.y());
	updateArea(layer, x1 - 2, y1 - 2, x2 + 2, y2 + 2, brush.bind);
}

void MapDocument::drawEllipse(Layer layer, const QPoint& origin, const QPoint& cell, const Brush& brush)
{
	if (layer == EditorState::EVENT)
		return;
	std::vector<short>& tiles = working(layer);
	tiles = layerData(layer);

	const int x1 = std::min(origin.x(), cell.x());
	const int x2 = std::max(origin.x(), cell.x());
	const int y1 = std::min(origin.y(), cell.y());
	const int y2 = std::max(origin.y(), cell.y());
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
			tiles[static_cast<size_t>(index(x, y))] = brush.at(x - origin.x(), y - origin.y());
		}
	updateArea(layer, x1 - 2, y1 - 2, x2 + 2, y2 + 2, brush.bind);
}

void MapDocument::drawFill(Layer layer, const QPoint& origin, const Brush& brush)
{
	if (layer == EditorState::EVENT)
		return;
	const bool lower = layer == EditorState::LOWER;
	std::vector<short>& tiles = working(layer);
	const std::vector<short> orig = tiles;
	// lower: the same terrain (autotiles differ by their borders); upper: the same tile
	auto key = [&](short tile) { return lower ? Autotile::translate(tile) : static_cast<int>(tile); };
	const int target = key(orig[static_cast<size_t>(index(origin.x(), origin.y()))]);
	// filling with the tile already there would change nothing
	if (brush.width == 1 && brush.height == 1 && key(brush.at(0, 0)) == target)
		return;

	// iterative: a recursion overflows the stack on large maps
	std::vector<bool> seen(orig.size(), false);
	std::vector<QPoint> todo = {origin};
	seen[static_cast<size_t>(index(origin.x(), origin.y()))] = true;
	const QPoint neighbours[] = {QPoint(0, -1), QPoint(-1, 0), QPoint(1, 0), QPoint(0, 1)};
	while (!todo.empty())
	{
		const QPoint p = todo.back();
		todo.pop_back();
		tiles[static_cast<size_t>(index(p.x(), p.y()))] = brush.at(p.x() - origin.x(), p.y() - origin.y());
		for (const QPoint& d : neighbours)
		{
			const QPoint n = p + d;
			if (n.x() < 0 || n.y() < 0 || n.x() >= m_map->width || n.y() >= m_map->height)
				continue;
			const size_t i = static_cast<size_t>(index(n.x(), n.y()));
			if (seen[i] || key(orig[i]) != target)
				continue;
			seen[i] = true;
			todo.push_back(n);
		}
	}
	updateArea(layer, 0, 0, m_map->width - 1, m_map->height - 1, brush.bind);
}

void MapDocument::updateArea(Layer layer, int x1, int y1, int x2, int y2, bool bind)
{
	if (layer == EditorState::LOWER && bind)
		bindArea(x1, y1, x2, y2);
	emit cellsChanged(QRect(QPoint(x1, y1), QPoint(x2, y2)) & bounds(), layer);
}

void MapDocument::bindArea(int x1, int y1, int x2, int y2)
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
			const size_t i = static_cast<size_t>(index(x, y));
			const int terrain = Autotile::translate(m_lower[i]);
			if (terrain >= 0 && !Autotile::isEblock(terrain))
				m_lower[i] = Autotile::bind(m_lower, m_map->width, m_map->height, x, y);
		}
}

void MapDocument::commitStroke(const QString& text)
{
	const bool lower = m_lower != m_map->lower_layer;
	const bool upper = m_upper != m_map->upper_layer;
	if (!lower && !upper) {
		return;
	}
	m_undoStack->beginMacro(text);
	if (lower) {
		std::vector<short> before = std::exchange(m_map->lower_layer, m_lower);
		m_undoStack->push(new UndoDraw(EditorState::LOWER, std::move(before), this));
	}
	if (upper) {
		std::vector<short> before = std::exchange(m_map->upper_layer, m_upper);
		m_undoStack->push(new UndoDraw(EditorState::UPPER, std::move(before), this));
	}
	m_undoStack->endMacro();
	emit cellsChanged(bounds(), EditorState::LOWER);
	emit cellsChanged(bounds(), EditorState::UPPER);
	emit edited();
}

void MapDocument::cancelStroke()
{
	m_lower = m_map->lower_layer;
	m_upper = m_map->upper_layer;
	emit cellsChanged(bounds(), EditorState::LOWER);
	emit cellsChanged(bounds(), EditorState::UPPER);
}

MapDocument::TileClipboard MapDocument::copyArea(const QRect& cells) const
{
	TileClipboard clipboard;
	clipboard.width = cells.width();
	clipboard.height = cells.height();
	// both layers, as RPG Maker; the drawing in progress is not taken
	for (int y = cells.top(); y <= cells.bottom(); y++)
		for (int x = cells.left(); x <= cells.right(); x++)
		{
			const size_t i = static_cast<size_t>(index(x, y));
			clipboard.lower.push_back(m_map->lower_layer[i]);
			clipboard.upper.push_back(m_map->upper_layer[i]);
		}
	return clipboard;
}

void MapDocument::fillArea(Layer layer, const QRect& cells, const Brush& brush)
{
	if (cells.isEmpty() || layer == EditorState::EVENT)
		return;
	m_moveContents = {};
	m_moveBackground = {};
	// RPG Maker fills the area of the current layer with the current tiles
	std::vector<short>& tiles = working(layer);
	for (int y = cells.top(); y <= cells.bottom(); y++)
		for (int x = cells.left(); x <= cells.right(); x++)
			tiles[static_cast<size_t>(index(x, y))] = brush.at(x - cells.left(), y - cells.top());
	if (layer == EditorState::LOWER)
		bindArea(cells.left() - 1, cells.top() - 1, cells.right() + 1, cells.bottom() + 1);
	commitStroke(tr("Clear"));
}

void MapDocument::beginMoveArea(const QRect& cells, short background)
{
	// taken when a drag starts; one left unfinished keeps its own
	if (!m_moveBackground.lower.empty())
		return;
	m_moveContents = {};
	m_moveContents.width = cells.width();
	m_moveContents.height = cells.height();
	m_moveBackground.lower = m_map->lower_layer;
	m_moveBackground.upper = m_map->upper_layer;
	for (int y = cells.top(); y <= cells.bottom(); ++y)
		for (int x = cells.left(); x <= cells.right(); ++x) {
			const size_t i = static_cast<size_t>(index(x, y));
			m_moveContents.lower.push_back(m_map->lower_layer[i]);
			m_moveContents.upper.push_back(m_map->upper_layer[i]);
			m_moveBackground.lower[i] = background;
			m_moveBackground.upper[i] = 10000;
		}
}

QRect MapDocument::moveAreaTo(const QPoint& top_left)
{
	if (m_moveBackground.lower.empty())
		return QRect();
	const QPoint pos(std::clamp(top_left.x(), 0, m_map->width - m_moveContents.width),
		std::clamp(top_left.y(), 0, m_map->height - m_moveContents.height));
	m_lower = m_moveBackground.lower;
	m_upper = m_moveBackground.upper;
	for (int y = 0; y < m_moveContents.height; ++y)
		for (int x = 0; x < m_moveContents.width; ++x) {
			const size_t i = static_cast<size_t>(index(pos.x() + x, pos.y() + y));
			const size_t j = static_cast<size_t>(x + y * m_moveContents.width);
			m_lower[i] = m_moveContents.lower[j];
			m_upper[i] = m_moveContents.upper[j];
		}
	emit cellsChanged(bounds(), EditorState::LOWER);
	emit cellsChanged(bounds(), EditorState::UPPER);
	return QRect(pos, QSize(m_moveContents.width, m_moveContents.height));
}

void MapDocument::endMoveArea(bool commit)
{
	if (commit)
		commitStroke(tr("Move"));
	else
		cancelStroke();
	// the next drag fills the cells it leaves with the lower tile picked then
	m_moveContents = {};
	m_moveBackground = {};
}

void MapDocument::startFloating(const TileClipboard& tiles, const QPoint& pos)
{
	m_float = tiles;
	m_floatPos = pos;
	m_floating = true;
	emit floatingChanged();
}

bool MapDocument::isFloating() const
{
	return m_floating;
}

const MapDocument::TileClipboard& MapDocument::floatingTiles() const
{
	return m_float;
}

QPoint MapDocument::floatingPos() const
{
	return m_floatPos;
}

void MapDocument::setFloatingPos(const QPoint& pos)
{
	m_floatPos = pos;
}

void MapDocument::commitFloating()
{
	if (!m_floating)
		return;
	m_floating = false;
	emit floatingChanged();
	for (int y = 0; y < m_float.height; y++)
		for (int x = 0; x < m_float.width; x++)
		{
			const int map_x = m_floatPos.x() + x;
			const int map_y = m_floatPos.y() + y;
			if (map_x < 0 || map_y < 0 || map_x >= m_map->width || map_y >= m_map->height)
				continue;
			const size_t i = static_cast<size_t>(index(map_x, map_y));
			const size_t j = static_cast<size_t>(x + y * m_float.width);
			m_lower[i] = m_float.lower[j];
			m_upper[i] = m_float.upper[j];
		}
	// RPG Maker transfers the stored tile variants unchanged, including
	// autotile edges. Pasting must not reshape them or their neighbours.
	commitStroke(tr("Paste"));
}

void MapDocument::cancelFloating()
{
	const bool floating = m_floating;
	m_floating = false;
	if (floating)
		emit floatingChanged();
}

const lcf::rpg::Event* MapDocument::findEvent(int id) const
{
	for (const auto& ev : m_map->events)
		if (ev.ID == id)
			return &ev;
	return nullptr;
}

const lcf::rpg::Event* MapDocument::eventAt(int x, int y) const
{
	for (const auto& ev : m_map->events)
		if (index(x, y) == index(ev.x, ev.y))
			return &ev;
	return nullptr;
}

int MapDocument::eventIdAt(int x, int y) const
{
	for (const auto& ev : m_map->events)
		if (ev.x == x && ev.y == y)
			return ev.ID;
	return 0;
}

int MapDocument::firstFreeEventId() const
{
	int id = 1;
	while (findEvent(id))
		++id;
	return id;
}

bool MapDocument::isFreeCell(int x, int y) const
{
	return !eventAt(x, y) && startAt(x, y) < 0;
}

lcf::rpg::Event MapDocument::newEvent(int x, int y) const
{
	lcf::rpg::Event event;
	event.ID = firstFreeEventId();
	event.name = ToDBString(QString("EV%1").arg(QString::number(event.ID), 4, QLatin1Char('0')));
	event.x = x;
	event.y = y;
	event.pages.push_back(EventPageDefaults::newPage());
	return event;
}

bool MapDocument::commitEvent(const lcf::rpg::Event& event)
{
	// The map can have been read again while the event was edited: by its id
	auto& events = m_map->events;
	auto ev = std::find_if(events.begin(), events.end(), [&](const auto& e) { return e.ID == event.ID; });
	if (ev != events.end()) {
		if (*ev == event) {
			return false;
		}
		lcf::rpg::Event backup = *ev;
		*ev = event;
		m_undoStack->push(new UndoEvent(event.ID, backup, this));
	} else {
		events.push_back(event);
		m_undoStack->push(new UndoEvent(event.ID, std::nullopt, this));
	}
	emit eventsChanged();
	emit edited();
	return true;
}

bool MapDocument::pasteEvent(const lcf::rpg::Event& event, int x, int y)
{
	if (!isFreeCell(x, y))
		return false;
	// RPG Maker keeps the name, the event gets the first free id
	lcf::rpg::Event pasted = event;
	pasted.ID = firstFreeEventId();
	pasted.x = x;
	pasted.y = y;
	m_map->events.push_back(pasted);
	m_undoStack->push(new UndoEvent(pasted.ID, std::nullopt, this));
	emit eventsChanged();
	emit edited();
	return true;
}

bool MapDocument::deleteEventAt(int x, int y)
{
	auto& events = m_map->events;
	auto ev = std::find_if(events.begin(), events.end(), [&](const auto& e) { return index(x, y) == index(e.x, e.y); });
	if (ev == events.end())
		return false;
	const lcf::rpg::Event backup = *ev;
	events.erase(ev);
	m_undoStack->push(new UndoEvent(backup.ID, backup, this));
	emit eventsChanged();
	emit edited();
	return true;
}

bool MapDocument::moveEvent(int id, int x, int y)
{
	const auto* ev = findEvent(id);
	if (!ev || eventAt(x, y))
		return false;
	lcf::rpg::Event moved = *ev;
	moved.x = x;
	moved.y = y;
	// pushes the undo of the old position
	return commitEvent(moved);
}

void MapDocument::placeTeleport(int x, int y, int map_id, int to_x, int to_y, std::optional<int> direction)
{
	// As RPG Maker: a normal event touched by the hero, with Hide Screen,
	// Teleport and Show Screen (the default transitions)
	using Code = lcf::rpg::EventCommand::Code;
	auto command = [](Code code, const std::vector<int32_t>& parameters) {
		lcf::rpg::EventCommand cmd;
		cmd.code = static_cast<int32_t>(code);
		cmd.parameters = lcf::DBArray<int32_t>(parameters.begin(), parameters.end());
		return cmd;
	};
	std::vector<int32_t> teleport = {map_id, to_x, to_y};
	if (direction)
		teleport.push_back(*direction);
	lcf::rpg::EventPage page = EventPageDefaults::newPage();
	page.ID = 1;
	page.trigger = lcf::rpg::EventPage::Trigger_touched;
	page.event_commands = {command(Code::EraseScreen, {-1}), command(Code::Teleport, teleport),
		command(Code::ShowScreen, {-1})};

	lcf::rpg::Event event = newEvent(x, y);
	event.pages = {page};
	// added with undo, as a new event
	commitEvent(event);
}

int MapDocument::startAt(int x, int y) const
{
	// the last one is drawn over the others
	lcf::rpg::Start start = startData();
	for (int kind = Airship; kind >= Party; kind--) {
		const StartFields fields = startFields(start, kind);
		if (fields.map_id == id() && fields.x == x && fields.y == y)
			return kind;
	}
	return -1;
}

std::optional<QPoint> MapDocument::startCell(int kind) const
{
	lcf::rpg::Start start = startData();
	const StartFields fields = startFields(start, kind);
	if (fields.map_id != id())
		return std::nullopt;
	return QPoint(fields.x, fields.y);
}

void MapDocument::setStartPosition(int kind, int x, int y)
{
	const lcf::rpg::Start old_start = startData();
	const StartFields fields = startFields(m_project.treeMap().start, kind);
	fields.map_id = id();
	fields.x = x;
	fields.y = y;
	pushStart(old_start);
}

void MapDocument::pushStart(const lcf::rpg::Start& old_start)
{
	if (old_start == startData())
		return;
	// The start is in the map tree, saved with this map; the map it left,
	// open in another tab, draws it no more
	m_undoStack->push(new UndoStart(old_start, this));
	notifyStartChanged(m_project, old_start);
	emit edited();
}

QList<int> MapDocument::eventsIn(const QRect& cells) const
{
	QList<int> events;
	const QRect area = cells & bounds();
	for (const auto& ev : m_map->events)
		if (area.contains(ev.x, ev.y))
			events.append(ev.ID);
	return events;
}

QList<int> MapDocument::startsIn(const QRect& cells) const
{
	QList<int> starts;
	const QRect area = cells & bounds();
	for (int kind = Party; kind <= Airship; kind++)
		if (const auto cell = startCell(kind); cell && area.contains(*cell))
			starts.append(kind);
	return starts;
}

bool MapDocument::canMoveItems(const QList<int>& events, const QList<int>& starts, const QPoint& offset) const
{
	lcf::rpg::Start start = startData();
	// RPG Maker's rule of the drops: an empty cell, here or one left by the
	// items moved
	auto free = [&](const QPoint& cell) {
		if (!bounds().contains(cell))
			return false;
		for (const auto& ev : m_map->events)
			if (ev.x == cell.x() && ev.y == cell.y() && !events.contains(ev.ID))
				return false;
		for (int kind = Party; kind <= Airship; kind++) {
			const StartFields fields = startFields(start, kind);
			if (fields.map_id == id() && fields.x == cell.x() && fields.y == cell.y() && !starts.contains(kind))
				return false;
		}
		return true;
	};
	for (int ev_id : events)
		if (const auto* ev = findEvent(ev_id); ev && !free(QPoint(ev->x, ev->y) + offset))
			return false;
	for (int kind : starts) {
		// one placed on another map since it was selected stays there
		const StartFields fields = startFields(start, kind);
		if (fields.map_id == id() && !free(QPoint(fields.x, fields.y) + offset))
			return false;
	}
	return true;
}

bool MapDocument::moveItems(const QList<int>& events, const QList<int>& starts, const QPoint& offset)
{
	if (offset.isNull() || !canMoveItems(events, starts, offset))
		return false;
	m_undoStack->beginMacro(tr("Move Events"));
	bool moved_events = false;
	for (int ev_id : events) {
		auto& list = m_map->events;
		auto ev = std::find_if(list.begin(), list.end(), [ev_id](const auto& e) { return e.ID == ev_id; });
		if (ev == list.end())
			continue;
		const lcf::rpg::Event before = *ev;
		ev->x += offset.x();
		ev->y += offset.y();
		m_undoStack->push(new UndoEvent(ev_id, before, this));
		moved_events = true;
	}
	const lcf::rpg::Start old_start = startData();
	for (int kind : starts) {
		const StartFields fields = startFields(m_project.treeMap().start, kind);
		if (fields.map_id != id())
			continue;
		fields.x += offset.x();
		fields.y += offset.y();
	}
	const bool moved_starts = !(old_start == startData());
	if (moved_starts)
		m_undoStack->push(new UndoStart(old_start, this));
	m_undoStack->endMacro();
	if (moved_events)
		emit eventsChanged();
	notifyStartChanged(m_project, old_start);
	emit edited();
	return true;
}

std::vector<lcf::rpg::Event> MapDocument::copyEvents(const QList<int>& events, const QPoint& origin) const
{
	std::vector<lcf::rpg::Event> copies;
	for (int ev_id : events) {
		if (const auto* ev = findEvent(ev_id)) {
			copies.push_back(*ev);
			copies.back().x -= origin.x();
			copies.back().y -= origin.y();
		}
	}
	return copies;
}

void MapDocument::deleteEvents(const QList<int>& events)
{
	if (events.isEmpty())
		return;
	m_undoStack->beginMacro(tr("Delete Events"));
	for (int ev_id : events) {
		auto& list = m_map->events;
		auto ev = std::find_if(list.begin(), list.end(), [ev_id](const auto& e) { return e.ID == ev_id; });
		if (ev == list.end())
			continue;
		const lcf::rpg::Event backup = *ev;
		list.erase(ev);
		m_undoStack->push(new UndoEvent(ev_id, backup, this));
	}
	m_undoStack->endMacro();
	emit eventsChanged();
	emit edited();
}

bool MapDocument::canPasteEvents(const std::vector<lcf::rpg::Event>& events, const QPoint& cell) const
{
	if (events.empty())
		return false;
	for (const auto& ev : events) {
		const QPoint target = cell + QPoint(ev.x, ev.y);
		if (!bounds().contains(target) || !isFreeCell(target.x(), target.y()))
			return false;
	}
	return true;
}

QList<int> MapDocument::pasteEvents(const std::vector<lcf::rpg::Event>& events, const QPoint& cell)
{
	if (!canPasteEvents(events, cell))
		return {};
	QList<int> ids;
	m_undoStack->beginMacro(tr("Paste Events"));
	for (const auto& ev : events) {
		// RPG Maker keeps the names, the events get the first free ids
		lcf::rpg::Event pasted = ev;
		pasted.ID = firstFreeEventId();
		pasted.x += cell.x();
		pasted.y += cell.y();
		m_map->events.push_back(pasted);
		m_undoStack->push(new UndoEvent(pasted.ID, std::nullopt, this));
		ids.append(pasted.ID);
	}
	m_undoStack->endMacro();
	emit eventsChanged();
	emit edited();
	return ids;
}

bool MapDocument::setMapProperties(const lcf::rpg::Map& map, const lcf::rpg::MapInfo& info)
{
	if (map == *m_map && info == m_info)
		return false;
	const bool size_changed = map.width != m_map->width || map.height != m_map->height;
	auto* undo = new UndoMap(*m_map, m_info, this);
	*m_map = map;
	m_info = info;
	m_lower = m_map->lower_layer;
	m_upper = m_map->upper_layer;
	// Kept in memory with undo and saved with the map, like a drawing:
	// saving here would also save the drawing and forget its undo
	m_undoStack->push(undo);
	// a resize can have removed events
	emit mapReplaced(size_changed);
	emit edited();
	return true;
}

void MapDocument::shiftMap(int dx, int dy)
{
	// tiles, events and the start positions on this map: one undo step
	commitFloating();
	const lcf::rpg::Map old_map = *m_map;
	const lcf::rpg::Start old_start = startData();
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
	setMapData(shifted, m_info);
	m_undoStack->beginMacro(tr("Shift Map"));
	m_undoStack->push(new UndoMap(old_map, m_info, this));
	if (!(old_start == start))
		m_undoStack->push(new UndoStart(old_start, this));
	m_undoStack->endMacro();
	notifyStartChanged(m_project, old_start);
	emit edited();
}
