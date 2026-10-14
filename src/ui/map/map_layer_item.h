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

#include <QImage>
#include <QList>
#include <QMetaObject>
#include <QPoint>
#include <QPointer>
#include <QQuickItem>
#include <QSet>
#include <array>
#include <memory>
#include <vector>
#include "model/map_document.h"
#include "render/animation_clock.h"
#include "render/chipset_tiles.h"
#include "render/tile_atlas.h"

/**
 * The tiles of a map drawn by the scene graph: the lower layer, the upper
 * one or both, at a zoom, as TilePainter draws them (without the events,
 * the start positions and the panorama, which MapEventsView and
 * MapPanoramaView draw over and under it; MapPreview.qml stacks them).
 *
 * The map is cut in chunks of ChunkSize x ChunkSize cells: only the chunks
 * in sight have nodes, and a change of cells (MapDocument::cellsChanged)
 * makes again the chunks whose tiles differ. The water and the animated
 * tiles follow an AnimationClock. Tiles are quads textured from a
 * TileAtlas, sampled without smoothing, whose textures the views of a
 * window share (MapTextureCache); the software scene graph, which has no
 * custom geometry, gets an image per chunk instead.
 *
 * In grid mode (gridTiles set) it draws a list of tile ids in rows, for
 * the palette and the tile pickers, and the document is not used.
 */
class MapLayerView : public QQuickItem
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(MapDocument* document READ document WRITE setDocument NOTIFY documentChanged)
	Q_PROPERTY(LayerSet layers READ layers WRITE setLayers NOTIFY layersChanged)
	Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
	Q_PROPERTY(bool dimmed READ dimmed WRITE setDimmed NOTIFY dimmedChanged)
	Q_PROPERTY(bool darkened READ darkened WRITE setDarkened NOTIFY darkenedChanged)
	Q_PROPERTY(int chipsetId READ chipsetId WRITE setChipsetId NOTIFY chipsetIdChanged)
	Q_PROPERTY(QList<int> gridTiles READ gridTiles WRITE setGridTiles NOTIFY gridTilesChanged)
	Q_PROPERTY(int gridColumns READ gridColumns WRITE setGridColumns NOTIFY gridColumnsChanged)
	Q_PROPERTY(int tileSize READ tileSize NOTIFY tilesChanged)
	Q_PROPERTY(qreal cellSize READ cellSize NOTIFY cellSizeChanged)
	Q_PROPERTY(QSize cells READ cells NOTIFY cellsChanged)

public:
	enum LayerSet
	{
		Lower = 1,
		Upper = 2,
		Both = Lower | Upper
	};
	Q_ENUM(LayerSet)

	/** The side of a chunk, in cells */
	static constexpr int ChunkSize = 32;
	/** The opacity of a dimmed layer: the upper layer while the lower one
	 *  is edited */
	static constexpr qreal DimmedOpacity = 0.7;

	explicit MapLayerView(QQuickItem* parent = nullptr);
	~MapLayerView() override;

	/** The map drawn: its working layers, with the stroke in progress */
	MapDocument* document() const;
	void setDocument(MapDocument* document);

	LayerSet layers() const;
	void setLayers(LayerSet layers);

	/** Item pixels per pixel of a tile: a cell is tileSize * zoom wide */
	qreal zoom() const;
	void setZoom(qreal zoom);

	/** The layers drawn faded (DimmedOpacity): the upper layer while the
	 *  lower one is edited */
	bool dimmed() const;
	void setDimmed(bool dimmed);

	/** The tiles drawn at half their brightness (darken), as RPG Maker
	 *  shows the lower layer while the upper one is edited */
	bool darkened() const;
	void setDarkened(bool darkened);
	/** An image at half its brightness: each colour channel halved, the
	 *  alpha kept (ARGB32_Premultiplied). Darkened textures draw a layer
	 *  darkened exactly, in the software scene graph too (no shader). */
	static QImage darken(const QImage& image);

	/** The chipset whose tiles are drawn, loaded by Core; -1 (the default):
	 *  the chipset of the document's map */
	int chipsetId() const;
	void setChipsetId(int id);

	/** Grid mode: these tile ids, gridColumns a row; a negative id leaves
	 *  its cell empty. Empty: the document is drawn. */
	QList<int> gridTiles() const;
	void setGridTiles(const QList<int>& tiles);
	int gridColumns() const;
	void setGridColumns(int columns);

	/** The tiles drawn, in place of those of chipsetId (tests, previews);
	 *  nullptr goes back to chipsetId */
	void setChipsetTiles(std::shared_ptr<const ChipsetTiles> tiles);
	const std::shared_ptr<const ChipsetTiles>& chipsetTiles() const;
	/** Load the tiles of chipsetId again (the database changed) */
	Q_INVOKABLE void reloadTiles();
	/** The tiles of a chipset of Core's project, the same for every view
	 *  showing it while one does (so they share its atlas and textures);
	 *  reload: read again. nullptr without a project. */
	static std::shared_ptr<const ChipsetTiles> sharedChipsetTiles(int chipset_id, bool reload = false);

	/** The clock of the animated tiles; nullptr: AnimationClock::shared() */
	void setClock(AnimationClock* clock);

	/** The pixels of a tile, 16 without tiles */
	int tileSize() const;
	qreal cellSize() const;
	/** The cells drawn: the map's, or the grid's */
	QSize cells() const;
	/** A zoom at which the pixels of the tiles stay sharp: each covers a
	 *  power of two of pixels of the screen (1/8 to 16). Nearest sampling at other zooms (1.5 pixels of the screen
	 *  a pixel) makes uneven pixels. */
	static qreal snapScale(qreal zoom, qreal device_pixel_ratio);
	/** snapScale at a pixel ratio, by default the item's window's (1
	 *  without one); QML gives Screen.devicePixelRatio to follow it */
	Q_INVOKABLE qreal snapZoom(qreal zoom, qreal device_pixel_ratio = 0) const;

	/** The cell under a point of the item (may be outside cells()) */
	Q_INVOKABLE QPoint cellAt(const QPointF& pos) const;
	Q_INVOKABLE QRectF cellRect(const QPoint& cell) const;

	/** The chunks (x, y in chunks) whose nodes were made again since the
	 *  last call, and how many chunks have nodes; for the tests */
	QList<QPoint> takeRebuiltChunks();
	int builtChunkCount() const;

signals:
	void documentChanged();
	void layersChanged();
	void zoomChanged();
	void dimmedChanged();
	void darkenedChanged();
	void chipsetIdChanged();
	void gridTilesChanged();
	void gridColumnsChanged();
	void tilesChanged();
	void cellSizeChanged();
	void cellsChanged();

protected:
	QSGNode* updatePaintNode(QSGNode* old_node, UpdatePaintNodeData* data) override;
	void itemChange(ItemChange change, const ItemChangeData& value) override;
	void geometryChange(const QRectF& new_geometry, const QRectF& old_geometry) override;

private:
	bool gridMode() const;
	/** The tile of a cell of a layer (0 lower, 1 upper; the grid is 0), -1 for none */
	short tileAt(int layer, int x, int y) const;
	/** The chunks over cells of a layer may have changed */
	void markChanged(const QRect& cells, int layer);
	/** Every chunk is compared again; reset: dropped and made again */
	void markAllChanged(bool reset);
	void loadTiles(bool reload = false);
	void updateSize();
	void connectClock();
	void onTick();
	/** The image of a sheet of the atlas in a frame, darkened or not */
	const QImage& sheetImage(TileAtlas::Sheet sheet, int frame);
	/** Draw again when the item moves in its viewport (a Flickable scrolled) */
	void watchViewport();

	QPointer<MapDocument> m_doc;
	QList<QMetaObject::Connection> m_docConnections;
	LayerSet m_layers = Both;
	qreal m_zoom = 1;
	bool m_dimmed = false;
	bool m_darkened = false;
	int m_chipsetId = -1;
	QList<int> m_gridTiles;
	int m_gridColumns = 6;
	std::shared_ptr<const ChipsetTiles> m_tiles;
	bool m_explicitTiles = false;
	std::shared_ptr<const TileAtlas> m_atlas;
	// the images of m_darkAtlas darkened, by sheet and frame, made once
	// darkened
	std::weak_ptr<const TileAtlas> m_darkAtlas;
	std::array<std::vector<QImage>, TileAtlas::SheetCount> m_darkImages;
	QSize m_cells;

	QPointer<AnimationClock> m_clock;
	QMetaObject::Connection m_clockConnection;
	int m_waterFrame = 0;
	int m_tileFrame = 0;

	// Read and cleared by updatePaintNode (the GUI thread waits meanwhile):
	// the chunks to compare, by layer, and whether every chunk must be
	// dropped
	QSet<int> m_changed[2];
	bool m_compareAll = true;
	bool m_reset = true;
	// Set by updatePaintNode: what the nodes show
	bool m_showsWater = false;
	bool m_showsAnimated = false;
	QList<QPoint> m_rebuilt;
	int m_builtChunks = 0;

	QList<QMetaObject::Connection> m_viewportConnections;
};
