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

#include "map_editor.h"
#include <QCoreApplication>
#include <QKeySequence>
#include <algorithm>
#include <cmath>
#include <numbers>
#include "defines.h"
#include "common/dbstring.h"
#include "model/autotile.h"

namespace {
// the events copied with the selection tool, their cells from the top left
// of the selection; for every map
std::vector<lcf::rpg::Event>& itemsClipboard()
{
	static std::vector<lcf::rpg::Event> events;
	return events;
}

// a step of the zoom each time the fingers of a pinch spread (or close) by this
constexpr qreal PinchStep = std::numbers::sqrt2;
}

MapEditor::MapEditor(MapDocument* document, EditorState& state, QObject* parent) :
	QObject(parent), m_doc(document), m_state(state)
{
	// a pixel of the screen every 40 ms
	m_antsTimer.setInterval(40);
	connect(&m_antsTimer, &QTimer::timeout, this, [this]() {
		m_antsOffset = (m_antsOffset + 1) % 1200;
		emit antsOffsetChanged();
	});
	connect(&m_state, &EditorState::layerChanged, this, &MapEditor::onLayerChanged);
	connect(&m_state, &EditorState::toolChanged, this, &MapEditor::onToolChanged);
	connect(m_doc, &MapDocument::floatingChanged, this, &MapEditor::onFloatingChanged);
	// a save, whoever asks for it, puts down the pasted tiles as a click would
	connect(m_doc, &MapDocument::aboutToSave, this, &MapEditor::commitFloating);
	connect(m_doc, &MapDocument::loaded, this, &MapEditor::clearSelection);
	connect(m_doc, &MapDocument::loaded, this, &MapEditor::mapChanged);
	connect(m_doc, &MapDocument::mapReplaced, this, &MapEditor::mapChanged);
	connect(m_doc, &MapDocument::loaded, this, &MapEditor::historyChanged);
	connect(m_doc, &MapDocument::edited, this, &MapEditor::historyChanged);
	m_cursor = currentCursor();
}

MapDocument* MapEditor::document() const
{
	return m_doc;
}

EditorState* MapEditor::state() const
{
	return &m_state;
}

QString MapEditor::mapName() const
{
	return m_doc->name();
}

QSize MapEditor::mapSize() const
{
	return m_doc->hasMap() ? m_doc->bounds().size() : QSize();
}

int MapEditor::chipsetId() const
{
	return m_doc->hasMap() ? m_doc->map().chipset_id : -1;
}

QPoint MapEditor::scrollPosition() const
{
	return QPoint(m_doc->mapInfo().scrollbar_x, m_doc->mapInfo().scrollbar_y);
}

void MapEditor::setScrollPosition(const QPoint& pixels)
{
	m_doc->setScrollPosition(pixels.x(), pixels.y());
}

bool MapEditor::isFloating() const
{
	return m_doc->isFloating();
}

QRect MapEditor::floatingRect() const
{
	if (!m_doc->isFloating())
		return QRect();
	const auto& tiles = m_doc->floatingTiles();
	return QRect(m_doc->floatingPos(), QSize(tiles.width, tiles.height));
}

QList<int> MapEditor::floatingLower() const
{
	if (!m_doc->isFloating())
		return {};
	const auto& tiles = m_doc->floatingTiles().lower;
	return QList<int>(tiles.begin(), tiles.end());
}

QList<int> MapEditor::floatingUpper() const
{
	if (!m_doc->isFloating())
		return {};
	const auto& tiles = m_doc->floatingTiles().upper;
	return QList<int>(tiles.begin(), tiles.end());
}

bool MapEditor::canUndo() const
{
	return m_doc->canUndo();
}

bool MapEditor::canRedo() const
{
	return m_doc->canRedo();
}

QPoint MapEditor::pointerCell() const
{
	return m_cur;
}

QRect MapEditor::brushRect() const
{
	return m_brush;
}

QPoint MapEditor::selectedCell() const
{
	return m_selecting ? m_last : QPoint(-1, -1);
}

QPoint MapEditor::markedCell() const
{
	// the selected items have their own
	if (!m_selecting || !itemCells().isEmpty())
		return QPoint(-1, -1);
	return m_mark;
}

QRect MapEditor::selectionArea() const
{
	return m_selArea;
}

QRect MapEditor::selectionRect() const
{
	return m_selArea.translated(m_itemsOffset);
}

QList<int> MapEditor::selectedEvents() const
{
	return m_selEvents;
}

QList<int> MapEditor::selectedStarts() const
{
	return m_selStarts;
}

QList<QPoint> MapEditor::itemCells() const
{
	QList<QPoint> cells;
	for (int ev_id : m_selEvents)
		if (const auto* ev = m_doc->findEvent(ev_id))
			cells.append(QPoint(ev->x, ev->y) + m_itemsOffset);
	for (int kind : m_selStarts)
		if (const auto cell = m_doc->startCell(kind))
			cells.append(*cell + m_itemsOffset);
	return cells;
}

QPoint MapEditor::itemsOffset() const
{
	return m_itemsOffset;
}

int MapEditor::antsOffset() const
{
	return m_antsOffset;
}

MapEditor::Cursor MapEditor::cursor() const
{
	return m_cursor;
}

bool MapEditor::isDragging() const
{
	return m_dragging;
}

QString MapEditor::hoverText() const
{
	return m_hoverText;
}

qreal MapEditor::scale() const
{
	return m_scale;
}

void MapEditor::setScale(qreal scale)
{
	scale = snapScale(scale);
	if (scale == m_scale)
		return;
	m_scale = scale;
	emit scaleChanged();
}

qreal MapEditor::snapScale(qreal scale) const
{
	return snappedScale(scale, m_devicePixelRatio);
}

qreal MapEditor::snappedScale(qreal scale, qreal device_pixel_ratio)
{
	// Pixel art stays sharp only when each of its pixels covers a power of
	// two of screen pixels (1/4, 1/2, 1, 2, 4...): the scale is rounded to
	// one, in device pixels (Retina, Windows scaling...)
	const qreal dpr = device_pixel_ratio > 0 ? device_pixel_ratio : 1.0;
	const double device = std::clamp(scale * dpr, 1.0 / 8, 16.0);
	return std::exp2(std::round(std::log2(device))) / dpr;
}

qreal MapEditor::devicePixelRatio() const
{
	return m_devicePixelRatio;
}

void MapEditor::setDevicePixelRatio(qreal ratio)
{
	if (ratio == m_devicePixelRatio)
		return;
	m_devicePixelRatio = ratio;
	emit devicePixelRatioChanged();
}

QRectF MapEditor::viewCells() const
{
	return m_viewCells;
}

void MapEditor::setViewCells(const QRectF& cells)
{
	if (cells == m_viewCells)
		return;
	m_viewCells = cells;
	emit viewCellsChanged();
}

void MapEditor::press(int x, int y, Qt::MouseButton button)
{
	onPress(x, y, button);
	refresh();
}

void MapEditor::move(int x, int y, Qt::MouseButtons buttons)
{
	onMove(x, y, buttons);
	refresh();
}

void MapEditor::release(int x, int y, Qt::MouseButton button, Qt::MouseButtons buttons)
{
	onRelease(x, y, button, buttons);
	refresh();
}

void MapEditor::doubleClick(int x, int y)
{
	onDoubleClick(x, y);
	refresh();
}

void MapEditor::leave()
{
	// the pointer cell stays: a paste on the event layer goes there
	setBrushRect(QRect());
}

void MapEditor::onPress(int x, int y, Qt::MouseButton button)
{
	const bool inside = setPointer(x, y);
	const Layer layer = m_state.layer();
	const EditorState::Tool tool = m_state.tool();
	if (button == Qt::RightButton)
	{
		if (m_drawing)
		{
			stopDrawing();
			return;
		}
		if (selectingItems())
		{
			// the menu of the selection tool, on the selected events; Paste
			// puts the copied ones on the cell clicked
			if (!inside || m_itemsSelecting || m_itemsMoving)
				return;
			selectCell(m_cur.x(), m_cur.y());
			emit contextMenuRequested(AreaMenu, m_cur);
			return;
		}
		if (layer == EditorState::EVENT)
		{
			// the other tools are not used on the event layer (RPG Maker)
			m_dragEventId = 0;
			m_dragStart = -1;
			if (!inside)
				return;
			selectCell(m_cur.x(), m_cur.y());
			emit contextMenuRequested(EventMenu, m_cur);
			return;
		}
		if (tool == EditorState::ZOOM)
		{
			zoomOut();
			return;
		}
		if (!inside || m_selDragging || m_selMoving || m_floatDragging)
			return;
		if (tool == EditorState::SELECT)
		{
			emit contextMenuRequested(AreaMenu, m_cur);
			return;
		}
		pickTile();
		return;
	}
	if (button != Qt::LeftButton)
		return;
	// what the press grabs moves once the pointer is dragged (startDrag)
	m_dragStarted = false;

	if (selectingItems())
	{
		if (!inside)
			return;
		selectCell(m_cur.x(), m_cur.y());
		if (m_selArea.contains(m_cur)) {
			// drag the selected events and start positions
			m_itemsMoving = true;
			m_itemsGrab = m_cur;
			setItemsOffset(QPoint());
		} else {
			// a rectangle of cells, or a click on an item
			clearSelection();
			m_itemsSelecting = true;
			m_first = m_cur;
			m_selArea = QRect(m_cur, QSize(1, 1));
			updateSelection();
		}
		return;
	}
	if (layer == EditorState::EVENT) // Select tile, maybe to drag its event
	{
		// Do not allow selecting the (-1, -1) spot
		if (!inside)
			return;
		selectCell(m_cur.x(), m_cur.y());
		// a start position hides the event of its cell
		m_dragStart = m_doc->startAt(m_cur.x(), m_cur.y());
		m_dragEventId = m_dragStart < 0 ? m_doc->eventIdAt(m_cur.x(), m_cur.y()) : 0;
		return;
	}
	if (tool == EditorState::ZOOM)
	{
		zoomIn();
		return;
	}
	// Do not draw on invalid coordinates
	if (!inside)
		return;

	m_first = m_cur;
	switch (tool)
	{
	case EditorState::PENCIL:
		m_drawing = true;
		m_doc->drawPen(layer, m_cur, m_first, brush());
		break;
	case EditorState::RECTANGLE:
		m_drawing = true;
		m_doc->drawRect(layer, m_first, m_cur, brush());
		break;
	case EditorState::CIRCLE:
		m_drawing = true;
		m_doc->drawEllipse(layer, m_first, m_cur, brush());
		break;
	case EditorState::FILL:
		m_drawing = true;
		m_doc->drawFill(layer, m_first, brush());
		break;
	case EditorState::SELECT:
		if (m_doc->isFloating())
		{
			// drag the pasted area, or put it on the map with a click outside
			const QPoint pos = m_doc->floatingPos();
			const auto& tiles = m_doc->floatingTiles();
			if (QRect(pos, QSize(tiles.width, tiles.height)).contains(m_cur))
			{
				m_floatDragging = true;
				m_floatGrab = m_cur - pos;
			}
			else
				commitFloating();
			break;
		}
		if (m_selArea.contains(m_cur)) {
			// The original editor leaves the first lower palette tile
			// (without an autotile variant) and a transparent upper tile.
			short background = m_state.selection(0, 0, EditorState::LOWER);
			if (background < 5000)
				background -= background % 50;
			m_doc->beginMoveArea(m_selArea, background);
			m_selMoving = true;
			m_selGrab = m_cur - m_selArea.topLeft();
			break;
		}
		clearSelection();
		m_selDragging = true;
		m_selArea = QRect(m_cur, QSize(1, 1));
		updateSelection();
		break;
	default:
		break;
	}
}

void MapEditor::onMove(int x, int y, Qt::MouseButtons buttons)
{
	const QPoint old = m_cur;
	if (!setPointer(x, y)) {
		updateBrush();
		return;
	}
	if (m_cur == old)
		return;
	updateBrush();
	const Layer layer = m_state.layer();
	if (m_drawing)
	{
		switch (m_state.tool())
		{
		case EditorState::PENCIL:
			m_doc->drawPen(layer, m_cur, m_first, brush());
			break;
		case EditorState::RECTANGLE:
			m_doc->drawRect(layer, m_first, m_cur, brush());
			break;
		case EditorState::CIRCLE:
			m_doc->drawEllipse(layer, m_first, m_cur, brush());
			break;
		default:
			break;
		}
	}
	if (m_selDragging || m_itemsSelecting)
	{
		m_selArea = QRect(m_first, m_cur).normalized();
		updateSelection();
	}
	if (m_dragStarted && (buttons & Qt::LeftButton))
		dragTo();

	// Show coordinates of current tile
	QString status_msg = QString("(%0, %1)").arg(m_cur.x()).arg(m_cur.y());
	if (layer == EditorState::EVENT)
	{
		// Show events on current tile
		for (const lcf::rpg::Event& evt : m_doc->map().events)
		{
			if (QPoint(evt.x, evt.y) == m_cur)
			{
				status_msg.append(QString(" - " + QCoreApplication::translate("MapScene", "Event") + " %0: %1")
					.arg(evt.ID).arg(ToQString(evt.name)));
			}
		}
	}
	m_hoverText = status_msg;
	emit cellHovered(status_msg);
}

void MapEditor::dragTo()
{
	if (m_itemsMoving)
	{
		setItemsOffset(m_cur - m_itemsGrab);
		updateSelection();
	}
	if (m_selMoving) {
		// the tiles go along, inside the map
		const QRect area = m_doc->moveAreaTo(m_cur - m_selGrab);
		if (!area.isNull()) {
			m_selArea.moveTopLeft(area.topLeft());
			updateSelection();
		}
	}
	if (m_floatDragging)
	{
		const QPoint pos = m_cur - m_floatGrab;
		m_doc->setFloatingPos(pos);
		m_selArea = QRect(pos, QSize(m_doc->floatingTiles().width, m_doc->floatingTiles().height));
		updateSelection();
		emit floatingRectChanged();
	}
	if (m_dragEventId || m_dragStart >= 0)
	{
		// the cell the event or the start position is dragged to
		m_mark = m_cur;
		emit selectionChanged();
	}
}

void MapEditor::startDrag()
{
	if (m_dragStarted)
		return;
	m_dragStarted = true;
	// the cell the pointer reached meanwhile counts
	if (m_cur.x() >= 0)
		dragTo();
	refresh();
}

void MapEditor::cancelPress()
{
	// Another window took the pointer before the release (the event window
	// opened by a double-click): nothing dragged is dropped
	m_dragEventId = 0;
	m_dragStart = -1;
	if (m_itemsMoving) {
		m_itemsMoving = false;
		setItemsOffset(QPoint());
		updateSelection();
	}
	// what else the press did ends as a release would end it
	onRelease(m_cur.x(), m_cur.y(), Qt::LeftButton, Qt::NoButton);
	refresh();
}

void MapEditor::onRelease(int x, int y, Qt::MouseButton button, Qt::MouseButtons buttons)
{
	if (m_cancelled && !buttons)
	{
		m_cancelled = false;
		return;
	}
	if (button != Qt::LeftButton)
		return;
	if (m_drawing && !m_cancelled)
	{
		m_drawing = false;
		m_doc->commitStroke();
	}
	m_selDragging = false;
	if (m_selMoving) {
		m_selMoving = false;
		m_doc->endMoveArea(true);
	}
	m_floatDragging = false;
	if (m_itemsSelecting)
	{
		m_itemsSelecting = false;
		selectItems(m_selArea);
	}
	if (m_itemsMoving)
	{
		m_itemsMoving = false;
		const QPoint offset = m_itemsOffset;
		setItemsOffset(QPoint());
		// dropped out of the map, or where an item is not free to go: nothing moves
		if (setPointer(x, y) && !offset.isNull() && canMoveItems(offset)) {
			moveItems(offset);
			selectCell(m_itemsGrab.x() + offset.x(), m_itemsGrab.y() + offset.y());
		} else {
			updateSelection();
		}
	}
	if (m_dragEventId)
	{
		const int id = m_dragEventId;
		m_dragEventId = 0;
		setPointer(x, y);
		// RPG Maker: onto an empty cell only; a click (the pointer not
		// dragged) moves nothing, whatever cell it is released on
		if (m_dragStarted && m_cur.x() >= 0 && m_cur != m_last && m_doc->isFreeCell(m_cur.x(), m_cur.y()))
		{
			m_doc->moveEvent(id, m_cur.x(), m_cur.y());
			selectCell(m_cur.x(), m_cur.y());
		}
		else
			selectCell(m_last.x(), m_last.y());
	}
	if (m_dragStart >= 0)
	{
		const int kind = m_dragStart;
		m_dragStart = -1;
		setPointer(x, y);
		// RPG Maker: onto an empty cell only, as an event
		if (m_dragStarted && m_cur.x() >= 0 && m_cur.y() >= 0 && m_cur != m_last && m_doc->isFreeCell(m_cur.x(), m_cur.y()))
		{
			m_doc->setStartPosition(kind, m_cur.x(), m_cur.y());
			selectCell(m_cur.x(), m_cur.y());
		}
		else
			selectCell(m_last.x(), m_last.y());
	}
}

void MapEditor::onDoubleClick(int x, int y)
{
	if (m_state.layer() != EditorState::EVENT)
		return;

	// Do not allow putting events on invalid coordinates
	if (!setPointer(x, y))
		return;

	// the second press starts no drag: the window opened takes the pointer
	// before its release
	m_dragEventId = 0;
	m_dragStart = -1;
	if (m_itemsMoving) {
		m_itemsMoving = false;
		setItemsOffset(QPoint());
		updateSelection();
	}
	selectCell(m_cur.x(), m_cur.y());
	// a start position is not an event
	if (m_doc->startAt(m_cur.x(), m_cur.y()) >= 0)
		return;
	if (m_doc->eventAt(m_cur.x(), m_cur.y()))
		editEvent();
	else
		newEvent();
}

bool MapEditor::wheel(int delta, Qt::KeyboardModifiers modifiers)
{
	// Ctrl + wheel zooms, by the steps of the zoom tool
	if (!(modifiers & Qt::ControlModifier))
		return false;
	if (delta > 0)
		zoomIn();
	else if (delta < 0)
		zoomOut();
	return true;
}

void MapEditor::pinch(qreal factor)
{
	// A trackpad pinch zooms by the steps of the zoom tool, as Ctrl + wheel:
	// a step each time the fingers spread (or close) by about 1.4
	m_pinch *= factor;
	if (m_pinch >= PinchStep && m_scale < MaxScale) {
		setScale(m_scale * 2);
		m_pinch /= 2;
	} else if (m_pinch <= PinchStep / 2 && m_scale > MinScale) {
		setScale(m_scale / 2);
		m_pinch *= 2;
	}
	// at a limit, the fingers going on do not pile up
	m_pinch = std::clamp(m_pinch, PinchStep / 2, PinchStep);
}

void MapEditor::resetPinch()
{
	m_pinch = 1;
}

void MapEditor::zoomIn()
{
	if (m_scale < MaxScale)
		setScale(m_scale * 2);
}

void MapEditor::zoomOut()
{
	if (m_scale > MinScale)
		setScale(m_scale / 2);
}

bool MapEditor::handleKey(int key, Qt::KeyboardModifiers modifiers, bool run)
{
	const bool no_modifier = (modifiers & ~Qt::KeypadModifier) == Qt::NoModifier;
	const bool enter = no_modifier && (key == Qt::Key_Return || key == Qt::Key_Enter);
	// Backspace: the Delete key of Mac keyboards
	const bool del = no_modifier && (key == Qt::Key_Delete || key == Qt::Key_Backspace);
	// as QKeyEvent::matches: the keypad and group switch modifiers make no difference
	const int combined = (modifiers.toInt() | key) & ~(Qt::KeypadModifier | Qt::GroupSwitchModifier).toInt();
	const QKeySequence sequence(QKeyCombination::fromCombined(combined));
	auto matches = [&sequence](QKeySequence::StandardKey standard) {
		return QKeySequence::keyBindings(standard).contains(sequence);
	};
	void (MapEditor::*slot)() = nullptr;

	if (selectingItems())
	{
		// the keys of the selection tool, on the selected events
		const bool has_events = !m_selEvents.isEmpty();
		if (matches(QKeySequence::Cut) && has_events)
			slot = &MapEditor::cutArea;
		else if (matches(QKeySequence::Copy) && has_events)
			slot = &MapEditor::copyArea;
		else if (matches(QKeySequence::Paste) && !itemsClipboard().empty())
			slot = &MapEditor::pasteArea;
		else if (del && has_events)
			slot = &MapEditor::clearArea;
		else if (matches(QKeySequence::SelectAll))
			slot = &MapEditor::selectAll;
		else if (no_modifier && key == Qt::Key_Escape && !m_selArea.isEmpty())
			slot = &MapEditor::clearSelection;
		// the selection replaces the event of the cell, but for Enter
		else if (!enter)
			return false;
	}
	if (m_state.layer() == EditorState::EVENT)
	{
		// the arrows move the selected cell, as in RPG Maker
		int dx = 0, dy = 0;
		if (!slot && no_modifier)
		{
			dx = key == Qt::Key_Left ? -1 : key == Qt::Key_Right ? 1 : 0;
			dy = key == Qt::Key_Up ? -1 : key == Qt::Key_Down ? 1 : 0;
		}
		if (dx || dy)
		{
			if (run) {
				moveSelectedCell(dx, dy);
				refresh();
			}
			return true;
		}
		// the keys of the event on the cell selected with the mouse; the
		// selection tool's first
		if (!slot && !m_selecting)
			return false;
		const bool has_event = m_doc->eventAt(m_last.x(), m_last.y());
		const bool has_start = m_doc->startAt(m_last.x(), m_last.y()) >= 0;
		if (slot)
			;
		else if (enter && !has_start)
			slot = has_event ? &MapEditor::editEvent : &MapEditor::newEvent;
		else if (matches(QKeySequence::Cut) && has_event)
			slot = &MapEditor::cutEvent;
		else if (matches(QKeySequence::Copy) && has_event)
			slot = &MapEditor::copyEvent;
		else if (matches(QKeySequence::Paste) && !has_event && !has_start && m_state.eventClipboard())
			slot = &MapEditor::pasteEvent;
		else if (del && has_event)
			slot = &MapEditor::deleteEvent;
	}
	else if (m_state.tool() == EditorState::SELECT && !m_drawing)
	{
		const bool has_area = m_doc->isFloating() || !m_selArea.isEmpty();
		if (matches(QKeySequence::Cut) && has_area)
			slot = &MapEditor::cutArea;
		else if (matches(QKeySequence::Copy) && has_area)
			slot = &MapEditor::copyArea;
		else if (matches(QKeySequence::Paste) && m_state.tileClipboard().width > 0)
			slot = &MapEditor::pasteArea;
		else if (del && has_area)
			slot = &MapEditor::clearArea;
		else if (matches(QKeySequence::SelectAll))
			slot = &MapEditor::selectAll;
		else if (no_modifier && key == Qt::Key_Escape && has_area)
			slot = &MapEditor::cancelFloating;
	}
	if (!slot)
		return false;
	if (run) {
		(this->*slot)();
		refresh();
	}
	return true;
}

bool MapEditor::canMoveItems(const QPoint& offset) const
{
	return m_doc->canMoveItems(m_selEvents, m_selStarts, offset);
}

QVariantMap MapEditor::actionStates(Menu menu) const
{
	QVariantMap states;
	auto set = [&states](const char* name, bool enabled, bool visible = true) {
		states.insert(QLatin1String(name), QVariantMap{{"enabled", enabled}, {"visible", visible}});
	};
	if (menu == EventMenu)
	{
		const bool has_event = m_doc->eventAt(m_last.x(), m_last.y());
		const bool has_start = m_doc->startAt(m_last.x(), m_last.y()) >= 0;
		const bool free = !has_event && !has_start;
		set("actionEventNew", free, !has_event);
		// a start is not an event: the event it hides is not edited
		set("actionEventEdit", !has_start, has_event);
		set("actionEventCopy", has_event);
		set("actionEventCut", has_event);
		set("actionEventPaste", free && m_state.eventClipboard());
		set("actionEventDelete", has_event);
		// RPG Maker places them on empty cells only
		set("actionPlaceTeleport", free);
		set("actionPlacePartyStart", free);
		set("actionPlaceBoatStart", free);
		set("actionPlaceShipStart", free);
		set("actionPlaceAirshipStart", free);
		set("actionRunHere", true);
		return states;
	}
	if (selectingItems())
	{
		// the selected events; Paste puts the copied ones on the cell clicked
		const bool has_events = !m_selEvents.isEmpty();
		set("actionAreaCut", has_events);
		set("actionAreaCopy", has_events);
		set("actionAreaClear", has_events);
		set("actionAreaPaste", !itemsClipboard().empty());
	}
	else
	{
		const bool has_area = m_doc->isFloating() || !m_selArea.isEmpty();
		set("actionAreaCut", has_area);
		set("actionAreaCopy", has_area);
		set("actionAreaClear", has_area);
		set("actionAreaPaste", m_state.tileClipboard().width > 0);
	}
	set("actionAreaSelectAll", true);
	return states;
}

void MapEditor::selectTile(int x, int y)
{
	if (m_cur != QPoint(x, y)) {
		m_cur = QPoint(x, y);
		emit pointerCellChanged();
	}
	if (m_state.layer() == EditorState::EVENT)
		selectCell(x, y);
	refresh();
}

void MapEditor::pickTile()
{
	// RPG Maker's eyedropper: the tile under the cursor becomes the current tile
	const short tile = m_doc->workingTile(m_state.layer(), m_cur.x(), m_cur.y());
	const int terrain = Autotile::translate(tile);
	if (terrain < 0 || Autotile::isFblock(terrain) != (m_state.layer() == EditorState::UPPER))
		return;
	m_state.setSelection({Autotile::translate(terrain, SAMPLE)}, 1, 1);
}

void MapEditor::newEvent()
{
	if (!m_doc->isFreeCell(m_last.x(), m_last.y()))
		return;
	emit newEventRequested(m_last);
}

void MapEditor::editEvent()
{
	// a start position hides the event of its cell
	if (m_doc->startAt(m_last.x(), m_last.y()) >= 0)
		return;
	if (const int id = m_doc->eventIdAt(m_last.x(), m_last.y()))
		emit editEventRequested(id);
}

void MapEditor::copyEvent()
{
	if (const auto* ev = m_doc->eventAt(m_last.x(), m_last.y())) {
		// shared: an event can be pasted on another map
		m_state.setEventClipboard(*ev);
	}
}

void MapEditor::cutEvent()
{
	copyEvent();
	deleteEvent();
}

void MapEditor::pasteEvent()
{
	if (const lcf::rpg::Event* clipboard = m_state.eventClipboard())
		m_doc->pasteEvent(*clipboard, m_last.x(), m_last.y());
}

void MapEditor::deleteEvent()
{
	m_doc->deleteEventAt(m_last.x(), m_last.y());
}

void MapEditor::placeTeleport()
{
	if (!m_doc->isFreeCell(m_last.x(), m_last.y()))
		return;
	emit placeTeleportRequested(m_last);
}

void MapEditor::placeStart(int kind)
{
	// RPG Maker: on an empty cell only, as a drop; the boat, the ship and
	// the airship follow the party
	if (!m_doc->isFreeCell(m_last.x(), m_last.y()))
		return;
	m_doc->setStartPosition(std::clamp(kind, 0, 3), m_last.x(), m_last.y());
}

void MapEditor::runHere()
{
	emit runHereRequested(m_last);
}

void MapEditor::copyArea()
{
	if (m_state.layer() == EditorState::EVENT) {
		copyItems();
		return;
	}
	if (m_doc->isFloating()) {
		m_state.setTileClipboard(m_doc->floatingTiles());
		return;
	}
	if (m_selArea.isEmpty())
		return;
	m_state.setTileClipboard(m_doc->copyArea(m_selArea));
}

void MapEditor::cutArea()
{
	if (m_state.layer() == EditorState::EVENT) {
		copyItems();
		deleteItems();
		refresh();
		return;
	}
	if (m_doc->isFloating()) {
		m_state.setTileClipboard(m_doc->floatingTiles());
		cancelFloating();
		return;
	}
	copyArea();
	m_doc->fillArea(m_state.layer(), m_selArea, brush());
}

void MapEditor::pasteArea()
{
	if (m_state.layer() == EditorState::EVENT) {
		// on the cell under the mouse, else the cell selected
		if (m_cur.x() >= 0 && m_cur.y() >= 0)
			pasteItems(m_cur.x(), m_cur.y());
		else
			pasteItems(m_last.x(), m_last.y());
		refresh();
		return;
	}
	commitFloating();
	clearSelection();
	const EditorState::TileClipboard& clipboard = m_state.tileClipboard();
	if (clipboard.width <= 0 || clipboard.height <= 0)
		return;
	// in the middle of the view, moved inside the map when it fits
	const QPointF center = m_viewCells.center();
	int x = static_cast<int>(center.x()) - clipboard.width / 2;
	int y = static_cast<int>(center.y()) - clipboard.height / 2;
	x = std::max(0, std::min(x, m_doc->map().width - clipboard.width));
	y = std::max(0, std::min(y, m_doc->map().height - clipboard.height));
	// the selection follows (onFloatingChanged)
	m_doc->startFloating(clipboard, QPoint(x, y));
	refresh();
}

void MapEditor::clearArea()
{
	if (m_state.layer() == EditorState::EVENT) {
		deleteItems();
		refresh();
		return;
	}
	if (m_doc->isFloating()) {
		cancelFloating();
		return;
	}
	m_doc->fillArea(m_state.layer(), m_selArea, brush());
}

void MapEditor::selectAll()
{
	if (m_state.layer() == EditorState::EVENT) {
		selectItems(m_doc->bounds());
		refresh();
		return;
	}
	commitFloating();
	clearSelection();
	m_selArea = m_doc->bounds();
	updateSelection();
	refresh();
}

void MapEditor::clearSelection()
{
	// the tiles dragged go back
	if (m_selMoving)
		m_doc->endMoveArea(false);
	m_selMoving = false;
	m_selDragging = false;
	// the events and the start positions selected on the event layer
	m_selEvents.clear();
	m_selStarts.clear();
	m_itemsSelecting = false;
	m_itemsMoving = false;
	m_selArea = QRect();
	setItemsOffset(QPoint());
	updateSelection();
	refresh();
}

void MapEditor::commitFloating()
{
	if (!m_doc->isFloating())
		return;
	// RPG Maker: the dotted line disappears
	clearSelection();
	m_doc->commitFloating();
	refresh();
}

void MapEditor::cancelFloating()
{
	m_doc->cancelFloating();
	m_floatDragging = false;
	clearSelection();
}

void MapEditor::undo()
{
	clearSelection();
	m_doc->undo();
}

void MapEditor::redo()
{
	clearSelection();
	m_doc->redo();
}

void MapEditor::shiftMap(int dx, int dy)
{
	commitFloating();
	clearSelection();
	m_doc->shiftMap(dx, dy);
}

void MapEditor::onLayerChanged()
{
	if (m_drawing)
		stopDrawing();
	if (m_selecting)
		stopSelecting();
	m_dragEventId = 0;
	m_dragStart = -1;
	commitFloating();
	// the tile selection ends; the selection tool selects events there
	if (m_state.layer() == EditorState::EVENT)
		clearSelection();
	updateBrush();
	refresh();
}

void MapEditor::onToolChanged()
{
	if (m_drawing)
		stopDrawing();
	if (m_selecting)
		stopSelecting();
	// RPG Maker: another tool ends the selection
	commitFloating();
	clearSelection();
	updateBrush();
	refresh();
}

void MapEditor::onFloatingChanged()
{
	emit floatingChanged();
	emit floatingRectChanged();
	if (!m_doc->isFloating()) {
		m_floatDragging = false;
		refresh();
		return;
	}
	const auto& tiles = m_doc->floatingTiles();
	m_selArea = QRect(m_doc->floatingPos(), QSize(tiles.width, tiles.height));
	updateSelection();
	refresh();
}

bool MapEditor::setPointer(int x, int y)
{
	const bool inside = m_doc->bounds().contains(x, y);
	const QPoint cell = inside ? QPoint(x, y) : QPoint(-1, -1);
	if (cell != m_cur) {
		m_cur = cell;
		emit pointerCellChanged();
	}
	return inside;
}

void MapEditor::updateBrush()
{
	const EditorState::Tool tool = m_state.tool();
	const bool shown = m_cur.x() >= 0 && m_cur.y() >= 0 && m_state.layer() != EditorState::EVENT
		&& tool != EditorState::ZOOM && tool != EditorState::SELECT;
	if (!shown) {
		setBrushRect(QRect());
		return;
	}
	// the pen draws the whole selection of the palette, cut at the edges of the map
	const int w = tool == EditorState::PENCIL ? std::min(m_state.selWidth(), m_doc->map().width - m_cur.x()) : 1;
	const int h = tool == EditorState::PENCIL ? std::min(m_state.selHeight(), m_doc->map().height - m_cur.y()) : 1;
	setBrushRect(QRect(m_cur, QSize(w, h)));
}

void MapEditor::setBrushRect(const QRect& cells)
{
	if (cells == m_brush)
		return;
	m_brush = cells;
	emit brushRectChanged();
}

MapDocument::Brush MapEditor::brush() const
{
	MapDocument::Brush brush;
	brush.width = m_state.selWidth();
	brush.height = m_state.selHeight();
	brush.tiles.clear();
	for (int y = 0; y < brush.height; y++)
		for (int x = 0; x < brush.width; x++)
			brush.tiles.push_back(m_state.selection(x, y));
	return brush;
}

void MapEditor::stopDrawing()
{
	m_cancelled = true;
	m_drawing = false;
	m_doc->cancelStroke();
}

void MapEditor::stopSelecting()
{
	// no m_cancelled: it would drop the next stroke on another layer
	m_selecting = false;
	emit selectionChanged();
}

void MapEditor::selectCell(int x, int y)
{
	m_last = QPoint(x, y);
	m_mark = m_last;
	m_selecting = true;
	emit selectionChanged();
}

void MapEditor::moveSelectedCell(int dx, int dy)
{
	int x = m_last.x() + dx, y = m_last.y() + dy;
	if (!m_selecting)
	{
		// no cell yet: the top left one shown (none before the view is)
		if (m_viewCells.isEmpty())
			return;
		x = static_cast<int>(std::ceil(m_viewCells.left()));
		y = static_cast<int>(std::ceil(m_viewCells.top()));
	}
	x = std::clamp(x, 0, m_doc->map().width - 1);
	y = std::clamp(y, 0, m_doc->map().height - 1);
	selectCell(x, y);
	// scrolled only as far as needed to show it
	emit revealRequested(QPoint(x, y));
}

bool MapEditor::selectingItems() const
{
	return m_state.layer() == EditorState::EVENT && m_state.tool() == EditorState::SELECT;
}

void MapEditor::selectItems(const QRect& cells)
{
	m_selEvents = m_doc->eventsIn(cells);
	m_selStarts = m_doc->startsIn(cells);
	m_selArea = m_selEvents.isEmpty() && m_selStarts.isEmpty() ? QRect() : cells & m_doc->bounds();
	updateSelection();
}

void MapEditor::moveItems(const QPoint& offset)
{
	if (!m_doc->moveItems(m_selEvents, m_selStarts, offset))
		return;
	m_selArea.translate(offset);
	updateSelection();
}

void MapEditor::copyItems()
{
	// the start positions are not copied: there is one of each
	std::vector<lcf::rpg::Event> events = m_doc->copyEvents(m_selEvents, m_selArea.topLeft());
	if (!events.empty())
		itemsClipboard() = std::move(events);
}

void MapEditor::deleteItems()
{
	if (m_selEvents.isEmpty())
		return;
	m_doc->deleteEvents(m_selEvents);
	// the start positions stay selected
	m_selEvents.clear();
	if (m_selStarts.isEmpty())
		m_selArea = QRect();
	updateSelection();
}

void MapEditor::pasteItems(int x, int y)
{
	const auto& events = itemsClipboard();
	if (!m_doc->canPasteEvents(events, QPoint(x, y)))
		return;
	clearSelection();
	// the pasted events stay selected
	m_selEvents = m_doc->pasteEvents(events, QPoint(x, y));
	QRect area;
	for (int ev_id : m_selEvents)
		if (const auto* ev = m_doc->findEvent(ev_id))
			area |= QRect(ev->x, ev->y, 1, 1);
	m_selArea = area;
	updateSelection();
}

void MapEditor::setItemsOffset(const QPoint& offset)
{
	if (offset == m_itemsOffset)
		return;
	m_itemsOffset = offset;
	emit itemsOffsetChanged();
}

void MapEditor::updateSelection()
{
	// the dashes march around a selection only
	if (m_selArea.isEmpty())
		m_antsTimer.stop();
	else if (!m_antsTimer.isActive())
		m_antsTimer.start();
	emit selectionChanged();
}

void MapEditor::refresh()
{
	const Cursor cursor = currentCursor();
	if (cursor != m_cursor) {
		m_cursor = cursor;
		emit cursorChanged();
	}
	const bool dragging = currentlyDragging();
	if (dragging != m_dragging) {
		m_dragging = dragging;
		emit draggingChanged();
	}
}

MapEditor::Cursor MapEditor::currentCursor() const
{
	const EditorState::Tool tool = m_state.tool();
	// the event layer has the arrow, and the move cursor while an event is dragged
	if (m_itemsMoving)
		return m_itemsOffset.isNull() || canMoveItems(m_itemsOffset) ? MoveCursor : ForbiddenCursor;
	if (tool == EditorState::SELECT && m_selArea.contains(m_cur))
		return MoveCursor;
	if (m_state.layer() != EditorState::EVENT || tool == EditorState::SELECT)
		return static_cast<Cursor>(tool);
	if (m_dragEventId || m_dragStart >= 0) {
		// the drop needs an empty cell of the map, or the cell dragged from
		const bool home = m_cur == m_last;
		const bool allowed = home || (m_cur.x() >= 0 && m_cur.y() >= 0 && m_doc->isFreeCell(m_cur.x(), m_cur.y()));
		return allowed ? MoveCursor : ForbiddenCursor;
	}
	return ArrowCursor;
}

bool MapEditor::currentlyDragging() const
{
	return m_drawing || m_selDragging || m_selMoving || m_floatDragging || m_itemsSelecting || m_itemsMoving
		|| m_dragEventId || m_dragStart >= 0;
}
