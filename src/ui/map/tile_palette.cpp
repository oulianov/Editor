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

#include "tile_palette.h"
#include <QPainter>
#include <algorithm>
#include <utility>
#include <vector>
#include "defines.h"
#include "model/autotile.h"
#include "render/tile_painter.h"

TilePalette::TilePalette(QObject* parent) :
	QObject(parent)
{
}

EditorState* TilePalette::state() const
{
	return m_state;
}

void TilePalette::setState(EditorState* state)
{
	if (state == m_state)
		return;
	for (const auto& connection : m_stateConnections)
		disconnect(connection);
	m_stateConnections.clear();
	m_state = state;
	if (m_state) {
		m_stateConnections << connect(m_state, &EditorState::layerChanged, this, [this]() {
			if (upper() != m_upper) {
				m_upper = upper();
				emit tilesChanged();
			}
			// each layer keeps its own tiles picked
			showState();
		});
		// the eyedropper of the map picks tiles too
		m_stateConnections << connect(m_state, &EditorState::selectionChanged, this, [this]() {
			if (!m_settingState)
				showState();
		});
	}
	m_upper = upper();
	showState();
	emit stateChanged();
	emit tilesChanged();
}

bool TilePalette::upper() const
{
	return m_state && m_state->layer() != EditorState::LOWER;
}

QList<int> TilePalette::tiles() const
{
	QList<int> tiles(Columns, -1);
	if (m_upper) {
		for (int i = 0; i < UpperTiles; i++)
			tiles.append(Autotile::translate(i + LowerTiles));
		return tiles;
	}
	for (int i = 0; i < LowerTiles; i++) {
		// deep water is shown surrounded by water
		tiles.append(i == 2 ? Autotile::translate(2, 0, 240) : Autotile::translate(i, 15));
	}
	return tiles;
}

int TilePalette::columns() const
{
	return Columns;
}

int TilePalette::rows() const
{
	return 1 + (m_upper ? UpperTiles : LowerTiles) / Columns;
}

QRect TilePalette::selection() const
{
	return m_selection;
}

bool TilePalette::isPicking() const
{
	return m_picking;
}

void TilePalette::press(int x, int y, Qt::MouseButton button)
{
	if (button == Qt::RightButton) {
		// the drag is cancelled: the tiles picked are framed again
		if (m_picking)
			m_cancelled = true;
		setSelection(m_last);
		return;
	}
	if (button != Qt::LeftButton)
		return;
	m_cancelled = false;
	m_initial = QPoint(x, y);
	m_current = m_initial;
	setPicking(true);
	setSelection(dragged());
}

void TilePalette::move(int x, int y)
{
	if (!m_picking || m_cancelled)
		return;
	// the eraser row is picked alone
	if (m_initial.y() < 1)
		return;
	// a drag from the tiles does not reach the eraser row
	m_current = QPoint(x, y < 1 ? m_current.y() : y);
	setSelection(dragged());
}

void TilePalette::release(int, int, Qt::MouseButton button)
{
	if (button != Qt::LeftButton || !m_picking)
		return;
	setPicking(false);
	if (std::exchange(m_cancelled, false))
		return;
	pick(m_selection);
}

QRect TilePalette::dragged() const
{
	if (m_initial.y() < 1)
		return QRect(0, 0, Columns, 1);
	int left = std::clamp(std::min(m_initial.x(), m_current.x()), 0, Columns - 1);
	int right = std::clamp(std::max(m_initial.x(), m_current.x()), 0, Columns - 1);
	int top = std::max(std::min(m_initial.y(), m_current.y()), 1);
	int bottom = std::min(std::max(m_initial.y(), m_current.y()), rows() - 1);
	// at most 6 rows, from the cell the drag started on
	if (bottom - top > 5) {
		if (m_initial.y() <= top)
			bottom = top + 5;
		else
			top = bottom - 5;
	}
	return QRect(QPoint(left, top), QPoint(right, bottom));
}

void TilePalette::pick(const QRect& cells)
{
	if (!m_state || cells.isEmpty())
		return;
	std::vector<short> tiles;
	int width = cells.width();
	int height = cells.height();
	if (cells.top() < 1) {
		// RPG Maker's empty tiles: water (the first lower tile, as in a new
		// map) and the first upper tile, transparent. The Player cannot draw
		// other ids (NTILE corrupted the maps)
		tiles.push_back(m_upper ? 10000 : Autotile::translate(0, SAMPLE));
		width = 1;
		height = 1;
	} else {
		for (int y = cells.top(); y <= cells.bottom(); y++)
			for (int x = cells.left(); x <= cells.right(); x++) {
				const int index = x + (y - 1) * Columns;
				tiles.push_back(Autotile::translate(m_upper ? index + LowerTiles : index, SAMPLE));
			}
	}
	m_last = cells;
	// the rectangle stays as dragged (the eraser row too)
	m_settingState = true;
	m_state->setSelection(std::move(tiles), width, height);
	m_settingState = false;
}

void TilePalette::showState()
{
	if (!m_state || m_state->layer() == EditorState::EVENT)
		return;
	// the palette index of the first tile: the terrain for lower tiles, from
	// 162 for upper tiles; the row 0 is the eraser
	int index = Autotile::translate(m_state->selection(0, 0));
	if (m_state->layer() == EditorState::UPPER)
		index -= LowerTiles;
	if (index < 0)
		return;
	m_last = QRect(index % Columns, index / Columns + 1, m_state->selWidth(), m_state->selHeight());
	setSelection(m_last);
}

void TilePalette::setSelection(const QRect& cells)
{
	if (cells == m_selection)
		return;
	m_selection = cells;
	emit selectionChanged();
}

void TilePalette::setPicking(bool picking)
{
	if (picking == m_picking)
		return;
	m_picking = picking;
	emit pickingChanged();
}

TransparencyPattern::TransparencyPattern(QQuickItem* parent) :
	QQuickPaintedItem(parent)
{
	setOpaquePainting(true);
}

int TransparencyPattern::eraserCells() const
{
	return m_eraserCells;
}

void TransparencyPattern::setEraserCells(int cells)
{
	if (cells == m_eraserCells)
		return;
	m_eraserCells = cells;
	update();
	emit eraserChanged();
}

qreal TransparencyPattern::cellSize() const
{
	return m_cellSize;
}

void TransparencyPattern::setCellSize(qreal size)
{
	if (qFuzzyCompare(size, m_cellSize))
		return;
	m_cellSize = size;
	update();
	emit eraserChanged();
}

void TransparencyPattern::paint(QPainter* painter)
{
	painter->fillRect(QRectF(0, 0, width(), height()), TilePainter::transparencyBrush());
	if (m_eraserCells <= 0 || m_cellSize <= 0)
		return;
	// TilePainter::eraserRow at the size of the cells: transparent cells
	// crossed out in red, the tile is removed
	const qreal unit = m_cellSize / 32;
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setPen(QPen(QColor(0xd0, 0x30, 0x30), 2 * unit));
	painter->setBrush(Qt::NoBrush);
	for (int i = 0; i < m_eraserCells; i++) {
		const qreal x = i * m_cellSize;
		painter->drawLine(QPointF(x + 6 * unit, 26 * unit), QPointF(x + 26 * unit, 6 * unit));
		painter->drawRect(QRectF(x + 0.5 * unit, 0.5 * unit, 31 * unit, 31 * unit));
	}
}
