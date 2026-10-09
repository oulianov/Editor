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

#include "map_properties_dialog.h"
#include "ui_map_properties_dialog.h"
#include "core.h"
#include "common/dbstring.h"
#include "ui/picker/picker_audio_widget.h"
#include "ui/picker/picker_backdrop_widget.h"
#include "ui/picker/picker_panorama_widget.h"
#include "ui/picker/picker_dialog.h"
#include "encounter_list.h"
#include "chip_selector_dialog.h"
#include "ui/common/map_preview.h"
#include <QPainter>
#include <QToolButton>
#include <array>
#include <QMessageBox>
#include "common/lcf_widget_binding.h"

MapPropertiesDialog::MapPropertiesDialog(ProjectData& project, lcf::rpg::MapInfo &info, lcf::rpg::Map &map, QWidget *parent) :
	QDialog(parent),
	ui(new Ui::MapPropertiesDialog),
	m_info(info),
	m_map(map),
	m_project(project)
{
	ui->setupUi(this);

	m_info_copy = m_info;
	m_map_copy = m_map;

	auto& database = project.database();

	for (int i = 0; i < static_cast<int>(database.chipsets.size()); i++) {
		ui->comboTileset->addItem(ToQString(database.chipsets[static_cast<size_t>(i)].name), i + 1);
	}

	ui->comboWrapping->addItem(tr("None"), lcf::rpg::Map::ScrollType_none);
	ui->comboWrapping->addItem(tr("Vertical"), lcf::rpg::Map::ScrollType_vertical);
	ui->comboWrapping->addItem(tr("Horizontal"), lcf::rpg::Map::ScrollType_horizontal);
	ui->comboWrapping->addItem(tr("Both"), lcf::rpg::Map::ScrollType_both);

	m_buttonGroupBGM = new QButtonGroup(this);
	m_buttonGroupBGM->addButton(ui->radioBGMparent);
	m_buttonGroupBGM->setId(ui->radioBGMparent, lcf::rpg::MapInfo::MusicType_parent);
	m_buttonGroupBGM->addButton(ui->radioBGMevent);
	m_buttonGroupBGM->setId(ui->radioBGMevent, lcf::rpg::MapInfo::MusicType_event);
	m_buttonGroupBGM->addButton(ui->radioBGMspecify);
	m_buttonGroupBGM->setId(ui->radioBGMspecify, lcf::rpg::MapInfo::MusicType_specific);

	m_buttonGroupBackdrop = new QButtonGroup(this);
	m_buttonGroupBackdrop->addButton(ui->radioBackdropParent);
	m_buttonGroupBackdrop->setId(ui->radioBackdropParent, lcf::rpg::MapInfo::BGMType_parent);
	m_buttonGroupBackdrop->addButton(ui->radioBackdropTerrain);
	m_buttonGroupBackdrop->setId(ui->radioBackdropTerrain, lcf::rpg::MapInfo::BGMType_terrain);
	m_buttonGroupBackdrop->addButton(ui->radioBackdropSpecify);
	m_buttonGroupBackdrop->setId(ui->radioBackdropSpecify, lcf::rpg::MapInfo::BGMType_specific);

	m_buttonGroupTeleport = new QButtonGroup(this);
	m_buttonGroupTeleport->addButton(ui->radioTeleportParent);
	m_buttonGroupTeleport->setId(ui->radioTeleportParent, lcf::rpg::MapInfo::TriState_parent);
	m_buttonGroupTeleport->addButton(ui->radioTeleportAllow);
	m_buttonGroupTeleport->setId(ui->radioTeleportAllow, lcf::rpg::MapInfo::TriState_allow);
	m_buttonGroupTeleport->addButton(ui->radioTeleportForbid);
	m_buttonGroupTeleport->setId(ui->radioTeleportForbid, lcf::rpg::MapInfo::TriState_forbid);

	m_buttonGroupEscape = new QButtonGroup(this);
	m_buttonGroupEscape->addButton(ui->radioEscapeParent);
	m_buttonGroupEscape->setId(ui->radioEscapeParent, lcf::rpg::MapInfo::TriState_parent);
	m_buttonGroupEscape->addButton(ui->radioEscapeAllow);
	m_buttonGroupEscape->setId(ui->radioEscapeAllow, lcf::rpg::MapInfo::TriState_allow);
	m_buttonGroupEscape->addButton(ui->radioEscapeForbid);
	m_buttonGroupEscape->setId(ui->radioEscapeForbid, lcf::rpg::MapInfo::TriState_forbid);

	m_buttonGroupSave = new QButtonGroup(this);
	m_buttonGroupSave->addButton(ui->radioSaveParent);
	m_buttonGroupSave->setId(ui->radioSaveParent, lcf::rpg::MapInfo::TriState_parent);
	m_buttonGroupSave->addButton(ui->radioSaveAllow);
	m_buttonGroupSave->setId(ui->radioSaveAllow, lcf::rpg::MapInfo::TriState_allow);
	m_buttonGroupSave->addButton(ui->radioSaveForbid);
	m_buttonGroupSave->setId(ui->radioSaveForbid, lcf::rpg::MapInfo::TriState_forbid);

	m_panoramaItem = new QGraphicsPixmapItem();

	ui->spinDungeonRoomHeight->setValue(m_map_copy.generator_height);
	ui->spinDungeonRoomWidth->setValue(m_map_copy.generator_width);
	ui->spinEncounterRate->setValue(m_info_copy.encounter_steps);
	ui->checkDungeonSurroundWithWalls->setChecked(m_map_copy.generator_surround);
	ui->groupUseGenerator->setChecked(m_map_copy.generator_flag);
	ui->radioDungeonSinglePassage->setChecked(m_map_copy.generator_mode == 0);
	ui->radioDungeonLinkedRooms->setChecked(m_map_copy.generator_mode == 1);
	ui->radioDungeonMaze->setChecked(m_map_copy.generator_mode == 2);
	ui->radioDungeonOpenRoom->setChecked(m_map_copy.generator_mode == 3);
	ui->radioDungeonPassage1_1->setChecked(m_map_copy.generator_tiles == 0);
	ui->radioDungeonPassage2_2->setChecked(m_map_copy.generator_tiles == 1);
	// room dimensions are only used by "Rooms Linked with Passages"
	ui->spinDungeonRoomWidth->setEnabled(m_map_copy.generator_mode == 1);
	ui->spinDungeonRoomHeight->setEnabled(m_map_copy.generator_mode == 1);
	ui->groupUpperWall->setChecked(m_map_copy.generator_upper_wall);
	ui->groupFloorB->setChecked(m_map_copy.generator_floor_b);
	ui->groupFloorC->setChecked(m_map_copy.generator_floor_c);
	ui->groupObstacleB->setChecked(m_map_copy.generator_extra_b);
	ui->groupObstacleC->setChecked(m_map_copy.generator_extra_c);

	// The tiles of the boxes: 9 cells of the palette, 6 lower tiles then 3
	// blocks of 2 x 2 upper tiles. A new map has RPG Maker's defaults.
	m_generatorX = m_map_copy.generator_x;
	m_generatorY = m_map_copy.generator_y;
	m_generatorTiles = m_map_copy.generator_tile_ids;
	m_generatorX.resize(GeneratorBoxes, 0);
	m_generatorY.resize(GeneratorBoxes, 0);
	if (m_generatorTiles.size() < 18) {
		m_generatorTiles = {49, 49, 49, 49, 49, 49,
			10000, 10001, 10006, 10007, 10000, 10001, 10006, 10007, 10000, 10001, 10006, 10007};
	}
	for (int box = 0; box < GeneratorBoxes; box++) {
		connect(generatorButton(box), &QToolButton::clicked, this, [this, box] { pickGeneratorTiles(box); });
	}

	m_encounters = new EncounterList(project, ui->tableEncounters, this);
	m_encounters->setEncounters(m_info_copy.encounters);

	ui->graphicsPanorama->setScene(new QGraphicsScene(this));
	ui->graphicsPanorama->scene()->addItem(m_panoramaItem);
	QPixmap pix;
	if (m_map_copy.parallax_flag)
	{
		pix = QPixmap(m_project.project().findFile(PANORAMA, ToQString(m_map_copy.parallax_name), FileFinder::FileType::Image));
		if (!pix)
			pix = QPixmap(core().rtpPath(PANORAMA,ToQString(m_map_copy.parallax_name)));
		m_panoramaItem->setPixmap(pix);
	}

	//TODO: Show generator tiles.
	if (m_info_copy.parent_map == 0)
	{
		ui->radioBackdropParent->setEnabled(false);
		ui->radioBGMparent->setEnabled(false);
		ui->radioTeleportParent->setEnabled(false);
		ui->radioSaveParent->setEnabled(false);
		ui->radioEscapeParent->setEnabled(false);
	}

	new_panorama = m_map_copy.parallax_name;
	new_music = m_info_copy.music;

	old_width = m_map_copy.width;
	old_height = m_map_copy.height;

	ui->lineBGMname->setText(ToQString(m_info_copy.music.name));

	LcfWidgetBinding::connect(this, ui->lineName);
	LcfWidgetBinding::connect(this, ui->lineBackdropName);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboTileset);
	LcfWidgetBinding::connect<int32_t>(this, ui->comboWrapping);
	LcfWidgetBinding::connect<int32_t>(this, ui->spinWidth);
	LcfWidgetBinding::connect<int32_t>(this, ui->spinHeight);
	LcfWidgetBinding::connect<bool>(this, ui->groupPanorama);
	LcfWidgetBinding::connect<bool>(this, ui->groupHorizontalScroll);
	LcfWidgetBinding::connect<bool>(this, ui->groupVerticalScroll);
	LcfWidgetBinding::connect<bool>(this, ui->checkHorizontalAutoscroll);
	LcfWidgetBinding::connect<bool>(this, ui->checkVerticalAutoscroll);
	LcfWidgetBinding::connect<int32_t>(this, ui->spinHorizontalScrollSpeed);
	LcfWidgetBinding::connect<int32_t>(this, ui->spinVerticalScrollSpeed);
	LcfWidgetBinding::connect<int32_t>(this, m_buttonGroupBGM);
	LcfWidgetBinding::connect<int32_t>(this, m_buttonGroupBackdrop);
	LcfWidgetBinding::connect<int32_t>(this, m_buttonGroupTeleport);
	LcfWidgetBinding::connect<int32_t>(this, m_buttonGroupEscape);
	LcfWidgetBinding::connect<int32_t>(this, m_buttonGroupSave);

	LcfWidgetBinding::bind(ui->lineName, m_info_copy.name);
	LcfWidgetBinding::bind(ui->lineBackdropName, m_info_copy.background_name);
	LcfWidgetBinding::bind(ui->comboTileset, m_map_copy.chipset_id);
	LcfWidgetBinding::bind(ui->comboWrapping, m_map_copy.scroll_type);
	LcfWidgetBinding::bind(ui->spinWidth, m_map_copy.width);
	LcfWidgetBinding::bind(ui->spinHeight, m_map_copy.height);
	LcfWidgetBinding::bind(ui->groupPanorama, m_map_copy.parallax_flag);
	LcfWidgetBinding::bind(ui->groupHorizontalScroll, m_map_copy.parallax_loop_x);
	LcfWidgetBinding::bind(ui->groupVerticalScroll, m_map_copy.parallax_loop_y);
	LcfWidgetBinding::bind(ui->checkHorizontalAutoscroll, m_map_copy.parallax_auto_loop_x);
	LcfWidgetBinding::bind(ui->checkVerticalAutoscroll, m_map_copy.parallax_auto_loop_y);
	LcfWidgetBinding::bind(ui->spinHorizontalScrollSpeed, m_map_copy.parallax_sx);
	LcfWidgetBinding::bind(ui->spinVerticalScrollSpeed, m_map_copy.parallax_sy);
	LcfWidgetBinding::bind(m_buttonGroupBGM, m_info_copy.music_type);
	LcfWidgetBinding::bind(m_buttonGroupBackdrop, m_info_copy.background_type);
	LcfWidgetBinding::bind(m_buttonGroupTeleport, m_info_copy.teleport);
	LcfWidgetBinding::bind(m_buttonGroupEscape, m_info_copy.escape);
	LcfWidgetBinding::bind(m_buttonGroupSave, m_info_copy.save);

	ui->spinHorizontalScrollSpeed->setEnabled(m_map_copy.parallax_auto_loop_x);
	ui->spinVerticalScrollSpeed->setEnabled(m_map_copy.parallax_auto_loop_y);

	updateGeneratorTiles();
	connect(ui->comboTileset, &QComboBox::currentIndexChanged, this, [this] { updateGeneratorTiles(); });
}

MapPropertiesDialog::~MapPropertiesDialog()
{
	delete m_panoramaItem;

	delete ui;
}

void MapPropertiesDialog::accept() {
	int width = m_map_copy.width;
	int height = m_map_copy.height;
	if (width < old_width || height < old_height) {
		int result = QMessageBox::question(this,
			tr("Shrink map"),
			QString(tr("You are about to shrink the current map. All out of bounds map data and events will be deleted. This cannot be undone. Do you want to continue?")),
			QMessageBox::Yes | QMessageBox::No);

		if (result != QMessageBox::Yes) {
			return;
		}
	}

	if (ui->groupPanorama->isChecked()) {
		m_map_copy.parallax_name = new_panorama;
	} else {
		m_map_copy.parallax_flag = false;
		m_map_copy.parallax_name = ToDBString("");
		m_map_copy.parallax_loop_x = false;
		m_map_copy.parallax_loop_y = false;
		m_map_copy.parallax_auto_loop_x = false;
		m_map_copy.parallax_auto_loop_y = false;
		m_map_copy.parallax_sx = 0;
		m_map_copy.parallax_sy = 0;
	}

	new_music.name = ui->lineBGMname->text().toStdString();
	m_info_copy.music = new_music;

	m_info_copy.encounters = m_encounters->encounters();
	m_info_copy.encounter_steps = ui->spinEncounterRate->value();

	// Random dungeon generator
	m_map_copy.generator_flag = ui->groupUseGenerator->isChecked();
	if (ui->radioDungeonSinglePassage->isChecked()) {
		m_map_copy.generator_mode = lcf::rpg::Map::GeneratorMode_single_passage;
	} else if (ui->radioDungeonLinkedRooms->isChecked()) {
		m_map_copy.generator_mode = lcf::rpg::Map::GeneratorMode_linked_rooms;
	} else if (ui->radioDungeonMaze->isChecked()) {
		m_map_copy.generator_mode = lcf::rpg::Map::GeneratorMode_maze_passage;
	} else if (ui->radioDungeonOpenRoom->isChecked()) {
		m_map_copy.generator_mode = lcf::rpg::Map::GeneratorMode_open_room;
	}
	if (ui->radioDungeonPassage1_1->isChecked() || ui->radioDungeonPassage2_2->isChecked()) {
		m_map_copy.generator_tiles = ui->radioDungeonPassage2_2->isChecked()
			? lcf::rpg::Map::GeneratorTiles_two : lcf::rpg::Map::GeneratorTiles_one;
	}
	// only this mode shows them: other maps keep values outside 2..20 x 2..15
	if (ui->radioDungeonLinkedRooms->isChecked()) {
		m_map_copy.generator_width = ui->spinDungeonRoomWidth->value();
		m_map_copy.generator_height = ui->spinDungeonRoomHeight->value();
	}
	m_map_copy.generator_surround = ui->checkDungeonSurroundWithWalls->isChecked();
	m_map_copy.generator_upper_wall = ui->groupUpperWall->isChecked();
	m_map_copy.generator_floor_b = ui->groupFloorB->isChecked();
	m_map_copy.generator_floor_c = ui->groupFloorC->isChecked();
	m_map_copy.generator_extra_b = ui->groupObstacleB->isChecked();
	m_map_copy.generator_extra_c = ui->groupObstacleC->isChecked();
	if (m_generatorChanged) {
		m_map_copy.generator_x = m_generatorX;
		m_map_copy.generator_y = m_generatorY;
		m_map_copy.generator_tile_ids = m_generatorTiles;
	}

	// Resize map if map bounds have been changed
	if (width != old_width || height != old_height) {
		auto old_lower_layer = m_map_copy.lower_layer;
		auto old_upper_layer = m_map_copy.upper_layer;
		int old_tile_counter = 0;

		m_map_copy.lower_layer.clear();
		m_map_copy.upper_layer.clear();
		for (int y = 0; y < height; y++) {
			if (y < old_height) {
				for (int x = 0; x < width; x++) {
					if (x < old_width) {
						m_map_copy.lower_layer.push_back(old_lower_layer[old_tile_counter]);
						m_map_copy.upper_layer.push_back(old_upper_layer[old_tile_counter]);
						old_tile_counter++;
					} else {
						m_map_copy.lower_layer.push_back(0);
						m_map_copy.upper_layer.push_back(10000);
					}
				}
				if (width < old_width) {
					old_tile_counter += (old_width - width);
				}
			} else {
				for (int x = 0; x < width; x++) {
					m_map_copy.lower_layer.push_back(0);
					m_map_copy.upper_layer.push_back(10000);
				}
			}
		}

		// Delete out of bounds events
		if (width < old_width || height < old_height) {
			std::vector<lcf::rpg::Event>::iterator ev = m_map_copy.events.begin();
			while (ev != m_map_copy.events.end()) {
				if (ev->x >= width || ev->y >= height) {
					ev = m_map_copy.events.erase(ev);
				} else {
					++ev;
				}
			}
		}
	}

	m_info = m_info_copy;
	m_map = m_map_copy;

	QDialog::accept();
}

void MapPropertiesDialog::on_groupPanorama_toggled(bool arg1)
{
	m_panoramaItem->setVisible(arg1);
}

QToolButton* MapPropertiesDialog::generatorButton(int box) const {
	const std::array<QToolButton*, GeneratorBoxes> buttons = {
		ui->toolCeiling, ui->toolLowerWall, ui->toolUpperWall,
		ui->toolFloorA, ui->toolFloorB, ui->toolFloorC,
		ui->toolObstacleA, ui->toolObstacleB, ui->toolObstacleC};
	return buttons[static_cast<size_t>(box)];
}

QRect MapPropertiesDialog::generatorCells(int box) const {
	if (box < ObstacleA) {
		const int terrain = std::clamp(core().translate(static_cast<short>(m_generatorTiles[static_cast<size_t>(box)])), 0, 161);
		return QRect(terrain % 6, terrain / 6, 1, 1);
	}
	// an obstacle of 1 x 1, 2 x 1 or 1 x 2 tiles leaves the other cells at 10000
	const size_t first = 6 + static_cast<size_t>(box - ObstacleA) * 4;
	const int tile = std::clamp(core().translate(static_cast<short>(m_generatorTiles[first])) - 162, 0, 143);
	const bool wide = m_generatorTiles[first + 1] != 10000 || m_generatorTiles[first + 3] != 10000;
	const bool tall = m_generatorTiles[first + 2] != 10000 || m_generatorTiles[first + 3] != 10000;
	return QRect(tile % 6, tile / 6, wide ? 2 : 1, tall ? 2 : 1);
}

void MapPropertiesDialog::setGeneratorCells(int box, const QRect& cells) {
	m_generatorX[static_cast<size_t>(box)] = static_cast<uint32_t>(cells.x());
	m_generatorY[static_cast<size_t>(box)] = static_cast<uint32_t>(cells.y());
	if (box < ObstacleA) {
		const int terrain = cells.y() * 6 + cells.x();
		// As RPG Maker stores them: autotiles with the pattern 49, which the
		// generator replaces by the borders of their neighbours
		int16_t tile;
		if (terrain < 3) {
			tile = static_cast<int16_t>(terrain * 1000 + 49);
		} else if (terrain < 6) {
			tile = static_cast<int16_t>(3000 + (terrain - 3) * 50);
		} else if (terrain < 18) {
			tile = static_cast<int16_t>(4000 + (terrain - 6) * 50 + 49);
		} else {
			tile = static_cast<int16_t>(5000 + terrain - 18);
		}
		m_generatorTiles[static_cast<size_t>(box)] = tile;
	} else {
		const size_t first = 6 + static_cast<size_t>(box - ObstacleA) * 4;
		for (int i = 0; i < 4; i++) {
			const int dx = i % 2;
			const int dy = i / 2;
			const bool used = dx < cells.width() && dy < cells.height();
			m_generatorTiles[first + static_cast<size_t>(i)] = static_cast<int16_t>(
				used ? 10000 + (cells.y() + dy) * 6 + cells.x() + dx : 10000);
		}
	}
	m_generatorChanged = true;
}

void MapPropertiesDialog::pickGeneratorTiles(int box) {
	const bool obstacle = box >= ObstacleA;
	ChipSelectorDialog dialog(ui->comboTileset->currentData().toInt(),
		obstacle ? Core::UPPER : Core::LOWER, obstacle ? QSize(2, 2) : QSize(1, 1), this);
	dialog.setSelection(generatorCells(box));
	if (dialog.exec() != QDialog::Accepted || dialog.selection().isEmpty()) {
		return;
	}
	setGeneratorCells(box, dialog.selection());
	updateGeneratorTiles();
}

void MapPropertiesDialog::updateGeneratorTiles() {
	const int size = 32;
	const int chipset_id = ui->comboTileset->currentData().toInt();
	if (chipset_id != m_paletteChipset) {
		// the chipset of this map, which may not be the one of the open map
		PreviewChipset chipset(chipset_id);
		core().setBackground(Core::transparencyBrush().texture());
		m_lowerTiles = core().paletteTiles(Core::LOWER, size);
		core().beginPainting(m_lowerTiles);
		core().renderTile(core().translate(2, 0, 240), QRect(2 * size, 0, size, size));
		core().endPainting();
		m_upperTiles = core().paletteTiles(Core::UPPER, size);
		m_paletteChipset = chipset_id;
	}
	for (int box = 0; box < GeneratorBoxes; box++) {
		const QRect cells = generatorCells(box);
		const bool obstacle = box >= ObstacleA;
		const QPixmap& tiles = obstacle ? m_upperTiles : m_lowerTiles;
		QPixmap icon(obstacle ? 2 * size : size, obstacle ? 2 * size : size);
		icon.fill(Qt::transparent);
		QPainter painter(&icon);
		painter.drawPixmap(QRect(QPoint(0, 0), cells.size() * size), tiles,
			QRect(cells.topLeft() * size, cells.size() * size));
		painter.end();
		generatorButton(box)->setIcon(QIcon(icon));
	}
}

void MapPropertiesDialog::on_pushAddEncounter_clicked() {
	m_encounters->add();
}

void MapPropertiesDialog::on_pushRemoveEncounter_clicked() {
	m_encounters->remove();
}

void MapPropertiesDialog::on_pushSetPanorama_clicked() {
	auto* widget = new PickerPanoramaWidget(this);
	PickerDialog dialog(m_project, FileFinder::FileType::Image, widget, this);
	QObject::connect(&dialog, &PickerDialog::fileSelected, [&](const QString& baseName) {
		new_panorama = ToDBString(baseName);
	});
	dialog.setDirectoryAndFile(PANORAMA, ToQString(new_panorama));
	dialog.exec();

	if (!new_panorama.empty()) {
		QPixmap pix = QPixmap(m_project.project().findFile(PANORAMA, ToQString(new_panorama), FileFinder::FileType::Image));
		if (!pix) {
			pix = QPixmap(core().rtpPath(PANORAMA, ToQString(new_panorama)));
		}
		m_panoramaItem->setPixmap(pix);
	}
}

void MapPropertiesDialog::on_toolSetBGM_clicked() {
	auto* widget = new PickerAudioWidget(new_music, this);
	PickerDialog dialog(m_project, FileFinder::FileType::Music, widget, this);
	QObject::connect(&dialog, &PickerDialog::fileSelected, [&](const QString& baseName) {
		ui->lineBGMname->setText(baseName);
		new_music.name = baseName.toStdString();
		new_music.fadein = widget->fadeInTime();
		new_music.volume = widget->volume();
		new_music.tempo = widget->tempo();
		new_music.balance = widget->balance();
	});
	dialog.setDirectoryAndFile(MUSIC, ui->lineBGMname->text());
	dialog.exec();
}

void MapPropertiesDialog::on_toolSetBackdrop_clicked() {
	auto* widget = new PickerBackdropWidget(this);
	PickerDialog dialog(m_project, FileFinder::FileType::Image, widget, this);
	QObject::connect(&dialog, &PickerDialog::fileSelected, [&](const QString& baseName) {
		ui->lineBackdropName->setText(baseName);
	});
	dialog.setDirectoryAndFile(BACKDROP, ui->lineBackdropName->text());
	dialog.exec();
}
