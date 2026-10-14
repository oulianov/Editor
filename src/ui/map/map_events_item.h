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

#include <QHash>
#include <QImage>
#include <QList>
#include <QMap>
#include <QMetaObject>
#include <QPoint>
#include <QPointer>
#include <QQuickItem>
#include <memory>
#include <vector>
#include "model/editor_state.h"
#include "model/map_document.h"
#include "render/chipset_tiles.h"
#include "render/tile_atlas.h"

/**
 * The events of a map and the start positions on it, drawn by the scene
 * graph over its upper layer (MapLayerView) as TilePainter draws them: each
 * event the graphic of its first page (a frame of its charset, or a tile)
 * in a yellow frame; the party and the vehicles the markers of RPG Maker
 * 2003, with their letters, in place of the events of their cells.
 *
 * Every node is an image or a rectangle, which the software scene graph
 * draws too. The charsets are read once per file (PictureCache) and their
 * textures shared in the window with the tiles' (MapTextureCache); only
 * the events near the part in sight have nodes.
 */
class MapEventsView : public QQuickItem
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(MapDocument* document READ document WRITE setDocument NOTIFY documentChanged)
	Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
	Q_PROPERTY(EditorState::Layer editedLayer READ editedLayer WRITE setEditedLayer NOTIFY editedLayerChanged)
	Q_PROPERTY(qreal markerOpacity READ markerOpacity NOTIFY editedLayerChanged)
	Q_PROPERTY(bool dimmed READ dimmed WRITE setDimmed NOTIFY dimmedChanged)
	Q_PROPERTY(QList<int> movingEvents READ movingEvents WRITE setMovingEvents NOTIFY movingChanged)
	Q_PROPERTY(QList<int> movingStarts READ movingStarts WRITE setMovingStarts NOTIFY movingChanged)
	Q_PROPERTY(QPoint moveOffset READ moveOffset WRITE setMoveOffset NOTIFY movingChanged)

public:
	/** The opacity of the events while the lower layer is edited: they fade
	 *  with the upper layer (MapLayerView::DimmedOpacity) */
	static constexpr qreal DimmedOpacity = 0.7;

	explicit MapEventsView(QQuickItem* parent = nullptr);
	~MapEventsView() override;

	MapDocument* document() const;
	void setDocument(MapDocument* document);

	/** Item pixels per pixel of a tile, as MapLayerView's */
	qreal zoom() const;
	void setZoom(qreal zoom);

	/** The layer edited: RPG Maker shows the start positions at half
	 *  opacity on the tile layers */
	EditorState::Layer editedLayer() const;
	void setEditedLayer(EditorState::Layer layer);
	qreal markerOpacity() const;

	/** The events faded (DimmedOpacity), not the start positions */
	bool dimmed() const;
	void setDimmed(bool dimmed);

	/** Events (ids) and start positions (TilePainter::StartMarker) dragged
	 *  by the selection tool: drawn moveOffset cells away */
	QList<int> movingEvents() const;
	void setMovingEvents(const QList<int>& ids);
	QList<int> movingStarts() const;
	void setMovingStarts(const QList<int>& kinds);
	QPoint moveOffset() const;
	void setMoveOffset(const QPoint& offset);

	/** The tiles of the tile graphics, in place of those of the map's
	 *  chipset (tests); nullptr goes back to them */
	void setChipsetTiles(std::shared_ptr<const ChipsetTiles> tiles);
	/** Read the charsets and the chipset again (the database or a file changed) */
	Q_INVOKABLE void reload();

	/** The events drawn and their cells, by id */
	QMap<int, QPoint> eventCells() const;
	/** The start positions drawn on this map and their cells, by kind */
	QMap<int, QPoint> markerCells() const;
	/** The events with nodes (tests) */
	int builtEventCount() const;

signals:
	void documentChanged();
	void zoomChanged();
	void editedLayerChanged();
	void dimmedChanged();
	void movingChanged();

protected:
	QSGNode* updatePaintNode(QSGNode* old_node, UpdatePaintNodeData* data) override;
	void updatePolish() override;
	void itemChange(ItemChange change, const ItemChangeData& value) override;
	void geometryChange(const QRectF& new_geometry, const QRectF& old_geometry) override;

private:
	// an event as drawn: its cell and its graphic
	struct Event
	{
		int id = 0;
		QPoint cell;
		// a frame of a charset: its picture, the part drawn
		std::shared_ptr<const QImage> charset;
		QRect source;
		// else a tile of the upper layer, -1 for none
		short tile = -1;
	};
	struct Marker
	{
		int kind = 0;
		QPoint cell;
		QImage image;
	};

	/** The events and the markers again, from the document (GUI thread) */
	void relayout();
	/** The pictures of the markers at the size of a cell on the screen */
	void paintMarkers();
	void loadTiles();
	int tileSize() const;
	void updateSize();
	void changed();
	void watchViewport();

	QPointer<MapDocument> m_doc;
	QList<QMetaObject::Connection> m_docConnections;
	qreal m_zoom = 1;
	EditorState::Layer m_layer = EditorState::EVENT;
	bool m_dimmed = false;
	QList<int> m_movingEvents;
	QList<int> m_movingStarts;
	QPoint m_moveOffset;
	std::shared_ptr<const ChipsetTiles> m_tiles;
	bool m_explicitTiles = false;
	std::shared_ptr<const TileAtlas> m_atlas;
	// the charsets by name, while the events show them
	QHash<QString, std::shared_ptr<const QImage>> m_charsets;

	// Read by updatePaintNode (the GUI thread waits meanwhile)
	std::vector<Event> m_events;
	std::vector<Marker> m_markers;
	bool m_changed = true;
	// Set by updatePaintNode
	int m_builtEvents = 0;

	QList<QMetaObject::Connection> m_viewportConnections;
};
