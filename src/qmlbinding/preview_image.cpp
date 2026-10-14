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

#include "preview_image.h"
#include <QPainter>

PreviewImage::PreviewImage(QQuickItem* parent) : QQuickPaintedItem(parent)
{
}

QImage PreviewImage::image() const
{
	return m_image;
}

void PreviewImage::setImage(const QImage& image)
{
	m_image = image;
	updateSize();
	update();
	emit imageChanged();
}

qreal PreviewImage::ratio() const
{
	return m_ratio;
}

void PreviewImage::setRatio(qreal ratio)
{
	if (ratio <= 0 || ratio == m_ratio) {
		return;
	}
	m_ratio = ratio;
	updateSize();
	update();
	emit imageChanged();
}

void PreviewImage::updateSize()
{
	setImplicitSize(m_image.width() / m_ratio, m_image.height() / m_ratio);
}

void PreviewImage::paint(QPainter* painter)
{
	if (m_image.isNull()) {
		return;
	}
	// centered, without smoothing: pixel art
	const QSizeF size(m_image.width() / m_ratio, m_image.height() / m_ratio);
	const QRectF target(QPointF((width() - size.width()) / 2, (height() - size.height()) / 2), size);
	painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
	painter->drawImage(target, m_image);
}
