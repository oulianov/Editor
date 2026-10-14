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

#include "map_events_item.h"
#include <QCoreApplication>
#include <QMatrix4x4>
#include <QPixmap>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGOpacityNode>
#include <QSGRectangleNode>
#include <QSGTexture>
#include <QSGTransformNode>
#include <algorithm>
#include <cmath>
#include "core.h"
#include "defines.h"
#include "map_layer_item.h"
#include "map_texture_cache.h"
#include "common/dbstring.h"
#include "common/filefinder.h"
#include "model/project.h"
#include "render/picture_cache.h"
#include "render/tile_painter.h"

namespace {

// Core's, when there are no tiles to take it from
constexpr int DefaultTileSize = 16;
// TilePainter's pen
const QColor FrameColor(255, 255, 0);

// The zoom (geometry in pixels of the screen), the events, the markers
// over them. It holds the textures of its images, shared with the other
// views of the window.
class EventsNode : public QSGTransformNode
{
public:
	EventsNode()
	{
		appendChildNode(events);
		appendChildNode(markers);
	}

	void clear()
	{
		for (QSGNode* layer : {static_cast<QSGNode*>(events), static_cast<QSGNode*>(markers)})
			while (QSGNode* child = layer->firstChild())
				delete child;
	}

	QSGOpacityNode* events = new QSGOpacityNode();
	QSGOpacityNode* markers = new QSGOpacityNode();
	std::vector<std::shared_ptr<QSGTexture>> textures;
	// the cells with nodes, and the size of a cell they were made for
	QRect cells;
	qreal cellPixels = 0;
};

// The picture of a charset of Core's project as Core reads it for the map:
// in the project, else in the RTP, else gray squares
std::shared_ptr<const QImage> loadCharset(const QString& name)
{
	std::shared_ptr<const QImage> charset;
	if (core().project())
		charset = PictureCache::load(core().project()->findFile(CHARSET, name, FileFinder::FileType::Image));
	if (!charset)
		charset = PictureCache::load(core().rtpPath(CHARSET, name));
	if (!charset) {
		static const auto dummy = std::make_shared<const QImage>(
			TilePainter::createDummyPixmap(288, 256).toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied));
		charset = dummy;
	}
	return charset;
}

// A marker as TilePainter draws it in a cell of pixels x pixels of the
// screen; the same picture for every view (one texture in a window)
QImage markerImage(int kind, const QString& letter, int pixels, qreal dpr)
{
	static QHash<QString, QImage> images;
	const QString key = QStringLiteral("%1 %2 %3 %4").arg(kind).arg(letter).arg(pixels).arg(dpr);
	if (const auto it = images.constFind(key); it != images.constEnd())
		return it.value();
	QPixmap pixmap(pixels, pixels);
	pixmap.setDevicePixelRatio(dpr);
	pixmap.fill(Qt::transparent);
	TilePainter painter;
	painter.begin(pixmap);
	const int cell = static_cast<int>(pixels / dpr);
	painter.renderMarker(static_cast<TilePainter::StartMarker>(kind), letter, QRect(0, 0, cell, cell));
	painter.end();
	// a few zooms are used at once
	if (images.size() > 64)
		images.clear();
	const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
	images.insert(key, image);
	return image;
}

}

MapEventsView::MapEventsView(QQuickItem* parent) :
	QQuickItem(parent)
{
	setFlag(ItemHasContents);
	// clipRect() is then the part of the item in its Flickable (or window)
	setFlag(ItemObservesViewport);
}

MapEventsView::~MapEventsView() = default;

MapDocument* MapEventsView::document() const
{
	return m_doc;
}

void MapEventsView::setDocument(MapDocument* document)
{
	if (m_doc == document && m_docConnections.size() == (document ? 4 : 0))
		return;
	for (const auto& connection : m_docConnections)
		disconnect(connection);
	m_docConnections.clear();
	m_doc = document;
	if (m_doc) {
		m_docConnections << connect(m_doc, &MapDocument::eventsChanged, this, &MapEventsView::relayout);
		m_docConnections << connect(m_doc, &MapDocument::startChanged, this, &MapEventsView::relayout);
		m_docConnections << connect(m_doc, &MapDocument::mapReplaced, this, [this]() {
			// Map Properties can change the chipset and the size
			loadTiles();
			updateSize();
			relayout();
		});
		m_docConnections << connect(m_doc, &QObject::destroyed, this, [this]() {
			setDocument(nullptr);
		});
	}
	loadTiles();
	updateSize();
	relayout();
	emit documentChanged();
}

qreal MapEventsView::zoom() const
{
	return m_zoom;
}

void MapEventsView::setZoom(qreal zoom)
{
	if (zoom <= 0 || qFuzzyCompare(zoom, m_zoom))
		return;
	m_zoom = zoom;
	updateSize();
	changed();
	emit zoomChanged();
}

EditorState::Layer MapEventsView::editedLayer() const
{
	return m_layer;
}

void MapEventsView::setEditedLayer(EditorState::Layer layer)
{
	if (layer == m_layer)
		return;
	m_layer = layer;
	update();
	emit editedLayerChanged();
}

qreal MapEventsView::markerOpacity() const
{
	return m_layer == EditorState::EVENT ? 1.0 : 0.5;
}

bool MapEventsView::dimmed() const
{
	return m_dimmed;
}

void MapEventsView::setDimmed(bool dimmed)
{
	if (dimmed == m_dimmed)
		return;
	m_dimmed = dimmed;
	update();
	emit dimmedChanged();
}

QList<int> MapEventsView::movingEvents() const
{
	return m_movingEvents;
}

void MapEventsView::setMovingEvents(const QList<int>& ids)
{
	if (ids == m_movingEvents)
		return;
	m_movingEvents = ids;
	relayout();
	emit movingChanged();
}

QList<int> MapEventsView::movingStarts() const
{
	return m_movingStarts;
}

void MapEventsView::setMovingStarts(const QList<int>& kinds)
{
	if (kinds == m_movingStarts)
		return;
	m_movingStarts = kinds;
	relayout();
	emit movingChanged();
}

QPoint MapEventsView::moveOffset() const
{
	return m_moveOffset;
}

void MapEventsView::setMoveOffset(const QPoint& offset)
{
	if (offset == m_moveOffset)
		return;
	m_moveOffset = offset;
	relayout();
	emit movingChanged();
}

void MapEventsView::setChipsetTiles(std::shared_ptr<const ChipsetTiles> tiles)
{
	m_explicitTiles = tiles != nullptr;
	m_tiles = std::move(tiles);
	loadTiles();
	relayout();
}

void MapEventsView::reload()
{
	m_charsets.clear();
	if (!m_explicitTiles && m_doc && m_doc->hasMap())
		m_tiles = MapLayerView::sharedChipsetTiles(m_doc->map().chipset_id, true);
	loadTiles();
	relayout();
}

QMap<int, QPoint> MapEventsView::eventCells() const
{
	QMap<int, QPoint> cells;
	for (const Event& event : m_events)
		cells.insert(event.id, event.cell);
	return cells;
}

QMap<int, QPoint> MapEventsView::markerCells() const
{
	QMap<int, QPoint> cells;
	for (const Marker& marker : m_markers)
		cells.insert(marker.kind, marker.cell);
	return cells;
}

int MapEventsView::builtEventCount() const
{
	return m_builtEvents;
}

void MapEventsView::relayout()
{
	m_events.clear();
	m_markers.clear();
	if (!m_doc || !m_doc->hasMap()) {
		m_charsets.clear();
		changed();
		return;
	}
	// the start positions on this map, which replace the events of their cells
	for (int kind = TilePainter::PARTY; kind <= TilePainter::AIRSHIP; kind++) {
		// the selected ones where they are dragged
		const QPoint offset = m_movingStarts.contains(kind) ? m_moveOffset : QPoint();
		if (const auto cell = m_doc->startCell(kind))
			m_markers.push_back({kind, *cell + offset, QImage()});
	}
	auto hasMarker = [this](const QPoint& cell) {
		return std::any_of(m_markers.begin(), m_markers.end(), [&](const Marker& marker) { return marker.cell == cell; });
	};
	QHash<QString, std::shared_ptr<const QImage>> charsets;
	for (const lcf::rpg::Event& source : m_doc->map().events) {
		const QPoint cell = QPoint(source.x, source.y) + (m_movingEvents.contains(source.ID) ? m_moveOffset : QPoint());
		// TilePainter draws nothing for an event without pages
		if (hasMarker(cell) || source.pages.empty())
			continue;
		Event event;
		event.id = source.ID;
		event.cell = cell;
		const lcf::rpg::EventPage& page = source.pages[0];
		if (page.character_name.empty()) {
			event.tile = static_cast<short>(page.character_index + 10000);
		} else {
			const QString name = ToQString(page.character_name);
			auto& charset = charsets[name];
			if (!charset)
				charset = m_charsets.contains(name) ? m_charsets.value(name) : loadCharset(name);
			event.charset = charset;
			// the cell of its frame, without its top 6 rows and its bottom 2
			const int index = page.character_index;
			const QPoint frame((index % 4) * 72 + page.character_pattern * 24, (index / 4) * 128 + page.character_direction * 32);
			event.source = QRect(frame + QPoint(0, 6), QSize(24, 24));
		}
		m_events.push_back(std::move(event));
	}
	// the charsets no event shows are let go
	m_charsets = std::move(charsets);
	changed();
}

void MapEventsView::paintMarkers()
{
	const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
	const int pixels = std::max(1, qRound(tileSize() * m_zoom * dpr));
	const QString letters[] = {
		//: Letter on the map marker of the party's start position
		QCoreApplication::translate("MapScene", "P"),
		//: Letter on the map marker of the boat's start position
		QCoreApplication::translate("MapScene", "B"),
		//: Letter on the map marker of the ship's start position
		QCoreApplication::translate("MapScene", "S"),
		//: Letter on the map marker of the airship's start position
		QCoreApplication::translate("MapScene", "A"),
	};
	for (Marker& marker : m_markers)
		marker.image = markerImage(marker.kind, letters[marker.kind], pixels, dpr);
}

void MapEventsView::loadTiles()
{
	if (!m_explicitTiles)
		m_tiles = m_doc && m_doc->hasMap() ? MapLayerView::sharedChipsetTiles(m_doc->map().chipset_id) : nullptr;
	m_atlas = m_tiles ? TileAtlas::forTiles(m_tiles) : nullptr;
	changed();
}

int MapEventsView::tileSize() const
{
	return m_atlas && m_atlas->tileSize() > 0 ? m_atlas->tileSize() : DefaultTileSize;
}

void MapEventsView::updateSize()
{
	const QSize cells = m_doc && m_doc->hasMap() ? QSize(m_doc->map().width, m_doc->map().height) : QSize();
	setImplicitSize(cells.width() * tileSize() * m_zoom, cells.height() * tileSize() * m_zoom);
}

void MapEventsView::changed()
{
	m_changed = true;
	// the markers are painted for the window's pixels
	polish();
	update();
}

void MapEventsView::updatePolish()
{
	paintMarkers();
}

void MapEventsView::watchViewport()
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

void MapEventsView::itemChange(ItemChange change, const ItemChangeData& value)
{
	if ((change == ItemSceneChange && value.window) || change == ItemParentHasChanged)
		watchViewport();
	if ((change == ItemSceneChange && value.window) || change == ItemDevicePixelRatioHasChanged)
		changed();
	QQuickItem::itemChange(change, value);
}

void MapEventsView::geometryChange(const QRectF& new_geometry, const QRectF& old_geometry)
{
	QQuickItem::geometryChange(new_geometry, old_geometry);
	update();
}

QSGNode* MapEventsView::updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*)
{
	auto* root = static_cast<EventsNode*>(old_node);
	if (!m_doc || !m_doc->hasMap() || (m_events.empty() && m_markers.empty())) {
		delete root;
		m_builtEvents = 0;
		return nullptr;
	}
	if (!root) {
		root = new EventsNode();
		m_changed = true;
	}
	const qreal dpr = window()->effectiveDevicePixelRatio();
	// a cell in pixels of the screen: whole ones at the zooms of snapScale
	const qreal cell = tileSize() * m_zoom * dpr;

	// the top-left corner of the map on a pixel of the screen, the nodes in
	// pixels of the screen
	const QPointF origin = mapToScene(QPointF(0, 0));
	QMatrix4x4 matrix;
	matrix.translate(static_cast<float>(std::round(origin.x() * dpr) / dpr - origin.x()),
		static_cast<float>(std::round(origin.y() * dpr) / dpr - origin.y()));
	matrix.scale(static_cast<float>(1 / dpr));
	if (matrix != root->matrix())
		root->setMatrix(matrix);
	root->events->setOpacity(m_dimmed ? DimmedOpacity : 1.0);
	root->markers->setOpacity(markerOpacity());

	// the cells in sight; those around them have nodes too, for a scroll
	const QRectF visible = clipRect();
	const qreal cell_size = tileSize() * m_zoom;
	QRect in_sight;
	if (visible.width() > 0 && visible.height() > 0)
		in_sight = QRect(QPoint(static_cast<int>(std::floor(visible.left() / cell_size)), static_cast<int>(std::floor(visible.top() / cell_size))),
			QPoint(static_cast<int>(std::ceil(visible.right() / cell_size)), static_cast<int>(std::ceil(visible.bottom() / cell_size))));
	if (!m_changed && root->cellPixels == cell && (in_sight.isEmpty() || root->cells.contains(in_sight)))
		return root;
	const QRect cells = in_sight.adjusted(-in_sight.width() / 2 - 1, -in_sight.height() / 2 - 1, in_sight.width() / 2 + 1, in_sight.height() / 2 + 1);

	root->clear();
	std::vector<std::shared_ptr<QSGTexture>> textures;
	auto textureOf = [&](const QImage& image) {
		std::shared_ptr<QSGTexture> texture = MapTextureCache::texture(window(), image);
		if (std::find(textures.begin(), textures.end(), texture) == textures.end())
			textures.push_back(texture);
		return texture.get();
	};
	auto cellRect = [&](const QPoint& at) {
		const int x1 = static_cast<int>(std::round(at.x() * cell));
		const int y1 = static_cast<int>(std::round(at.y() * cell));
		return QRect(QPoint(x1, y1), QPoint(static_cast<int>(std::round((at.x() + 1) * cell)) - 1,
			static_cast<int>(std::round((at.y() + 1) * cell)) - 1));
	};
	auto addRect = [&](QSGNode* parent, const QRect& rect) {
		QSGRectangleNode* node = window()->createRectangleNode();
		node->setColor(FrameColor);
		node->setRect(QRectF(rect));
		parent->appendChildNode(node);
	};
	auto addImage = [&](QSGNode* parent, QSGTexture* texture, const QRect& rect, const QRectF& source) {
		QSGImageNode* node = window()->createImageNode();
		node->setTexture(texture);
		node->setOwnsTexture(false);
		node->setFiltering(QSGTexture::Nearest);
		node->setRect(QRectF(rect));
		node->setSourceRect(source);
		parent->appendChildNode(node);
	};

	// TilePainter::renderEvent and renderEventFrame, a pixel of their
	// picture `unit` pixels of the screen
	const int unit = std::max(1, static_cast<int>(std::round(dpr)));
	m_builtEvents = 0;
	for (const Event& event : m_events) {
		if (!cells.contains(event.cell))
			continue;
		const QRect dest = cellRect(event.cell);
		const int inset = dest.width() / unit / 6 * unit;
		const QRect inside = dest.adjusted(inset, inset, -inset, -inset);
		const QRect frame = inside.adjusted(-unit, -unit, unit, unit);
		addRect(root->events, QRect(frame.left(), frame.top(), frame.width(), unit));
		addRect(root->events, QRect(frame.left(), frame.bottom() - unit + 1, frame.width(), unit));
		addRect(root->events, QRect(frame.left(), frame.top(), unit, frame.height()));
		addRect(root->events, QRect(frame.right() - unit + 1, frame.top(), unit, frame.height()));
		m_builtEvents++;
		if (inside.isEmpty())
			continue;
		if (event.charset) {
			// a charset smaller than its frames: the part it has
			const QRect source = event.source & event.charset->rect();
			if (source.isEmpty())
				continue;
			const qreal sx = inside.width() / qreal(event.source.width());
			const qreal sy = inside.height() / qreal(event.source.height());
			const QRect target(QPoint(inside.left() + qRound((source.left() - event.source.left()) * sx),
					inside.top() + qRound((source.top() - event.source.top()) * sy)),
				QSize(qRound(source.width() * sx), qRound(source.height() * sy)));
			addImage(root->events, textureOf(*event.charset), target, QRectF(source));
		} else if (m_atlas && !m_atlas->isEmpty(event.tile)) {
			// a tile of the upper layer: the texture of the map's tiles
			addImage(root->events, textureOf(m_atlas->image(TileAtlas::Still)), inside, QRectF(m_atlas->rect(event.tile)));
		}
	}
	for (const Marker& marker : m_markers) {
		if (!cells.contains(marker.cell) || marker.image.isNull())
			continue;
		addImage(root->markers, textureOf(marker.image), cellRect(marker.cell), QRectF(marker.image.rect()));
	}
	root->textures = std::move(textures);
	root->cells = cells;
	root->cellPixels = cell;
	m_changed = false;
	return root;
}
