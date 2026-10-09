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
#include <QGraphicsPixmapItem>
#include <QTableWidgetItem>
#include <lcf/rpg/mapinfo.h>
#include <lcf/rpg/map.h>
#include <QButtonGroup>

class ProjectData;
class EncounterList;
class QToolButton;

namespace Ui {
class MapPropertiesDialog;
}

class MapPropertiesDialog : public QDialog
{
	Q_OBJECT

public:
	explicit MapPropertiesDialog(ProjectData& project, lcf::rpg::MapInfo &info, lcf::rpg::Map &map, QWidget *parent = nullptr);
	~MapPropertiesDialog();

private slots:
	void accept();

	void on_groupPanorama_toggled(bool arg1);

	void on_pushAddEncounter_clicked();

	void on_pushRemoveEncounter_clicked();

	void on_pushSetPanorama_clicked();

	void on_toolSetBGM_clicked();

	void on_toolSetBackdrop_clicked();

private:
	Ui::MapPropertiesDialog *ui;

	/** The tile boxes of the dungeon generator, in the order of generator_x and generator_y */
	enum GeneratorBox { Ceiling, LowerWall, UpperWall, FloorA, FloorB, FloorC, ObstacleA, ObstacleB, ObstacleC, GeneratorBoxes };
	QToolButton* generatorButton(int box) const;
	/** The tiles of a box in palette cells (6 per row): one lower tile, or up to 2 x 2 upper tiles */
	QRect generatorCells(int box) const;
	void setGeneratorCells(int box, const QRect& cells);
	void pickGeneratorTiles(int box);
	/** Draws the boxes with the tiles of the chipset chosen for this map */
	void updateGeneratorTiles();

	QGraphicsPixmapItem *m_panoramaItem;

	EncounterList *m_encounters;

	lcf::rpg::MapInfo &m_info;
	lcf::rpg::Map &m_map;
	lcf::rpg::MapInfo m_info_copy;
	lcf::rpg::Map m_map_copy;

	std::vector<uint32_t> m_generatorX;
	std::vector<uint32_t> m_generatorY;
	std::vector<int16_t> m_generatorTiles;
	/** Written back only when a tile was chosen: maps keep what they had */
	bool m_generatorChanged = false;
	/** The palette tiles of m_paletteChipset, for the boxes */
	int m_paletteChipset = -1;
	QPixmap m_lowerTiles;
	QPixmap m_upperTiles;

	ProjectData& m_project;

	lcf::DBString new_panorama;
	lcf::rpg::Music new_music;

	int old_width;
	int old_height;

	QButtonGroup* m_buttonGroupBGM = nullptr;
	QButtonGroup* m_buttonGroupBackdrop = nullptr;
	QButtonGroup* m_buttonGroupTeleport = nullptr;
	QButtonGroup* m_buttonGroupEscape = nullptr;
	QButtonGroup* m_buttonGroupSave = nullptr;
};

