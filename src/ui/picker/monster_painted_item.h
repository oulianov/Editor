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

#include "sprite_painted_item.h"

/**
 * A monster's battler graphic as the Player draws it: palette color 0
 * transparent, the hue rotated (its color alteration). The whole picture,
 * at the scale of the item, centered; outside a picker a large monster is
 * made smaller to fit, a small one stays at its size.
 */
class MonsterPaintedItem : public SpritePaintedItem {
	Q_OBJECT
	QML_ELEMENT
	/** The color alteration, 0 to 359 degrees */
	Q_PROPERTY(int hue READ hue WRITE setHue NOTIFY hueChanged)

public:
	explicit MonsterPaintedItem(QQuickItem* parent = nullptr);

	int hue() const;
	void setHue(int hue);

	void paint(QPainter* painter) override;

signals:
	void hueChanged();

protected:
	QPixmap loadImage() const override;
	void mousePressEvent(QMouseEvent* event) override;

private:
	int m_hue = 0;
};
