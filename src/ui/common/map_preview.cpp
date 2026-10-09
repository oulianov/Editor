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

#include "map_preview.h"
#include "core.h"
#include "common/dbstring.h"
#include "ui/main_window.h"
#include "ui/map/map_scene.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace {

// the map open in the main window: its unsaved changes are shown
MapScene* openScene() {
	for (QWidget* widget : QApplication::topLevelWidgets()) {
		if (auto* window = qobject_cast<MainWindow*>(widget)) {
			return window->currentScene();
		}
	}
	return nullptr;
}

}

PreviewChipset::PreviewChipset(int chipset_id) :
	m_blocker(&core()), m_background(core().background()), m_previous(core().chipsetId())
{
	if (core().project()) {
		core().LoadChipset(chipset_id);
	}
}

PreviewChipset::~PreviewChipset()
{
	// the chipset of the open map again (0 without one: the next map opened
	// loads its own and tells the palette)
	if (core().project()) {
		core().LoadChipset(m_previous);
	}
	core().setBackground(m_background);
}

MapPreview::MapPreview(QWidget* parent) :
	QAbstractScrollArea(parent),
	m_scrollTimer(new QTimer(this))
{
	setFocusPolicy(Qt::StrongFocus);
	// RPG Maker scrolls a drag past the edge every 40 ms
	m_scrollTimer->setInterval(40);
	connect(m_scrollTimer, &QTimer::timeout, this, &MapPreview::autoScroll);
}

std::unique_ptr<lcf::rpg::Map> MapPreview::loadMap(int map_id) {
	if (MapScene* scene = openScene(); scene && scene->id() == map_id) {
		return std::make_unique<lcf::rpg::Map>(scene->mapData());
	}
	if (!core().project() || map_id <= 0) {
		return nullptr;
	}
	return core().project()->loadMap(map_id);
}

int MapPreview::openMapId() {
	MapScene* scene = openScene();
	return scene ? scene->id() : 0;
}

bool MapPreview::showMap(int map_id) {
	auto map = loadMap(map_id);
	if (!map) {
		clear();
		return false;
	}
	showMap(*map, map_id);
	return true;
}

void MapPreview::showMap(const lcf::rpg::Map& map, int map_id) {
	const int width = std::max(map.width, 1);
	const int height = std::max(map.height, 1);
	// Drawn once, at most about 9 million pixels: a 500 x 500 map gets 6
	// pixels per tile, and the view scales it
	const int tile = std::clamp(static_cast<int>(std::sqrt(9e6 / (width * height))), 2, core().tileSize());
	QPixmap image(width * tile, height * tile);
	image.fill(Qt::black);
	{
		PreviewChipset chipset(map.chipset_id);
		// lower tiles are drawn over the panorama, as in the map view
		core().LoadBackground(map.parallax_flag ? ToQString(map.parallax_name) : QString());
		core().beginPainting(image);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				const size_t index = static_cast<size_t>(y * map.width + x);
				const QRect rect(x * tile, y * tile, tile, tile);
				if (index < map.lower_layer.size()) {
					core().renderTile(map.lower_layer[index], rect);
				}
				if (index < map.upper_layer.size()) {
					core().renderTile(map.upper_layer[index], rect);
				}
			}
		}
		core().endPainting();
	}
	m_events.clear();
	for (const auto& event : map.events) {
		m_events.emplace_back(event.x, event.y);
	}
	m_mapId = map_id;
	m_image = image;
	m_columns = width;
	m_rows = height;
	m_selection = m_selection.intersected(QRect(0, 0, width, height));
	updateScrollBars();
	viewport()->update();
}

void MapPreview::showImage(const QPixmap& image, int columns, int rows) {
	m_events.clear();
	m_mapId = 0;
	m_image = image;
	m_columns = std::max(columns, 1);
	m_rows = std::max(rows, 1);
	m_selection = m_selection.intersected(QRect(0, 0, m_columns, m_rows));
	updateScrollBars();
	viewport()->update();
}

void MapPreview::clear() {
	m_events.clear();
	m_mapId = 0;
	m_image = QPixmap();
	m_columns = 0;
	m_rows = 0;
	m_selection = QRect();
	updateScrollBars();
	viewport()->update();
}

void MapPreview::setShowEvents(bool show) {
	m_showEvents = show;
	viewport()->update();
}

void MapPreview::setCellSize(int size) {
	size = std::clamp(size, 4, 64);
	if (size == m_cellSize) {
		return;
	}
	// the middle of the view stays where it is
	const QPointF middle((horizontalScrollBar()->value() + viewport()->width() / 2.0) / m_cellSize,
		(verticalScrollBar()->value() + viewport()->height() / 2.0) / m_cellSize);
	m_cellSize = size;
	updateScrollBars();
	horizontalScrollBar()->setValue(static_cast<int>(middle.x() * size - viewport()->width() / 2.0));
	verticalScrollBar()->setValue(static_cast<int>(middle.y() * size - viewport()->height() / 2.0));
	viewport()->update();
	emit cellSizeChanged(size);
}

QWidget* MapPreview::createZoomButtons(QWidget* parent) {
	auto* buttons = new QWidget(parent);
	auto* layout = new QHBoxLayout(buttons);
	layout->setContentsMargins(0, 0, 0, 0);
	const struct { const char* name; const char* text; int size; } zooms[] = {
		{"zoomFull", QT_TR_NOOP("1/1"), 32},
		{"zoomHalf", QT_TR_NOOP("1/2"), 16},
		{"zoomQuarter", QT_TR_NOOP("1/4"), 8},
	};
	for (const auto& zoom : zooms) {
		auto* button = new QToolButton(buttons);
		button->setObjectName(QString::fromLatin1(zoom.name));
		button->setText(tr(zoom.text));
		button->setCheckable(true);
		button->setChecked(m_cellSize == zoom.size);
		const int size = zoom.size;
		connect(button, &QToolButton::clicked, this, [this, size] { setCellSize(size); });
		connect(this, &MapPreview::cellSizeChanged, button, [button, size](int value) { button->setChecked(value == size); });
		layout->addWidget(button);
	}
	layout->addStretch();
	return buttons;
}

void MapPreview::setRectangleMode(bool enabled, const QSize& max_size) {
	m_rectangleMode = enabled;
	m_maxSize = max_size;
}

void MapPreview::setSelection(const QRect& cells) {
	const QRect selection = cells.intersected(QRect(0, 0, m_columns, m_rows));
	if (selection == m_selection) {
		return;
	}
	m_selection = selection;
	viewport()->update();
	emit selectionChanged(m_selection);
}

void MapPreview::setCell(int x, int y) {
	setSelection(QRect(x, y, 1, 1));
}

void MapPreview::centerOn(int x, int y) {
	if (!isVisible()) {
		m_pendingCenter = QPoint(x, y);
	}
	horizontalScrollBar()->setValue(x * m_cellSize + m_cellSize / 2 - viewport()->width() / 2);
	verticalScrollBar()->setValue(y * m_cellSize + m_cellSize / 2 - viewport()->height() / 2);
}

QPoint MapPreview::cellPosition(int x, int y) {
	const QRect cell(x * m_cellSize - horizontalScrollBar()->value(),
		y * m_cellSize - verticalScrollBar()->value(), m_cellSize, m_cellSize);
	if (!viewport()->rect().contains(cell)) {
		centerOn(x, y);
	}
	return QPoint(x * m_cellSize + m_cellSize / 2 - horizontalScrollBar()->value(),
		y * m_cellSize + m_cellSize / 2 - verticalScrollBar()->value());
}

QPoint MapPreview::cellAt(const QPoint& pos) const {
	const int x = (pos.x() + horizontalScrollBar()->value()) / m_cellSize;
	const int y = (pos.y() + verticalScrollBar()->value()) / m_cellSize;
	return QPoint(std::clamp(x, 0, std::max(m_columns - 1, 0)), std::clamp(y, 0, std::max(m_rows - 1, 0)));
}

void MapPreview::updateScrollBars() {
	const QSize area = viewport()->size();
	horizontalScrollBar()->setRange(0, std::max(0, m_columns * m_cellSize - area.width()));
	verticalScrollBar()->setRange(0, std::max(0, m_rows * m_cellSize - area.height()));
	horizontalScrollBar()->setPageStep(area.width());
	verticalScrollBar()->setPageStep(area.height());
	horizontalScrollBar()->setSingleStep(m_cellSize);
	verticalScrollBar()->setSingleStep(m_cellSize);
}

void MapPreview::resizeEvent(QResizeEvent* event) {
	QAbstractScrollArea::resizeEvent(event);
	updateScrollBars();
}

void MapPreview::showEvent(QShowEvent* event) {
	QAbstractScrollArea::showEvent(event);
	updateScrollBars();
	if (m_pendingCenter.x() >= 0) {
		centerOn(m_pendingCenter.x(), m_pendingCenter.y());
		m_pendingCenter = QPoint(-1, -1);
	}
}

void MapPreview::paintEvent(QPaintEvent*) {
	QPainter painter(viewport());
	painter.fillRect(viewport()->rect(), palette().color(QPalette::Dark));
	if (m_image.isNull()) {
		return;
	}
	const QPoint origin(-horizontalScrollBar()->value(), -verticalScrollBar()->value());
	const QRect content(origin, QSize(m_columns * m_cellSize, m_rows * m_cellSize));
	const QRect visible = content.intersected(viewport()->rect());
	if (visible.isEmpty()) {
		return;
	}
	// only the cells shown: the image of a large map is big
	const int x0 = (visible.left() - origin.x()) / m_cellSize;
	const int y0 = (visible.top() - origin.y()) / m_cellSize;
	const int x1 = std::min((visible.right() - origin.x()) / m_cellSize, m_columns - 1);
	const int y1 = std::min((visible.bottom() - origin.y()) / m_cellSize, m_rows - 1);
	const QRect target(origin.x() + x0 * m_cellSize, origin.y() + y0 * m_cellSize,
		(x1 - x0 + 1) * m_cellSize, (y1 - y0 + 1) * m_cellSize);
	const double source_w = static_cast<double>(m_image.width()) / m_columns;
	const double source_h = static_cast<double>(m_image.height()) / m_rows;
	const QRectF source(x0 * source_w, y0 * source_h, (x1 - x0 + 1) * source_w, (y1 - y0 + 1) * source_h);
	// transparent tiles of a chipset show the gray squares
	painter.fillRect(target, Core::transparencyBrush());
	painter.setRenderHint(QPainter::SmoothPixmapTransform, m_cellSize < source_w);
	painter.drawPixmap(QRectF(target), m_image, source);

	auto cellRect = [&](int x, int y, int w = 1, int h = 1) {
		return QRect(origin.x() + x * m_cellSize, origin.y() + y * m_cellSize, w * m_cellSize, h * m_cellSize);
	};
	if (m_showEvents) {
		painter.setPen(QPen(Qt::white, 1));
		painter.setBrush(QColor(255, 255, 255, 96));
		const int inset = std::max(1, m_cellSize / 4);
		for (const QPoint& event : m_events) {
			if (event.x() >= x0 && event.x() <= x1 && event.y() >= y0 && event.y() <= y1) {
				painter.drawRect(cellRect(event.x(), event.y()).adjusted(inset, inset, -inset, -inset));
			}
		}
	}
	if (!m_selection.isEmpty()) {
		// black and white, visible on any tile
		const QRect rect = cellRect(m_selection.x(), m_selection.y(), m_selection.width(), m_selection.height());
		painter.setBrush(Qt::NoBrush);
		painter.setPen(QPen(Qt::black, 3));
		painter.drawRect(rect.adjusted(1, 1, -2, -2));
		painter.setPen(QPen(Qt::white, 1));
		painter.drawRect(rect.adjusted(1, 1, -2, -2));
	}
}

void MapPreview::mousePressEvent(QMouseEvent* event) {
	if (event->button() != Qt::LeftButton || m_image.isNull()) {
		QAbstractScrollArea::mousePressEvent(event);
		return;
	}
	m_anchor = cellAt(event->position().toPoint());
	m_lastPos = event->position().toPoint();
	m_dragging = true;
	setCell(m_anchor.x(), m_anchor.y());
}

void MapPreview::mouseMoveEvent(QMouseEvent* event) {
	if (!m_dragging) {
		return;
	}
	m_lastPos = event->position().toPoint();
	dragTo(m_lastPos);
	if (!viewport()->rect().contains(m_lastPos) && !m_scrollTimer->isActive()) {
		m_scrollTimer->start();
	}
}

void MapPreview::mouseReleaseEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton) {
		m_dragging = false;
		m_scrollTimer->stop();
	}
}

void MapPreview::mouseDoubleClickEvent(QMouseEvent* event) {
	if (event->button() != Qt::LeftButton || m_image.isNull()) {
		return;
	}
	const QPoint cell = cellAt(event->position().toPoint());
	if (!m_selection.contains(cell)) {
		setCell(cell.x(), cell.y());
	}
	emit cellActivated(cell.x(), cell.y());
}

void MapPreview::dragTo(const QPoint& pos) {
	QPoint cell = cellAt(pos);
	if (!m_rectangleMode) {
		setCell(cell.x(), cell.y());
		return;
	}
	if (m_maxSize.isValid()) {
		cell.setX(std::clamp(cell.x(), m_anchor.x() - m_maxSize.width() + 1, m_anchor.x() + m_maxSize.width() - 1));
		cell.setY(std::clamp(cell.y(), m_anchor.y() - m_maxSize.height() + 1, m_anchor.y() + m_maxSize.height() - 1));
	}
	setSelection(QRect(m_anchor, cell).normalized());
}

void MapPreview::autoScroll() {
	const QRect area = viewport()->rect();
	const int dx = m_lastPos.x() < area.left() ? -m_cellSize : m_lastPos.x() > area.right() ? m_cellSize : 0;
	const int dy = m_lastPos.y() < area.top() ? -m_cellSize : m_lastPos.y() > area.bottom() ? m_cellSize : 0;
	if (!m_dragging || (dx == 0 && dy == 0)) {
		m_scrollTimer->stop();
		return;
	}
	horizontalScrollBar()->setValue(horizontalScrollBar()->value() + dx);
	verticalScrollBar()->setValue(verticalScrollBar()->value() + dy);
	dragTo(m_lastPos);
}

void MapPreview::wheelEvent(QWheelEvent* event) {
	if (!(event->modifiers() & Qt::ControlModifier)) {
		QAbstractScrollArea::wheelEvent(event);
		return;
	}
	const int delta = event->angleDelta().y();
	if (delta != 0) {
		setCellSize(delta > 0 ? m_cellSize * 2 : m_cellSize / 2);
	}
	event->accept();
}

void MapPreview::keyPressEvent(QKeyEvent* event) {
	QPoint step;
	switch (event->key()) {
	case Qt::Key_Left: step = QPoint(-1, 0); break;
	case Qt::Key_Right: step = QPoint(1, 0); break;
	case Qt::Key_Up: step = QPoint(0, -1); break;
	case Qt::Key_Down: step = QPoint(0, 1); break;
	default:
		QAbstractScrollArea::keyPressEvent(event);
		return;
	}
	if (m_image.isNull()) {
		return;
	}
	// the cursor, or the rectangle with its size, moves by one cell
	QRect moved = m_selection.isEmpty() ? QRect(0, 0, 1, 1) : m_selection.translated(step);
	moved.moveLeft(std::clamp(moved.left(), 0, std::max(0, m_columns - moved.width())));
	moved.moveTop(std::clamp(moved.top(), 0, std::max(0, m_rows - moved.height())));
	setSelection(moved);
	cellPosition(moved.x(), moved.y());
}
