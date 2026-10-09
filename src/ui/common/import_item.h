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

#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>
#include <functional>

/**
 * The image shown by ImportImageDialog: reports the pixel clicked, which
 * becomes the transparent color.
 */
class ImportItem : public QGraphicsPixmapItem
{
public:
	explicit ImportItem(const QPixmap& pix = QPixmap());

	/** Called with the pixel (in image coordinates) clicked or dragged over */
	void setPickHandler(std::function<void(QPoint)> handler);

protected:
	void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
	void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

private:
	void pick(const QPointF& pos);

	std::function<void(QPoint)> m_handler;
};
