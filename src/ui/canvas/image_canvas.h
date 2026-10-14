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
#include <QQuickPaintedItem>
#include <QtQmlIntegration>

/**
 * A picture of the game drawn pixel for pixel at a whole zoom, centered in
 * the item, on what is behind it (no background): the previews of the
 * database pages. Its implicit size is the picture's at zoom.
 */
class ImageCanvas : public QQuickPaintedItem
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)
	/** Points of the item for a pixel of the picture */
	Q_PROPERTY(int zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
	/** Where the picture starts in the item (it is centered), in points */
	Q_PROPERTY(QPointF origin READ origin NOTIFY originChanged)

public:
	explicit ImageCanvas(QQuickItem* parent = nullptr);

	QImage image() const;
	void setImage(const QImage& image);
	int zoom() const;
	void setZoom(int zoom);
	QPointF origin() const;

	/** The pixel of the picture at a point of the item (outside: beyond its edges) */
	Q_INVOKABLE QPointF pixelAt(qreal x, qreal y) const;

	void paint(QPainter* painter) override;

signals:
	void imageChanged();
	void zoomChanged();
	void originChanged();

protected:
	void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
	void updateSize();

	QImage m_image;
	int m_zoom = 1;
};
