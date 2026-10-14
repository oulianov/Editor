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

#include <QColor>
#include <QList>
#include <QMetaObject>
#include <QPointer>
#include <QQuickItem>
#include <QRect>
#include <vector>
#include "map_editor.h"

/**
 * What the map view draws over a map: the frame of the cells the tool draws
 * on (black and white), the selection of the selection tool with its
 * marching ants, the selected cell and the cells of the selected items
 * (yellow), the dashed boundaries of an area (magenta) and the grid of the
 * event layer. What to draw comes from its MapEditor; the lines are whole
 * pixels of the screen at every zoom.
 *
 * Each line or dash is a rectangle node, which both scene graphs draw, and
 * only the part in sight has nodes: a large map or selection costs no more
 * than a small one. It also has the editor's cursor, over the map.
 *
 * Its size is the map's (MapLayerView's); MapCanvas.qml puts it over
 * MapPreview.qml.
 */
class MapOverlay : public QQuickItem
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(MapEditor* editor READ editor WRITE setEditor NOTIFY editorChanged)
	Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
	Q_PROPERTY(qreal cellSize READ cellSize WRITE setCellSize NOTIFY cellSizeChanged)
	Q_PROPERTY(bool grid READ grid WRITE setGrid NOTIFY gridChanged)
	Q_PROPERTY(QRect area READ area WRITE setArea NOTIFY areaChanged)
	Q_PROPERTY(int rectangleCount READ rectangleCount NOTIFY drawn)

public:
	explicit MapOverlay(QQuickItem* parent = nullptr);
	~MapOverlay() override;

	MapEditor* editor() const;
	void setEditor(MapEditor* editor);

	/** Item pixels per pixel of a tile, as MapLayerView's */
	qreal zoom() const;
	void setZoom(qreal zoom);
	/** The pixels of a cell (MapLayerView::cellSize) */
	qreal cellSize() const;
	void setCellSize(qreal size);

	/** The grid of the event layer */
	bool grid() const;
	void setGrid(bool shown);

	/** The boundaries of an area of the map, in cells (QmlShell::showAreaRequested); empty: none */
	QRect area() const;
	void setArea(const QRect& cells);

	/** The rectangles drawn (tests: only the part in sight has them) */
	int rectangleCount() const;

signals:
	void editorChanged();
	void zoomChanged();
	void cellSizeChanged();
	void gridChanged();
	void areaChanged();
	void drawn();

protected:
	QSGNode* updatePaintNode(QSGNode* old_node, UpdatePaintNodeData* data) override;
	void updatePolish() override;
	void itemChange(ItemChange change, const ItemChangeData& value) override;
	void geometryChange(const QRectF& new_geometry, const QRectF& old_geometry) override;

private:
	struct Rectangle
	{
		QRectF rect;
		QColor color;
	};

	/** Something drawn changed: laid out again before the next frame */
	void changed();
	void applyCursor();
	/** Draw again when the item moves in its viewport (a Flickable scrolled) */
	void watchViewport();

	QPointer<MapEditor> m_editor;
	QList<QMetaObject::Connection> m_editorConnections;
	qreal m_zoom = 1;
	qreal m_cellSize = 16;
	bool m_grid = false;
	QRect m_area;
	// laid out by updatePolish, read by updatePaintNode (the GUI thread waits)
	std::vector<Rectangle> m_rectangles;
	QList<QMetaObject::Connection> m_viewportConnections;
};
