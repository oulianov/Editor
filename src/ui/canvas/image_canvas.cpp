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

#include "image_canvas.h"
#include <QPainter>
#include <algorithm>
#include <cmath>

ImageCanvas::ImageCanvas(QQuickItem* parent) : QQuickPaintedItem(parent)
{
	// the pixels of the game stay square
	setAntialiasing(false);
	setSmooth(false);
}

QImage ImageCanvas::image() const
{
	return m_image;
}

void ImageCanvas::setImage(const QImage& image)
{
	const bool resized = image.size() != m_image.size();
	m_image = image;
	emit imageChanged();
	if (resized) {
		updateSize();
	}
	update();
}

int ImageCanvas::zoom() const
{
	return m_zoom;
}

void ImageCanvas::setZoom(int zoom)
{
	zoom = std::max(1, zoom);
	if (zoom != m_zoom) {
		m_zoom = zoom;
		emit zoomChanged();
		updateSize();
		update();
	}
}

QPointF ImageCanvas::origin() const
{
	return QPointF(std::floor((width() - m_image.width() * m_zoom) / 2), std::floor((height() - m_image.height() * m_zoom) / 2));
}

QPointF ImageCanvas::pixelAt(qreal x, qreal y) const
{
	return (QPointF(x, y) - origin()) / m_zoom;
}

void ImageCanvas::updateSize()
{
	setImplicitSize(m_image.width() * m_zoom, m_image.height() * m_zoom);
	emit originChanged();
}

void ImageCanvas::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
	QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
	if (newGeometry.size() != oldGeometry.size()) {
		emit originChanged();
	}
}

void ImageCanvas::paint(QPainter* painter)
{
	if (m_image.isNull()) {
		return;
	}
	painter->drawImage(QRectF(origin(), QSizeF(m_image.size()) * m_zoom), m_image);
}
