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

#include "import_item.h"
#include <QCursor>
#include <QtMath>

ImportItem::ImportItem(const QPixmap& pix) :
	QGraphicsPixmapItem(pix)
{
	setCursor(Qt::CrossCursor);
}

void ImportItem::setPickHandler(std::function<void(QPoint)> handler)
{
	m_handler = std::move(handler);
}

void ImportItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		pick(event->pos());
		event->accept();
		return;
	}
	QGraphicsPixmapItem::mousePressEvent(event);
}

void ImportItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	// dragging over the image changes the color live, as in RPG Maker
	if (event->buttons() & Qt::LeftButton) {
		pick(event->pos());
	}
}

void ImportItem::pick(const QPointF& pos)
{
	const QPoint pixel(qFloor(pos.x()), qFloor(pos.y()));
	if (m_handler && pixmap().rect().contains(pixel)) {
		m_handler(pixel);
	}
}
