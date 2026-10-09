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
#include <lcf/rpg/map.h>
#include "model/dungeon_generator.h"

class ProjectData;
class QComboBox;
class QLineEdit;
class QSpinBox;

/**
 * Generate Dungeon (RPG Maker's TFORMEDDUNGEON): a new map with the settings
 * of a map that uses the Random Dungeon Generator, its own name, tileset and
 * size, and a new dungeon.
 */
class GenerateDungeonDialog : public QDialog
{
	Q_OBJECT

public:
	GenerateDungeonDialog(ProjectData& project, const lcf::rpg::Map& source,
		const QString& name, QWidget* parent = nullptr);

	QString name() const;

	/** The cells kept walkable and reachable: the entrance and the exit, unless at 0, 0 */
	std::vector<DungeonGenerator::Point> keepPoints() const;

	/** Random by default; fixed by the tests */
	void setSeed(uint32_t seed);

	/** The source map without its events, the tileset and size of the dialog, a new dungeon */
	lcf::rpg::Map map() const;

private:
	void updateRanges();

	lcf::rpg::Map m_source;
	uint32_t m_seed;
	QLineEdit* m_name;
	QComboBox* m_tileset;
	QSpinBox* m_width;
	QSpinBox* m_height;
	QSpinBox* m_entranceX;
	QSpinBox* m_entranceY;
	QSpinBox* m_exitX;
	QSpinBox* m_exitY;
};
