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
 * A picture of the game enlarged pixel for pixel (Ez.ZoomView shows it in a
 * scrolled view): the part of it the item shows, at zoom, from contentX and
 * contentY, with gray squares where it is transparent. Only what is shown
 * is drawn: a panorama at x16 costs no more than at x1.
 */
class ZoomImageView : public QQuickPaintedItem
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)
	/** Points of the item for a pixel of the image */
	Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
	/** Where the image starts, from the top left of the item (a Flickable's contentX: negative when centered) */
	Q_PROPERTY(qreal contentX READ contentX WRITE setContentX NOTIFY contentChanged)
	Q_PROPERTY(qreal contentY READ contentY WRITE setContentY NOTIFY contentChanged)
	/** The image at zoom, in points */
	Q_PROPERTY(qreal contentWidth READ contentWidth NOTIFY sizeChanged)
	Q_PROPERTY(qreal contentHeight READ contentHeight NOTIFY sizeChanged)

public:
	explicit ZoomImageView(QQuickItem* parent = nullptr);

	QImage image() const;
	void setImage(const QImage& image);
	qreal zoom() const;
	void setZoom(qreal zoom);
	qreal contentX() const;
	void setContentX(qreal x);
	qreal contentY() const;
	void setContentY(qreal y);
	qreal contentWidth() const;
	qreal contentHeight() const;

	void paint(QPainter* painter) override;

signals:
	void imageChanged();
	void zoomChanged();
	void contentChanged();
	void sizeChanged();

private:
	QImage m_image;
	qreal m_zoom = 1;
	qreal m_contentX = 0;
	qreal m_contentY = 0;
};
