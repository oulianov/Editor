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

#pragma once

#include <QAbstractScrollArea>
#include <QPixmap>
#include <QSignalBlocker>
#include <memory>
#include <vector>
#include <lcf/rpg/map.h>

class QTimer;

/**
 * Draws with the tiles of another chipset while it lives. Core holds one
 * chipset, the one of the map being edited: it is loaded back at the end and
 * the palette is never told. Keep it for the drawing only, never across an
 * event loop (exec(), timers), or the map view would draw with wrong tiles.
 */
class PreviewChipset
{
public:
	explicit PreviewChipset(int chipset_id);
	~PreviewChipset();
	PreviewChipset(const PreviewChipset&) = delete;
	PreviewChipset& operator=(const PreviewChipset&) = delete;

private:
	QSignalBlocker m_blocker;
	QPixmap m_background;
	int m_previous;
};

/**
 * A read-only view of a map (RPG Maker's TFORMMAPVIEWER): both layers and
 * the events, a cell cursor or a dragged rectangle. Click picks a cell,
 * double-click accepts, the wheel scrolls and Ctrl+wheel zooms. It can also
 * show any grid of cells cut from an image, such as the tiles of a chipset.
 */
class MapPreview : public QAbstractScrollArea
{
	Q_OBJECT

public:
	explicit MapPreview(QWidget* parent = nullptr);

	/** The map with this id; false when it cannot be read */
	bool showMap(int map_id);
	void showMap(const lcf::rpg::Map& map, int map_id = 0);
	/** A grid of cells cut from an image, such as the tiles of a chipset */
	void showImage(const QPixmap& image, int columns, int rows);
	void clear();

	int mapId() const { return m_mapId; }
	int columns() const { return m_columns; }
	int rows() const { return m_rows; }

	/** Draws the events of the map as small squares */
	void setShowEvents(bool show);
	/** The size of a cell on screen: 32 is the map at 1/1 */
	void setCellSize(int size);
	int cellSize() const { return m_cellSize; }
	/** The 1/1, 1/2 and 1/4 buttons of RPG Maker's map views, for this view */
	QWidget* createZoomButtons(QWidget* parent);

	/** Dragging selects a rectangle no bigger than max_size (an invalid size: no limit) */
	void setRectangleMode(bool enabled, const QSize& max_size = QSize());
	/** The cell picked or the rectangle selected, in cells; empty without one */
	QRect selection() const { return m_selection; }
	void setSelection(const QRect& cells);
	void setCell(int x, int y);
	/** Scrolls the cell to the middle of the view (once shown, when called before) */
	void centerOn(int x, int y);
	/** The middle of a cell in viewport coordinates, scrolled into view (scripts click there) */
	QPoint cellPosition(int x, int y);

	/** The map with this id: the map of the open tab as edited, the others as saved */
	static std::unique_ptr<lcf::rpg::Map> loadMap(int map_id);
	/** The id of the map of the open tab, 0 without one */
	static int openMapId();

signals:
	void selectionChanged(const QRect& cells);
	/** Double-click on a cell */
	void cellActivated(int x, int y);
	void cellSizeChanged(int size);

protected:
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void showEvent(QShowEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void wheelEvent(QWheelEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

private:
	QPoint cellAt(const QPoint& pos) const;
	void dragTo(const QPoint& pos);
	void autoScroll();
	void updateScrollBars();

	QPixmap m_image;
	int m_columns = 0;
	int m_rows = 0;
	int m_mapId = 0;
	std::vector<QPoint> m_events;
	bool m_showEvents = true;
	int m_cellSize = 32;
	bool m_rectangleMode = false;
	QSize m_maxSize;
	QRect m_selection;
	QPoint m_anchor;
	QPoint m_lastPos;
	bool m_dragging = false;
	/** centerOn() before the view had its size */
	QPoint m_pendingCenter{-1, -1};
	QTimer* m_scrollTimer;
};
