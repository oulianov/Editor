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

#include "map_panorama_item.h"
#include <QElapsedTimer>
#include <QMatrix4x4>
#include <QQuickWindow>
#include <QSGClipNode>
#include <QSGGeometry>
#include <QSGImageNode>
#include <QSGRectangleNode>
#include <QSGTexture>
#include <QSGTransformNode>
#include <algorithm>
#include <cmath>
#include <limits>
#include "core.h"
#include "defines.h"
#include "map_layer_item.h"
#include "map_texture_cache.h"
#include "common/dbstring.h"
#include "common/filefinder.h"
#include "model/project.h"
#include "render/panorama.h"
#include "render/picture_cache.h"

namespace {

// The frames of the game, as EasyRPG Player counts them
constexpr qint64 FramesPerSecond = 60;
constexpr qint64 NsPerSecond = 1'000'000'000;
// a frame of the window, about
constexpr int FrameMs = 17;

// The zoom, the copies of the picture in sight, or the magenta of a map
// without a panorama. It holds the texture of the picture, shared with the
// other views of the window. A scrolling panorama has its copies in a clip
// (the map) and a move (the scroll).
class PanoramaNode : public QSGTransformNode
{
public:
	void clear()
	{
		while (QSGNode* child = firstChild())
			delete child;
		none = nullptr;
		scroll = nullptr;
		copies.clear();
	}

	std::shared_ptr<QSGTexture> texture;
	qint64 picture = 0;
	QSGRectangleNode* none = nullptr;
	QSGTransformNode* scroll = nullptr;
	// where the copies are, in pixels of the map (before the scroll)
	std::vector<QRect> copies;
};

// The step of Game_Map::Parallax::Update for a speed of Map Properties
// (-8 to 8): 2^|speed| 32nds of a pixel a frame, a positive speed moving
// the picture right or down
int scrollStep(int speed)
{
	speed = std::clamp(speed, -8, 8);
	return speed < 0 ? (1 << -speed) : speed > 0 ? -(1 << speed) : 0;
}

qint64 floorDiv(qint64 a, qint64 b)
{
	return a / b - (a % b != 0 && (a < 0) != (b < 0));
}

// How far right (or down) a picture size pixels wide scrolled in frame,
// as the Player draws it: its position in 32nds of a pixel wraps around
// the picture, and is drawn at the pixel left of it
int scrolled(qint64 frame, int step, int size)
{
	if (!step || size <= 0)
		return 0;
	const qint64 left = floorDiv(frame * step, 32) % size;
	return static_cast<int>((size - left) % size);
}

// The first frame after frame that moves the picture a pixel
qint64 nextMove(qint64 frame, int step)
{
	const qint64 pixel = floorDiv(frame * step, 32);
	qint64 next = frame + 1;
	// a step is 2 to 256: 16 frames at most
	while (floorDiv(next * step, 32) == pixel)
		next++;
	return next;
}

// The time of the game, from the first panorama view: the views of a map
// (the floating tiles' panorama...) scroll together
qint64 scrollTime()
{
	static const QElapsedTimer clock = [] {
		QElapsedTimer timer;
		timer.start();
		return timer;
	}();
	return clock.nsecsElapsed();
}

}

MapPanoramaView::MapPanoramaView(QQuickItem* parent) :
	QQuickItem(parent)
{
	setFlag(ItemHasContents);
	// clipRect() is then the part of the item in its Flickable (or window)
	setFlag(ItemObservesViewport);
	m_scrollTimer.setSingleShot(true);
	m_scrollTimer.setTimerType(Qt::PreciseTimer);
	connect(&m_scrollTimer, &QTimer::timeout, this, &MapPanoramaView::scroll);
}

MapPanoramaView::~MapPanoramaView() = default;

MapDocument* MapPanoramaView::document() const
{
	return m_doc;
}

void MapPanoramaView::setDocument(MapDocument* document)
{
	if (m_doc == document && m_docConnections.size() == (document ? 2 : 0))
		return;
	for (const auto& connection : m_docConnections)
		disconnect(connection);
	m_docConnections.clear();
	m_doc = document;
	if (m_doc) {
		// Map Properties can change the panorama and the size
		m_docConnections << connect(m_doc, &MapDocument::mapReplaced, this, &MapPanoramaView::reload);
		m_docConnections << connect(m_doc, &QObject::destroyed, this, [this]() {
			setDocument(nullptr);
		});
	}
	reload();
	emit documentChanged();
}

qreal MapPanoramaView::zoom() const
{
	return m_zoom;
}

void MapPanoramaView::setZoom(qreal zoom)
{
	if (zoom <= 0 || qFuzzyCompare(zoom, m_zoom))
		return;
	m_zoom = zoom;
	updateSize();
	update();
	emit zoomChanged();
}

bool MapPanoramaView::darkened() const
{
	return m_darkened;
}

void MapPanoramaView::setDarkened(bool darkened)
{
	if (darkened == m_darkened)
		return;
	m_darkened = darkened;
	m_changed = true;
	update();
	emit darkenedChanged();
}

QString MapPanoramaView::name() const
{
	return m_name;
}

bool MapPanoramaView::found() const
{
	return m_name.isEmpty() || m_picture;
}

QImage MapPanoramaView::picture() const
{
	return m_picture ? *m_picture : QImage();
}

void MapPanoramaView::reload()
{
	QString name;
	QPoint step;
	if (m_doc && m_doc->hasMap() && m_doc->map().parallax_flag) {
		const lcf::rpg::Map& map = m_doc->map();
		name = ToQString(map.parallax_name);
		// only an axis that loops scrolls by itself
		step = QPoint(map.parallax_loop_x && map.parallax_auto_loop_x ? scrollStep(map.parallax_sx) : 0,
			map.parallax_loop_y && map.parallax_auto_loop_y ? scrollStep(map.parallax_sy) : 0);
	}
	// as Core::panorama: in the project, not in the RTP
	std::shared_ptr<const QImage> picture;
	if (!name.isEmpty() && core().project())
		picture = PictureCache::load(core().project()->findFile(PANORAMA, name, FileFinder::FileType::Image));
	const bool changed = name != m_name || picture != m_picture;
	m_name = name;
	m_picture = std::move(picture);
	m_scrollStep = step;
	if (!m_picture || m_picture->cacheKey() != m_darkKey) {
		m_darkPicture = QImage();
		m_darkKey = 0;
	}
	m_changed = true;
	updateSize();
	update();
	scroll();
	if (changed)
		emit pictureChanged();
}

int MapPanoramaView::builtCopyCount() const
{
	return m_builtCopies;
}

QSize MapPanoramaView::mapPixels() const
{
	if (!m_doc || !m_doc->hasMap())
		return QSize();
	return QSize(m_doc->map().width, m_doc->map().height) * core().tileSize();
}

void MapPanoramaView::updateSize()
{
	const QSize pixels = mapPixels();
	setImplicitSize(pixels.width() * m_zoom, pixels.height() * m_zoom);
}

void MapPanoramaView::watchViewport()
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

void MapPanoramaView::scroll()
{
	m_scrollTimer.stop();
	const qint64 now = scrollTime();
	const qint64 frame = now * FramesPerSecond / NsPerSecond;
	const QSize size = m_picture ? m_picture->size() : QSize();
	const QPoint shift(scrolled(frame, m_scrollStep.x(), size.width()), scrolled(frame, m_scrollStep.y(), size.height()));
	if (shift != m_scrollShift) {
		m_scrollShift = shift;
		update();
	}
	if (!m_picture || m_scrollStep.isNull() || !isVisible() || !window()) {
		disconnect(m_frameConnection);
		return;
	}
	// The frames of the window show the moves: once the next one is less
	// than a frame away, each frame asks for the next (update(), which only
	// this item draws again). Further, a timer wakes them up a frame before.
	if (!m_frameConnection)
		m_frameConnection = connect(window(), &QQuickWindow::afterAnimating, this, &MapPanoramaView::scroll);
	qint64 next = std::numeric_limits<qint64>::max();
	for (const int step : {m_scrollStep.x(), m_scrollStep.y()})
		if (step)
			next = std::min(next, nextMove(frame, step));
	const qint64 wait = (next * NsPerSecond / FramesPerSecond - now) / 1'000'000;
	if (wait < FrameMs)
		update();
	else
		m_scrollTimer.start(static_cast<int>(std::min<qint64>(wait - FrameMs, std::numeric_limits<int>::max())));
}

void MapPanoramaView::itemChange(ItemChange change, const ItemChangeData& value)
{
	if ((change == ItemSceneChange && value.window) || change == ItemParentHasChanged)
		watchViewport();
	if (change == ItemSceneChange) {
		// the frames of the new window, if any
		disconnect(m_frameConnection);
		scroll();
	} else if (change == ItemVisibleHasChanged) {
		scroll();
	}
	QQuickItem::itemChange(change, value);
}

void MapPanoramaView::geometryChange(const QRectF& new_geometry, const QRectF& old_geometry)
{
	QQuickItem::geometryChange(new_geometry, old_geometry);
	update();
}

QSGNode* MapPanoramaView::updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*)
{
	auto* root = static_cast<PanoramaNode*>(old_node);
	const QSize map = mapPixels();
	if (map.isEmpty() || !found()) {
		delete root;
		m_builtCopies = 0;
		return nullptr;
	}
	if (!root) {
		root = new PanoramaNode();
		m_changed = true;
	}

	// the zoom, the top-left corner of the map on a pixel of the screen
	const qreal dpr = window()->effectiveDevicePixelRatio();
	const QPointF origin = mapToScene(QPointF(0, 0));
	QMatrix4x4 matrix;
	matrix.translate(static_cast<float>(std::round(origin.x() * dpr) / dpr - origin.x()),
		static_cast<float>(std::round(origin.y() * dpr) / dpr - origin.y()));
	matrix.scale(static_cast<float>(m_zoom));
	if (matrix != root->matrix())
		root->setMatrix(matrix);

	if (!m_picture) {
		// no panorama: magenta under the whole map
		if (m_changed || !root->none) {
			root->clear();
			root->texture.reset();
			root->picture = 0;
			root->none = window()->createRectangleNode();
			const QColor none = Panorama::NoneColor;
			root->none->setColor(m_darkened ? QColor(none.red() / 2, none.green() / 2, none.blue() / 2) : none);
			root->appendChildNode(root->none);
		}
		root->none->setRect(QRectF(QPoint(0, 0), map));
		m_builtCopies = 0;
		m_changed = false;
		return root;
	}

	// the copies in sight, in pixels of the map
	const QRectF visible = clipRect();
	const QRect area = visible.width() > 0 && visible.height() > 0
		? QRect(QPoint(static_cast<int>(std::floor(visible.left() / m_zoom)), static_cast<int>(std::floor(visible.top() / m_zoom))),
			QPoint(static_cast<int>(std::ceil(visible.right() / m_zoom)) - 1, static_cast<int>(std::ceil(visible.bottom() / m_zoom)) - 1))
		: QRect();
	// An axis that scrolls has the copies of a map one picture larger,
	// moved back by a picture less the scroll: wherever the scroll is, they
	// cover the map (cut at its edges), and only the move changes
	const QSize size = m_picture->size();
	const QSize back(m_scrollStep.x() ? size.width() : 0, m_scrollStep.y() ? size.height() : 0);
	const bool scrolls = !back.isNull();
	const std::vector<Panorama::Copy> copies = Panorama::copies(size, map + back,
		area.isEmpty() ? area : area.adjusted(0, 0, back.width(), back.height()));
	if (m_darkened && m_darkKey != m_picture->cacheKey()) {
		m_darkPicture = MapLayerView::darken(*m_picture);
		m_darkKey = m_picture->cacheKey();
	}
	const QImage& shown = m_darkened ? m_darkPicture : *m_picture;
	std::vector<QRect> targets;
	for (const Panorama::Copy& copy : copies)
		targets.push_back(copy.target);
	if (m_changed || root->none || root->picture != shown.cacheKey() || targets != root->copies || scrolls != (root->scroll != nullptr)) {
		root->clear();
		if (root->picture != shown.cacheKey()) {
			root->texture = MapTextureCache::texture(window(), shown);
			root->picture = shown.cacheKey();
		}
		QSGNode* parent = root;
		if (scrolls) {
			const QRectF rect(QPoint(0, 0), map);
			auto* clip = new QSGClipNode();
			auto* geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
			QSGGeometry::updateRectGeometry(geometry, rect);
			clip->setGeometry(geometry);
			clip->setFlag(QSGNode::OwnsGeometry);
			clip->setIsRectangular(true);
			clip->setClipRect(rect);
			root->scroll = new QSGTransformNode();
			clip->appendChildNode(root->scroll);
			root->appendChildNode(clip);
			parent = root->scroll;
		}
		for (const Panorama::Copy& copy : copies) {
			QSGImageNode* node = window()->createImageNode();
			node->setTexture(root->texture.get());
			node->setOwnsTexture(false);
			// a pixel of the picture a pixel of the map, as the tiles
			node->setFiltering(QSGTexture::Nearest);
			node->setRect(QRectF(copy.target));
			node->setSourceRect(QRectF(copy.source));
			parent->appendChildNode(node);
		}
		root->copies = std::move(targets);
	}
	if (root->scroll) {
		// on a pixel of the screen, as the corner of the map
		const double pixels = m_zoom * dpr;
		QMatrix4x4 move;
		move.translate(static_cast<float>(std::round((m_scrollShift.x() - back.width()) * pixels) / pixels),
			static_cast<float>(std::round((m_scrollShift.y() - back.height()) * pixels) / pixels));
		if (move != root->scroll->matrix())
			root->scroll->setMatrix(move);
	}
	m_builtCopies = static_cast<int>(root->copies.size());
	m_changed = false;
	return root;
}
