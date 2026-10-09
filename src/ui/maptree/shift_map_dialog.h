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

#include <QDialog>

class QButtonGroup;
class QSpinBox;

/**
 * Shift Map (RPG Maker's TFORMMAPSHIFT): a direction and a number of cells
 * to move the whole map by.
 */
class ShiftMapDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ShiftMapDialog(QWidget* parent = nullptr);

	/** The shift in cells, from the direction and the number of units */
	int dx() const;
	int dy() const;

private:
	enum Direction { Up, Left, Right, Down };

	QButtonGroup* m_direction;
	QSpinBox* m_units;
};
