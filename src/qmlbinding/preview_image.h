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
 * A picture the models make (QImage): a font preview, the symbols of the
 * messages... drawn pixel for pixel, at its size in points (ratio: its
 * pixels for a point, 2 for pictures made crisp for HiDPI screens).
 */
class PreviewImage : public QQuickPaintedItem
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)
	Q_PROPERTY(qreal ratio READ ratio WRITE setRatio NOTIFY imageChanged)

public:
	explicit PreviewImage(QQuickItem* parent = nullptr);

	QImage image() const;
	void setImage(const QImage& image);
	qreal ratio() const;
	void setRatio(qreal ratio);

	void paint(QPainter* painter) override;

signals:
	void imageChanged();

private:
	void updateSize();

	QImage m_image;
	qreal m_ratio = 1;
};
