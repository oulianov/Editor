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
#include <QMatrix4x4>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGRectangleNode>
#include <QSGTexture>
#include <QSGTransformNode>
#include <cmath>
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

// The zoom, the copies of the picture in sight, or the magenta of a map
// without a panorama. It holds the texture of the picture, shared with the
// other views of the window.
class PanoramaNode : public QSGTransformNode
{
public:
	void clear()
	{
		while (QSGNode* child = firstChild())
			delete child;
		none = nullptr;
		copies.clear();
	}

	std::shared_ptr<QSGTexture> texture;
	qint64 picture = 0;
	QSGRectangleNode* none = nullptr;
	// where the copies are, in pixels of the map
	std::vector<QRect> copies;
};

}

MapPanoramaView::MapPanoramaView(QQuickItem* parent) :
	QQuickItem(parent)
{
	setFlag(ItemHasContents);
	// clipRect() is then the part of the item in its Flickable (or window)
	setFlag(ItemObservesViewport);
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
	if (m_doc && m_doc->hasMap() && m_doc->map().parallax_flag)
		name = ToQString(m_doc->map().parallax_name);
	// as Core::panorama: in the project, not in the RTP
	std::shared_ptr<const QImage> picture;
	if (!name.isEmpty() && core().project())
		picture = PictureCache::load(core().project()->findFile(PANORAMA, name, FileFinder::FileType::Image));
	const bool changed = name != m_name || picture != m_picture;
	m_name = name;
	m_picture = std::move(picture);
	if (!m_picture || m_picture->cacheKey() != m_darkKey) {
		m_darkPicture = QImage();
		m_darkKey = 0;
	}
	m_changed = true;
	updateSize();
	update();
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

void MapPanoramaView::itemChange(ItemChange change, const ItemChangeData& value)
{
	if ((change == ItemSceneChange && value.window) || change == ItemParentHasChanged)
		watchViewport();
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
	const std::vector<Panorama::Copy> copies = Panorama::copies(m_picture->size(), map, area);
	if (m_darkened && m_darkKey != m_picture->cacheKey()) {
		m_darkPicture = MapLayerView::darken(*m_picture);
		m_darkKey = m_picture->cacheKey();
	}
	const QImage& shown = m_darkened ? m_darkPicture : *m_picture;
	std::vector<QRect> targets;
	for (const Panorama::Copy& copy : copies)
		targets.push_back(copy.target);
	if (m_changed || root->none || root->picture != shown.cacheKey() || targets != root->copies) {
		root->clear();
		if (root->picture != shown.cacheKey()) {
			root->texture = MapTextureCache::texture(window(), shown);
			root->picture = shown.cacheKey();
		}
		for (const Panorama::Copy& copy : copies) {
			QSGImageNode* node = window()->createImageNode();
			node->setTexture(root->texture.get());
			node->setOwnsTexture(false);
			// a pixel of the picture a pixel of the map, as the tiles
			node->setFiltering(QSGTexture::Nearest);
			node->setRect(QRectF(copy.target));
			node->setSourceRect(QRectF(copy.source));
			root->appendChildNode(node);
		}
		root->copies = std::move(targets);
	}
	m_builtCopies = static_cast<int>(root->copies.size());
	m_changed = false;
	return root;
}
