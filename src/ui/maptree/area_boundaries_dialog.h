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

class MapPreview;
class QLabel;
class QPushButton;

/**
 * Define Area Boundaries (RPG Maker's TFORMMAPSETAREA): the parent map, on
 * which a box is dragged around the cells the area covers.
 */
class AreaBoundariesDialog : public QDialog
{
	Q_OBJECT

public:
	/** area in cells; an empty area starts without a box */
	AreaBoundariesDialog(int map_id, const QRect& area, QWidget* parent = nullptr);

	QRect area() const;

private:
	void updateStatus();

	MapPreview* m_preview;
	QLabel* m_status;
	QPushButton* m_ok;
};
