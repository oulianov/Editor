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

class ProjectData;
class MapPreview;
class QButtonGroup;
class QLabel;
class QPushButton;
class QTreeWidget;

/**
 * RPG Maker's "Select Location" (TFORMMAPSETPLACE): the map tree, a view of
 * the chosen map, and a cell picked on it. Double-click on a cell accepts.
 */
class MapLocationDialog : public QDialog
{
	Q_OBJECT

public:
	/** Asks for a map and a cell, starting at map_id, x, y; false when cancelled */
	static bool pick(ProjectData& project, int& map_id, int& x, int& y, QWidget* parent);
	/** The same with the Character Facing of RPG Maker: 0 retain, 1 up, 2 right, 3 down, 4 left */
	static bool pick(ProjectData& project, int& map_id, int& x, int& y, int& direction, QWidget* parent);
	/** A cell of one map, without the map tree (Set Event Location...) */
	static bool pickOnMap(ProjectData& project, int map_id, int& x, int& y, QWidget* parent);

	/** direction -1: no Character Facing; fixed_map: no map tree */
	MapLocationDialog(ProjectData& project, int map_id, int x, int y, int direction, bool fixed_map, QWidget* parent = nullptr);

	int mapId() const;
	QPoint cell() const;
	int direction() const;

private:
	void showMap(int map_id);
	void updateStatus();

	ProjectData& m_project;
	QTreeWidget* m_tree = nullptr;
	MapPreview* m_preview;
	QLabel* m_status;
	QButtonGroup* m_facing = nullptr;
	QPushButton* m_ok;
};
