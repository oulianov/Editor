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

#include <QWidget>
#include <lcf/rpg/actor.h>
#include "picker_child_widget.h"

class QButtonGroup;
class QGraphicsScene;
class QGraphicsRectItem;
class QGraphicsPixmapItem;

class PickerCharsetWidget : public PickerChildWidget {
	Q_OBJECT
public:
	PickerCharsetWidget(int index, QWidget* parent = nullptr) : PickerChildWidget(parent), m_index(index) {}
	/** Also the facing and the frame, like RPG Maker's event graphic: the
	 *  whole sheet is shown, a click picks a pose of a character */
	PickerCharsetWidget(int index, int direction, int pattern, QWidget* parent = nullptr);

	void clicked(const QPointF& pos) override;

	void imageChanged(QPixmap image) override;

	int index() const {
		return m_index;
	}
	/** 0 up, 1 right, 2 down, 3 left */
	int direction() const {
		return m_direction;
	}
	/** 0 left, 1 middle, 2 right */
	int pattern() const {
		return m_pattern;
	}

private:
	void updateRect();

	int m_index;
	int m_direction = 2;
	int m_pattern = 1;
	bool m_pose = false;
	QButtonGroup* m_facing = nullptr;
	QButtonGroup* m_frame = nullptr;
	QGraphicsRectItem* m_rect = nullptr;
	QGraphicsPixmapItem* m_pixmap = nullptr;
};
