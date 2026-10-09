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
#include <lcf/rpg/mapinfo.h>

class ProjectData;
class EncounterList;
class QLineEdit;
class QSpinBox;

/**
 * Area Properties (RPG Maker's TFORMEDAREA): the name, the boundaries on the
 * parent map and the monster groups met inside the area.
 */
class AreaPropertiesDialog : public QDialog
{
	Q_OBJECT

public:
	AreaPropertiesDialog(ProjectData& project, lcf::rpg::MapInfo& info, int map_width, int map_height, QWidget* parent = nullptr);

public slots:
	void accept() override;

private:
	void updateLimits();

	lcf::rpg::MapInfo& m_info;
	int m_mapWidth;
	int m_mapHeight;
	QLineEdit* m_name;
	QSpinBox* m_left;
	QSpinBox* m_top;
	QSpinBox* m_width;
	QSpinBox* m_height;
	EncounterList* m_encounters;
};
