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

#include "zoom_image_view.h"

#include <QPainter>
#include <cmath>

namespace {

/** The side of the gray squares behind the image, in points */
constexpr qreal checker_size = 8;

} // namespace

ZoomImageView::ZoomImageView(QQuickItem* parent) : QQuickPaintedItem(parent)
{
	// the pixels of the game stay square at any zoom
	setAntialiasing(false);
	setMipmap(false);
	setSmooth(false);
}

QImage ZoomImageView::image() const
{
	return m_image;
}

void ZoomImageView::setImage(const QImage& image)
{
	const bool resized = image.size() != m_image.size();
	m_image = image;
	emit imageChanged();
	if (resized) {
		emit sizeChanged();
	}
	update();
}

qreal ZoomImageView::zoom() const
{
	return m_zoom;
}

void ZoomImageView::setZoom(qreal zoom)
{
	zoom = std::max(zoom, 0.01);
	if (zoom == m_zoom) {
		return;
	}
	m_zoom = zoom;
	emit zoomChanged();
	emit sizeChanged();
	update();
}

qreal ZoomImageView::contentX() const
{
	return m_contentX;
}

void ZoomImageView::setContentX(qreal x)
{
	if (x != m_contentX) {
		m_contentX = x;
		emit contentChanged();
		update();
	}
}

qreal ZoomImageView::contentY() const
{
	return m_contentY;
}

void ZoomImageView::setContentY(qreal y)
{
	if (y != m_contentY) {
		m_contentY = y;
		emit contentChanged();
		update();
	}
}

qreal ZoomImageView::contentWidth() const
{
	return m_image.width() * m_zoom;
}

qreal ZoomImageView::contentHeight() const
{
	return m_image.height() * m_zoom;
}

void ZoomImageView::paint(QPainter* painter)
{
	if (m_image.isNull()) {
		return;
	}
	// the image in the item, and the part of it shown
	const QRectF placed(-m_contentX, -m_contentY, contentWidth(), contentHeight());
	const QRectF shown = placed.intersected(QRectF(0, 0, width(), height()));
	if (shown.isEmpty()) {
		return;
	}

	// gray squares where the image is transparent, fixed to the image
	painter->save();
	painter->setClipRect(shown);
	painter->fillRect(shown, QColor(204, 204, 204));
	const int first_column = static_cast<int>(std::floor((shown.left() - placed.left()) / checker_size));
	const int first_row = static_cast<int>(std::floor((shown.top() - placed.top()) / checker_size));
	for (int row = first_row; placed.top() + row * checker_size < shown.bottom(); ++row) {
		for (int column = first_column + ((first_column + row) & 1); placed.left() + column * checker_size < shown.right(); column += 2) {
			painter->fillRect(QRectF(placed.left() + column * checker_size, placed.top() + row * checker_size,
				checker_size, checker_size), Qt::white);
		}
	}
	painter->restore();

	// the pixels of the image under the part shown, enlarged without smoothing
	const QRectF source((shown.left() - placed.left()) / m_zoom, (shown.top() - placed.top()) / m_zoom,
		shown.width() / m_zoom, shown.height() / m_zoom);
	const QRect pixels = source.toAlignedRect().intersected(m_image.rect());
	const QRectF target(placed.left() + pixels.left() * m_zoom, placed.top() + pixels.top() * m_zoom,
		pixels.width() * m_zoom, pixels.height() * m_zoom);
	painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
	painter->setClipRect(shown);
	painter->drawImage(target, m_image, pixels);
}
