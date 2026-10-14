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

#include "monster_painted_item.h"
#include "defines.h"
#include "model/enemy_graphics.h"
#include <QMouseEvent>
#include <QPainter>

MonsterPaintedItem::MonsterPaintedItem(QQuickItem* parent) : SpritePaintedItem(parent) {
	setDirectory(QStringLiteral(MONSTER));
}

int MonsterPaintedItem::hue() const {
	return m_hue;
}

void MonsterPaintedItem::setHue(int hue) {
	if (m_hue == hue) {
		return;
	}
	m_hue = hue;
	m_image = {};
	emit hueChanged();
	reload();
}

QPixmap MonsterPaintedItem::loadImage() const {
	// transparent in the picker too, as the Player draws it
	return EnemyGraphics::load(imagePath(), m_hue);
}

void MonsterPaintedItem::paint(QPainter* painter) {
	if (m_image.isNull() || width() <= 0 || height() <= 0) {
		return;
	}
	qreal scale = std::min(width() / m_image.width(), height() / m_image.height());
	if (!m_pickerMode) {
		scale = std::min(scale, 1.0);
	}
	const QSizeF size(m_image.width() * scale, m_image.height() * scale);
	const QRectF target(QPointF((width() - size.width()) / 2, (height() - size.height()) / 2), size);
	if (m_transparent) {
		painter->setOpacity(0.6);
	}
	painter->drawPixmap(target, m_image, m_image.rect());
}

void MonsterPaintedItem::mousePressEvent(QMouseEvent* event) {
	// one picture: nothing to choose in it
	event->ignore();
}
