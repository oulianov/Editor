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

#include "map_overlay_item.h"
#include <QQuickWindow>
#include <QSGRectangleNode>
#include <QSGRendererInterface>
#include <QSGTransformNode>
#include <algorithm>
#include <cmath>
#include <utility>
#include "map_cursors.h"

namespace {

// Rectangles in pixels of the screen, from the top left corner of the item;
// only their part in sight is kept
class Layout
{
public:
	explicit Layout(const QRect& visible) : m_visible(visible) {}

	std::vector<std::pair<QRect, QColor>> rects;

	void fill(const QRect& rect, const QColor& color)
	{
		const QRect shown = rect & m_visible;
		if (!shown.isEmpty())
			rects.emplace_back(shown, color);
	}

	/** A frame inside outer, thickness pixels thick */
	void frame(const QRect& outer, int thickness, const QColor& color)
	{
		if (outer.width() <= 2 * thickness || outer.height() <= 2 * thickness) {
			fill(outer, color);
			return;
		}
		fill(QRect(outer.left(), outer.top(), outer.width(), thickness), color);
		fill(QRect(outer.left(), outer.bottom() - thickness + 1, outer.width(), thickness), color);
		fill(QRect(outer.left(), outer.top() + thickness, thickness, outer.height() - 2 * thickness), color);
		fill(QRect(outer.right() - thickness + 1, outer.top() + thickness, thickness, outer.height() - 2 * thickness), color);
	}

	/**
	 * The dashes of a frame inside outer, thickness pixels thick, as QPainter
	 * dashes a rectangle: along its middle line clockwise from the top left
	 * corner, with flat ends and mitered corners; offset pixels forward
	 */
	void dashes(const QRect& outer, int thickness, int dash, int gap, int offset, const QColor& color)
	{
		const int left = outer.left();
		const int top = outer.top();
		const int right = outer.left() + outer.width();
		const int bottom = outer.top() + outer.height();
		// the sides of the middle line
		const double width = right - left - thickness;
		const double height = bottom - top - thickness;
		if (width < 0 || height < 0 || dash <= 0)
			return;
		const double half = thickness / 2.0;
		const int period = dash + gap;
		auto dashed = [&](double distance) {
			double phase = std::fmod(distance - offset, period);
			if (phase < 0)
				phase += period;
			return phase < dash;
		};
		// the runs of dashed pixels from first to last along a side
		auto runs = [&](int first, int last, auto distance, auto piece) {
			int start = -1;
			for (int i = first; i <= last + 1; i++) {
				const bool on = i <= last && dashed(distance(i));
				if (on && start < 0) {
					start = i;
				} else if (!on && start >= 0) {
					fill(piece(start, i - start), color);
					start = -1;
				}
			}
		};
		const int x1 = std::max(left, m_visible.left());
		const int x2 = std::min(right - 1, m_visible.right());
		const int y1 = std::max(top, m_visible.top());
		const int y2 = std::min(bottom - 1, m_visible.bottom());
		auto inSight = [this](const QRect& band) { return band.intersects(m_visible); };
		if (inSight(QRect(left, top, right - left, thickness))) {
			runs(x1, x2, [&](int x) { return std::clamp(x + 0.5 - (left + half), 0.0, width); },
				[&](int start, int length) { return QRect(start, top, length, thickness); });
		}
		if (inSight(QRect(right - thickness, top, thickness, bottom - top))) {
			runs(y1, y2, [&](int y) { return width + std::clamp(y + 0.5 - (top + half), 0.0, height); },
				[&](int start, int length) { return QRect(right - thickness, start, thickness, length); });
		}
		if (inSight(QRect(left, bottom - thickness, right - left, thickness))) {
			runs(x1, x2, [&](int x) { return width + height + std::clamp(right - half - (x + 0.5), 0.0, width); },
				[&](int start, int length) { return QRect(start, bottom - thickness, length, thickness); });
		}
		if (inSight(QRect(left, top, thickness, bottom - top))) {
			runs(y1, y2, [&](int y) { return 2 * width + height + std::clamp(bottom - half - (y + 0.5), 0.0, height); },
				[&](int start, int length) { return QRect(left, start, thickness, length); });
		}
	}

private:
	QRect m_visible;
};

/** The pixels from left (included) to right (excluded) */
QRect span(int left, int top, int right, int bottom)
{
	return QRect(left, top, right - left, bottom - top);
}

}

MapOverlay::MapOverlay(QQuickItem* parent) :
	QQuickItem(parent)
{
	setFlag(ItemHasContents);
	// clipRect() is then the part of the item in its Flickable
	setFlag(ItemObservesViewport);
}

MapOverlay::~MapOverlay() = default;

MapEditor* MapOverlay::editor() const
{
	return m_editor;
}

void MapOverlay::setEditor(MapEditor* editor)
{
	if (editor == m_editor)
		return;
	for (const auto& connection : m_editorConnections)
		disconnect(connection);
	m_editorConnections.clear();
	m_editor = editor;
	if (m_editor) {
		for (auto signal : {&MapEditor::brushRectChanged, &MapEditor::selectionChanged, &MapEditor::itemsOffsetChanged,
				&MapEditor::antsOffsetChanged, &MapEditor::mapChanged})
			m_editorConnections << connect(m_editor, signal, this, &MapOverlay::changed);
		m_editorConnections << connect(m_editor, &MapEditor::cursorChanged, this, &MapOverlay::applyCursor);
	}
	applyCursor();
	changed();
	emit editorChanged();
}

qreal MapOverlay::zoom() const
{
	return m_zoom;
}

void MapOverlay::setZoom(qreal zoom)
{
	if (zoom <= 0 || qFuzzyCompare(zoom, m_zoom))
		return;
	m_zoom = zoom;
	changed();
	emit zoomChanged();
}

qreal MapOverlay::cellSize() const
{
	return m_cellSize;
}

void MapOverlay::setCellSize(qreal size)
{
	if (size <= 0 || qFuzzyCompare(size, m_cellSize))
		return;
	m_cellSize = size;
	changed();
	emit cellSizeChanged();
}

bool MapOverlay::grid() const
{
	return m_grid;
}

void MapOverlay::setGrid(bool shown)
{
	if (shown == m_grid)
		return;
	m_grid = shown;
	changed();
	emit gridChanged();
}

QRect MapOverlay::area() const
{
	return m_area;
}

void MapOverlay::setArea(const QRect& cells)
{
	if (cells == m_area)
		return;
	m_area = cells;
	changed();
	emit areaChanged();
}

int MapOverlay::rectangleCount() const
{
	return static_cast<int>(m_rectangles.size());
}

void MapOverlay::changed()
{
	polish();
}

void MapOverlay::applyCursor()
{
#if QT_CONFIG(cursor)
	// over the map only
	switch (m_editor ? m_editor->cursor() : MapEditor::ArrowCursor) {
	case MapEditor::ArrowCursor:
		unsetCursor();
		break;
	case MapEditor::MoveCursor:
		setCursor(MapCursors::move());
		break;
	case MapEditor::ForbiddenCursor:
		setCursor(QCursor(Qt::ForbiddenCursor));
		break;
	default:
		// the tools' are in the order of the tools
		setCursor(MapCursors::forTool(static_cast<EditorState::Tool>(m_editor->cursor())));
		break;
	}
#endif
}

void MapOverlay::watchViewport()
{
	for (const auto& connection : m_viewportConnections)
		disconnect(connection);
	m_viewportConnections.clear();
	// a parent moving moves the part in sight, and the item on the pixels
	// of the screen
	for (QQuickItem* item = parentItem(); item; item = item->parentItem())
		for (auto signal : {&QQuickItem::xChanged, &QQuickItem::yChanged, &QQuickItem::widthChanged, &QQuickItem::heightChanged})
			m_viewportConnections << connect(item, signal, this, &MapOverlay::changed);
}

void MapOverlay::itemChange(ItemChange change, const ItemChangeData& value)
{
	if (change == ItemSceneChange && value.window) {
		watchViewport();
		changed();
	} else if (change == ItemParentHasChanged) {
		watchViewport();
	} else if (change == ItemDevicePixelRatioHasChanged) {
		changed();
	}
	QQuickItem::itemChange(change, value);
}

void MapOverlay::geometryChange(const QRectF& new_geometry, const QRectF& old_geometry)
{
	QQuickItem::geometryChange(new_geometry, old_geometry);
	changed();
}

void MapOverlay::updatePolish()
{
	m_rectangles.clear();
	const QQuickWindow* shown_in = window();
	if (!m_editor || !shown_in || m_cellSize <= 0) {
		update();
		emit drawn();
		return;
	}
	const qreal dpr = shown_in->effectiveDevicePixelRatio();
	// from the pixel of the screen nearest to the item's corner: the lines
	// stay on whole pixels wherever it is
	const QPointF origin = mapToScene(QPointF()) * dpr;
	const QPointF shift(std::round(origin.x()) - origin.x(), std::round(origin.y()) - origin.y());
	const QRectF clip = clipRect();
	// a little more: the frames of the cells at the edges reach past them
	const QRect visible = QRectF(clip.topLeft() * dpr, clip.size() * dpr).toAlignedRect().adjusted(-8, -8, 8, 8);
	Layout layout(visible);
	auto px = [dpr](qreal logical) { return static_cast<int>(std::round(logical * dpr)); };
	// the edge of a column or a row of cells
	auto edge = [&](int cell) { return px(cell * m_cellSize); };
	// a pixel of the view (two on screens of twice the resolution)
	const int pixel = std::max(1, px(1));
	const QSize map = m_editor->mapSize();

	// The grid of the event layer: lines of a pixel of the view on the first
	// pixels of the cells, the last ones past the map
	if (m_grid && !map.isEmpty()) {
		const qreal cell = m_cellSize * dpr;
		const int first_x = std::clamp(static_cast<int>(std::floor(visible.left() / cell)), 0, map.width());
		const int last_x = std::clamp(static_cast<int>(std::ceil(visible.right() / cell)), 0, map.width());
		const int first_y = std::clamp(static_cast<int>(std::floor(visible.top() / cell)), 0, map.height());
		const int last_y = std::clamp(static_cast<int>(std::ceil(visible.bottom() / cell)), 0, map.height());
		const int right = edge(map.width()) + pixel;
		const int bottom = edge(map.height()) + pixel;
		for (int x = first_x; x <= last_x; x++)
			layout.fill(QRect(edge(x), 0, pixel, bottom), Qt::black);
		for (int y = first_y; y <= last_y; y++)
			layout.fill(QRect(0, edge(y), right, pixel), Qt::black);
	}

	// The selection of the selection tool, where its items are dragged: a
	// line of 1.5 pixels of the view inside its edges (whole pixels of the
	// screen), white under black dashes marching clockwise
	if (!m_editor->selectionArea().isEmpty()) {
		const QRect cells = m_editor->selectionRect();
		const QRect line = span(edge(cells.left()), edge(cells.top()), edge(cells.right() + 1), edge(cells.bottom() + 1));
		const int width = std::max(1, static_cast<int>(std::round(1.5 * dpr)));
		layout.frame(line, width, Qt::white);
		layout.dashes(line, width, 2 * width, 2 * width, m_editor->antsOffset(), Qt::black);
	}

	// The cells the tool draws on: 3 pixels of the view in black over their
	// edges, one in white on them (on the right and at the bottom, past
	// them: where QPainter draws the lines of a rectangle)
	if (const QRect brush = m_editor->brushRect(); !brush.isEmpty()) {
		const int left = edge(brush.left());
		const int top = edge(brush.top());
		const int right = edge(brush.right() + 1);
		const int bottom = edge(brush.bottom() + 1);
		layout.frame(span(left - pixel, top - pixel, right + 2 * pixel, bottom + 2 * pixel), 3 * pixel, Qt::black);
		layout.frame(span(left, top, right + pixel, bottom + pixel), pixel, Qt::white);
	}

	// The selected cell (or the cell an event is dragged to) and the cells of
	// the selected items: yellow, 2 pixels of the tiles over their edges
	const int yellow = std::max(1, px(2 * m_zoom));
	auto cellFrame = [&](const QPoint& cell) {
		const int left = edge(cell.x()) - yellow / 2;
		const int top = edge(cell.y()) - yellow / 2;
		layout.frame(span(left, top, edge(cell.x() + 1) - yellow / 2 + yellow, edge(cell.y() + 1) - yellow / 2 + yellow),
			yellow, Qt::yellow);
	};
	if (const QPoint cell = m_editor->markedCell(); cell.x() >= 0)
		cellFrame(cell);
	for (const QPoint& cell : m_editor->itemCells())
		cellFrame(cell);

	// The boundaries of an area, over everything: magenta dashes of 2
	// pixels of the view over its edges (Qt::DashLine)
	if (!m_area.isEmpty()) {
		const int width = 2 * pixel;
		const int left = edge(m_area.left()) - width / 2;
		const int top = edge(m_area.top()) - width / 2;
		const QRect line = span(left, top, edge(m_area.right() + 1) - width / 2 + width, edge(m_area.bottom() + 1) - width / 2 + width);
		layout.dashes(line, width, 4 * width, 2 * width, 0, Qt::magenta);
	}

	// The software scene graph takes what opaque nodes cover out of what it
	// paints again: hundreds of lines (the grid, the ants) make that region
	// slower to work out than the lines to paint. Not quite opaque, they
	// are painted over the rest; the difference does not show.
	const bool software = shown_in->rendererInterface()->graphicsApi() == QSGRendererInterface::Software;
	m_rectangles.reserve(layout.rects.size());
	for (auto [rect, color] : layout.rects) {
		if (software)
			color.setAlpha(254);
		m_rectangles.push_back({QRectF((rect.x() + shift.x()) / dpr, (rect.y() + shift.y()) / dpr,
			rect.width() / dpr, rect.height() / dpr), color});
	}
	update();
	emit drawn();
}

QSGNode* MapOverlay::updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*)
{
	if (m_rectangles.empty()) {
		delete old_node;
		return nullptr;
	}
	// a transform node, not a plain one: the software scene graph places
	// the nodes added later from the state of their parent, which it keeps
	// for transform, clip and opacity nodes only
	QSGNode* root = old_node ? old_node : new QSGTransformNode();
	// the nodes of the last frame take the new rectangles, in their order
	QSGNode* child = root->firstChild();
	for (const Rectangle& rectangle : m_rectangles) {
		auto* node = static_cast<QSGRectangleNode*>(child);
		if (!node) {
			node = window()->createRectangleNode();
			root->appendChildNode(node);
		}
		if (node->rect() != rectangle.rect)
			node->setRect(rectangle.rect);
		if (node->color() != rectangle.color)
			node->setColor(rectangle.color);
		child = node->nextSibling();
	}
	while (child) {
		QSGNode* next = child->nextSibling();
		root->removeChildNode(child);
		delete child;
		child = next;
	}
	return root;
}
