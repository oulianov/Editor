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

#include "map_layer_item.h"
#include <QMatrix4x4>
#include <QPainter>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGImageNode>
#include <QSGOpacityNode>
#include <QSGRendererInterface>
#include <QSGTexture>
#include <QSGTextureMaterial>
#include <QSGTransformNode>
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <utility>
#include "core.h"
#include "map_texture_cache.h"
#include "model/project.h"

namespace {

// Core's, when there are no tiles to take it from
constexpr int DefaultTileSize = 16;
// Past this many moving cells in sight, too small to see them move, the
// software scene graph, which paints its chunks again for each frame, stops
// animating them
constexpr size_t MaxMovingCells = 40000;

constexpr int CellsPerChunk = MapLayerView::ChunkSize * MapLayerView::ChunkSize;

// The nodes of a chunk of a layer, and what they show
struct Chunk
{
	QSGNode* node = nullptr;
	// the tiles drawn, row by row, -1 past the edge of the map
	std::vector<short> ids;
	// the scene graph: the quads of each sheet of the atlas
	std::array<QSGGeometryNode*, TileAtlas::SheetCount> sheets{};
	// the software one: the chunk painted, and the cells that move in it
	QSGImageNode* image = nullptr;
	QImage pixels;
	std::vector<int> moving;
	bool water = false;
	bool animated = false;
	int waterFrame = 0;
	int tileFrame = 0;
};

// The node of the view: the zoom, and a node for each layer holding the
// chunks in sight. It holds the textures of the atlas, shared with the
// other views of the window (MapTextureCache), released with it on the
// thread of the scene graph.
class MapLayerNode : public QSGTransformNode
{
public:
	MapLayerNode()
	{
		for (QSGOpacityNode*& layer : layers) {
			layer = new QSGOpacityNode();
			appendChildNode(layer);
		}
	}

	void clearTextures()
	{
		for (auto& frames : textures)
			frames.clear();
	}

	void resetChunks(const QSize& map_cells)
	{
		cells = map_cells;
		columns = (cells.width() + MapLayerView::ChunkSize - 1) / MapLayerView::ChunkSize;
		rows = (cells.height() + MapLayerView::ChunkSize - 1) / MapLayerView::ChunkSize;
		for (auto& layer : chunks) {
			for (Chunk& chunk : layer)
				delete chunk.node;
			layer.assign(static_cast<size_t>(columns * rows), Chunk());
		}
	}

	bool software = false;
	std::shared_ptr<const TileAtlas> atlas;
	// the textures are those of the darkened images
	bool darkened = false;
	std::array<std::vector<std::shared_ptr<QSGTexture>>, TileAtlas::SheetCount> textures;
	std::array<QSGOpacityNode*, 2> layers{};
	std::array<std::vector<Chunk>, 2> chunks;
	QSize cells;
	int columns = 0;
	int rows = 0;
	// software: the pixels of a cell in the images of the chunks
	int paintCell = 0;
};

bool isMoving(TileAtlas::Sheet sheet)
{
	return sheet != TileAtlas::Still;
}

// a tile of a chunk painted in software, cell pixels a side, from the
// image of its sheet
void paintTile(QPainter& painter, const QImage& sheet, const QRect& source, int index, int cell)
{
	const QRect dest((index % MapLayerView::ChunkSize) * cell, (index / MapLayerView::ChunkSize) * cell, cell, cell);
	painter.drawImage(dest, sheet, source);
}

}

MapLayerView::MapLayerView(QQuickItem* parent) :
	QQuickItem(parent)
{
	setFlag(ItemHasContents);
	// clipRect() is then the part of the item in its Flickable (or window)
	setFlag(ItemObservesViewport);
	loadTiles();
}

MapLayerView::~MapLayerView() = default;

MapDocument* MapLayerView::document() const
{
	return m_doc;
}

void MapLayerView::setDocument(MapDocument* document)
{
	if (m_doc == document && m_docConnections.size() == (document ? 3 : 0))
		return;
	for (const auto& connection : m_docConnections)
		disconnect(connection);
	m_docConnections.clear();
	m_doc = document;
	if (m_doc) {
		m_docConnections << connect(m_doc, &MapDocument::cellsChanged, this, &MapLayerView::markChanged);
		m_docConnections << connect(m_doc, &MapDocument::mapReplaced, this, [this]() {
			// Map Properties can change the chipset and the size
			loadTiles();
			updateSize();
			markAllChanged(false);
		});
		m_docConnections << connect(m_doc, &QObject::destroyed, this, [this]() {
			setDocument(nullptr);
		});
	}
	loadTiles();
	updateSize();
	markAllChanged(true);
	emit documentChanged();
}

MapLayerView::LayerSet MapLayerView::layers() const
{
	return m_layers;
}

void MapLayerView::setLayers(LayerSet layers)
{
	if (layers == m_layers)
		return;
	m_layers = layers;
	markAllChanged(true);
	emit layersChanged();
}

qreal MapLayerView::zoom() const
{
	return m_zoom;
}

void MapLayerView::setZoom(qreal zoom)
{
	if (zoom <= 0 || qFuzzyCompare(zoom, m_zoom))
		return;
	m_zoom = zoom;
	updateSize();
	update();
	emit zoomChanged();
	emit cellSizeChanged();
}

bool MapLayerView::dimmed() const
{
	return m_dimmed;
}

void MapLayerView::setDimmed(bool dimmed)
{
	if (dimmed == m_dimmed)
		return;
	m_dimmed = dimmed;
	update();
	emit dimmedChanged();
}

bool MapLayerView::darkened() const
{
	return m_darkened;
}

void MapLayerView::setDarkened(bool darkened)
{
	if (darkened == m_darkened)
		return;
	m_darkened = darkened;
	update();
	emit darkenedChanged();
}

QImage MapLayerView::darken(const QImage& image)
{
	// premultiplied, the colour halved stays within the alpha
	QImage dark = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
	for (int y = 0; y < dark.height(); y++) {
		auto* line = reinterpret_cast<QRgb*>(dark.scanLine(y));
		for (int x = 0; x < dark.width(); x++)
			line[x] = (line[x] & 0xff000000u) | ((line[x] >> 1) & 0x007f7f7fu);
	}
	return dark;
}

const QImage& MapLayerView::sheetImage(TileAtlas::Sheet sheet, int frame)
{
	if (!m_darkened)
		return m_atlas->image(sheet, frame);
	if (m_darkAtlas.lock() != m_atlas) {
		// kept while the atlas is the same: their textures are shared
		// (MapTextureCache), and a layer is darkened again and again
		for (int s = 0; s < TileAtlas::SheetCount; s++) {
			auto& images = m_darkImages[static_cast<size_t>(s)];
			images.clear();
			for (int f = 0; f < TileAtlas::frameCount(static_cast<TileAtlas::Sheet>(s)); f++)
				images.push_back(darken(m_atlas->image(static_cast<TileAtlas::Sheet>(s), f)));
		}
		m_darkAtlas = m_atlas;
	}
	return m_darkImages[static_cast<size_t>(sheet)][static_cast<size_t>(frame)];
}

int MapLayerView::chipsetId() const
{
	return m_chipsetId;
}

void MapLayerView::setChipsetId(int id)
{
	if (id == m_chipsetId)
		return;
	m_chipsetId = id;
	loadTiles();
	emit chipsetIdChanged();
}

QList<int> MapLayerView::gridTiles() const
{
	return m_gridTiles;
}

void MapLayerView::setGridTiles(const QList<int>& tiles)
{
	if (tiles == m_gridTiles)
		return;
	m_gridTiles = tiles;
	updateSize();
	markAllChanged(true);
	emit gridTilesChanged();
}

int MapLayerView::gridColumns() const
{
	return m_gridColumns;
}

void MapLayerView::setGridColumns(int columns)
{
	columns = std::max(columns, 1);
	if (columns == m_gridColumns)
		return;
	m_gridColumns = columns;
	updateSize();
	markAllChanged(true);
	emit gridColumnsChanged();
}

void MapLayerView::setChipsetTiles(std::shared_ptr<const ChipsetTiles> tiles)
{
	m_explicitTiles = tiles != nullptr;
	m_tiles = std::move(tiles);
	loadTiles();
}

const std::shared_ptr<const ChipsetTiles>& MapLayerView::chipsetTiles() const
{
	return m_tiles;
}

void MapLayerView::reloadTiles()
{
	loadTiles(true);
}

std::shared_ptr<const ChipsetTiles> MapLayerView::sharedChipsetTiles(int chipset_id, bool reload)
{
	if (chipset_id < 0)
		return nullptr;
	const std::shared_ptr<Project>& project = core().project();
	if (!project)
		return nullptr;
	struct Loaded
	{
		std::weak_ptr<Project> project;
		std::weak_ptr<const ChipsetTiles> tiles;
	};
	// on the GUI thread, as the pixmaps of the tiles
	static std::vector<Loaded> loaded;
	std::erase_if(loaded, [](const Loaded& entry) { return entry.tiles.expired() || entry.project.expired(); });
	lcf::rpg::Chipset chipset;
	for (const auto& c : project->database().chipsets)
		if (c.ID == chipset_id)
			chipset = c;
	for (auto it = loaded.begin(); it != loaded.end(); ++it) {
		const auto tiles = it->tiles.lock();
		if (it->project.lock() != project || tiles->id() != chipset_id)
			continue;
		// the database can have changed its image
		if (!reload && tiles->chipset() == chipset && tiles->tileSize() == core().tileSize())
			return tiles;
		loaded.erase(it);
		break;
	}
	// Core's when it has them loaded
	auto tiles = core().loadChipsetTiles(chipset_id);
	loaded.push_back({project, tiles});
	return tiles;
}

void MapLayerView::setClock(AnimationClock* clock)
{
	m_clock = clock;
	connectClock();
}

int MapLayerView::tileSize() const
{
	return m_atlas && m_atlas->tileSize() > 0 ? m_atlas->tileSize() : DefaultTileSize;
}

qreal MapLayerView::cellSize() const
{
	return tileSize() * m_zoom;
}

QSize MapLayerView::cells() const
{
	return m_cells;
}

qreal MapLayerView::snapScale(qreal zoom, qreal device_pixel_ratio)
{
	// the zoom in pixels of the screen (Retina,
	// Windows scaling...) rounded to a power of two
	const qreal dpr = device_pixel_ratio > 0 ? device_pixel_ratio : 1.0;
	const qreal device = std::clamp(zoom * dpr, 1.0 / 8, 16.0);
	return std::exp2(std::round(std::log2(device))) / dpr;
}

qreal MapLayerView::snapZoom(qreal zoom, qreal device_pixel_ratio) const
{
	if (device_pixel_ratio <= 0)
		device_pixel_ratio = window() ? window()->effectiveDevicePixelRatio() : 1.0;
	return snapScale(zoom, device_pixel_ratio);
}

QPoint MapLayerView::cellAt(const QPointF& pos) const
{
	return QPoint(static_cast<int>(std::floor(pos.x() / cellSize())), static_cast<int>(std::floor(pos.y() / cellSize())));
}

QRectF MapLayerView::cellRect(const QPoint& cell) const
{
	return QRectF(cell.x() * cellSize(), cell.y() * cellSize(), cellSize(), cellSize());
}

QList<QPoint> MapLayerView::takeRebuiltChunks()
{
	return std::exchange(m_rebuilt, {});
}

int MapLayerView::builtChunkCount() const
{
	return m_builtChunks;
}

bool MapLayerView::gridMode() const
{
	return !m_gridTiles.isEmpty();
}

short MapLayerView::tileAt(int layer, int x, int y) const
{
	if (gridMode()) {
		const qsizetype i = static_cast<qsizetype>(y) * m_gridColumns + x;
		const int id = i < m_gridTiles.size() ? m_gridTiles[i] : -1;
		return id < 0 || id > SHRT_MAX ? short(-1) : static_cast<short>(id);
	}
	if (!m_doc || !m_doc->hasMap())
		return -1;
	const auto& tiles = m_doc->workingLayer(layer == 0 ? EditorState::LOWER : EditorState::UPPER);
	const size_t i = static_cast<size_t>(y) * static_cast<size_t>(m_cells.width()) + static_cast<size_t>(x);
	return i < tiles.size() ? tiles[i] : short(-1);
}

void MapLayerView::markChanged(const QRect& cells, int layer)
{
	if (gridMode() || layer < 0 || layer > 1)
		return;
	const QRect in_map = cells & QRect(QPoint(0, 0), m_cells);
	if (in_map.isEmpty())
		return;
	const int columns = (m_cells.width() + ChunkSize - 1) / ChunkSize;
	for (int cy = in_map.top() / ChunkSize; cy <= in_map.bottom() / ChunkSize; cy++)
		for (int cx = in_map.left() / ChunkSize; cx <= in_map.right() / ChunkSize; cx++)
			m_changed[layer].insert(cx + cy * columns);
	update();
}

void MapLayerView::markAllChanged(bool reset)
{
	m_compareAll = true;
	m_reset = m_reset || reset;
	update();
}

void MapLayerView::loadTiles(bool reload)
{
	// no chipset: no tiles, never a null pointer
	static const auto none = std::make_shared<const ChipsetTiles>();
	if (!m_explicitTiles) {
		const int id = m_chipsetId >= 0 ? m_chipsetId : m_doc && m_doc->hasMap() ? m_doc->map().chipset_id : -1;
		m_tiles = sharedChipsetTiles(id, reload);
	}
	if (!m_tiles)
		m_tiles = none;
	if (m_atlas && m_atlas->tiles() == m_tiles)
		return;
	const int old_size = tileSize();
	m_atlas = TileAtlas::forTiles(m_tiles);
	markAllChanged(true);
	// the water of another chipset can move at another speed
	onTick();
	emit tilesChanged();
	if (tileSize() != old_size) {
		updateSize();
		emit cellSizeChanged();
	}
}

void MapLayerView::updateSize()
{
	QSize cells;
	if (gridMode())
		cells = QSize(m_gridColumns, static_cast<int>((m_gridTiles.size() + m_gridColumns - 1) / m_gridColumns));
	else if (m_doc && m_doc->hasMap())
		cells = QSize(m_doc->map().width, m_doc->map().height);
	if (cells != m_cells) {
		m_cells = cells;
		markAllChanged(true);
		emit cellsChanged();
	}
	setImplicitSize(m_cells.width() * cellSize(), m_cells.height() * cellSize());
}

void MapLayerView::connectClock()
{
	disconnect(m_clockConnection);
	// the shared clock once shown: a test sets its own before
	if (!m_clock && window())
		m_clock = &AnimationClock::shared();
	if (!m_clock)
		return;
	m_clockConnection = connect(m_clock, &AnimationClock::ticked, this, &MapLayerView::onTick);
	onTick();
}

void MapLayerView::onTick()
{
	if (!m_clock || !m_tiles)
		return;
	const int water = m_clock->waterFrame(m_tiles->chipset());
	const int tiles = m_clock->animatedTileFrame();
	if (water == m_waterFrame && tiles == m_tileFrame)
		return;
	const bool shown = (water != m_waterFrame && m_showsWater) || (tiles != m_tileFrame && m_showsAnimated);
	m_waterFrame = water;
	m_tileFrame = tiles;
	if (shown)
		update();
}

void MapLayerView::watchViewport()
{
	for (const auto& connection : m_viewportConnections)
		disconnect(connection);
	m_viewportConnections.clear();
	// a parent moving moves the part in sight, and the item on the pixels
	// of the screen
	for (QQuickItem* item = parentItem(); item; item = item->parentItem())
		for (auto signal : {&QQuickItem::xChanged, &QQuickItem::yChanged, &QQuickItem::widthChanged, &QQuickItem::heightChanged})
			m_viewportConnections << connect(item, signal, this, &QQuickItem::update);
}

void MapLayerView::itemChange(ItemChange change, const ItemChangeData& value)
{
	if (change == ItemSceneChange && value.window) {
		connectClock();
		watchViewport();
	} else if (change == ItemParentHasChanged) {
		watchViewport();
	}
	QQuickItem::itemChange(change, value);
}

void MapLayerView::geometryChange(const QRectF& new_geometry, const QRectF& old_geometry)
{
	QQuickItem::geometryChange(new_geometry, old_geometry);
	update();
}

QSGNode* MapLayerView::updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*)
{
	auto* root = static_cast<MapLayerNode*>(old_node);
	if (!m_atlas || m_atlas->tileSize() <= 0 || m_cells.isEmpty()) {
		delete root;
		m_builtChunks = 0;
		m_showsWater = false;
		m_showsAnimated = false;
		m_changed[0].clear();
		m_changed[1].clear();
		m_compareAll = true;
		return nullptr;
	}
	if (!root) {
		root = new MapLayerNode();
		// no custom geometry there: an image per chunk
		root->software = !QSGRendererInterface::isApiRhiBased(window()->rendererInterface()->graphicsApi());
		m_reset = true;
	}
	const TileAtlas& atlas = *m_atlas;
	const int tile_size = atlas.tileSize();
	const qreal dpr = window()->effectiveDevicePixelRatio();

	// the textures of the atlas, one per sheet and frame; darkened, the
	// chunks are made again with others (or painted again in software)
	if (root->atlas != m_atlas || root->darkened != m_darkened) {
		root->clearTextures();
		root->atlas = m_atlas;
		root->darkened = m_darkened;
		if (!root->software)
			for (int s = 0; s < TileAtlas::SheetCount; s++)
				for (int frame = 0; frame < TileAtlas::frameCount(static_cast<TileAtlas::Sheet>(s)); frame++) {
					root->textures[static_cast<size_t>(s)].push_back(
						MapTextureCache::texture(window(), sheetImage(static_cast<TileAtlas::Sheet>(s), frame)));
				}
		m_reset = true;
	}
	auto paint = [&](QPainter& painter, short id, int index) {
		const TileAtlas::Sheet sheet = TileAtlas::sheet(id);
		paintTile(painter, sheetImage(sheet, TileAtlas::frame(sheet, m_waterFrame, m_tileFrame)), atlas.rect(id), index, root->paintCell);
	};
	if (root->software) {
		// painted at the pixels shown, a power of two below the tile's: a
		// map seen whole needs no more
		int cell = tile_size;
		const qreal shown = tile_size * m_zoom * dpr;
		while (cell % 2 == 0 && cell / 2 >= shown)
			cell /= 2;
		if (cell != root->paintCell) {
			root->paintCell = cell;
			m_reset = true;
		}
	}
	if (m_reset || root->cells != m_cells) {
		root->resetChunks(m_cells);
		m_reset = false;
		m_compareAll = true;
	}

	// the zoom, the top-left corner of the map on a pixel of the screen
	const QPointF origin = mapToScene(QPointF(0, 0));
	QMatrix4x4 matrix;
	matrix.translate(static_cast<float>(std::round(origin.x() * dpr) / dpr - origin.x()),
		static_cast<float>(std::round(origin.y() * dpr) / dpr - origin.y()));
	matrix.scale(static_cast<float>(m_zoom));
	if (matrix != root->matrix())
		root->setMatrix(matrix);
	for (QSGOpacityNode* layer : root->layers)
		layer->setOpacity(m_dimmed ? DimmedOpacity : 1.0);

	// the chunks in sight; those next to them are kept for a scroll back
	const QRectF visible = clipRect();
	const qreal chunk_pixels = ChunkSize * cellSize();
	const bool any = visible.width() > 0 && visible.height() > 0;
	const int cx1 = any ? std::max(0, static_cast<int>(std::floor(visible.left() / chunk_pixels))) : 0;
	const int cy1 = any ? std::max(0, static_cast<int>(std::floor(visible.top() / chunk_pixels))) : 0;
	const int cx2 = any ? std::min(root->columns - 1, static_cast<int>(std::ceil(visible.right() / chunk_pixels)) - 1) : -1;
	const int cy2 = any ? std::min(root->rows - 1, static_cast<int>(std::ceil(visible.bottom() / chunk_pixels)) - 1) : -1;

	auto chunkIds = [&](int layer, int cx, int cy) {
		std::vector<short> ids(CellsPerChunk, short(-1));
		const int w = std::min(ChunkSize, m_cells.width() - cx * ChunkSize);
		const int h = std::min(ChunkSize, m_cells.height() - cy * ChunkSize);
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++)
				ids[static_cast<size_t>(y * ChunkSize + x)] = tileAt(layer, cx * ChunkSize + x, cy * ChunkSize + y);
		return ids;
	};

	auto build = [&](Chunk& chunk, int layer, int cx, int cy) {
		// a chunk joins the tree complete: the software renderer places the
		// nodes as they are added, from the state of their parent
		const bool added_chunk = !chunk.node;
		if (added_chunk)
			chunk.node = new QSGNode();
		chunk.water = false;
		chunk.animated = false;
		chunk.moving.clear();
		chunk.waterFrame = m_waterFrame;
		chunk.tileFrame = m_tileFrame;
		for (int i = 0; i < CellsPerChunk; i++) {
			const short id = chunk.ids[static_cast<size_t>(i)];
			if (atlas.isEmpty(id))
				continue;
			const TileAtlas::Sheet sheet = TileAtlas::sheet(id);
			chunk.water = chunk.water || sheet == TileAtlas::Water;
			chunk.animated = chunk.animated || sheet == TileAtlas::Animated;
			if (isMoving(sheet))
				chunk.moving.push_back(i);
		}
		const QPoint first(cx * ChunkSize * tile_size, cy * ChunkSize * tile_size);

		if (root->software) {
			const int cell = root->paintCell;
			const int w = std::min(ChunkSize, m_cells.width() - cx * ChunkSize);
			const int h = std::min(ChunkSize, m_cells.height() - cy * ChunkSize);
			chunk.pixels = QImage(w * cell, h * cell, QImage::Format_ARGB32_Premultiplied);
			chunk.pixels.fill(Qt::transparent);
			QPainter painter(&chunk.pixels);
			for (int i = 0; i < CellsPerChunk; i++) {
				const short id = chunk.ids[static_cast<size_t>(i)];
				if (!atlas.isEmpty(id))
					paint(painter, id, i);
			}
			painter.end();
			const bool added = !chunk.image;
			if (added) {
				chunk.image = window()->createImageNode();
				chunk.image->setFiltering(QSGTexture::Nearest);
				chunk.image->setOwnsTexture(true);
			}
			chunk.image->setTexture(window()->createTextureFromImage(chunk.pixels));
			chunk.image->setRect(QRectF(first, QSizeF(w * tile_size, h * tile_size)));
			chunk.image->setSourceRect(QRectF(chunk.pixels.rect()));
			if (added)
				chunk.node->appendChildNode(chunk.image);
		} else {
			// a quad per tile, in the sheet of its tile
			std::array<int, TileAtlas::SheetCount> quads{};
			for (short id : chunk.ids)
				if (!atlas.isEmpty(id))
					quads[TileAtlas::sheet(id)]++;
			for (int s = 0; s < TileAtlas::SheetCount; s++) {
				const auto sheet = static_cast<TileAtlas::Sheet>(s);
				QSGGeometryNode*& node = chunk.sheets[static_cast<size_t>(s)];
				const int count = quads[static_cast<size_t>(s)];
				if (count == 0) {
					delete node;
					node = nullptr;
					continue;
				}
				const bool added = !node;
				if (added) {
					node = new QSGGeometryNode();
					auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0, 0, QSGGeometry::UnsignedShortType);
					geometry->setDrawingMode(QSGGeometry::DrawTriangles);
					node->setGeometry(geometry);
					node->setFlag(QSGNode::OwnsGeometry);
					auto* material = new QSGTextureMaterial();
					// pixel art: no smoothing, no mipmaps
					material->setFiltering(QSGTexture::Nearest);
					material->setMipmapFiltering(QSGTexture::None);
					node->setMaterial(material);
					node->setFlag(QSGNode::OwnsMaterial);
				}
				QSGGeometry* geometry = node->geometry();
				geometry->allocate(count * 4, count * 6);
				QSGGeometry::TexturedPoint2D* vertices = geometry->vertexDataAsTexturedPoint2D();
				quint16* indices = geometry->indexDataAsUShort();
				int quad = 0;
				for (int i = 0; i < CellsPerChunk; i++) {
					const short id = chunk.ids[static_cast<size_t>(i)];
					if (atlas.isEmpty(id) || TileAtlas::sheet(id) != sheet)
						continue;
					const float x = static_cast<float>(first.x() + (i % ChunkSize) * tile_size);
					const float y = static_cast<float>(first.y() + (i / ChunkSize) * tile_size);
					const float size = static_cast<float>(tile_size);
					const QRectF t = atlas.textureRect(id);
					QSGGeometry::TexturedPoint2D* v = vertices + quad * 4;
					v[0].set(x, y, static_cast<float>(t.left()), static_cast<float>(t.top()));
					v[1].set(x + size, y, static_cast<float>(t.right()), static_cast<float>(t.top()));
					v[2].set(x, y + size, static_cast<float>(t.left()), static_cast<float>(t.bottom()));
					v[3].set(x + size, y + size, static_cast<float>(t.right()), static_cast<float>(t.bottom()));
					quint16* index = indices + quad * 6;
					const auto base = static_cast<quint16>(quad * 4);
					index[0] = base;
					index[1] = static_cast<quint16>(base + 1);
					index[2] = static_cast<quint16>(base + 2);
					index[3] = static_cast<quint16>(base + 2);
					index[4] = static_cast<quint16>(base + 1);
					index[5] = static_cast<quint16>(base + 3);
					quad++;
				}
				auto* material = static_cast<QSGTextureMaterial*>(node->material());
				material->setTexture(root->textures[static_cast<size_t>(s)][static_cast<size_t>(TileAtlas::frame(sheet, m_waterFrame, m_tileFrame))].get());
				if (added)
					chunk.node->appendChildNode(node);
				else
					node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
			}
		}
		if (added_chunk)
			root->layers[static_cast<size_t>(layer)]->appendChildNode(chunk.node);
	};

	// the layers drawn: the grid has one
	std::array<bool, 2> drawn = {gridMode() || (m_layers & Lower), !gridMode() && (m_layers & Upper)};
	m_builtChunks = 0;
	size_t moving_cells = 0;
	for (int layer = 0; layer < 2; layer++) {
		auto& chunks = root->chunks[static_cast<size_t>(layer)];
		for (int i = 0; i < static_cast<int>(chunks.size()); i++) {
			Chunk& chunk = chunks[static_cast<size_t>(i)];
			const int cx = i % root->columns;
			const int cy = i / root->columns;
			const bool in_sight = drawn[static_cast<size_t>(layer)] && cx >= cx1 && cx <= cx2 && cy >= cy1 && cy <= cy2;
			const bool near = drawn[static_cast<size_t>(layer)] && cx >= cx1 - 1 && cx <= cx2 + 1 && cy >= cy1 - 1 && cy <= cy2 + 1;
			if (chunk.node && !near) {
				delete chunk.node;
				chunk = Chunk();
				continue;
			}
			bool make = false;
			if (!chunk.node && in_sight) {
				chunk.ids = chunkIds(layer, cx, cy);
				make = true;
			} else if (chunk.node && (m_compareAll || m_changed[layer].contains(i))) {
				// a change can leave the tiles of a chunk as they were (a
				// stroke committed, cells next to it)
				std::vector<short> ids = chunkIds(layer, cx, cy);
				make = ids != chunk.ids;
				if (make)
					chunk.ids = std::move(ids);
			}
			if (make) {
				build(chunk, layer, cx, cy);
				m_rebuilt.append(QPoint(cx, cy));
			}
			if (chunk.node) {
				m_builtChunks++;
				moving_cells += chunk.moving.size();
			}
		}
		m_changed[layer].clear();
	}
	m_compareAll = false;

	// the frames of the water and of the animated tiles
	m_showsWater = false;
	m_showsAnimated = false;
	for (auto& chunks : root->chunks)
		for (Chunk& chunk : chunks) {
			if (!chunk.node)
				continue;
			m_showsWater = m_showsWater || chunk.water;
			m_showsAnimated = m_showsAnimated || chunk.animated;
			const bool water = chunk.water && chunk.waterFrame != m_waterFrame;
			const bool tiles = chunk.animated && chunk.tileFrame != m_tileFrame;
			if (!water && !tiles)
				continue;
			if (root->software) {
				if (moving_cells > MaxMovingCells)
					continue;
				// only the cells that move, each painted whole again
				QPainter painter(&chunk.pixels);
				painter.setCompositionMode(QPainter::CompositionMode_Source);
				for (int i : chunk.moving)
					paint(painter, chunk.ids[static_cast<size_t>(i)], i);
				painter.end();
				chunk.image->setTexture(window()->createTextureFromImage(chunk.pixels));
			} else {
				// the same quads on the image of another frame
				for (int s = TileAtlas::Water; s <= TileAtlas::Animated; s++) {
					QSGGeometryNode* node = chunk.sheets[static_cast<size_t>(s)];
					if (!node)
						continue;
					const int frame = TileAtlas::frame(static_cast<TileAtlas::Sheet>(s), m_waterFrame, m_tileFrame);
					static_cast<QSGTextureMaterial*>(node->material())->setTexture(root->textures[static_cast<size_t>(s)][static_cast<size_t>(frame)].get());
					node->markDirty(QSGNode::DirtyMaterial);
				}
			}
			chunk.waterFrame = m_waterFrame;
			chunk.tileFrame = m_tileFrame;
		}
	return root;
}
