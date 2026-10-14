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
#include <QTimer>

/**
 * Displays an animated character from a CharSet spritesheet.
 *
 * In a picker it shows the 8 characters of the sheet (cellIndex: the one
 * chosen); with poseMode, the whole sheet, a click choosing a character,
 * its facing and its frame (RPG Maker's Select Event Graphic).
 */
class CharSetPaintedItem : public SpritePaintedItem {
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(int facing READ facing WRITE setFacing NOTIFY facingChanged)
	Q_PROPERTY(int frame READ frame WRITE setFrame NOTIFY frameChanged)
	Q_PROPERTY(bool spin READ spin WRITE setSpin NOTIFY spinChanged)
	Q_PROPERTY(bool walk READ walk WRITE setWalk NOTIFY walkChanged)
	/** In a picker: the whole sheet, the facing and the frame chosen too */
	Q_PROPERTY(bool poseMode READ poseMode WRITE setPoseMode NOTIFY poseModeChanged)

public:
	enum Direction {
		Direction_up = 0,
		Direction_right = 1,
		Direction_down = 2,
		Direction_left = 3
	};
	Q_ENUM(Direction)

	enum Frame {
		Frame_left = 0,
		Frame_middle = 1,
		Frame_right = 2
	};
	Q_ENUM(Frame)

	explicit CharSetPaintedItem(QQuickItem* parent = nullptr);

	int facing() const;
	void setFacing(int facing);

	int frame() const;
	void setFrame(int frame);

	bool spin() const;
	void setSpin(bool spin);

	bool walk() const;
	void setWalk(bool walk);

	bool poseMode() const;
	void setPoseMode(bool poseMode);

	void paint(QPainter* painter) override;

signals:
	void characterIndexChanged();
	void facingChanged();
	void frameChanged();
	void spinChanged();
	void walkChanged();
	void poseModeChanged();

protected:
	QRectF sourceRect() const override;
	void mousePressEvent(QMouseEvent* event) override;
	void reload() override;

private:
	/** The whole sheet is shown (a picker in pose mode) */
	bool showsSheet() const;
	/** The scale of the sheet drawn in the item */
	qreal sheetScale() const;

	void tick();

	QTimer m_timer;

	int m_facing = Direction_down;
	int m_frame = Frame_middle;
	bool m_spin = false;
	bool m_walk = false;
	bool m_poseMode = false;

	int m_pattern = 0;
	int frame_count = 0;
};
